#ifndef INCLUDE_DYN_INSN_H
#define INCLUDE_DYN_INSN_H

#include "llvm.h"
#include "instruction.h"
#include "params.h"
#include "basic_block.h"
#include "port.h"

#include <stdio.h>
#include <iostream>
#include "utils.h"
#include "../SA/types.h"

typedef uint64_t Addr;
typedef uint64_t InstSeqNum;

using namespace std;

class Port;

struct PredictorHistory;

enum DynOpStatus
{
    INIT,
    ALLOCATED,
    READY,
    ISSUED,
    EXECUTING,
    FINISHED, // to commit
    COMMITED, // indicate that it can be fired to memory
    SLEEP     // status to indicate load is waiting for forwarding
};

class DynamicOperation;
using DynInstPtr = std::shared_ptr<DynamicOperation>;

constexpr uint64_t
mask(unsigned nbits)
{
    return (nbits >= 64) ? (uint64_t)-1LL : (1ULL << nbits) - 1;
}

// dynamic (custom) instructions created in runtime
class DynamicOperation
{
public:
    DynamicOperation() {}
    ~DynamicOperation();
    DynamicOperation(std::shared_ptr<SIM::Instruction> _static_insn, std::shared_ptr<InstConfig> config) : static_insn(_static_insn),
                                                                                                           totalCycle(config->runtime_cycles),
                                                                                                           portMapping(config->ports), fus(config->fus)
    {
        for (auto mapping : portMapping)
        {
            for (auto portID : mapping.second)
            {
                allowPorts.push_back(portID);
            }
            portStallCycles.push_back(mapping.first);
        }

        opcode = static_insn->getOpCode();

        llvm::Type *returnType = static_insn->getLLVMInsn()->getType();

        if (returnType->isFloatingPointTy())
        {
            isInt = false;
        }
    }

    void updatePortMapping(std::map<uint64_t, std::shared_ptr<InstConfig>> configs, double perc);

    bool isInt = true;

    bool flushStall = false;

    bool isVoidTy() { return static_insn->isVoidTy(); }
    bool isIntTy() { return static_insn->isIntTy(); }
    bool isFPTy() { return static_insn->isFPTy(); }

    std::vector<uint64_t> getFPOperandUIDs() { return static_insn->fpUIDs; }
    std::vector<uint64_t> getIntOperandUIDs() { return static_insn->intUIDs; }

    virtual bool isPhi() { return false; }
    virtual bool isCall() { return false; }
    virtual bool isLoad() { return false; }
    virtual bool isStore() { return false; }
    virtual bool isArith() { return false; }
    virtual bool isRet() { return false; }
    virtual bool isBr() { return false; }
    virtual bool isSwitch() { return false; }
    virtual bool isInvoke() { return false; }

    std::string getOpName() { return static_insn->getOpName(); }
    int getOpCode() { return opcode; }

    std::shared_ptr<SIM::Instruction> getStaticInsn() { return static_insn; }
    uint64_t getUID() { return static_insn->getUID(); }

    uint64_t getPC() { return static_insn->getUID(); }

    void setCurrentCycle(uint64_t _cycle) { currentCycle = _cycle; }
    virtual bool finish() { return currentCycle >= totalCycle; }
    void execute();

    // load is finished
    void setFinish(bool f) { finished = f; }

    std::vector<int> getPorts() { return portMapping[0].second; }
    std::vector<int> getAllowPorts() { return allowPorts; }

    virtual std::vector<uint64_t> getStaticDepUIDs() { return static_insn->getStaticDepUIDs(); }
    std::vector<uint64_t> getDynDeps() { return dynDeps; }
    std::vector<uint64_t> getDynUsers() { return dynUsers; }
    void addDynDeps(uint64_t dep) { dynDeps.push_back(dep); }
    void clearDynDeps() { dynDeps.clear(); }

    uint64_t getDynID() { return dynID; }
    virtual uint64_t getAddr() { return addr; }
    void updateDynID(uint64_t _dynID) { dynID = _dynID; }

    bool isSquashed = false;

    // used for function without body
    uint64_t cycle = 0;

    void reset()
    {
        setCurrentCycle(0);
        if (isLoad() || isStore())
            setFinish(false);
        isSquashed = false;
        status = DynOpStatus::INIT;
        E = 0;
        e = 0;
        D = 0;
        R = 0;
        clearDynDeps();
        freeFU();
        mispredict = false;
    }

    // use this to create different dyn instances but having same dynID
    // because we want to track if insn is squashed
    std::shared_ptr<DynamicOperation> clone() const { return std::static_pointer_cast<DynamicOperation>(createClone()); }
    virtual std::shared_ptr<DynamicOperation> createClone() const { return std::shared_ptr<DynamicOperation>(new DynamicOperation(*this)); }

