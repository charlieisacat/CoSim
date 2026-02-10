#include "tage/tage_branch_predictor.h"

#include <algorithm>
#include <random>

namespace {
// Deterministic RNG for repeatable experiments.
static uint32_t g_seed = 1;

static uint32_t ilog2_u32(uint32_t n) {
    uint32_t r = 0;
    while (n > 1) {
        n >>= 1;
        r++;
    }
    return r;
}

static uint32_t next_rand_u32() {
    // Simple LCG (deterministic, fast).
    g_seed = 1103515245u * g_seed + 12345u;
    return g_seed;
}
} // namespace

TaggedComponentEntry::TaggedComponentEntry()
{
    ctr = 0;
    tag = 0;
    uCtr = 0;
}

/* Signed up/down counter */
void TaggedComponentEntry::ctrUpdate(bool actual, uint32_t nbits)
{
    // Signed saturating counter.
    // Max signed value for n bits is (2^(n-1)-1), min is -(2^(n-1)).
    const int8_t max_val = static_cast<int8_t>((1u << (nbits - 1)) - 1u);
    const int8_t min_val = static_cast<int8_t>(-static_cast<int8_t>(1u << (nbits - 1)));
    if (actual) {
        if (ctr < max_val) ctr++;
    } else {
        if (ctr > min_val) ctr--;
    }
}

void TaggedComponentEntry::uCtrUpdate(bool actual, bool providerPred, bool altPred)
{
    if ((providerPred != altPred))
    {
        if (providerPred == actual)
        {
            if (uCtr < 3)
                uCtr++;
        }
        else
        {
            if (uCtr > 0)
                uCtr--;
        }
    }
}

TaggedComponent::TaggedComponent(uint32_t nSets)
    : entry(nSets)
{
}

bool TaggedComponent::matchTagAtIndex(uint32_t index, uint16_t tag) const
{
    return entry[index].tag == tag;
}

bool TaggedComponent::getPredAtIndex(uint32_t index) const
{
    return entry[index].ctr >= 0;
}

void TaggedComponent::uCtrPeriodicReset(uint64_t brCtr, uint32_t uBitPeriod)
{
    if (uBitPeriod == 0) return;
    // Match BOOM-style knob: periodic aging based on a programmable period.
    // This is a coarse software approximation.
    if ((brCtr & (static_cast<uint64_t>(uBitPeriod) - 1)) == 0) {
        for (auto& e : entry) {
            e.uCtr >>= 1;
        }
    }
}

TageBranchPredictor::TageBranchPredictor(std::string name, int core_id)
    : TageBranchPredictor(std::move(name), core_id, TageParams::BoomV3Default())
{
}

TageBranchPredictor::TageBranchPredictor(std::string name, int core_id, TageParams p)
    : altOnNewAlloc(0)
    , brCtr(0)
    , pHist(0)
    , gHist(0)
    , params(std::move(p))
{
    (void)name;
    (void)core_id;
    initDerivedTables();
}

void TageBranchPredictor::initDerivedTables()
{
    assert(!params.tableInfo.empty());
    for (const auto& t : params.tableInfo) {
        assert(t.nSets != 0);
        assert((t.nSets & (t.nSets - 1)) == 0); // pow2
        assert(t.tagBits > 0);
        assert(t.tagBits <= 16);
        assert(t.histLen <= static_cast<uint32_t>(kMaxHistLen));
    }

    const uint32_t n = nTables();

    histIndex.assign(n + 1, folded_history());
    histTag.assign(2, std::vector<folded_history>(n + 1));
    T.clear();
    T.resize(n + 1);

    histLength.assign(n + 1, 0);
    tagWidth.assign(n + 1, 0);
    idxBits.assign(n + 1, 0);
    idxMask.assign(n + 1, 0);
    tagMask.assign(n + 1, 0);
    GI.assign(n + 1, 0);
    GTAG.assign(n + 1, 0);

    // Table 1..n: shortest history to longest history.
    for (uint32_t i = 1; i <= n; i++) {
        const auto& ti = params.tableInfo[i - 1];
        histLength[i] = ti.histLen;
        tagWidth[i] = ti.tagBits;
        idxBits[i] = ilog2_u32(ti.nSets);
        idxMask[i] = ti.nSets - 1;
        tagMask[i] = static_cast<uint16_t>((1u << ti.tagBits) - 1u);

        T[i] = TaggedComponent(ti.nSets);
        histIndex[i].init(static_cast<int>(histLength[i]), static_cast<int>(idxBits[i]));
        histTag[0][i].init(histIndex[i].OLENGTH, static_cast<int>(tagWidth[i]));
        histTag[1][i].init(histIndex[i].OLENGTH, static_cast<int>(tagWidth[i] - 1));
    }

    providerComp = 0;
    altComp = 0;
    providerPred = false;
    altPred = false;
}

