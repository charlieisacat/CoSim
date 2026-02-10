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
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include <map>
#include <string>

#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"

using namespace llvm;
using namespace std;

namespace
{
    struct SplitBB : public PassInfoMixin<SplitBB>
    {
        SplitBB() {}

        PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM) {
            for (Function &F : M)
            {
                vector<BasicBlock *> bb_list;
                for (BasicBlock &BB : F)
                {
                    bb_list.push_back(&BB);
                }

                for (auto bb : bb_list)
                {
                    split_bb(*bb, F);
                }
            }
            return PreservedAnalyses::none();
        }

        void split_bb(BasicBlock &BB, Function &F)
        {
            int i = 0;
            for (Instruction &I : BB)
            {
                // Check if this is a function call instruction
                auto *callInst = dyn_cast<CallInst>(&I);
                auto *invokeInst = dyn_cast<InvokeInst>(&I);
                Function *calledFunction = nullptr;

                if (callInst)
                    calledFunction = callInst->getCalledFunction();
                else if (invokeInst)
                    calledFunction = invokeInst->getCalledFunction();
                else
                {
                    i++;
                    continue;
                }

                // Assuming undeclared functions will not have definitions
                if (calledFunction && !calledFunction->isDeclaration() || !calledFunction)
                {
                    // Step 1: Split block before the call instruction
                    // if call inst is the first of a bb, don't split to avoid empty bb
                    BasicBlock *first_split = nullptr;
                    if (i != 0)
                        first_split = SplitBlock(&BB, &I);
                    else
                        first_split = &BB;

                    // Step 2: Split block after the call instruction
                    Instruction *nextInst = I.getNextNode();
                    BasicBlock *second_split = nullptr;
                    // if call inst is before a terminator,
                    // only split if next inst is conditional branch
                    if (nextInst && !nextInst->isTerminator())
                    {
                        second_split = SplitBlock(first_split, nextInst);
                    }
                    else if (nextInst && nextInst->isTerminator())
                    {
                        BranchInst *branchInst = dyn_cast<BranchInst>(nextInst);
                        if (branchInst && branchInst->isConditional())
                        {
                            second_split = SplitBlock(first_split, nextInst);
                        }
                    }

                    if (second_split)
                    {
                        split_bb(*second_split, F);
                    }

                    return; // After splitting, return to avoid modifying the iterator
                }
                i++;
            }
        }
    };
}

// Register pass
extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo llvmGetPassPluginInfo() {
    return {
        LLVM_PLUGIN_API_VERSION,   // Plugin API version
        "SplitBBPlugin",           // Plugin name
        "v1.0",                    // Plugin version
        [](PassBuilder &PB) {      // Pass registration callback
            PB.registerPipelineParsingCallback(
                [](StringRef Name, ModulePassManager &MPM, ArrayRef<PassBuilder::PipelineElement> PE) {
                    if (Name == "splitbb") {
                        MPM.addPass(SplitBB());
                        return true;
                    }
                    return false;
                });
        }
    };
}