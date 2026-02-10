#ifndef __ALLOC_QUEUE_HH__
#define __ALLOC_QUEUE_HH__ 


#include "task/dyn_insn.h"
#include <list>


class AllocQueue
{
  public:

    /** Constructs an AQ. */
      AllocQueue(unsigned _numEntries, unsigned _width) : 
        numEntries(_numEntries), totalWidth(_width), freeEntries(_numEntries) {}

      /** Destructs the AQ. */
      ~AllocQueue() {}

      /** Returns whether or not the AQ is full. */
      bool isFull() { return freeEntries == 0; }
      bool isEmpty() { return freeEntries == numEntries; }

      /** Inserts a new instruction into the AQ. */
      bool insert(DynInstPtr new_inst, unsigned predDist);

      void sendInstToRename();

      DynInstPtr getInstAtPos(int pos);
    
      std::list<DynInstPtr> instsToRename;

      bool isStall() { return instsToRename.size() > 0; }
      bool hasNoAvalInstToRename() {return instsToRename.size() == 0;}
      DynInstPtr getInstToRename();

      uint64_t size( ){return instList.size();}

      int getInstInSameCL(DynInstPtr inst);

      bool hasDependency(int start, int end, DynInstPtr end_inst);

  private:

    /** List of all the instructions in the AQ. */
    std::list<DynInstPtr> instList;

    /** Number of free AQ entries left. */
    unsigned freeEntries;

    /** The number of entries in the instruction queue. */
    unsigned numEntries;

    /** The total number of instructions that can be issued in one cycle. */
    unsigned totalWidth;

    uint64_t decodeFusedInsts = 0;
};

#endif