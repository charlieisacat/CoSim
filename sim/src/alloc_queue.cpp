#include "alloc_queue.h"

void 
AllocQueue::sendInstToRename()
{
    int total_decoded = 0;

    auto order_it = instList.begin();
    auto order_end_it = instList.end();

    while(total_decoded < totalWidth && order_it != order_end_it)
    {
        DynInstPtr inst = *order_it;

        instsToRename.push_back(inst);

        ++total_decoded;

        instList.erase(order_it++);
        ++freeEntries;
        // printf("AllocQueue: freeentries: %d\n", freeEntries);
    }
}

DynInstPtr
AllocQueue::getInstAtPos(int pos) {
    assert(pos >= 0 && pos < (int)instList.size());
    auto it = instList.begin();
    std::advance(it, pos);
    return *it;
}

bool
AllocQueue::insert(DynInstPtr new_inst, unsigned predDist)
{
    // Make sure the instruction is valid
    assert(new_inst);
    assert(freeEntries != 0);

    instList.push_back(new_inst);

    --freeEntries;
    return false;
}

DynInstPtr
AllocQueue::getInstToRename()
{
    if(instsToRename.empty())
        return NULL;

    DynInstPtr inst = instsToRename.front();
    instsToRename.pop_front();

    return inst;
}

