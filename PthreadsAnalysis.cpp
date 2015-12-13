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
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

#include <fstream>

#define DET_BB_THRESHOLD 4
#define BB_INFO_FILE "./bbinfo.dat"

using namespace llvm;

namespace {

FunctionType *ftype = FunctionType::get(Type::getInt32Ty(getGlobalContext()), true);
FunctionType *bbprof_ftype = FunctionType::get(Type::getInt32Ty(getGlobalContext()), Type::getInt32Ty(getGlobalContext()), true);
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
        if (fun != nullptr) {
            if ( fun->getName().find( "pthread_mutex_lock" ) != std::string::npos ||
                 fun->getName().find( "pthread_cond_broadcast" ) != std::string::npos ||
                 fun->getName().find( "pthread_cond_timedwait" ) != std::string::npos ||
                 fun->getName().find( "pthread_cond_signal" ) != std::string::npos ||
                 fun->getName().find( "pthread_barrier_wait" ) != std::string::npos ||
                 fun->getName().find( "pthread_cond_wait" ) != std::string::npos) {
                Module *M = inst->getModule();
                if (M == nullptr) {
                    return;
                }

                // Wrap the function around with 319 and 320
                IRBuilder<> IRB(inst);
                FunctionType *dftype = FunctionType::get(Type::getInt32Ty(M->getContext()), true);
                Function *DetFunc = cast<Function>(M->getOrInsertFunction("syscall", dftype));
                CallInst::Create(DetFunc, {IRB.getInt32(319)})->insertBefore(inst);
                CallInst::Create(DetFunc, {IRB.getInt32(320)})->insertAfter(inst);
            }
        }
    }
};


char WrapPthreads::ID = 0;
static RegisterPass<WrapPthreads> X("wrap-pthreads", "Instrument pthreads functions with determinstic hints");

static RegisterStandardPasses DetPassRegistration(PassManagerBuilder::EP_OptimizerLast,
  [](const PassManagerBuilder&, legacy::PassManagerBase& PM) {
    PM.add(new WrapPthreads());
});
/*
 * Most BBs will end with __NR_syscall_det_tick
 * There we're gonna read the profile data
 */
struct InsertDetTicks : public BasicBlockPass {
    static char ID; // Pass identification, replacement for typeid
    uint64_t BB_cnt;
    uint64_t BB_prof_cnt;

    struct bb_info {
        uint64_t is_parallel;
        uint64_t total;
    } __attribute__((packed));

    struct bb_info *bb;
    InsertDetTicks() : BasicBlockPass(ID) {
        // First you read the profile data
        BB_cnt = 0;
        BB_prof_cnt = 0;

        uint64_t cnt;
        std::ifstream bbinfo_file(BB_INFO_FILE, std::ios::in | std::ios::binary);
        bbinfo_file.read((char *)&cnt, sizeof(uint64_t));
        errs() << "reading " << cnt << "\n";
        BB_prof_cnt = cnt;
        bb = (struct bb_info *) malloc(sizeof(struct bb_info) * cnt);
        memset(bb, 0, cnt * sizeof(struct bb_info));
        bbinfo_file.read((char *)bb, sizeof(struct bb_info) * cnt);
    }

    bool runOnBasicBlock(BasicBlock &BB) {
        // Skip those very small BBs, much heuristic, so random
        if (BB.size() < DET_BB_THRESHOLD) {
            return false;
        }

        Module *M = BB.getModule();
        if (M == nullptr) {
            return false;
        }

        BB_cnt ++;
        if (BB_cnt >= BB_prof_cnt) {
            // It goes way beyond the profile data
            return true;
        }
        Function *DetFunc = cast<Function>(M->getOrInsertFunction("syscall", ftype));
        IRBuilder<> IRB(&BB.back());

        if (bb[BB_cnt].total > 0) {
            errs() << "load me " << (bb[BB_cnt].total) << "\n";
            IRB.CreateCall(DetFunc, {IRB.getInt32(321), IRB.getInt64(bb[BB_cnt].total >> 8)});
        }

        return true;
    }
};

char InsertDetTicks::ID = 1;
static RegisterPass<InsertDetTicks> Y("insert-det-ticks", "Instrument code with determinstic ticks");

/*
 * Most BBs will end with __NR_syscall_det_tick
 *
 */
struct BBProfile : public BasicBlockPass {
    static char ID; // Pass identification, replacement for typeid
    long BB_cnt;
    BBProfile() : BasicBlockPass(ID) {
        BB_cnt = 0;
    }

    bool runOnBasicBlock(BasicBlock &BB) {
        if (BB.size() < DET_BB_THRESHOLD) {
            return false;
        }

        Module *M = BB.getModule();
        if (M == nullptr) {
            return false;
        }

        BB_cnt ++;
        errs() << "Inserting prof functions on BB: " << BB.getName() << " " << BB_cnt << "\n";
        Function *BBProfStart = cast<Function>(M->getOrInsertFunction("bbprof_start", bbprof_ftype));
        Function *BBProfEnd   = cast<Function>(M->getOrInsertFunction("bbprof_end"  , bbprof_ftype));

        IRBuilder<> IRBb(&BB.back());
        CallInst::Create(BBProfEnd, {IRBb.getInt32(BB_cnt)})->insertBefore(&BB.back());

        IRBuilder<> IRBf(&BB.front());
        CallInst::Create(BBProfStart, {IRBf.getInt32(BB_cnt)})->insertAfter(&BB.front());

        errs() << "lol:" << BB.getParent()->getName() << "\n";
        return true;
    }
};

char BBProfile::ID = 1;
static RegisterPass<BBProfile> Z("bb-profile", "Instrument basic blocks with profiling hints");
}
