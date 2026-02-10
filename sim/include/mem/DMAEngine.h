#ifndef DMA_ENGINE_H
#define DMA_ENGINE_H

#include <cstdint>
#include <deque>
#include <memory>
#include <queue>
#include <unordered_map>
#include <vector>

class Cache;
class MemTransaction;

#include "SA/Command.h"
#include "SA/types.h"

// A simple DMA engine model:
// - Accepts DMA requests associated with a Command
// - Splits into fixed-size (e.g., 64B) transactions
// - Models DRAM service with a fixed base latency and 1 txn/cycle bandwidth
// - Tracks completion per request-id and reports completed commands
class DMAEngine {
public:
    struct DMARequest {
        uint64_t id = 0;
        Command* cmd = nullptr;
        OperationType op = END;
        uint64_t base_addr = 0;
        uint64_t bytes = 0;
        bool is_read = true;

        uint64_t total_txns = 0;
        // Number of split transactions that have been *issued/sent* into the
        // memory system (L2 injection mode) or into the fallback DRAM pipeline.
        uint64_t issued_txns = 0;
        uint64_t done_txns = 0;

        // For stores (DMA writes), we may consider the command complete once
        // all sub-transactions are issued, even if responses return later.
        bool completed_early = false;
    };

private:
    struct DMATransaction {
        uint64_t req_id = 0;
        uint64_t addr = 0;
        uint32_t bytes = 0;
        cycles_t ready_cycle = 0;
    };

    struct TxnReadyEarlier {
        bool operator()(const DMATransaction& a, const DMATransaction& b) const {
            return a.ready_cycle > b.ready_cycle; // min-heap
        }
    };

    uint64_t next_req_id = 1;
    uint32_t burst_bytes = 64;
    cycles_t base_latency = 10;
    uint32_t bandwidth = 16; // Bytes per cycle
    uint32_t max_in_flight_mem_reqs = 16; // Max number of in-flight mem sub-requests
    uint32_t current_in_flight_mem_reqs = 0; // Current number of in-flight mem sub-requests

    // If set, DMA requests are split into MemTransactions and injected into this cache.
    // Otherwise, a simple fixed-latency model is used.
    std::shared_ptr<Cache> l2_cache = nullptr;
    int dma_hw_id = 1;
    std::deque<MemTransaction*> pending_inject;

    // For fallback mode flow control:
    std::deque<DMATransaction> pending_fallback_issue;
    cycles_t next_dram_issue_cycle = 0; // Ensures fallback DRAM issue spacing

    struct ZeroByteDone {
        uint64_t req_id = 0;
        cycles_t ready_cycle = 0;
    };
    std::deque<ZeroByteDone> pending_zero_done;

    // Incoming finished transactions from memory (L2 or Fallback) waiting for bus transmission
    std::deque<uint64_t> incoming_txns;
    
    cycles_t next_free_cycle = 0;

    struct Completion {
        uint64_t req_id = 0;
        cycles_t when = 0;
    };
    std::deque<Completion> scheduled_completions;

    // At most 1 transaction becomes ready each cycle (modeled via spacing).
    std::unordered_map<uint64_t, DMARequest> inflight;
    std::priority_queue<DMATransaction, std::vector<DMATransaction>, TxnReadyEarlier> pending_txns;

    std::vector<Command*> completed_cmds;

public:
    DMAEngine() = default;
    DMAEngine(uint32_t burst_bytes, cycles_t base_latency)
        : burst_bytes(burst_bytes), base_latency(base_latency) {}

    void reset();

    void set_base_latency(cycles_t lat) { base_latency = lat; }
    cycles_t get_base_latency() const { return base_latency; }

    void set_bandwidth(uint32_t bw) { bandwidth = bw; }
    void set_max_in_flight_mem_reqs(uint32_t max) { max_in_flight_mem_reqs = max; }

    // Enable cache-injection mode (DMA requests go through the cache hierarchy).
    void connectL2(std::shared_ptr<Cache> l2, int hw_id = 1);

    bool has_inflight() const { return !inflight.empty(); }

    // Submits a DMA request. Returns the request-id.
    uint64_t submit(Command* cmd, uint64_t dram_addr, uint64_t bytes, bool is_read, cycles_t issue_cycle);

    // Advances the DMA engine by one cycle (using the caller's cycle counter).
    void cycle(cycles_t now);

    // Called by caches when a DMA-originated MemTransaction completes.
    void recvResponse(MemTransaction* resp);

    // Pops and returns commands completed since last call.
    std::vector<Command*> take_completed();
};

#endif
