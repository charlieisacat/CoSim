#ifndef INCLUDE_BP_H
#define INCLUDE_BP_H

#include "assert.h"
#include <map>
#include <string>
#include "tage/tage.h"
#include "tage/tage_branch_predictor.h"

typedef enum
{
    bp_none,
    bp_always_NT,
    bp_always_T,
    bp_onebit,
    bp_twobit,
    bp_gshare,
    bp_perfect,
    bp_tage,
} TypeBpred;

const std::map<std::string, TypeBpred> strBPMap = {
    {"bp_none", bp_none},
    {"bp_always_NT", bp_always_NT},
    {"bp_always_T", bp_always_T},
    {"bp_onebit", bp_onebit},
    {"bp_twobit", bp_twobit},
    {"bp_gshare", bp_gshare},
    {"bp_perfect", bp_perfect},
    {"bp_tage", bp_tage},
};

class Bpred
{
public:
    /** \brief   */
    TypeBpred type = bp_perfect;

    /** \brief for bp_onebit, bp_twobit, bp_gshare   */
    int bht_size = 1;
    /** \brief   */
    int *bht;

    /** \brief for bp_gshare   */
    int gshare_global_hist_bits = 10;
    /** \brief   */
    int gshare_global_hist = 0;

    /** \bref   */
    Bpred(TypeBpred, int, int);

    /** \bref return true if predict successfully   */
    bool predict(uint64_t pc, bool actual_taken);
    void updateBP(uint64_t pc, bool actual_taken, bool pred_taken);

    // PREDICTOR *tagePredictor = nullptr;
    TageBranchPredictor *tagePredictor = nullptr;
};

#endif