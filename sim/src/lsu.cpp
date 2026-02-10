#include "lsu.h"
#include <iostream>

void LSUnit::issue(std::shared_ptr<DynamicOperation> op)
{
    assert(op->status == DynOpStatus::ALLOCATED);
    op->updateStatus();
}

void LSUnit::addInstruction(std::shared_ptr<DynamicOperation> op)
{
    if (op->isLoad())
        LDQ->addInstruction(op);
    if (op->isStore())
        STQ->addInstruction(op);
}

void LSUnit::commitInstruction(InstSeqNum seqNum)
{
    commitLoads(seqNum);
    commitStores(seqNum);
}

void LSUnit::sendReqToCache(MemTransaction *req)
{
#if 1
    cache->recvRequest(req);
#else // disable cache
    if (req->isLoad)
        recvResponse(req);
#endif
}

void LSUnit::wb()
{
    for (auto resp : responses)
    {
        if (resp->op && !resp->op->isSquashed)
        {
            LDQ->completeInstruction(resp->op);

        }
        delete resp;
    }
    responses.clear();
}

int LSUnit::getExecutingLoads()
{
    int n = 0;
    for (auto op : LDQ->instList)
    {
        if (op->status == DynOpStatus::EXECUTING)
        {
            n++;
        }
    }
    return n;
}

void LSUnit::doForwardData(std::shared_ptr<DynamicOperation> op)
{
    for (auto lop : op->forwardList)
    {
        assert(lop->status == DynOpStatus::SLEEP);
        LDQ->completeInstruction(lop);
        lop->setIsSent(true);
    }
    op->forwardList.clear();
}

void LSUnit::executeLoad(std::shared_ptr<DynamicOperation> op)
{

    if (op->status == DynOpStatus::ISSUED)
    {
        // perfect data fwd
        std::shared_ptr<DynamicOperation> fwdOp = forwardData(op);
        if (fwdOp)
        {
            op->dataForwarded = true;
            op->updateStatus(DynOpStatus::SLEEP);
            op->forwardOp = fwdOp;

            if (fwdOp->status >= DynOpStatus::READY)
            {
                op->setFinish(true);
                op->setIsSent(true);
                op->updateStatus(DynOpStatus::FINISHED);
            }
            else
            {
                fwdOp->forwardList.push_back(op);
            }
        }
        else
        {
            MemTransaction *req = new MemTransaction(op->getDynID(), 0, op, op->getAddr(), op->isLoad());
            if (cache->willAcceptTransaction(req))
            {
                sendReqToCache(req);
                op->updateStatus();
                op->setIsSent(true);

            }
            else
            {
                delete req;
            }
        }
    }
}

void LSUnit::executeStore(std::shared_ptr<DynamicOperation> op)
{
    // from issued to executing
    if (op->status == DynOpStatus::ISSUED)
    {
        op->updateStatus();
    }

    if (op->status >= DynOpStatus::READY)
        doForwardData(op);

    // std::cout<<"execute store: "<< op->getDynID()<< "  status="<<op->status<<std::endl;

    if (op->status == DynOpStatus::COMMITED && !op->isSent())
    {

        MemTransaction *req = new MemTransaction(op->getDynID(), 0, op, op->getAddr(), op->isLoad());
        if (cache->willAcceptTransaction(req))
        {
            sendReqToCache(req);
            op->setIsSent(true);

        }
        else
        {
            delete req;
        }
    }

}

void LSUnit::commitLoads(InstSeqNum doneSN)
{
    LDQ->commitMemOps(doneSN);
}

void LSUnit::commitStores(InstSeqNum doneSN)
{
    STQ->commitMemOps(doneSN);
}

bool LSUnit::canDispatch(std::shared_ptr<DynamicOperation> op, bool cpu)
{
    if (cpu)
    {
        if (op->isLoad())
        {

            if (ifLDQFull())
            {
                ldq_full++;
                return false;
            }
        }
        if (op->isStore())
        {
            if (ifSTQFull())
            {
                stq_full++;
                return false;
            }
        }
        return true;
    }

    return !ifLDQFull() && !ifSTQFull();
}

std::shared_ptr<DynamicOperation> LSUnit::forwardData(std::shared_ptr<DynamicOperation> op)
{
    std::shared_ptr<DynamicOperation> ret = nullptr;
    uint64_t dynID = 0;

    for (auto sop : STQ->instList)
    {
        if (sop->getAddr() == op->getAddr() &&
            sop->getDynID() < op->getDynID())
        {
            if (dynID != 0)
            {
                assert(sop->getDynID() != dynID);
            }
            // get the youngest store insn
            if (sop->getDynID() > dynID)
            {
                ret = sop;
                dynID = sop->getDynID();
            }
        }
    }

    return ret;
}
