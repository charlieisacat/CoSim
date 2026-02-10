// Created by Francisco Munoz Martinez on 25/06/2019

// This class is used in the simulator in order to limit the size of the fifo.

#ifndef __Fifo_h__
#define __Fifo_h__

#include <queue>
#include "DataPackage.h"
#include "types.h"
#include "Stats.h"
#include <cassert>
#include <iostream>
#include "utility.h"

class Fifo
{
private:
    std::queue<DataPackage *> fifo;
    unsigned int capacity;       // Capacity in number of bits
    unsigned int capacity_words; // Capacity in number of words allowed. i.e., capacity_words = capacity / size_word
    FifoStats fifoStats;         // Tracking parameters
public:
    Fifo(unsigned int capacity);
    bool isEmpty();
    bool isFull();
    void push(DataPackage *data);
    DataPackage *pop();
    DataPackage *front();
    unsigned int size(); // Return the number of elements in the fifo
    void printStats(std::ofstream &out, unsigned int indent);
    void printEnergy(std::ofstream &out, unsigned int indent);
};

class SyncFifo {
 private:
  std::queue<DataPackage*> fifo;
  unsigned int capacity;        // Capacity in number of bits
  unsigned int capacity_words;  // Capacity in number of words allowed. i.e.,
                                // capacity_words = capacity / size_word
  FifoStats fifoStats;          // Tracking parameters
  std::vector<DataPackage*> buffer;
  unsigned int cnt;  // Number of cycles to wait

 public:
  SyncFifo(unsigned int capacity, unsigned int _cnt = 1)
      : capacity(capacity) {
        cnt = _cnt - 1;

        if (cnt) buffer.assign(cnt, nullptr);
  }

  void push(DataPackage* data) { fifo.push(data); }

  DataPackage* pop() {
    DataPackage* queued = nullptr;
    if (!fifo.empty()) {
      queued = fifo.front();
      fifo.pop();
    }

    if (buffer.size()) {
      DataPackage* data = buffer[cnt - 1];

      for (unsigned int i = cnt - 1; i > 0; i--) {
        buffer[i] = buffer[i - 1];
      }

      buffer[0] = queued;

      return data;
    }

    return queued;
  }

  DataPackage* front() { assert(0); }

  void set_cnt(unsigned int n) {
    this->cnt = n - 1;
    buffer.resize(n - 1, nullptr);
  }

  void printStats(std::ofstream& out, unsigned int indent) {}
  void printEnergy(std::ofstream& out, unsigned int indent) {}

  void reset() {}

  bool isEmpty() const {
    for (unsigned int i = 0; i < cnt; i++) {
      if (buffer[i] != nullptr) return false;
    }
    return fifo.empty();
  }
};

class SyncFifoOld
{
private:
    struct Node
    {
        DataPackage *data;
      unsigned int remain_cnt; // Remaining dwell cycles
    };

    std::queue<Node> fifo;
    unsigned int capacity;
    FifoStats fifoStats;

    unsigned int cnt; // Dwell cycles per element
    unsigned int recover_cnt; // Saved dwell cycles per element (for reset)

public:
    SyncFifoOld(unsigned int capacity, unsigned int cnt = 1)
        : capacity(capacity), cnt(cnt)
    {
        recover_cnt = cnt;
    }

    bool isEmpty() const { return fifo.empty(); }

    bool isFull() const { return fifo.size() >= capacity; }

    void push(DataPackage *data)
    {
        Node node{data, cnt};
        fifo.push(node);

        // only for the first push
        cnt = 1;

        if (this->size() > this->fifoStats.max_occupancy)
        {
            this->fifoStats.max_occupancy = this->size();
        }
        this->fifoStats.n_pushes += 1; // To track information
    }

    void set_cnt(unsigned int n) {
        this->cnt = n;
        this->recover_cnt = n;
    }

    // Synchronous pop logic
    DataPackage *pop()
    {
        assert(!isEmpty());

        Node &frontNode = fifo.front();
        if (frontNode.remain_cnt > 1)
        {
            frontNode.remain_cnt--;
          return nullptr; // Not ready to dequeue yet
        }
        DataPackage *result = frontNode.data;
        fifo.pop();                  // Actually dequeue
        this->fifoStats.n_pops += 1; // To track information
        return result;
    }

    DataPackage *front()
    {
        assert(!isEmpty());
        DataPackage* pck = fifo.front().data;
        this->fifoStats.n_fronts+=1; //To track information
        return pck;
    }

    void reset() { cnt = recover_cnt; }

    unsigned int size() const { return fifo.size(); }
    void printStats(std::ofstream& out, unsigned int indent) {
        this->fifoStats.print(out, indent);
    }

    void printEnergy(std::ofstream &out, unsigned int indent = 0)
    {
        out << ind(indent) << "FIFO PUSH=" << fifoStats.n_pushes; //Same line
        out << ind(indent) << " POP=" << fifoStats.n_pops;  //Same line
        out << ind(indent) << " FRONT=" << fifoStats.n_fronts << std::endl; //New line 
    }
};

#endif
