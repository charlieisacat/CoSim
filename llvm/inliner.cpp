#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/IR/Function.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/Analysis/CallGraphSCCPass.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/ADT/SCCIterator.h"

using namespace llvm;

namespace
{
    static cl::opt<std::string> targetFunc("targetFunc", cl::desc("targetFunc"), cl::Optional);

    // Make the pass inherit from PassInfoMixin instead of CallGraphSCCPass
    struct BottomUpInliner : public PassInfoMixin<BottomUpInliner> {

        // Modify inlineFunc: remove recursive inlining; only inline call sites within the current function
        bool inlineFunc(Function *F) {
            bool changed = false;
            std::vector<CallBase *> instructionsToInline;
            for (auto &BB : *F)
            {
                for (auto &I : BB)
                {
                    if (auto *CB = dyn_cast<CallBase>(&I))
                    {
                        Function *callee = CB->getCalledFunction();
                        if (callee && !callee->isDeclaration())
                        {
                            instructionsToInline.push_back(CB);
                        }
                    }
                }
            }

            // Inline call instructions in the current function only; do not recursively process callees
            for (auto CB : instructionsToInline)
            {
                InlineFunctionInfo IFI;
                auto result = InlineFunction(*CB, IFI);
                if (result.isSuccess())
                {
                    changed = true;
                }
            }

            return changed;
        }

        // CallGraphSCCPass is no longer used in LLVM 22; use a custom callback instead
        PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM) {
            bool Changed = false;
            
            // Build and analyze the call graph
            CallGraph CG(M);
            
            // Use the SCC iterator for bottom-up traversal
            // SCC (Strongly Connected Components) are strongly connected components in the call graph
            // In a call graph, an SCC usually represents a set of mutually recursive functions
            // The SCC iterator yields SCCs in topological order, which naturally forms a bottom-up processing order:
            // 1. First process functions that call no other functions or only call already-processed functions
            // 2. Then process higher-level functions that call those functions
            // This ensures callees are inlined before their callers
            for (scc_iterator<CallGraph*> I = scc_begin(&CG), IE = scc_end(&CG); I != IE; ++I) {
                const std::vector<CallGraphNode*> &SCCNodes = *I;
                
                // Process each node in the SCC
                for (CallGraphNode* Node : SCCNodes) {
                    Function* F = Node->getFunction();
                    
                    if (F && !F->isDeclaration()) {
                        if (F->getName().str() == targetFunc) {
                            Changed |= inlineFunc(F);
                        }
                    }
                }
            }

            return Changed ? PreservedAnalyses::none() : PreservedAnalyses::all();
        }
    };
}

// Register the pass via PassBuilder
extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo llvmGetPassPluginInfo() {
    return {
        LLVM_PLUGIN_API_VERSION,   // Plugin API version
        "BottomUpInliner",         // Pass name
        "v1.0",                    // Pass version
        [](PassBuilder &PB) {      // Pass registration callback
            PB.registerPipelineParsingCallback(
                [](StringRef Name, ModulePassManager &MPM, ArrayRef<PassBuilder::PipelineElement> PE) {
                    if (Name == "bottom-up-inline") {
                        MPM.addPass(BottomUpInliner());
                        return true;
                    }
                    return false;
                });
        }
    };
}