    // State Machine: state tranfer
    virtual void updateStatus(DynOpStatus _status);
    virtual void updateStatus();

    DynOpStatus status = DynOpStatus::INIT;

    // llvm-mca style timeline
    uint64_t F0 = 0;
    uint64_t F1 = 0;
    uint64_t D = 0; // dispatch
    uint64_t e = 0; // execute
    uint64_t E = 0; // writeback
    uint64_t R = 0; // retire

    std::vector<std::vector<int>> fus;

    // occupied port and fu

    std::vector<int> usingFuIDs;
    std::vector<std::shared_ptr<Port>> usingPorts;

    void occupyFU(std::vector<int> avalIDs, std::vector<std::shared_ptr<Port>> avalPorts);
    void freeFU();

    std::vector<std::pair<int, std::vector<int>>> portMapping;
    std::vector<int> portStallCycles;

    // port id
    std::vector<int> allowPorts;

    std::string calleeName = "";
    std::string parentFuncName = "";

    bool mispredict = false;
    bool pred = false;
    bool cond = false;

    bool isExtractFunc = false;
    bool isPackingFunc = false;
    bool isEncapsulateFunc = false;
    std::vector<int> cpOps;
    std::vector<int> fuOps;

    void setRuntimeCycle(uint64_t cycle) { totalCycle = cycle; }

    uint32_t opcode = -1;

    std::vector<uint64_t> readRFUids;

    bool freeFUEarly() { return isEncapsulateFunc && (currentCycle >= stageCycle); }
    void setStageCycle(uint64_t _cycle) { stageCycle = _cycle; }

    uint64_t getTotalCycle() { return totalCycle; }

    // this is only used for accel
    // dependency count == 0
    bool ready() { return deps.size() == 0; }

    // for accel
    void addUser(std::shared_ptr<DynamicOperation> user) { users.push_back(user); }
    void addDep(std::shared_ptr<DynamicOperation> dep) { deps.push_back(dep); }
    std::vector<std::shared_ptr<DynamicOperation>> getDeps() { return deps; }

    void signalUser();
    void removeDep(uint64_t uid);

    std::shared_ptr<SIM::BasicBlock> targetBB = nullptr;
    uint64_t globalDynID = 0;
    bool offloaded = false;

    std::shared_ptr<DynamicOperation> prevBBTerminator = nullptr;
    bool isConditional();

    bool waitingForBrDecision = true;

    void setIsSent(bool val) {sent = val;}
    bool isSent() {return sent;}

    std::vector<std::shared_ptr<DynamicOperation>> forwardList;

    bool dataForwarded = false;
    // which op the load is forwarded from 
    std::shared_ptr<DynamicOperation> forwardOp = nullptr;

    bool cosimCanIssue = false;

    bool isCosimFunc = false;
    bool isCosimConfigFunc = false;
    bool isCosimFence = false;
    bool isPreload = false;
    bool isMatrixMultiply = false;
    bool isMatrixMultiplyFlip = false;
    bool isMatrixMultiplyStay = false;

    bool isCosimMem = false;
    bool isMvinAcc = false;
    bool isMvin = false;
    bool isMvout = false;
    bool isFake = false;

    uint64_t get_T_N() { return T_N; }
    uint64_t get_T_M() { return T_M; }
    uint64_t get_T_K() { return T_K; }

    address_t get_addr_MK() { return addr_MK; }
    address_t get_addr_MN() { return addr_MN; }
    address_t get_addr_KN() { return addr_KN; }

    uint64_t get_real_addr_MK() { return real_addr_MK; }
    uint64_t get_real_addr_MN() { return real_addr_MN; }
    uint64_t get_real_addr_KN() { return real_addr_KN; }

    //get offset
    uint64_t get_MK_offset() { return offset_to_MK; }
    uint64_t get_MN_offset() { return offset_to_MN; }
    uint64_t get_KN_offset() { return offset_to_KN; }

    //get N, M, K
    uint64_t get_N() { return N; }
    uint64_t get_M() { return M; }
    uint64_t get_K() { return K; }

    void setMatrixParams(uint64_t _T_N, uint64_t _T_M, uint64_t _T_K,
                         uint64_t _offset_to_MK, uint64_t _offset_to_KN, uint64_t _offset_to_MN)
    {
        T_N = _T_N;
        T_M = _T_M;
        T_K = _T_K;
        offset_to_MK = _offset_to_MK;
        offset_to_MN = _offset_to_MN;
        offset_to_KN = _offset_to_KN;
    }

