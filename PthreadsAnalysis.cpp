/*
 * PthreadsAnalysis.cpp
 * Copyright (C) 2015 Yuzhong Wen <supermartian@gmail.com>
 *
 * Distributed under terms of the MIT license.
 */

#define DEBUG_TYPE "pthread_"

#include "llvm/Pass.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/CallSite.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/ModuleUtils.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Debug.h"

using namespace llvm;

STATISTIC(NumPthreadMutexLock, "Number of pthread_mutex_lock");

namespace {
/*
 * pthread_mutex_lock would be wrapped by __NR_syscall_det_start and __NR_syscall_det_end
 *
 */
struct WrapPthreads : public FunctionPass {
    static char ID; // Pass identification, replacement for typeid
    WrapPthreads() : FunctionPass(ID) {}

    bool runOnFunction(Function &F) {
        for (auto &BB : F) {
            for (auto &Inst : BB) {
                if (isa<CallInst>(Inst)) {
                    TransformPthreadMutexLock(CallSite(&Inst));
                }
            }
        }
        return true;
    }

    void TransformPthreadMutexLock(CallSite c) {
        Function *fun = c.getCalledFunction();
        // Let's assume there's no indirect pthread_lock calls...
        if (fun) {
            if (fun->getName() == "pthread_mutex_lock") {
                NumPthreadMutexLock++;
            }
        }
    }
};

char WrapPthreads::ID = 0;
static RegisterPass<WrapPthreads> X("wrap-pthreads", "Instrument pthreads functions with determinstic hints");
}