void TageBranchPredictor::updateHistory(bool actual, uint64_t ip)
{
    gHist = (gHist << 1);
    if (actual)
        gHist |= (history_t)1;

    const uint32_t mask = (params.pathHistBits >= 32) ? 0xffffffffu : ((1u << params.pathHistBits) - 1u);
    pHist = ((pHist << 1) + static_cast<uint32_t>(ip & 1u)) & mask;

    for (uint32_t i = 1; i <= nTables(); i++) {
        histIndex[i].update(gHist);
        histTag[0][i].update(gHist);
        histTag[1][i].update(gHist);
    }
    return;
}

/*
 * Functions F(), gIndex() and gTag()
 * from https://github.com/masc-ucsc/esesc/blob/master/simu/libcore/IMLIBest.h#L1041-L1072
 */
static uint32_t F(uint32_t A, uint32_t size, uint32_t bank, uint32_t idxBits)
{
    uint32_t A1, A2;

    if (size < 32) A &= ((1u << size) - 1u);
    const uint32_t mask = (idxBits == 32) ? 0xffffffffu : ((1u << idxBits) - 1u);
    A1 = A & mask;
    A2 = A >> idxBits;

    const uint32_t sh = bank % idxBits;
    A2 = ((A2 << sh) & mask) + (A2 >> (idxBits - sh));
    A = A1 ^ A2;
    A = ((A << sh) & mask) + (A >> (idxBits - sh));
    return A;
}

uint32_t TageBranchPredictor::gIndex(uint64_t ip, int bank)
{
    const uint32_t b = static_cast<uint32_t>(bank);
    const uint32_t n = nTables();
    const uint32_t bits = idxBits[b];
    uint32_t shift = 0;
    // Keep the original style hash, but avoid negative/oversized shifts.
    const int32_t raw_shift = static_cast<int32_t>(bits) - static_cast<int32_t>(n - b - 1);
    if (raw_shift > 0 && raw_shift < 64) shift = static_cast<uint32_t>(raw_shift);

    const uint32_t len = std::min<uint32_t>(16u, histLength[b]);
    const uint32_t idx = static_cast<uint32_t>(ip) ^ static_cast<uint32_t>(ip >> shift)
        ^ histIndex[b].comp ^ F(pHist, len, b, bits);

    return idx & idxMask[b];
}

uint16_t
TageBranchPredictor::gTag(uint64_t ip, int bank)
{
    const uint32_t b = static_cast<uint32_t>(bank);
    const uint32_t tag = static_cast<uint32_t>(ip) ^ histTag[0][b].comp ^ (histTag[1][b].comp << 1);
    return static_cast<uint16_t>(tag) & tagMask[b];
}

bool TageBranchPredictor::predict(uint64_t ip, uint64_t target)
{
    const uint32_t n = nTables();
    for (uint32_t i = 1; i <= n; i++) {
        GI[i] = gIndex(ip, i);
        GTAG[i] = gTag(ip, i);
    }

    providerComp = 0;
    altComp = 0;

    /* Find hitting component with longest history */
    for (int i = static_cast<int>(n); i >= 1; i--) {
        if (T[i].matchTagAtIndex(GI[i], GTAG[i])) {
            providerComp = i;
            break;
        }
    }

    /* Alternate prediction component */
    for (int i = providerComp - 1; i >= 1; i--) {
        if (T[i].matchTagAtIndex(GI[i], GTAG[i])) {
            if ((altOnNewAlloc < 0) || (std::abs(2 * T[i].entry[GI[i]].ctr + 1) > 1)) {
                altComp = i;
                break;
            }
        }
    }

    if (providerComp > 0)
    {
        /* If there is a hitting component */
        if (altComp > 0)
        {
            altPred = T[altComp].getPredAtIndex(GI[altComp]);
        }
        else
        {
            /* Use base prediction to use as alternate prediction */
            altPred = basePredictor.predict(ip, target);
        }
        if ((altOnNewAlloc < 0) || (std::abs(2 * T[providerComp].entry[GI[providerComp]].ctr + 1) > 1) || (T[providerComp].entry[GI[providerComp]].uCtr != 0))
        {
            providerPred = T[providerComp].getPredAtIndex(GI[providerComp]);
        }
        else
        {
            providerPred = altPred;
        }
    }
    else
    {
        /* Only base prediction is considered */
        altPred = basePredictor.predict(ip, target);
        providerPred = altPred;
    }
    return providerPred;
}

