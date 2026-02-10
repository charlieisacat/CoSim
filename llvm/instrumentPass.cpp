#include "llvm/Pass.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Transforms/Utils/ModuleUtils.h"

#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/IntrinsicInst.h"

#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"

#include <string>
#include <vector>
#include <map>
#include <set>

using namespace llvm;
using namespace std;

static cl::opt<uint64_t> tgt_threshold("threshold",
                                       cl::desc("dump threshold"), cl::Optional);
static cl::opt<uint64_t> tgt_interval("interval",
                                      cl::desc("interval"), cl::Optional);
static cl::opt<int> simpoint("simpoint",
                             cl::desc("simpoint"), cl::Optional);

namespace
{
    // Convert instrumentPass to the new-style pass by inheriting from PassInfoMixin
    struct instrumentPass : public PassInfoMixin<instrumentPass>
    {
        // Remove the ID member; the new pass manager does not require it
        
        // Keep the original member variables
        GlobalVariable *TotalInstCount = nullptr;
        GlobalVariable *T = nullptr;
        FunctionCallee update_insncount_interval;
        FunctionCallee update_dump_status;
        GlobalVariable *global_flag_value = nullptr;

        map<BasicBlock *, GlobalVariable *> bbName;
        map<Function *, GlobalVariable *> func_name_map;

        Value *materializeToI64(Value *V, IRBuilder<> &Builder, Type *I64Ty) {
            Type *Ty = V->getType();
            if (Ty->isPointerTy()) {
                return Builder.CreatePtrToInt(V, I64Ty);
            }

            if (Ty->isIntegerTy()) {
                unsigned Width = Ty->getIntegerBitWidth();
                if (Width < 64) {
                    return Builder.CreateZExt(V, I64Ty);
                }
                if (Width > 64) {
                    return Builder.CreateTrunc(V, I64Ty);
                }
                if (Ty == I64Ty) {
                    return V;
                }
                return Builder.CreateBitCast(V, I64Ty);
            }

            if (Ty->isFloatingPointTy()) {
                unsigned Width = Ty->getPrimitiveSizeInBits();
                IntegerType *IntTy = IntegerType::get(Builder.getContext(), Width);
                Value *Bitcast = Builder.CreateBitCast(V, IntTy);
                if (Width < 64) {
                    return Builder.CreateZExt(Bitcast, I64Ty);
                }
                if (Width > 64) {
                    return Builder.CreateTrunc(Bitcast, I64Ty);
                }
                return Bitcast;
            }

            return nullptr;
        }

        void instrumentBasicBlock(BasicBlock &BB, Module &M)
        {
            LLVMContext &Ctx = BB.getContext();
            IRBuilder<> Builder(&BB);
            Instruction *Terminator = BB.getTerminator();
            Builder.SetInsertPoint(Terminator);

            uint64_t InstCount = BB.size();
            auto inc = ConstantInt::get(Type::getInt64Ty(Ctx), InstCount);
            auto threshold = ConstantInt::get(Type::getInt64Ty(Ctx), tgt_threshold);
            auto interval = ConstantInt::get(Type::getInt64Ty(Ctx), tgt_interval);

            Value *inst_count_ptr = Builder.CreateBitCast(TotalInstCount, PointerType::get(Type::getInt64Ty(Ctx), 0));
            Value *interval_ptr = Builder.CreateBitCast(T, PointerType::get(Type::getInt64Ty(Ctx), 0));

            Builder.CreateCall(update_insncount_interval, {inst_count_ptr, interval_ptr, threshold, inc, bbName[&BB]});

            Builder.SetInsertPoint(&BB, BB.getFirstInsertionPt());
            Value *flag_ptr = Builder.CreateBitCast(global_flag_value, PointerType::get(Type::getInt8Ty(Ctx), 0));
            Function *func = BB.getParent();
            Value *func_name = Builder.CreateBitCast(func_name_map[func], PointerType::get(Type::getInt8Ty(Ctx), 0));
            Builder.CreateCall(update_dump_status, {interval_ptr, interval, flag_ptr, bbName[&BB], func_name});
        }

