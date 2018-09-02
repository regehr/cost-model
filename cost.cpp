//===-------- opt-fuzz.cpp - Generate LL files to stress-test LLVM --------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This utility does bounded exhaustive generation of LLVM IR
// functions containing integer instructions. These can be used to
// stress-test different components of LLVM.
//
//===----------------------------------------------------------------------===//

#include "llvm/Analysis/CallGraphSCCPass.h"
#include "llvm/Bitcode/BitcodeReader.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/IRPrintingPasses.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/LegacyPassNameParser.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/NoFolder.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/PluginLoader.h"
#include "llvm/Support/PrettyStackTrace.h"
#include "llvm/Support/ToolOutputFile.h"
#include "llvm/Support/raw_ostream.h"
#include <algorithm>
#include <fcntl.h>
#include <pthread.h>
#include <sched.h>
#include <set>
#include <sstream>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

using namespace llvm;

static ExitOnError ExitOnErr;

static cl::opt<std::string>
InputFilename(cl::Positional, cl::desc("<input bitcode file>"),
              cl::init("-"), cl::value_desc("filename"));

static std::unique_ptr<Module> openInputFile(LLVMContext &Context) {
  std::unique_ptr<MemoryBuffer> MB =
      ExitOnErr(errorOrToExpected(MemoryBuffer::getFileOrSTDIN(InputFilename)));
  std::unique_ptr<Module> M =
      ExitOnErr(getOwningLazyBitcodeModule(std::move(MB), Context,
                                           /*ShouldLazyLoadMetadata=*/true));
  ExitOnErr(M->materializeAll());
  return M;
}

static int check(Module &M) {
  if (llvm::verifyModule(M)) {
    return 1;
  }
  llvm::outs() << "module: " << M.getName() << "\n";
  for (auto &F : M) {
    llvm::outs() << "function " << F.getName() << " \n";
    for (auto &B : F) {
      llvm::outs() << "block\n";
      for (auto &I : B) {
        llvm::outs() << "instruction\n";
      }
    }
  }
  return 0;
}

  // for each function in the module, print a weighted list of components that it contains
  // TODO: figure out how to represent additional structure

int main(int argc, char **argv) {
  PrettyStackTraceProgram X(argc, argv);
  cl::ParseCommandLineOptions(argc, argv, "llvm IR cost estimation generator\n");

  LLVMContext Context;
  std::unique_ptr<Module> M = openInputFile(Context);
  if (!M.get()) {
    llvm::report_fatal_error("Bitcode did not read correctly");
  }
  int ret = check(*(M.get()));

  return ret;
}
