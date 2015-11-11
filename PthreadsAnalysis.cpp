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

#define DET_BB_THRESHOLD 5

using namespace llvm;

namespace {

FunctionType *ftype = FunctionType::get(Type::getInt32Ty(getGlobalContext()), true);
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
                    TransformPthreadPrimitive(&Inst);
                }
            }
        }
        return true;
    }

    void TransformPthreadPrimitive(Instruction *inst) {
        Function *fun = CallSite(inst).getCalledFunction();
        // Let's assume there's no indirect pthread_lock calls...
        if (fun) {
            if ( fun->getName().find( "pthread_" ) == 0 && 
                 fun->getName().find( "pthread_mutexattr_" ) == std::string::npos &&
                 fun->getName().find( "pthread_mutex_unlock" ) == std::string::npos &&
                 fun->getName().find( "pthread_mutex_join" ) == std::string::npos &&
                 fun->getName().find( "pthread_mutex_create" ) == std::string::npos &&
                 fun->getName().find( "pthread_mutex_init" ) == std::string::npos &&
                 fun->getName().find( "pthread_mutex_destroy" ) == std::string::npos &&
                 fun->getName().find( "pthread_attr_" ) == std::string::npos) {
                Module *M = inst->getModule();
                if (M == nullptr) {
                    return;
                }

                // Wrap the function around with 319 and 320
                IRBuilder<> IRB(inst);
                Function *DetFunc = cast<Function>(M->getOrInsertFunction("syscall", ftype));
                CallInst::Create(DetFunc, {IRB.getInt32(319)})->insertBefore(inst);
                CallInst::Create(DetFunc, {IRB.getInt32(320)})->insertAfter(inst);
            }
        }
    }
};


char WrapPthreads::ID = 0;
static RegisterPass<WrapPthreads> X("wrap-pthreads", "Instrument pthreads functions with determinstic hints");

/*
 * Most BBs will end with __NR_syscall_det_tick
 *
 */
struct InsertDetTicks : public BasicBlockPass {
    static char ID; // Pass identification, replacement for typeid
    InsertDetTicks() : BasicBlockPass(ID) {}
    bool runOnBasicBlock(BasicBlock &BB) {
        // Skip those very small BBs, much heuristic, so random
        if (BB.size() < DET_BB_THRESHOLD) {
            return false;
        }

        Module *M = BB.getModule();
        if (M == nullptr) {
            return false;
        }

        IRBuilder<> IRB(&BB.back());

        // Insert the tick count at the end of BB
        Function *DetFunc = cast<Function>(M->getOrInsertFunction("syscall", ftype));
        IRB.CreateCall(DetFunc, {IRB.getInt32(321), IRB.getInt64(BB.size())});
        errs() << "BB with inst count: " << BB.size() << "\n";
        return true;
    }
};

char InsertDetTicks::ID = 1;
static RegisterPass<InsertDetTicks> Y("insert-det-ticks", "Instrument code with determinstic ticks");
}
