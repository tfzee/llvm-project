//===-- URCLInstrInfo.cpp - URCL Instruction Information ----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// URCLDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains the URCL implementation of the TargetInstrInfo class.
//
//===----------------------------------------------------------------------===//

#include "URCLInstrInfo.h"
#include "URCL.h"
#include "URCLMachineFunctionInfo.h"
#include "URCLSubtarget.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineMemOperand.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/Support/ErrorHandling.h"

using namespace llvm;

#define GET_INSTRINFO_CTOR_DTOR
#include "URCLGenInstrInfo.inc"

// Pin the vtable to this file.
void URCLInstrInfo::anchor() {}

URCLInstrInfo::URCLInstrInfo(const URCLSubtarget &ST)
    : URCLGenInstrInfo(ST, RI, URCL::ADJCALLSTACKDOWN, URCL::ADJCALLSTACKUP),
      RI(ST), Subtarget(ST) {}

void URCLInstrInfo::copyPhysReg(MachineBasicBlock &MBB,
                                MachineBasicBlock::iterator I,
                                const DebugLoc &DL, Register DestReg,
                                Register SrcReg, bool KillSrc,
                                bool RenamableDest, bool RenamableSrc) const {
  unsigned numSubRegs = 0;
  unsigned movOpc = 0;
  const unsigned *subRegIdx = nullptr;
  bool ExtraG0 = false;

  assert(URCL::IntRegsRegClass.contains(DestReg, SrcReg));
  BuildMI(MBB, I, DL, get(URCL::MOVrr), DestReg)
      .addReg(SrcReg, getKillRegState(KillSrc));
  return;
}

void URCLInstrInfo::storeRegToStackSlot(MachineBasicBlock &MBB,
                                        MachineBasicBlock::iterator I,
                                        Register SrcReg, bool isKill, int FI,
                                        const TargetRegisterClass *RC,
                                        Register VReg,
                                        MachineInstr::MIFlag Flags) const {
  DebugLoc DL;
  if (I != MBB.end())
    DL = I->getDebugLoc();

  if (RC == &URCL::IntRegsRegClass) {
    BuildMI(MBB, I, DL, get(URCL::LSTR_ri))
        .addFrameIndex(FI)
        .addImm(0)
        .addReg(SrcReg, getKillRegState(isKill))
        .setMIFlags(Flags);
  } else {
    llvm_unreachable("Can't store this register to stack slot");
  }
}

void URCLInstrInfo::loadRegFromStackSlot(MachineBasicBlock &MBB,
                                         MachineBasicBlock::iterator I,
                                         Register DestReg, int FI,
                                         const TargetRegisterClass *RC,
                                         Register VReg, unsigned SubReg,
                                         MachineInstr::MIFlag Flags) const {
  DebugLoc DL;
  if (I != MBB.end())
    DL = I->getDebugLoc();

  if (RC == &URCL::IntRegsRegClass) {
    BuildMI(MBB, I, DL, get(URCL::LLOD_ri))
        .addReg(DestReg, RegState::Define)
        .addFrameIndex(FI)
        .addImm(0)
        .setMIFlags(Flags);
  } else {
    llvm_unreachable("Can't load this register from stack slot");
  }
}
