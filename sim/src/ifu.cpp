#include "ifu.h"

void IFU::fakeIF(std::vector<std::shared_ptr<DynamicOperation>> &FTQ, std::vector<std::shared_ptr<DynamicOperation>> &processedOps,
                 std::vector<std::shared_ptr<DynamicOperation>> &fetchBuffer, std::queue<int> &fetchCount, uint64_t cycle)
{

    int n = fetchCount.front();
    fetchCount.pop();

    auto op = FTQ[0];

    assert(processedOps.size() >= 0);
    assert(processedOps.front()->getUID() == op->getUID());

    // add ops to fetchBuffer from processedOps
    for (int i = 0; i < n; i++)
    {
        if (processedOps.size() == 0)
            break;

        fetchBuffer.push_back(processedOps[0]);

        processedOps[0]->F0 = cycle;
        // processedOps[0]->F1 = cycle;

        // remove the op from processedOps
        processedOps.erase(processedOps.begin());
    }

    FTQ.erase(FTQ.begin());
}

void IFU::IF0(std::vector<std::shared_ptr<DynamicOperation>> &FTQ, uint64_t cycle)
{
    if (FTQ.size() == 0)
        return;

    pfPtr = (pfPtr + 1) % 32;

    // will never occupied for the first request
    if (!occupied)
    {
        auto op = FTQ[0];
        MemTransaction *req = new MemTransaction(op->getDynID(), 0, op, op->getUID() * 4, true, true);

        if (cache->willAcceptTransaction(req))
        {
            op->F0 = cycle;
            occupied = true;
            sendReqToCache(req);

            // remove the op from FTQ

            // cout << "FTQ erase = " << FTQ[0]->getUID() << "\n";
            FTQ.erase(FTQ.begin());

            // since the front item is removed
            pfPtr--;
        }
        else
        {
            delete req;
        }
    }

#if 0
    // TODO: move to ICache
    // prefetch (fetch directed prefetching)
    if (FTQ.size() >= 1 && pfPtr < FTQ.size())
    {
        auto nextOp = FTQ[pfPtr];

        // cancel prefetch if addr has been requested
        if(count(prefetchedAddrs.begin(), prefetchedAddrs.end(), nextOp->getUID() * 4))
        {
            return;
        }

        if (prefetchedAddrs.size() >= 32)
        {
            prefetchedAddrs.erase(prefetchedAddrs.begin());
        }
        prefetchedAddrs.push_back(nextOp->getUID() * 4);
        MemTransaction *prefetch_t = new MemTransaction(nextOp->getDynID(), -2,
                                                        nextOp, nextOp->getUID() * 4, true, true);
        prefetch_t->isPrefetch = true;
        if (cache->willAcceptTransaction(prefetch_t))
        {
            sendReqToCache(prefetch_t);
        }
    }
#endif
}

void IFU::sendReqToCache(MemTransaction *req)
{
    // recvResponse(req);
    cache->recvRequest(req);
}

void IFU::IF1(std::vector<std::shared_ptr<DynamicOperation>> &processedOps,
              std::vector<std::shared_ptr<DynamicOperation>> &fetchBuffer,
              std::vector<std::shared_ptr<DynamicOperation>> &to_decode, std::queue<int> &fetchCount, uint64_t cycle, uint64_t insnQueueMaxSize)
{
    // assert(responses.size() <= 1);

    if (responses.size())
    {
        auto resp = responses[0];
        if (!resp->op || resp->op->isSquashed)
        {
            assert(0);
            return;
        }

        int n = fetchCount.front();
        fetchCount.pop();

        uint64_t uid = resp->op->getUID();

        assert(processedOps.size() >= 0);
        // cout<<"cycle= "<<cycle<<" id="<<processedOps.front()->getUID()<<" uid="<<uid<<" n="<<n<<"\n";

        assert(processedOps.front()->getUID() == uid);
        // add ops to fetchBuffer from processedOps
        for (int i = 0; i < n; i++)
        {
            if (processedOps.size() == 0)
                break;

            // cout<<"     add uid="<<processedOps[0]->getUID()<<"\n";

            fetchBuffer.push_back(processedOps[0]);

            processedOps[0]->F0 = resp->op->F0;

            // remove the op from processedOps
            processedOps.erase(processedOps.begin());
        }

#if 1
        if (resp->signal_no_stall)
        {
            occupied = false;
        }
        responses.erase(responses.begin());
#endif

        // occupied = false;
        // responses.clear();
        delete resp;
    }
}