    void setConfigParams(uint64_t _N, uint64_t _M, uint64_t _K,
                         address_t _addr_MK, address_t _addr_KN,
                         address_t _addr_MN, uint64_t _T_N, uint64_t _T_M,
                         uint64_t _T_K) {
      N = _N;
      M = _M;
      K = _K;

      addr_MK = _addr_MK;
      addr_MN = _addr_MN;
      addr_KN = _addr_KN;

      T_N = _T_N;
      T_M = _T_M;
      T_K = _T_K;
    }

    void setRealAddr(uint64_t _addr_MK, uint64_t _addr_KN, uint64_t _addr_MN) {
      real_addr_MK = _addr_MK;
      real_addr_MN = _addr_MN;
      real_addr_KN = _addr_KN;
    }

    // Cosim Mem
    uint64_t dram = 0;
    uint64_t sp = 0;
    int cols = 0;
    int rows = 0;
    
    // sp, for building depenedency
    uint64_t sp_a_addr = 0;
    uint64_t sp_b_addr = 0;
    uint64_t sp_c_addr = 0;
protected:
    std::shared_ptr<SIM::Instruction> static_insn;

    uint64_t totalCycle = 1;
    uint64_t currentCycle = 0;
    uint64_t stageCycle = 1;

    std::vector<uint64_t> dynDeps;
    std::vector<uint64_t> dynUsers;

    std::vector<std::shared_ptr<DynamicOperation>> deps;
    std::vector<std::shared_ptr<DynamicOperation>> users;

    uint64_t dynID;

    uint64_t addr = 0;

    bool finished = false;

    bool sent = false;

    uint64_t T_N = 0;
    uint64_t T_M = 0;
    uint64_t T_K = 0;

    // addresses of automatedly generated matrices
    address_t addr_MK = 0;
    address_t addr_MN = 0;
    address_t addr_KN = 0;

    uint64_t N = 0;
    uint64_t M = 0;
    uint64_t K = 0;
    
    uint64_t offset_to_MK = 0;
    uint64_t offset_to_MN = 0;
    uint64_t offset_to_KN = 0;
    
    // addresses read from trace
    uint64_t real_addr_MK = 0;
    uint64_t real_addr_MN = 0;
    uint64_t real_addr_KN = 0;

};

enum FuncType
{
    EXTERNAL,
    INTERNAL,
    INVALID
};

class SwitchDynamicOperation : public DynamicOperation
{
public:
    SwitchDynamicOperation(std::shared_ptr<SIM::Instruction> _static_insn, std::shared_ptr<InstConfig> config) : DynamicOperation(_static_insn, config)
    {

        llvm::Instruction *llvmInsn = static_insn->getLLVMInsn();
        llvm::SwitchInst *switchInsn = llvm::dyn_cast<llvm::SwitchInst>(llvmInsn);

        for (auto &Case : switchInsn->cases())
        {
            uint64_t caseValue = std::move(Case.getCaseValue()->getSExtValue());
            llvm::BasicBlock *destBB = Case.getCaseSuccessor();
            // std::cout << "caseValue=" << caseValue << "bbname=" << destBB->getName().str() << "\n";
            caseBBMap.insert(std::make_pair<int, llvm::Value *>(caseValue, destBB));
        }

        defaultCaseDest = switchInsn->getDefaultDest();
        // std::cout << "defualt case=" << defaultCaseDest->getName().str() << "\n";
    }
    virtual bool isSwitch() override { return true; }
    llvm::Value *getDestValue(int cond);
    int getCondition() { return cond; }
    void setCondition(int _cond) { cond = _cond; }

    virtual std::shared_ptr<DynamicOperation> createClone() const override { return std::shared_ptr<SwitchDynamicOperation>(new SwitchDynamicOperation(*this)); }

private:
    std::map<int, llvm::Value *> caseBBMap;
    llvm::Value *defaultCaseDest = nullptr;
    int cond = 0;
};

