#include "window.h"
#include <iostream>
#include "task/llvm.h"

bool Window::canDispatch(std::shared_ptr<DynamicOperation> insn)
{
  if (insn->isCosimFunc) return usedCosimQueueSize < cosimQueueSize;
  return issueQueueUsed < issueQueueCapacity;
}

void Window::addInstruction(std::shared_ptr<DynamicOperation> op)
{
    instList.push_back(op);
    robUsed++;
}

bool Window::retireInstruction(std::shared_ptr<Bpred> bp, std::shared_ptr<Statistic> stat, std::shared_ptr<LSUnit> lsu, uint64_t &flushDynID, uint64_t cycle, uint64_t &funcCycle)
{
    bool flush = false;
    bool memOrdering = false;
    // bool mispredict = false;
    int n = 0;

    auto order_it = instList.begin();
    auto order_end_it = instList.end();

    while(n < retireSize && order_it != order_end_it) {
        DynInstPtr inst = *order_it;

        if (inst->isCosimFunc && inst->status != DynOpStatus::FINISHED && !inst->cosimCanIssue) {
          inst->cosimCanIssue = true;
        //   std::cout<<"cycle="<<cycle<< " cosim func dynid="<<inst->getDynID()<<" can issue now\n";
          order_it++;
          n++;
          break;
        }

        if (inst->status != DynOpStatus::FINISHED) break;

        if (inst->isBr() && inst->isConditional())
        {
            if(inst->mispredict)
                bp->updateBP(inst->getUID(), inst->cond, inst->pred);

            totalPredictNum++;
            if (inst->mispredict)
                mispredictNum++;
        }
        
        inst->R = cycle;

        robUsed--;
        totalCommit++;

        stat->updateInsnCount(inst);
        doneSeqNum = inst->getDynID();

        inst->updateStatus();
        // std::cout << "retire rob=" << inst->getDynID() << " cycle = "<<cycle << " type=" << inst->getOpName() << " totalCommit=" << totalCommit << " addr=" << inst->getAddr()<<" status="<<inst->status << "\n";
        // std::cout << "sID=" << inst->getUID() << " dynID=" << inst->getDynID() << " opname=" << inst->getOpName() <<" F0="<<inst->F0<<" F1="<<inst->F1 <<" D=" << inst->D << " e=" << inst->e << " E=" << inst->E << " R=" << inst->R << "\n";

        n++;
        order_it = instList.erase(order_it);
    }

    return flush;
}

bool Window::canIssue(std::shared_ptr<DynamicOperation> op)
{
    // assert(opEntryMap.find(op->getDynID()) != opEntryMap.end());
    // if (op->status == DynOpStatus::READY)
    //     return true;

    if (op->isCosimFunc) return op->cosimCanIssue && checkInsnReady(op);
    return checkInsnReady(op);

    // auto entryID = opEntryMap[op->getDynID()];
    // return ROB[entryID]->ready;
}

bool Window::checkInsnReady(std::shared_ptr<DynamicOperation> op)
{
    bool ready = true;
    auto deps = op->getDeps();
    if(deps.size() == 0)
        return true;

    // std::cout << "ready dynid=" << op->getDynID() << " opname=" << op->getOpName() << " deps size=" << deps.size() << "\n";

    for(auto dep : deps) {
        // std::cout << "  >>> dep=" << dep->getDynID() << " ready=" << (dep->status >= DynOpStatus::FINISHED) << "\n";
        ready &= (dep->status >= DynOpStatus::FINISHED);
    }

    return ready;
}
