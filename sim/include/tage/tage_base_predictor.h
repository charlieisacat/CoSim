#ifndef TAGE_BASE_PREDICTOR
#define TAGE_BASE_PREDICTOR

#include <cassert>
#include <cstdint>
#include <vector>

// Simple bimodal predictor intended to act as the "base" predictor under TAGE.
// This is modeled after BOOM's BIM predictor:
// - 2-bit unsigned saturating counters in [0..3]
// - predict taken iff counter[1] == 1 (i.e. counter >= 2)
// - reset initializes to weakly-taken (2)

class SimpleBimodalTable
{

public:
    explicit SimpleBimodalTable(uint32_t entries)
        : m_num_entries(entries), m_mask(entries - 1), m_table(entries, 2)
    {
        assert(m_num_entries != 0);
        assert((m_num_entries & (m_num_entries - 1)) == 0); // pow2
    }

    bool predict(uint64_t ip, uint64_t target)
    {
        (void)target;
        const uint32_t index = indexForIP(ip);
        return (m_table[index] & 0b10u) != 0;
    }

    void update(bool predicted, bool actual, uint64_t ip, uint64_t target)
    {
        (void)predicted;
        (void)target;
        const uint32_t index = indexForIP(ip);
        uint8_t v = m_table[index] & 0x3u;

        // BOOM's bimWrite: saturating update.
        if (actual) {
            if (v < 3) v++;
        } else {
            if (v > 0) v--;
        }
        m_table[index] = v;
    }

    void reset(bool weaklyTaken = true)
    {
        const uint8_t init = weaklyTaken ? 2u : 1u;
        std::fill(m_table.begin(), m_table.end(), init);
    }

private:
    uint32_t indexForIP(uint64_t ip) const {
        // BOOM effectively indexes BIM by a fetch-aligned PC-derived index.
        // For this software model, keep it simple and deterministic.
        return static_cast<uint32_t>(ip) & m_mask;
    }

private:
    uint32_t m_num_entries;
    uint32_t m_mask;
    std::vector<uint8_t> m_table;
};

class TageBasePredictor
    : public SimpleBimodalTable
{

public:
    TageBasePredictor() : SimpleBimodalTable(8192)
    {
    }
};

#endif /* TAGE_BASE_PREDICTOR */