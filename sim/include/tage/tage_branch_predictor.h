#ifndef TAGE_BRANCH_PREDICTOR_H
#define TAGE_BRANCH_PREDICTOR_H

#include <bitset>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "tage_base_predictor.h"

// A software TAGE model that can be configured like BOOM's TAGE:
// each table has explicit (nSets, histLen, tagBits).
// The implementation keeps a compile-time maximum history length.
constexpr int kMaxHistLen = 256;
using history_t = std::bitset<kMaxHistLen + 1>;

struct TageTableInfo {
    uint32_t nSets;
    uint32_t histLen;
    uint32_t tagBits;
};

struct TageParams {
    // Prediction counter width (BOOM uses 3 bits).
    uint32_t ctrBits = 3;
    // Usefulness aging period (BOOM default is 2048).
    uint32_t uBitPeriod = 2048;
    // Path history length used by the classic gIndex hash.
    uint32_t pathHistBits = 16;
    // Tagged tables from shortest history to longest history.
    std::vector<TageTableInfo> tableInfo;

    static TageParams BoomV3Default() {
        TageParams p;
        p.ctrBits = 3;
        p.uBitPeriod = 2048;
        p.pathHistBits = 16;
        // BOOM v3 default BoomTageParams.tableInfo:
        // (nSets, histLen, tagSz)
        p.tableInfo = {
            {128,  2, 7},
            {128,  4, 7},
            {256,  8, 8},
            {256, 16, 8},
            {128, 32, 9},
            {128, 64, 9},
        };
        return p;
    }
};

/*
 * class folded_history: Used for index and tag computation.
 * from https://github.com/masc-ucsc/esesc/blob/master/simu/libcore/IMLIBest.h#L228-L261
 */
class folded_history
{
public:
    uint32_t comp;
    int CLENGTH;
    int OLENGTH;
    int OUTPOINT;

    folded_history() {}

    void init(int original_length, int compressed_length)
    {
        comp = 0;
        OLENGTH = original_length;
        CLENGTH = compressed_length;
        // std::cout << __FUNCTION__ << "::"  << OLENGTH << ", " << CLENGTH << "\n";
        assert(OLENGTH >= 0);
        assert(OLENGTH <= kMaxHistLen);
        assert(CLENGTH > 0);
        assert(CLENGTH <= 31);
        OUTPOINT = OLENGTH % CLENGTH;
    }

    void update(const history_t& h)
    {
        comp = (comp << 1) | h[0];
        comp ^= static_cast<uint32_t>(h[OLENGTH]) << OUTPOINT;
        comp ^= (comp >> CLENGTH);
        comp &= (static_cast<uint32_t>(1) << CLENGTH) - 1;
    }
};

class TaggedComponentEntry
{
public:
    int8_t ctr;      // prediction depends on the sign
    uint16_t tag;    // partial tag
    uint8_t uCtr;    // usefulness counter (0..3)
    TaggedComponentEntry();
    void ctrUpdate(bool actual, uint32_t nbits);
    void uCtrUpdate(bool actual, bool providerPred, bool altPred);
};

class TaggedComponent
{
public:
    TaggedComponent() = default;

    explicit TaggedComponent(uint32_t nSets);

    void uCtrPeriodicReset(uint64_t brCtr, uint32_t uBitPeriod);
    bool matchTagAtIndex(uint32_t index, uint16_t tag) const;
    bool getPredAtIndex(uint32_t index) const;

    uint32_t nSets() const { return static_cast<uint32_t>(entry.size()); }

    std::vector<TaggedComponentEntry> entry;
};

class TageBranchPredictor
{
public:
    int altOnNewAlloc;
    uint64_t brCtr;
    uint32_t pHist;
    history_t gHist;

    TageParams params;

    // 1-indexed arrays for convenience (table 1..nTables).
    std::vector<folded_history> histIndex;
    std::vector<std::vector<folded_history>> histTag;

    std::vector<TaggedComponent> T;
    TageBasePredictor basePredictor;

    std::vector<uint32_t> histLength;
    std::vector<uint32_t> tagWidth;
    std::vector<uint32_t> idxBits;
    std::vector<uint32_t> idxMask;
    std::vector<uint16_t> tagMask;

    int providerComp, altComp;
    bool providerPred, altPred;
    std::vector<uint32_t> GI;
    std::vector<uint16_t> GTAG;

    // Backward-compatible constructor: uses BOOM v3 defaults.
    TageBranchPredictor(std::string name, int core_id);

    // Preferred constructor: explicit BOOM-like parameters.
    TageBranchPredictor(std::string name, int core_id, TageParams p);

    uint32_t gIndex(uint64_t ip, int bank);
    uint16_t gTag(uint64_t ip, int bank);
    void updateHistory(bool actual, uint64_t ip);
    void allocate(bool actual);

    bool predict(uint64_t ip, uint64_t target);
    void update(bool predicted, bool actual, uint64_t ip, uint64_t target);

private:
    void initDerivedTables();
    uint32_t nTables() const { return static_cast<uint32_t>(params.tableInfo.size()); }
};
#endif // TAGE_BRANCH_PREDICTOR_H
