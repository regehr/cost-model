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

static const int MaxInsns = 10;

using namespace llvm;

static ExitOnError ExitOnErr;

static cl::list<std::string> InputFileNames(cl::Positional, cl::desc("<Input files>"), cl::OneOrMore);

static std::unique_ptr<Module> openInputFile(LLVMContext &Context, StringRef InputFilename) {
  std::unique_ptr<MemoryBuffer> MB =
      ExitOnErr(errorOrToExpected(MemoryBuffer::getFileOrSTDIN(InputFilename)));
  std::unique_ptr<Module> M =
      ExitOnErr(getOwningLazyBitcodeModule(std::move(MB), Context,
                                           /*ShouldLazyLoadMetadata=*/true));
  ExitOnErr(M->materializeAll());
  return M;
}

// TODO:
// figure out how to represent additional structure
// bitwidths
// constant argument
// instruction pairs / triples

static int check(Module &M) {
  if (llvm::verifyModule(M))
    report_fatal_error("module didn't verify");
  for (auto &F : M) {
    std::map<unsigned, int> Insts;
    int Count = 0;
    for (auto &B : F) {
      for (auto &I : B) {
        Count++;
        auto Op = I.getOpcode();
	auto ret = Insts.insert(std::pair<unsigned, int>(Op, 1));
	if (!ret.second) {
	  int prev = ret.first->second;
	  Insts.erase(Op);
	  auto ret2 = Insts.insert(std::pair<unsigned, int>(Op, prev + 1));
	  if (!ret2.second)
	    llvm::report_fatal_error("map");
	}
      }
    }
    if (Count <= MaxInsns) {
      llvm::outs() << "function: " << F.getName() << "\n";
      for (auto &I : Insts) {
        llvm::outs() << "  " << Instruction::getOpcodeName(I.first) << " " << I.second << "\n";
      }
    }
  }
  return 0;
}

int main(int argc, char **argv) {
  PrettyStackTraceProgram X(argc, argv);
  cl::ParseCommandLineOptions(argc, argv, "llvm IR cost estimation generator\n");

  LLVMContext Context;
  for (auto InputFileName : InputFileNames) {
    std::unique_ptr<Module> M = openInputFile(Context, InputFileName);
    if (!M.get()) {
      llvm::report_fatal_error("Bitcode did not read correctly");
    }
    int ret = check(*(M.get()));
  }

  return 0;
}
