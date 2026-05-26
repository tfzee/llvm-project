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
  MachineRegisterInfo &MRI = MF.getRegInfo();
  const TargetInstrInfo &TII = *MF.getSubtarget().getInstrInfo();
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
    assert(ObjOff % 4 == 0);
    assert(StackSize % 4 == 0);
    int Offset = (ObjOff / 4) + (StackSize / 4) + OldImm;
    MI.getOperand(FIOperandNum).ChangeToRegister(FrameReg, false);
    MI.getOperand(FIOperandNum + 1).ChangeToImmediate(Offset);
    return false;
  }

  Register FrameByteReg = URCL::R16;
  BuildMI(MBB, II, DL, TII.get(URCL::BSLri), FrameByteReg)
      .addReg(FrameReg)
      .addImm(2);

  Register FinalReg;
  if (MI.getOperand(0).isReg() && MI.getOperand(0).isDef()) {
    FinalReg = MI.getOperand(0).getReg();
  } else {
    FinalReg = FrameByteReg;
  }
  BuildMI(MBB, II, DL, TII.get(URCL::ADDri), FinalReg)
      .addReg(FrameByteReg)
      .addImm(Offset);

  // if (MI.getOpcode() == URCL::LSTR_ri || MI.getOpcode() == URCL::LLOD_ri ||
  //     MI.getOpcode() == URCL::ADDri) {
  //   int FrameIndex = MI.getOperand(FIOperandNum).getIndex();
  //   int Offset = MFI.getObjectOffset(FrameIndex) +
  //                MI.getOperand(FIOperandNum + 1).getImm();
  //   Offset += MFI.getStackSize();
  //   MI.getOperand(FIOperandNum).ChangeToRegister(FrameByteReg, false);
  //   MI.getOperand(FIOperandNum + 1).ChangeToImmediate(Offset);
  //   return false;
  // }
  // if (MI.getOpcode() == URCL::STR_r) {
  //   MI.setDesc(TII.get(URCL::LSTR_ri));
  //   MachineOperand DataReg = MI.getOperand(0);
  //   MI.removeOperand(0);
  //   MI.getOperand(0).ChangeToRegister(FrameByteReg, false);
  //   MI.addOperand(MachineOperand::CreateImm(Offset));
  //   MI.addOperand(DataReg);
  //   return false;
  // }
  // if (MI.getOpcode() == URCL::LOD_r) {
  //   MI.setDesc(TII.get(URCL::LLOD_ri));
  //   MI.getOperand(1).ChangeToRegister(FrameByteReg, false);
  //   MI.addOperand(MachineOperand::CreateImm(Offset));
  //   return false;
  // }

  MI.getOperand(FIOperandNum).ChangeToRegister(FinalReg, false, false, true);

  return false;
}

Register URCLRegisterInfo::getFrameRegister(const MachineFunction &MF) const {
  return URCL::SP;
}
