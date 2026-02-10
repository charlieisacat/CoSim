#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/IR/Attributes.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/raw_ostream.h"

#include <string>
#include <map>
#include <fstream>
#include <set>

using namespace llvm;
using namespace std;

struct renameBBs : public PassInfoMixin<renameBBs> {
    map<string, unsigned> Names;

    renameBBs() {}

    PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM) {
        int i = 0;
        for (Function &F : M) {
            for (BasicBlock &BB : F) {
                BB.setName("r" + to_string(i));
                i++;
            }
        }
        return PreservedAnalyses::none();
    }
};

struct markInline : public PassInfoMixin<markInline> {
    markInline() {}

    PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM) {
        for (Function &F : M) {
            if (!F.isDeclaration()) {
                F.removeFnAttr(Attribute::OptimizeNone);
                if (F.getName().str() != "main") {
                    F.removeFnAttr(Attribute::NoInline);
                    F.addFnAttr(Attribute::AlwaysInline);
                }
            }
        }
        return PreservedAnalyses::none();
    }
};

// Register passes
class PassPlugin : public PassPluginLibraryInfo {
public:
    PassPlugin() : PassPluginLibraryInfo() {
        // Register renameBBs pass
        PassBuilder().registerPipelineParsingCallback(
            [](StringRef Name, ModulePassManager &MPM, ArrayRef<PassBuilder::PipelineElement> PE) {
                if (Name == "renameBBs") {
                    MPM.addPass(renameBBs());
                    return true;
                }
                return false;
            });
        
        // Register markInline pass
        PassBuilder().registerPipelineParsingCallback(
            [](StringRef Name, ModulePassManager &MPM, ArrayRef<PassBuilder::PipelineElement> PE) {
                if (Name == "markInline") {
                    MPM.addPass(markInline());
                    return true;
                }
                return false;
            });
    }
};

// LLVM pass plugin initialization
extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo llvmGetPassPluginInfo() {
    return {
        LLVM_PLUGIN_API_VERSION,   // Plugin API version
        "MyPassPlugin",            // Plugin name
        "v1.0",                    // Plugin version
        [](PassBuilder &PB) {      // Pass registration callback
            PB.registerPipelineParsingCallback(
                [](StringRef Name, ModulePassManager &MPM, ArrayRef<PassBuilder::PipelineElement> PE) {
                    if (Name == "renameBBs") {
                        MPM.addPass(renameBBs());
                        return true;
                    } else if (Name == "markInline") {
                        MPM.addPass(markInline());
                        return true;
                    }
                    return false;
                });
        }
    };
}
