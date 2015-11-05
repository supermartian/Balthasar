/*
 * PthreadsAnalysis.cpp
 * Copyright (C) 2015 Yuzhong Wen <supermartian@gmail.com>
 *
 * Distributed under terms of the MIT license.
 */


#include "llvm/Pass.h"
#include "llvm/Support/CallSite.h"
#include "llvm/Instructions.h"
#include "llvm/Support/InstVisitor.h"

using namespace llvm;

namespace {
  struct WrapPthreads : public BasicBlockPass {
      virtual bool runOnBasicBlock(BasicBlock &BB) {
      }
  };

  struct InsertTicks : public BasicBlockPass {
  };

  RegisterPass<WrapPthreads> X("wrap-pthreads", "Instrument pthreads functions with determinstic hints");
  RegisterPass<InsertTicks> X("insert-ticks", "Instrument code with determinstic ticks");
}