class CallDynamicOperation : public DynamicOperation
{
public:
    CallDynamicOperation(std::shared_ptr<SIM::Instruction> _static_insn, std::shared_ptr<InstConfig> config) : DynamicOperation(_static_insn, config)
    {
        llvm::CallInst *callInst = llvm::dyn_cast<llvm::CallInst>(static_insn->getLLVMInsn());
        auto func = getCalledFunc();

        if (func)
        {
            calleeName = func->getName().str();
            // std::cout<<"calleeName "<<calleeName<<"\n";

            // Retrieve custom metadata by key
            if (llvm::MDNode *CustomMD = func->getMetadata(func->getName().str()))
            {
                hasBody = false;
                isEncapsulateFunc = true;
                int i = 0;
                for (const auto &op : CustomMD->operands())
                {
                    auto *meta = llvm::dyn_cast<llvm::MDString>(op.get());
                    if (i == 0)
                    {
                        opcode = std::stoi(meta->getString().str());
                    }
                    else if (i == 1)
                    {
                        cpOps = splitAndConvertToInt(meta->getString().str());
                    }
                    else if (i == 2)
                    {
                        fuOps = splitAndConvertToInt(meta->getString().str());
                    }

                    i++;
                }
            }
            else if (llvm::MDNode *CustomMD = func->getMetadata("packing_func"))
            {
                opcode = 1023;
                hasBody = false;
                isPackingFunc = true;
            }
            else if (llvm::MDNode *CustomMD = func->getMetadata("extract_func"))
            {
                opcode = 1024;
                hasBody = false;
                isExtractFunc = true;
            }
            else if(calleeName.find("cosim_") != std::string::npos) {
                hasBody = false;
                isCosimFunc = true;
                if(calleeName == "cosim_config")
                    isCosimConfigFunc = true;
                else if(calleeName == "cosim_fence")
                    isCosimFence = true;
                else if(calleeName == "cosim_preload") {
                    isPreload = true;
                } else if (calleeName == "cosim_matrix_multiply") {
                    isMatrixMultiply = true;
                } else if (calleeName == "cosim_matrix_multiply_flip") {
                    isMatrixMultiplyFlip = true;
                } else if (calleeName == "cosim_matrix_multiply_stay") {
                    isMatrixMultiplyStay = true;
                } else if (calleeName == "cosim_mvin_acc") {
                    isCosimMem = true;
                    isMvinAcc = true;
                } else if (calleeName == "cosim_mvin") {
                    isCosimMem = true;
                    isMvin = true;
                } else if (calleeName == "cosim_mvout") {
                    isCosimMem = true;
                    isMvout = true;
                } else if(calleeName == "cosim_fake") {
                    isFake = true;
                }
            }
            else
            {
                // no metadata
                if (func->isDeclaration() || func->isIntrinsic())
                {
                    hasBody = false;
                }
                else
                {
                    std::cout<<"Function "<<calleeName<<" has body\n";
                    hasBody = true;
                }

            }
        }
        else
        {
            std::cout<<"Cannot get called function\n";
            hasBody = false;
        }
    }
    virtual bool isCall() override { return true; }

    void setFuncType(FuncType _funcType) { funcType = _funcType; }
    llvm::Function *getCalledFunc();
    bool shouldRead();

    virtual std::shared_ptr<DynamicOperation> createClone() const override { return std::shared_ptr<CallDynamicOperation>(new CallDynamicOperation(*this)); }

    virtual bool finish() override
    {
        if (isExtractFunc)
        {
            return finished && (currentCycle >= totalCycle);
        }

        return currentCycle >= totalCycle;
    }

    bool hasBody = false;

private:
    uint64_t latency = 0;
    FuncType funcType = FuncType::INVALID;
};

class LoadDynamicOperation : public DynamicOperation
{
public:
    LoadDynamicOperation(std::shared_ptr<SIM::Instruction> _static_insn, std::shared_ptr<InstConfig> config) : DynamicOperation(_static_insn, config) {}
    virtual bool isLoad() override { return true; }
    virtual uint64_t getAddr() override { return addr; }
    void setAddr(uint64_t _addr) { addr = _addr; }

    virtual bool finish() override { return finished && (currentCycle >= totalCycle); }

    virtual std::shared_ptr<DynamicOperation> createClone() const override { return std::shared_ptr<LoadDynamicOperation>(new LoadDynamicOperation(*this)); }

private:
    uint64_t addr = 0;
};

class StoreDynamicOperation : public DynamicOperation
{
public:
    StoreDynamicOperation(std::shared_ptr<SIM::Instruction> _static_insn, std::shared_ptr<InstConfig> config) : DynamicOperation(_static_insn, config) {}
    virtual bool isStore() override { return true; }
    virtual uint64_t getAddr() override { return addr; }
    void setAddr(uint64_t _addr) { addr = _addr; }

    virtual bool finish() override { return currentCycle >= totalCycle; }
    virtual std::shared_ptr<DynamicOperation> createClone() const override { return std::shared_ptr<StoreDynamicOperation>(new StoreDynamicOperation(*this)); }

private:
    uint64_t addr = 0;
};