        // Replace runOnModule with run to match the new-pass-manager API
        PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM)
        {
            LLVMContext &Ctx = M.getContext();
            auto I64Ty = Type::getInt64Ty(Ctx);
            auto I32Ty = Type::getInt32Ty(Ctx);
            auto VoidTy = Type::getVoidTy(Ctx);
            auto I8Ty = Type::getInt8Ty(Ctx);

            IRBuilder<> Builder(Ctx);

            const std::set<std::string> InterfaceFunctionNames = {
                "cosim_preload",
                "cosim_matrix_multiply",
                "cosim_matrix_multiply_flip",
                "cosim_matrix_multiply_stay",
                "cosim_fence",
                "cosim_config",
                "cosim_mvin_acc",
                "cosim_mvin",
                "cosim_mvout",
                "cosim_fake"
            };

            FunctionCallee dump_data_func = M.getOrInsertFunction("log_helper_dump_data", VoidTy, I64Ty, I8Ty);

            errs() << "thre=" << tgt_threshold << " inter=" << tgt_interval << " simpoint=" << simpoint << "\n";

            global_flag_value = new GlobalVariable(
                M,
                Type::getInt8Ty(Ctx),                                     // Type: uint8_t (i8)
                false,                                                    // isConstant
                GlobalValue::ExternalLinkage,                             // linkage type
                ConstantInt::get(Type::getInt8Ty(Ctx), simpoint ? 0 : 1), // initializer
                "global_flag_value"                                       // name
            );

            TotalInstCount = new GlobalVariable(
                M, Type::getInt64Ty(Ctx), false, GlobalValue::CommonLinkage,
                ConstantInt::get(Type::getInt64Ty(Ctx), 0), "TotalInstCount");

            T = new GlobalVariable(
                M, Type::getInt64Ty(Ctx), false, GlobalValue::CommonLinkage,
                ConstantInt::get(Type::getInt64Ty(Ctx), 0), "T");

            update_insncount_interval = M.getOrInsertFunction("update_insncount_interval", VoidTy, PointerType::get(I64Ty, 0), PointerType::get(I64Ty, 0), I64Ty, I64Ty, PointerType::get(I32Ty, 0));
            update_dump_status = M.getOrInsertFunction("update_dump_status", VoidTy, PointerType::get(I64Ty, 0), I64Ty, PointerType::get(I8Ty, 0), PointerType::get(I32Ty, 0), PointerType::get(I8Ty, 0));

            if (simpoint)
            {
                for (Function &F : M)
                {
                    if (!F.isDeclaration() && !F.isIntrinsic())
                    {
                        string fname = F.getName().str();

                        func_name_map[&F] = new GlobalVariable(
                            M,
                            ArrayType::get(Type::getInt8Ty(Ctx), fname.size() + 1), // +1 for null terminator
                            true,                                                   // isConstant
                            GlobalValue::ExternalLinkage,
                            nullptr);

                        // Create an initializer for the global variable
                        std::vector<Constant *> char_array;
                        for (char c : fname)
                        {
                            char_array.push_back(ConstantInt::get(Type::getInt8Ty(Ctx), c));
                        }
                        char_array.push_back(ConstantInt::get(Type::getInt8Ty(Ctx), 0)); // null terminator

                        // Create a global variable initializer
                        func_name_map[&F]->setInitializer(ConstantArray::get(ArrayType::get(Type::getInt8Ty(Ctx), fname.size() + 1), char_array));
                    }

                    for (BasicBlock &BB : F)
                    {
                        string name = BB.getName().str();
                        name.erase(remove(name.begin(), name.end(), 'r'), name.end());
                        int id = stoi(name);

                        assert(!bbName.count(&BB));

                        bbName[&BB] = new GlobalVariable(
                            M,
                            I32Ty,
                            true,
                            GlobalValue::ExternalLinkage,
                            ConstantInt::get(I32Ty, id));
                    }
                }

                for (auto &F : M)
                {
                    for (BasicBlock &BB : F)
                    {
                        instrumentBasicBlock(BB, M);
                    }
                }
            }

            for (auto &F : M)
            {
                auto fname = F.getName().str();
                if (fname == "update_dump_status" ||
                    fname == "update_insncount_interval" ||
                    fname == "log_helper_dump_data" ||
                    fname == "log_helper_flush_buffer" ||
                    fname == "log_helper_init" ||
                    fname == "log_helper_finish" ||
                    fname == "cosim_config" ||
                    fname == "cosim_preload" ||
                    fname == "cosim_matrix_multiply" ||
                    fname == "cosim_matrix_multiply_flip" ||
                    fname == "cosim_matrix_multiply_stay" ||
                    fname == "cosim_fence" ||
                    fname == "cosim_mvin_acc" ||
                    fname == "cosim_mvin" ||
                    fname == "cosim_mvout" ||
                    fname == "cosim_fake")
                    continue;

                for (BasicBlock &BB : F)
                {
                    for (Instruction &I : BB)
                    {
                        if (LoadInst *LI = dyn_cast<LoadInst>(&I))
                        {
                            IRBuilder<> Builder(LI);
                            Value *Address = LI->getPointerOperand();
                            if (Address == global_flag_value || Address == TotalInstCount || Address == T)
                            {
                                continue;
                            }

                            Value *flag_value = Builder.CreateLoad(global_flag_value->getValueType(), global_flag_value);
                            Value *CastAddress = Builder.CreatePtrToInt(Address, I64Ty);
                            Builder.CreateCall(dump_data_func, {CastAddress, flag_value});
                        }
                        else if (StoreInst *SI = dyn_cast<StoreInst>(&I))
                        {
                            IRBuilder<> Builder(SI);
                            Value *Address = SI->getPointerOperand();
                            if (Address == global_flag_value || Address == TotalInstCount || Address == T)
                            {
                                continue;
                            }

                            Value *flag_value = Builder.CreateLoad(global_flag_value->getValueType(), global_flag_value);
                            Value *CastAddress = Builder.CreatePtrToInt(Address, I64Ty);
                            Builder.CreateCall(dump_data_func, {CastAddress, flag_value});
                        }
                        else if (BranchInst *BI = dyn_cast<BranchInst>(&I))
                        {
                            IRBuilder<> Builder(BI);
                            Value *Cond = nullptr;

                            if (BI->isConditional())
                            {
                                Cond = BI->getCondition();
                                Value *flag_value = Builder.CreateLoad(global_flag_value->getValueType(), global_flag_value);
                                Value *CondAsInt = Builder.CreateZExtOrBitCast(Cond, I64Ty);
                                Builder.CreateCall(dump_data_func, {CondAsInt, flag_value});
                            }
                        }
                        else if (SwitchInst *SWI = dyn_cast<SwitchInst>(&I))
                        {
                            IRBuilder<> Builder(SWI);
                            Value *Cond = nullptr;
                            Cond = SWI->getCondition();

                            Value *CondAsInt = Builder.CreateZExtOrBitCast(Cond, I64Ty);
                            Value *flag_value = Builder.CreateLoad(global_flag_value->getValueType(), global_flag_value);
                            Builder.CreateCall(dump_data_func, {CondAsInt, flag_value});
                        }
                        else if (CallBase *CB = dyn_cast<CallBase>(&I))
                        {
                            Function *Called = CB->getCalledFunction();
                            if (!Called)
                            {
                                continue;
                            }

                            if (!InterfaceFunctionNames.count(Called->getName().str()))
                            {
                                continue;
                            }

                            IRBuilder<> Builder(CB);

                            unsigned NumArgs = CB->arg_size();

                            if(NumArgs == 0)
                                continue;

                            Value *flag_value = Builder.CreateLoad(global_flag_value->getValueType(), global_flag_value);

                            for (unsigned ArgIdx = 0; ArgIdx < NumArgs; ++ArgIdx)
                            {
                                Value *Arg = CB->getArgOperand(ArgIdx);
                                Value *AsI64 = materializeToI64(Arg, Builder, I64Ty);
                                if (!AsI64)
                                {
                                    continue;
                                }

                                Builder.CreateCall(dump_data_func, {AsI64, flag_value});
                            }
                        }
                    }
                }
            }



            FunctionType *collectTy = FunctionType::get(Builder.getVoidTy(), false);
            Function *start = Function::Create(collectTy, Function::ExternalLinkage,
                                               "tracer_start", &M);
            BasicBlock *startBB = BasicBlock::Create(M.getContext(), "startBB",
                                                     start);
            Builder.SetInsertPoint(startBB);
            appendToGlobalCtors(M, start, 0);

            FunctionCallee init_func = M.getOrInsertFunction("log_helper_init", VoidTy);
            Builder.CreateCall(init_func);
            Builder.CreateRetVoid();

            Function *finish = Function::Create(collectTy, Function::ExternalLinkage,
                                                "tracer_finish", &M);
            BasicBlock *finishBB = BasicBlock::Create(M.getContext(), "finishBB",
                                                      finish);
            Builder.SetInsertPoint(finishBB);
            appendToGlobalDtors(M, finish, 0);

            FunctionCallee finish_fnc = M.getOrInsertFunction("log_helper_finish", VoidTy);
            Builder.CreateCall(finish_fnc);
            Builder.CreateRetVoid();

            // New-style passes return PreservedAnalyses instead of bool
            return PreservedAnalyses::none();
        }
    };
}

// Remove legacy RegisterPass/RegisterStandardPasses registration

// LLVM pass plugin initialization (new-style registration)
extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo llvmGetPassPluginInfo() {
    return {
        LLVM_PLUGIN_API_VERSION,   // Plugin API version
        "InstrumentPassPlugin",    // Plugin name
        "v1.0",                    // Plugin version
        [](PassBuilder &PB) {      // Pass registration callback
            PB.registerPipelineParsingCallback(
                [](StringRef Name, ModulePassManager &MPM, ArrayRef<PassBuilder::PipelineElement> PE) {
                    if (Name == "instrumentPass") {
                        MPM.addPass(instrumentPass());
                        return true;
                    }
                    return false;
                });
        }
    };
}