#include "mem/DMAEngine.h"

#include <cassert>
#include <algorithm>

#include "mem/cache.h"
#include "mem/mem_transaction.h"

void DMAEngine::reset() {
    inflight.clear();
    while (!pending_txns.empty()) {
        pending_txns.pop();
    }
    pending_inject.clear();
    pending_zero_done.clear();
    completed_cmds.clear();
    next_req_id = 1;
    current_in_flight_mem_reqs = 0;
    pending_fallback_issue.clear();
    next_dram_issue_cycle = 0;
    incoming_txns.clear();
    scheduled_completions.clear();
    next_free_cycle = 0;
}

void DMAEngine::connectL2(std::shared_ptr<Cache> l2, int hw_id) {
    l2_cache = std::move(l2);
    dma_hw_id = hw_id;
    if (l2_cache) {
        l2_cache->setDMAEngine(this);
    }
}

uint64_t DMAEngine::submit(Command* cmd, uint64_t dram_addr, uint64_t bytes,
                           bool is_read, cycles_t issue_cycle) {
    assert(cmd);

    DMARequest req;
    req.id = next_req_id++;
    req.cmd = cmd;
    req.op = cmd->get_op_type();
    req.base_addr = dram_addr;
    req.bytes = bytes;
    req.is_read = is_read;
    req.issued_txns = 0;
    req.completed_early = false;

    if (bytes == 0) {
        // Model at least 1-cycle delay even for 0B transfers.
        // This is independent of cache-injection vs fallback mode.
        req.total_txns = 0;
        req.done_txns = 0;
        pending_zero_done.push_back(ZeroByteDone{req.id, issue_cycle + 1});
    } else if (l2_cache) {
            uint64_t n = (bytes + burst_bytes - 1) / burst_bytes;
            req.total_txns = n;
            req.done_txns = 0;

            for (uint64_t i = 0; i < n; i++) {
                uint64_t off = i * burst_bytes;
                auto* t = new MemTransaction(-2, dma_hw_id, nullptr, dram_addr + off,
                                             is_read, /*fromICache=*/false);
                t->fromDMA = true;
                t->dma_req_id = req.id;
                pending_inject.push_back(t);
            }
    } else {
        // Fallback: simple fixed-latency model without real cache/DRAM injection.
        uint64_t n = (bytes + burst_bytes - 1) / burst_bytes;
        req.total_txns = n;
        req.done_txns = 0;

        for (uint64_t i = 0; i < n; i++) {
            uint64_t off = i * burst_bytes;
            uint32_t chunk = burst_bytes;
            if (off + chunk > bytes) {
                chunk = static_cast<uint32_t>(bytes - off);
            }

            DMATransaction t;
            t.req_id = req.id;
            t.addr = dram_addr + off;
            t.bytes = chunk;
            pending_fallback_issue.push_back(t); // Queue for flow control
        }
    }

    inflight.emplace(req.id, req);
    return req.id;
}