class ArithDynamicOperation : public DynamicOperation
{
public:
    ArithDynamicOperation(std::shared_ptr<SIM::Instruction> _static_insn, std::shared_ptr<InstConfig> config) : DynamicOperation(_static_insn, config) {}
    virtual bool isArith() override { return true; }

    virtual std::shared_ptr<DynamicOperation> createClone() const override { return std::shared_ptr<ArithDynamicOperation>(new ArithDynamicOperation(*this)); }
};

class RetDynamicOperation : public DynamicOperation
{
public:
    RetDynamicOperation(std::shared_ptr<SIM::Instruction> _static_insn, std::shared_ptr<InstConfig> config) : DynamicOperation(_static_insn, config)
    {

        llvm::Instruction *llvmInsn = static_insn->getLLVMInsn();
        llvm::ReturnInst *ret = llvm::dyn_cast<llvm::ReturnInst>(llvmInsn);

        llvm::Function *parentFunc = ret->getFunction();
        if (parentFunc)
        {
            parentFuncName = parentFunc->getName();
        }
    }
    virtual bool isRet() override { return true; }

    virtual std::shared_ptr<DynamicOperation> createClone() const override { return std::shared_ptr<RetDynamicOperation>(new RetDynamicOperation(*this)); }
};

class BrDynamicOperation : public DynamicOperation
{
public:
    BrDynamicOperation(std::shared_ptr<SIM::Instruction> _static_insn, std::shared_ptr<InstConfig> config) : DynamicOperation(_static_insn, config)
    {
        llvm::Instruction *llvmInsn = static_insn->getLLVMInsn();
        llvm::BranchInst *br = llvm::dyn_cast<llvm::BranchInst>(llvmInsn);

        trueDestValue = br->getSuccessor(0);

        if (br->isConditional())
            falseDestValue = br->getSuccessor(1);
    }
    virtual bool isBr() override { return true; }
    bool getCondition() { return cond; }
    llvm::Value *getDestValue(bool cond) { return cond == 0 ? falseDestValue : trueDestValue; }
    void setCondition(bool _cond) { cond = _cond; }

    virtual std::shared_ptr<DynamicOperation> createClone() const override { return std::shared_ptr<BrDynamicOperation>(new BrDynamicOperation(*this)); }

private:
    llvm::Value *trueDestValue = nullptr;
    llvm::Value *falseDestValue = nullptr;
};

class InvokeDynamicOperation : public DynamicOperation
{
public:
    InvokeDynamicOperation(std::shared_ptr<SIM::Instruction> _static_insn, std::shared_ptr<InstConfig> config) : DynamicOperation(_static_insn, config)
    {
        llvm::Instruction *llvmInsn = static_insn->getLLVMInsn();
        llvm::InvokeInst *invoke = llvm::dyn_cast<llvm::InvokeInst>(llvmInsn);
        // llvm::errs()<<*invoke<<"\n";

        auto func = getCalledFunc();
        if (func)
        {
            if (func->isDeclaration() || func->isIntrinsic())
            {
                // llvm::errs()<<"--------- false\n";
                hasBody = false;
            }
            else
            {
                // llvm::errs()<<"-------- true\n";
                hasBody = true;
            }
        }
        else
        {
            hasBody = false;
        }

        normalDest = invoke->getNormalDest();
        // llvm::errs()<<*normalDest<<"\n";
    }

    virtual bool isInvoke() override { return true; }
    virtual std::shared_ptr<DynamicOperation> createClone() const override { return std::shared_ptr<InvokeDynamicOperation>(new InvokeDynamicOperation(*this)); }
    llvm::Function *getCalledFunc();
    llvm::Value *getDestValue() { return normalDest; }

    bool hasBody = false;

private:
    llvm::BasicBlock *normalDest = nullptr;
};

class PhiDynamicOperation : public DynamicOperation
{
public:
    PhiDynamicOperation(std::shared_ptr<SIM::Instruction> _static_insn, std::shared_ptr<InstConfig> config) : DynamicOperation(_static_insn, config) {}
    virtual bool isPhi() override { return true; }
    virtual std::vector<uint64_t> getStaticDepUIDs() override
    {
        if (validIncoming)
            return {incomingValueID};
        return {};
    }
    void setPreviousBB(std::shared_ptr<SIM::BasicBlock> previousBB);

    virtual std::shared_ptr<DynamicOperation> createClone() const override { return std::shared_ptr<PhiDynamicOperation>(new PhiDynamicOperation(*this)); }

private:
    // std::shared_ptr<SIM::BasicBlock> previousBB = nullptr;

    uint64_t incomingValueID = 0;
    bool validIncoming = false;
};
#endif
