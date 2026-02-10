#include "branch_predictor.h"
#include "tage/tage.h"
#include "tage/tage_branch_predictor.h"

#include <iostream>

Bpred::Bpred(TypeBpred type, int bht_size, int ghrSize) : type(type), bht_size(bht_size)
{

  // 1-bit saturating counters BP
  if (type == bp_onebit)
  {
    bht = new int[bht_size];
    for (int i = 0; i < bht_size; i++)
      bht[i] = 1; // init to taken
  }
  // 2-bit saturating counters BP
  else if (type == bp_twobit)
  {
    bht = new int[bht_size];
    for (int i = 0; i < bht_size; i++)
      bht[i] = 2; // init to weakly taken
  }
  // gshare BP: made of a table of 2-bit saturating counters + a global history register
  else if (type == bp_gshare)
  {
    gshare_global_hist = 0;
    gshare_global_hist_bits = ghrSize;
    bht = new int[bht_size];
    for (int i = 0; i < bht_size; i++)
      bht[i] = 1; // init to weakly not-taken
  }
  else if (type == bp_tage)
  {
    // Initialize the TAGE predictor
    // tagePredictor = new PREDICTOR();

TageParams p;
p.ctrBits = 3;          // keep BOOM-style 3-bit signed ctrs
p.uBitPeriod = 4096;    // age usefulness a bit slower than BOOM (often helps)
p.pathHistBits = 16;

// 8 tagged tables: more tables + longer max history than BOOM default (64 -> 256)
p.tableInfo = {
  //  nSets  histLen tagBits
  {  256,      2,      8 },
  {  256,      4,      8 },
  {  512,      8,      9 },
  {  512,     16,      9 },
  {  256,     32,     10 },
  {  256,     64,     10 },
  {  128,    128,     11 },
  {  128,    256,     11 }, // fits your kMaxHistLen=256
};
    // tagePredictor = new TageBranchPredictor("123", 0, TageParams::BoomV3Default());
    tagePredictor = new TageBranchPredictor("123", 0, p);
  }
  else if (type != bp_none && type != bp_always_NT && type != bp_always_T && type != bp_perfect)
  {
    std::cout << "Unknown branch predictor!!!\n";
    assert(false);
  }
}

// predict a branch and return if the prediction was correct
bool Bpred::predict(uint64_t pc, bool actual_taken)
{
  if (type == bp_none)
  {
    return false;
  }
  else if (type == bp_perfect)
  {
    return true;
  }
  // models the behavior of an Always-NOT-Taken static branch predictor
  else if (type == bp_always_NT)
  {
    if (actual_taken)
      return false;
    else
      return true;
  }
  // models the behavior of an Always-Taken static branch predictor
  else if (type == bp_always_T)
  {
    if (actual_taken)
      return true;
    else
      return false;
  }
  // dynamic branch predictor with 1-bit saturating counters
  else if (type == bp_onebit)
  {
    // query the predictor
    int idx = pc & (bht_size - 1);
    int pred_taken = (bht[idx] == 1);

    // update the 1-bit counter
    if (actual_taken)
    {
      bht[idx]++;                       // increase counter
      bht[idx] = std::min(bht[idx], 1); // force the counter to be in the range [0..1]
    }
    else
    {
      bht[idx]--;                       // decrease counter
      bht[idx] = std::max(bht[idx], 0); // force the counter to be in the range [0..1]
    }
    assert(bht[idx] >= 0 && bht[idx] <= 1);
    return (pred_taken == actual_taken);
  }
  // dynamic branch predictor with 2-bit saturating counters
  else if (type == bp_twobit)
  {
    // query the predictor
    int idx = pc & (bht_size - 1);
    int pred_taken = (bht[idx] >= 2); // 0,1 not taken  |  2,3 taken

    // update the 2-bit counter
    if (actual_taken)
    {
      bht[idx]++;                       // increase counter
      bht[idx] = std::min(bht[idx], 3); // force the counter to be in the range [0..3]
    }
    else
    {
      bht[idx]--;                       // decrease counter
      bht[idx] = std::max(bht[idx], 0); // force the counter to be in the range [0..3]
    }
    assert(bht[idx] >= 0 && bht[idx] <= 3);
    return (pred_taken == actual_taken);
  }
  // GSHARE branch predictor with 2-bit saturating counters
  else if (type == bp_gshare)
  {
    // query the predictor
    int idx = (pc ^ gshare_global_hist) & (bht_size - 1);
    int pred_taken = (bht[idx] >= 2); // 0,1 not taken  |  2,3 taken

    // update the 2-bit counter
    if (actual_taken)
    {
      bht[idx]++;                       // increase counter
      bht[idx] = std::min(bht[idx], 3); // force the counter to be in the range [0..3]
    }
    else
    {
      bht[idx]--;                       // decrease counter
      bht[idx] = std::max(bht[idx], 0); // force the counter to be in the range [0..3]
    }
    assert(bht[idx] >= 0 && bht[idx] <= 3);

    // update the global history register
    int mask = (1 << gshare_global_hist_bits) - 1;
    gshare_global_hist = ((gshare_global_hist << 1) | actual_taken) & mask;
    return (pred_taken == actual_taken);
  }
  else if(type == bp_tage)
  {

    bool pred_taken = tagePredictor->predict(pc, 0);
    return pred_taken;

#if 0
    // query the TAGE predictor
    bool pred_taken = tagePredictor->GetPrediction(pc);
    return pred_taken;
    
    // update the TAGE predictor
    // tagePredictor->UpdatePredictor(pc, actual_taken, pred_taken, 0);
    // return (pred_taken == actual_taken);
#endif
  }
  else
    return true; // default to perfect branch predictor
}

void Bpred::updateBP(uint64_t pc, bool actual_taken, bool pred_taken)
{
  if(type == bp_tage)
  {
    // tagePredictor->UpdatePredictor(pc, actual_taken, pred_taken, 0);
    tagePredictor->update(pred_taken, actual_taken, pc, 0);
  }
}