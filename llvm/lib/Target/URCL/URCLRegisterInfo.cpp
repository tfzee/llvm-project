//===-- URCLRegisterInfo.cpp - URCL Register Information ----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains the URCL implementation of the TargetRegisterInfo class.
//
//===----------------------------------------------------------------------===//

#include "URCLRegisterInfo.h"
#include "MCTargetDesc/URCLMCTargetDesc.h"
#include "URCL.h"
#include "URCLSubtarget.h"
#include "llvm/ADT/BitVector.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/RegisterScavenging.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/CodeGen/TargetRegisterInfo.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

#define GET_REGINFO_TARGET_DESC
#include "URCLGenRegisterInfo.inc"

URCLRegisterInfo::URCLRegisterInfo(const URCLSubtarget &STI)
    : URCLGenRegisterInfo(URCL::R16) {}

const MCPhysReg *
URCLRegisterInfo::getCalleeSavedRegs(const MachineFunction *MF) const {
  return CSR_SaveList;
}

const uint32_t *
URCLRegisterInfo::getCallPreservedMask(const MachineFunction &MF,
                                       CallingConv::ID CC) const {
  return CSR_RegMask;
}

// const uint32_t *
// URCLRegisterInfo::getRTCallPreservedMask(CallingConv::ID CC) const {
//   return RTCSR_RegMask;
// }

BitVector URCLRegisterInfo::getReservedRegs(const MachineFunction &MF) const {
  BitVector Reserved(getNumRegs());
  // const URCLSubtarget &Subtarget = MF.getSubtarget<URCLSubtarget>();

  Reserved.set(URCL::SP);
  Reserved.set(URCL::R0);
  Reserved.set(URCL::R16);

  assert(checkAllSuperRegsMarked(Reserved));
  return Reserved;
}

bool URCLRegisterInfo::eliminateFrameIndex(MachineBasicBlock::iterator II,
                                           int SPAdj, unsigned FIOperandNum,
                                           RegScavenger *RS) const {
  assert(SPAdj == 0 && "Unexpected");

  MachineInstr &MI = *II;
  MachineFunction &MF = *MI.getParent()->getParent();
  MachineFrameInfo &MFI = MF.getFrameInfo();
  const TargetInstrInfo &TII = *MF.getSubtarget().getInstrInfo();
  const TargetRegisterInfo &TRI = *MF.getSubtarget().getRegisterInfo();
  const URCLSubtarget &Subtarget = MF.getSubtarget<URCLSubtarget>();
  const uint32_t Align = Subtarget.getWordSizeBytes();

  DebugLoc DL = MI.getDebugLoc();
  MachineBasicBlock &MBB = *MI.getParent();

  int FrameIndex = MI.getOperand(FIOperandNum).getIndex();
  const URCLFrameLowering *TFI = getFrameLowering(MF);

  Register FrameReg;
  int Offset = TFI->getFrameIndexReference(MF, FrameIndex, FrameReg).getFixed();

  // we assume its only stack slots for this so we dont need fancy byte
  // adressing
  if (MI.getOpcode() == URCL::LSTR_ri || MI.getOpcode() == URCL::LLOD_ri) {
    int FrameIndex = MI.getOperand(FIOperandNum).getIndex();
    auto ObjOff = MFI.getObjectOffset(FrameIndex);
    auto OldImm = MI.getOperand(FIOperandNum + 1).getImm();
    auto StackSize = MFI.getStackSize();
    assert(OldImm == 0);
    assert(ObjOff % Align == 0);
    assert(StackSize % Align == 0);
    int Offset = (ObjOff / Align) + (StackSize / Align) + OldImm;
    MI.getOperand(FIOperandNum).ChangeToRegister(FrameReg, false);
    MI.getOperand(FIOperandNum + 1).ChangeToImmediate(Offset);
    return false;
  }

  Register FinalReg = URCL::R16;
  if (MI.getOperand(0).isReg() && MI.getOperand(0).isDef()) {
    Register DestReg = MI.getOperand(0).getReg();
    if (!MI.mayStore() && !MI.readsRegister(DestReg, &TRI)) {
      FinalReg = DestReg;
    }
  }
  bool IsFrameRegKilled = MI.killsRegister(FrameReg, &TRI);

  if (Offset % Align == 0 && Offset != 0 && Align > 1) {
    // if we can do the offset on word level then delay the shift till after
    BuildMI(MBB, II, DL, TII.get(URCL::ADDri), FinalReg)
        .addReg(FrameReg, getKillRegState(IsFrameRegKilled))
        .addImm(Offset / Align);

    BuildMI(MBB, II, DL, TII.get(URCL::BSLri), FinalReg)
        .addReg(FinalReg, RegState::Kill)
        .addImm(Align / 2);

  } else if (Align > 1) {
    BuildMI(MBB, II, DL, TII.get(URCL::BSLri), FinalReg)
        .addReg(FrameReg, getKillRegState(IsFrameRegKilled))
        .addImm(Align / 2);

    if (Offset != 0) {
      BuildMI(MBB, II, DL, TII.get(URCL::ADDri), FinalReg)
          .addReg(FinalReg, RegState::Kill)
          .addImm(Offset);
    }
  }

  bool IsFinalRegKilled = (FinalReg == URCL::R16);
  MI.getOperand(FIOperandNum)
      .ChangeToRegister(FinalReg, /*isDef=*/false, /*isImp=*/false,
                        IsFinalRegKilled);

  return false;
}

Register URCLRegisterInfo::getFrameRegister(const MachineFunction &MF) const {
  return URCL::SP;
}