/*
 * Function allocate():
 * 1. Find alloc
 *      1.1. True if prediction was wrong
 *      1.2. If it was delivering the correct prediction, no need to allocate
 *           a new entry even if the overall prediction was wrong, alloc = false
 *      1.3. Update altOnNewAlloc
 *
 * 2. If alloc is true, do allocation
 *
 */
void TageBranchPredictor::allocate(bool actual)
{
    const uint32_t n = nTables();
    const uint32_t r = next_rand_u32();

    bool alloc = ((providerPred != actual) && (providerComp < static_cast<int>(n)));
    if (providerComp > 0) {
        bool longestPred = T[providerComp].getPredAtIndex(GI[providerComp]);
        bool pseudoNewAlloc = (std::abs(2 * T[providerComp].entry[GI[providerComp]].ctr + 1) <= 1);
        if (pseudoNewAlloc) {
            if (longestPred == actual)
                alloc = false;
            if (longestPred != altPred) {
                if (altPred == actual) {
                    if (altOnNewAlloc < 7) altOnNewAlloc++;
                } else if (altOnNewAlloc > -8) {
                    altOnNewAlloc--;
                }
            }
        }
    }

    if (!alloc) return;

    // Find the minimum usefulness among candidate tables (those longer than provider).
    uint8_t min_u = 3;
    for (uint32_t i = static_cast<uint32_t>(providerComp + 1); i <= n; i++) {
        min_u = std::min<uint8_t>(min_u, T[i].entry[GI[i]].uCtr);
    }

    // If no entry is free (all u>0), age them slightly to create space.
    if (min_u > 0) {
        for (uint32_t i = static_cast<uint32_t>(providerComp + 1); i <= n; i++) {
            auto& e = T[i].entry[GI[i]];
            if (e.uCtr > 0) e.uCtr--;
        }
    }

    // Collect candidates with u==0.
    std::vector<uint32_t> candidates;
    candidates.reserve(n);
    for (uint32_t i = static_cast<uint32_t>(providerComp + 1); i <= n; i++) {
        if (T[i].entry[GI[i]].uCtr == 0) candidates.push_back(i);
    }
    if (candidates.empty()) return;

    const uint32_t chosen = candidates[r % candidates.size()];
    auto& ne = T[chosen].entry[GI[chosen]];
    ne.tag = GTAG[chosen];
    ne.ctr = actual ? 0 : -1;
    ne.uCtr = 0;
}

void TageBranchPredictor::update(bool predicted, bool actual, uint64_t ip, uint64_t target)
{
    // std::cout << __FUNCTION__ << "::Predicted: " << predicted << ", Actual: " << actual << "\n";
    brCtr++;
    allocate(actual);

    if (providerComp > 0) {
        T[providerComp].entry[GI[providerComp]].ctrUpdate(actual, params.ctrBits);
        if (T[providerComp].entry[GI[providerComp]].uCtr == 0) {
            if (altComp > 0) {
                T[altComp].entry[GI[altComp]].ctrUpdate(actual, params.ctrBits);
            } else {
                basePredictor.update(predicted, actual, ip, target);
            }
        }

        // BUGFIX: providerComp can be 0 (base predictor only). Only update uCtr when provider exists.
        T[providerComp].entry[GI[providerComp]].uCtrUpdate(actual, providerPred, altPred);
    } else {
        basePredictor.update(predicted, actual, ip, target);
    }

    updateHistory(actual, ip);
    for (uint32_t i = 1; i <= nTables(); i++) {
        T[i].uCtrPeriodicReset(brCtr, params.uBitPeriod);
    }
    return;
}
