#ifndef INCLUDE_IFU_H
#define INCLUDE_IFU_H

#include <memory>
#include <vector>
#include <queue>

#include "task/dyn_insn.h"
#include "mem/mem_transaction.h"
#include "mem/cache.h"
using namespace std;

class Cache;

class IFU
{

public:
    IFU(std::shared_ptr<Cache> _cache) : cache(_cache) {}
    void IF0(std::vector<std::shared_ptr<DynamicOperation>> &FTQ, uint64_t cycle);
    void IF1(std::vector<std::shared_ptr<DynamicOperation>> &processedOps,
              std::vector<std::shared_ptr<DynamicOperation>> &fetchBuffer,
              std::vector<std::shared_ptr<DynamicOperation>> &insnQueue, std::queue<int> &fetchCount, uint64_t cycle, uint64_t insnQueueMaxSize);
    
    void fakeIF(std::vector<std::shared_ptr<DynamicOperation>> &FTQ, std::vector<std::shared_ptr<DynamicOperation>> &processedOps,
              std::vector<std::shared_ptr<DynamicOperation>> &fetchBuffer,
               std::queue<int> &fetchCount, uint64_t cycle);

    int getPackageSize() { return packageSize; }
    void recvResponse(MemTransaction *resp) { responses.push_back(resp); }

    void notifyNoStall(){occupied = false;}
    bool isOccupied() { return occupied; }

private:
    int packageSize = 4;
    bool occupied = false;
    std::vector<MemTransaction *> responses;

    void sendReqToCache(MemTransaction *req);
    std::shared_ptr<Cache> cache;

    std::vector<int> prefetchedAddrs;
    int pfPtr = 0;
};

#endif
