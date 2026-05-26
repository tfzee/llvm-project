//===-- URCLInstrInfo.h - URCL Instruction Information --------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains the URCL implementation of the TargetInstrInfo class.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_URCL_URCLINSTRINFO_H
#define LLVM_LIB_TARGET_URCL_URCLINSTRINFO_H

#include "URCLRegisterInfo.h"
#include "llvm/CodeGen/TargetInstrInfo.h"

#define GET_INSTRINFO_HEADER
#include "URCLGenInstrInfo.inc"

namespace llvm {

class URCLSubtarget;

class URCLInstrInfo : public URCLGenInstrInfo {
  const URCLRegisterInfo RI;
  const URCLSubtarget &Subtarget;
  virtual void anchor();

public:
  explicit URCLInstrInfo(const URCLSubtarget &ST);
  void copyPhysReg(MachineBasicBlock &MBB, MachineBasicBlock::iterator I,
                   const DebugLoc &DL, Register DestReg, Register SrcReg,
                   bool KillSrc, bool RenamableDest = false,
                   bool RenamableSrc = false) const override;
  void storeRegToStackSlot(MachineBasicBlock &MBB,
                           MachineBasicBlock::iterator I, Register SrcReg,
                           bool isKill, int FI, const TargetRegisterClass *RC,
                           Register VReg,
                           MachineInstr::MIFlag Flags) const override;

  void loadRegFromStackSlot(MachineBasicBlock &MBB,
                            MachineBasicBlock::iterator I, Register DestReg,
                            int FI, const TargetRegisterClass *RC,
                            Register VReg, unsigned SubReg,
                            MachineInstr::MIFlag Flags) const override;
  const URCLRegisterInfo &getRegisterInfo() const { return RI; }
};
} // namespace llvm

#endif
