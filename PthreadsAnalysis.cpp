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
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/ModuleUtils.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Debug.h"


using namespace llvm;

namespace {

#define DET_BB_THRESHOLD 3
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
            }
        }
    }
};


char WrapPthreads::ID = 0;
static RegisterPass<WrapPthreads> X("wrap-pthreads", "Instrument pthreads functions with determinstic hints");

struct InsertDetTicks : public BasicBlockPass {
    static char ID; // Pass identification, replacement for typeid
    InsertDetTicks() : BasicBlockPass(ID) {}
    bool runOnBasicBlock(BasicBlock &BB) {
        // Skip those very small BBs, much heuristic, so random
        if (BB.size() < DET_BB_THRESHOLD) {
            return false;
        }

        if (BB.getTerminator() == nullptr) {
            return false;
        }

        Module *M = BB.getModule();
        if (M == nullptr) {
            return false;
        }

        IRBuilder<> IRB(&BB.back());
        Function *DetTickFunc = cast<Function>(M->getOrInsertFunction("syscall", IRB.getInt32Ty(), IRB.getInt32Ty(), IRB.getInt64Ty(), nullptr));
        //CallInst *DetTickInst = CallInst::Create(DetTickFunc, {IRB.getInt32(321), IRB.getInt64(BB.size())});

        IRB.CreateCall(DetTickFunc, {IRB.getInt32(321), IRB.getInt64(BB.size())});
        errs() << "BB with inst count: " << BB.size() << "\n";
        return true;
    }
};

char InsertDetTicks::ID = 1;
static RegisterPass<InsertDetTicks> Y("insert-det-ticks", "Instrument code with determinstic ticks");
}