void DMAEngine::cycle(cycles_t now) {
    if (l2_cache) {
        // 1) Try to inject pending split transactions into L2 (like LSU).
        while (!pending_inject.empty()) {
            if (current_in_flight_mem_reqs >= max_in_flight_mem_reqs) {
                break; // Sliding window flow control
            }
            MemTransaction* t = pending_inject.front();
            if (!l2_cache->willAcceptTransaction(t)) {
                break;
            }
            l2_cache->recvRequest(t);
            pending_inject.pop_front();
            current_in_flight_mem_reqs++;

            // For stores: consider the command complete once all sub-requests
            // have been issued (sent) into the memory system.
            auto it = inflight.find(t->dma_req_id);
            if (it != inflight.end()) {
                DMARequest& req = it->second;
                req.issued_txns++;
                if (!req.is_read && !req.completed_early && req.issued_txns >= req.total_txns) {
                    completed_cmds.push_back(req.cmd);
                    req.completed_early = true;
                }
            }
        }

        // 2) Handle 0B transfers (complete after 1 cycle).
        while (!pending_zero_done.empty() && pending_zero_done.front().ready_cycle <= now) {
            uint64_t req_id = pending_zero_done.front().req_id;
            pending_zero_done.pop_front();

            auto it = inflight.find(req_id);
            if (it == inflight.end()) {
                continue;
            }
            completed_cmds.push_back(it->second.cmd);
            inflight.erase(it);
        }
    } else {
        // Fallback mode: Issue from pending_fallback_issue respecting flow control and 1-per-cycle bandwidth
        while (!pending_fallback_issue.empty()) {
             if (current_in_flight_mem_reqs >= max_in_flight_mem_reqs) {
                break; // Sliding window flow control
            }

            // Bandwidth model: one txn becomes ready per cycle.
            if (next_dram_issue_cycle > now) {
                 break;
            }

            DMATransaction t = pending_fallback_issue.front();
            pending_fallback_issue.pop_front();

            cycles_t issue_time = std::max(now, next_dram_issue_cycle);
            t.ready_cycle = issue_time + base_latency;
            
            pending_txns.push(t);
            current_in_flight_mem_reqs++;
            next_dram_issue_cycle = issue_time + 1;

            // For stores: consider the command complete once all sub-requests
            // have been issued (sent) into the fallback DRAM pipeline.
            auto it = inflight.find(t.req_id);
            if (it != inflight.end()) {
                DMARequest& req = it->second;
                req.issued_txns++;
                if (!req.is_read && !req.completed_early && req.issued_txns >= req.total_txns) {
                    completed_cmds.push_back(req.cmd);
                    req.completed_early = true;
                }
            }
        }

        // retire all transactions that become ready at/before 'now'.
        while (!pending_txns.empty()) {
            const DMATransaction& t = pending_txns.top();
            if (t.ready_cycle > now) {
                break;
            }

            // Move finished DRAM accesses to bus queue
            incoming_txns.push_back(t.req_id);
            pending_txns.pop();
            current_in_flight_mem_reqs--; // Sub-req finished DRAM part
        }
    }

    // 3) Process incoming transactions through bandwidth limit
    while (!incoming_txns.empty()) {
        uint64_t req_id = incoming_txns.front();
        incoming_txns.pop_front();

        cycles_t delay = (bandwidth > 0) ? (burst_bytes + bandwidth - 1) / bandwidth : 1;
        cycles_t start_time = std::max(now, next_free_cycle);

        cycles_t finish_time = start_time + delay;
        next_free_cycle = finish_time;
        
        scheduled_completions.push_back({req_id, finish_time});
    }

    // 4) Process completed bus transfers
    while (!scheduled_completions.empty()) {
        const Completion& c = scheduled_completions.front();
        if (c.when > now) {
            break;
        }

        uint64_t req_id = c.req_id;
        scheduled_completions.pop_front();

        auto it = inflight.find(req_id);
        if (it != inflight.end()) {
            DMARequest& req = it->second;
            req.done_txns++;

            if (req.done_txns >= req.total_txns) {
                // Loads complete on data return; stores may have been completed
                // early at issue-time (do not double-complete).
                if (req.is_read || !req.completed_early) {
                    completed_cmds.push_back(req.cmd);
                }
                inflight.erase(it);
            }
        }
    }
}

void DMAEngine::recvResponse(MemTransaction* resp) {
    if (!resp || !resp->fromDMA) {
        assert(0);
        return;
    }

    auto it = inflight.find(resp->dma_req_id);
    assert(it != inflight.end());
    
    current_in_flight_mem_reqs--; // Returned from L2 memory system

    // Queue for bandwidth simulation instead of immediate completion
    incoming_txns.push_back(resp->dma_req_id);
}

std::vector<Command*> DMAEngine::take_completed() {
    std::vector<Command*> out;
    out.swap(completed_cmds);
    return out;
}
