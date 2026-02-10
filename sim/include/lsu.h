#ifndef INCLUDE_LSU_H
#define INCLUDE_LSU_H
#include <cstdint>
#include <vector>
#include <memory>
#include <map>

#include "task/llvm.h"
#include "task/dyn_insn.h"
#include "mem/mem_transaction.h"
#include "mem/cache.h"

class Cache;

struct LDSTQ
{
    int size = 0;
    int usedSize = 0;

    std::list<std::shared_ptr<DynamicOperation>> instList;

    LDSTQ(int _size)
    {
        size = _size;
    }

    void addInstruction(std::shared_ptr<DynamicOperation> op)
    {
        assert(op->isLoad() || op->isStore());
        // std::cout << "add entry op type=" << op->isLoad() << " ---" << op->isStore() <<" dynID="<<op->getDynID() << "\n";
        // std::cout << "used size=" << usedSize << "\n";

        instList.push_back(op);
        usedSize++;
    }

    // only load will call this
    void completeInstruction(std::shared_ptr<DynamicOperation> op)
    {
        // std::cout << "dynID=" << op->getDynID() << " name=" << op->getOpName() << " isload=" << op->isLoad() << "\n";
        assert(op->status == DynOpStatus::EXECUTING ||
               op->status == DynOpStatus::SLEEP);
        // update status in writeback
        op->setFinish(true);
        op->updateStatus(DynOpStatus::FINISHED);
    }

    void commitMemOps(InstSeqNum doneSN)
    { 
        auto order_it = instList.begin();
        auto order_end_it = instList.end();

        // if(order_it != order_end_it)
            // std::cout<<((*order_it)->isLoad()?"Load":"Store:") <<" donSN=" << doneSN<< " dynid="<< 
            // (*order_it)->getDynID() << " COmmited="<<((*order_it)->status == DynOpStatus::COMMITED)<<" issent="<< (*order_it)->isSent()<<
            // " usedSize="<<usedSize<<" commitstatus="<<(*order_it)->status<<"\n";

        while(order_it != order_end_it && ((*order_it)->status == DynOpStatus::COMMITED) && (*order_it)->isSent() && (*order_it)->getDynID() <= doneSN)
        {
            DynInstPtr inst = *order_it;

            order_it = instList.erase(order_it);
            usedSize--;
            // std::cout << "dynID=" << inst->getDynID() << " name=" << inst->getOpName() << " isload=" << inst->isLoad()<<" usedSize=" << usedSize<< "\n";
        }
    }

    bool ifFull()
    {
        return usedSize >= size;
    }

    int getUsedSize()
    {
        return usedSize;
    }
};

class LSUnit
{
public:
    LSUnit(std::shared_ptr<Cache> _cache, int ldqSize, int stqSize) : cache(_cache)
    {
        LDQ = std::make_shared<LDSTQ>(ldqSize);
        STQ = std::make_shared<LDSTQ>(stqSize);
    }

    // allocate entry in ld/st queue
    void addInstruction(std::shared_ptr<DynamicOperation> op);
    void commitInstruction(InstSeqNum doneSN);
    void wb();

    void commitStores(InstSeqNum doneSN);
    void commitLoads(InstSeqNum doneSN);

    void executeStore(std::shared_ptr<DynamicOperation> op);
    void executeLoad(std::shared_ptr<DynamicOperation> op);

    int getExecutingLoads();

    bool ifLDQFull() { return LDQ->ifFull(); }
    bool ifSTQFull() { return STQ->ifFull(); }
    
    int getLDQUsedSize() { return LDQ->getUsedSize(); }
    int getSTQUsedSize() { return STQ->getUsedSize(); }

    // Mem Transaction
    void recvResponse(MemTransaction *resp) { responses.push_back(resp); }
    void sendReqToCache(MemTransaction *req);

    // can allocate entry
    bool canDispatch(std::shared_ptr<DynamicOperation> op, bool cpu = true);

    void issue(std::shared_ptr<DynamicOperation> op);

    std::shared_ptr<DynamicOperation> forwardData(std::shared_ptr<DynamicOperation> op);

    void doForwardData(std::shared_ptr<DynamicOperation> entry);

    int lsu_port = 4;
    int used_port = 0;

    int ldq_full = 0;
    int stq_full = 0;

private:
    std::shared_ptr<LDSTQ> LDQ;
    std::shared_ptr<LDSTQ> STQ;

    // Mem Transaction
    std::vector<MemTransaction *> responses;

    // L1Cache
    std::shared_ptr<Cache> cache = nullptr;
    // cache->recvRequests();
};

#endif