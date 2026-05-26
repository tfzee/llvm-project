//===-- URCLFrameLowering.cpp - URCL Frame Information ------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains the URCL implementation of TargetFrameLowering class.
//
//===----------------------------------------------------------------------===//

#include "URCLFrameLowering.h"
#include "MCTargetDesc/URCLMCTargetDesc.h"
#include "URCLInstrInfo.h"
#include "URCLMachineFunctionInfo.h"
#include "URCLSubtarget.h"
#include "llvm/CodeGen/CFIInstBuilder.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineModuleInfo.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetOptions.h"

using namespace llvm;

static cl::opt<bool>
    DisableLeafProc("disable-URCL-leaf-proc", cl::init(false),
                    cl::desc("Disable URCL leaf procedure optimization."),
                    cl::Hidden);

void URCLFrameLowering::emitSPAdjustment(MachineFunction &MF,
                                         MachineBasicBlock &MBB,
                                         MachineBasicBlock::iterator MBBI,
                                         int NumBytes, unsigned SUBri,
                                         unsigned ADDri) const {
  DebugLoc DL;
  const URCLInstrInfo &TII =
      *static_cast<const URCLInstrInfo *>(MF.getSubtarget().getInstrInfo());

  if (NumBytes < 0) {
    BuildMI(MBB, MBBI, DL, TII.get(SUBri), URCL::SP)
        .addReg(URCL::SP)
        .addImm(((-NumBytes) + 3) / 4);
  } else {
    BuildMI(MBB, MBBI, DL, TII.get(ADDri), URCL::SP)
        .addReg(URCL::SP)
        .addImm((NumBytes + 3) / 4);
  }
  return;
}

URCLFrameLowering::URCLFrameLowering(const URCLSubtarget &ST)
    : TargetFrameLowering(TargetFrameLowering::StackGrowsDown, Align(8), 0,
                          Align(8),
                          /*StackRealignable=*/false) {}

void URCLFrameLowering::emitPrologue(MachineFunction &MF,
                                     MachineBasicBlock &MBB) const {
  // URCLMachineFunctionInfo *FuncInfo = MF.getInfo<URCLMachineFunctionInfo>();

  assert(&MF.front() == &MBB && "Shrink-wrapping not yet supported");
  MachineFrameInfo &MFI = MF.getFrameInfo();
  // const URCLSubtarget &Subtarget = MF.getSubtarget<URCLSubtarget>();
  MachineBasicBlock::iterator MBBI = MBB.begin();

  int NumBytes = (int)MFI.getStackSize();

  // if (MFI.adjustsStack() && hasReservedCallFrame(MF))
  //   NumBytes += MFI.getMaxCallFrameSize();

  // NumBytes = Subtarget.getAdjustedFrameSize(NumBytes);

  NumBytes = alignTo(NumBytes, MFI.getMaxAlign());

  MFI.setStackSize(NumBytes);

  if (NumBytes != 0)
    emitSPAdjustment(MF, MBB, MBBI, -NumBytes, URCL::SUBri, URCL::ADDri);

  if (MF.needsFrameMoves()) {
    CFIInstBuilder CFIBuilder(MBB, MBBI, MachineInstr::NoFlags);
    CFIBuilder.buildDefCFARegister(URCL::SP);
    CFIBuilder.buildWindowSave();
    // CFIBuilder.buildRegister(URCL::O7, URCL::I7);
  }
}

MachineBasicBlock::iterator URCLFrameLowering::eliminateCallFramePseudoInstr(
    MachineFunction &MF, MachineBasicBlock &MBB,
    MachineBasicBlock::iterator I) const {
  if (!hasReservedCallFrame(MF)) {
    MachineInstr &MI = *I;
    int Size = MI.getOperand(0).getImm();
    if (MI.getOpcode() == URCL::ADJCALLSTACKDOWN)
      Size = -Size;

    if (Size)
      emitSPAdjustment(MF, MBB, I, Size, URCL::SUBri, URCL::ADDri);
  }
  return MBB.erase(I);
}

void URCLFrameLowering::emitEpilogue(MachineFunction &MF,
                                     MachineBasicBlock &MBB) const {
  // URCLMachineFunctionInfo *FuncInfo = MF.getInfo<URCLMachineFunctionInfo>();
  MachineBasicBlock::iterator MBBI = MBB.getLastNonDebugInstr();
  // const URCLInstrInfo &TII =
  //     *static_cast<const URCLInstrInfo *>(MF.getSubtarget().getInstrInfo());
  DebugLoc Dl = MBBI->getDebugLoc();
  assert((MBBI->getOpcode() == URCL::RET) &&
         "Can only put epilog before 'retl' or 'maybe tail_call' instruction!");
  MachineFrameInfo &MFI = MF.getFrameInfo();

  int NumBytes = (int)MFI.getStackSize();
  if (NumBytes != 0)
    emitSPAdjustment(MF, MBB, MBBI, NumBytes, URCL::SUBri, URCL::ADDri);

  // if (MBBI->getOpcode() == URCL::TAIL_CALL) {
  // MBB.addLiveIn(URCL::O7);
  // BuildMI(MBB, MBBI, dl, TII.get(URCL::ORrr), URCL::G1)
  //     .addReg(URCL::G0)
  //     .addReg(URCL::O7);
  // BuildMI(MBB, MBBI, dl, TII.get(URCL::ORrr), URCL::O7)
  //     .addReg(URCL::G0)
  // .addReg(URCL::G1);
  // }
}

bool URCLFrameLowering::hasReservedCallFrame(const MachineFunction &MF) const {
  // Reserve call frame if there are no variable sized objects on the stack.
  return !MF.getFrameInfo().hasVarSizedObjects();
}

// hasFPImpl - Return true if the specified function should have a dedicated
// frame pointer register.  This is true if the function has variable sized
// allocas or if frame pointer elimination is disabled.
bool URCLFrameLowering::hasFPImpl(const MachineFunction &MF) const {
  const MachineFrameInfo &MFI = MF.getFrameInfo();
  return MF.getTarget().Options.DisableFramePointerElim(MF) ||
         MFI.hasVarSizedObjects() || MFI.isFrameAddressTaken();
}

StackOffset
URCLFrameLowering::getFrameIndexReference(const MachineFunction &MF, int FI,
                                          Register &FrameReg) const {
  int64_t FrameOffset = MF.getFrameInfo().getObjectOffset(FI);
  FrameReg = URCL::SP;
  return StackOffset::getFixed(FrameOffset + MF.getFrameInfo().getStackSize());
}

// [[maybe_unused]] static bool verifyLeafProcRegUse(MachineRegisterInfo *MRI) {

//   for (unsigned reg = URCL::I0; reg <= URCL::I7; ++reg)
//     if (MRI->isPhysRegUsed(reg))
//       return false;

//   for (unsigned reg = URCL::L0; reg <= URCL::L7; ++reg)
//     if (MRI->isPhysRegUsed(reg))
//       return false;

//   return true;
// }

// bool URCLFrameLowering::isLeafProc(MachineFunction &MF) const {

//   MachineRegisterInfo &MRI = MF.getRegInfo();
//   MachineFrameInfo &MFI = MF.getFrameInfo();

//   return !(MFI.hasCalls()                 // has calls
//            || MRI.isPhysRegUsed(URCL::L0) // Too many registers needed
//            || MRI.isPhysRegUsed(URCL::O6) // %sp is used
//            || hasFP(MF)                   // need %fp
//            || MF.hasInlineAsm());         // has inline assembly
// }

// void URCLFrameLowering::remapRegsForLeafProc(MachineFunction &MF) const {
//   MachineRegisterInfo &MRI = MF.getRegInfo();
//   // Remap %i[0-7] to %o[0-7].
//   for (unsigned reg = URCL::I0; reg <= URCL::I7; ++reg) {
//     if (!MRI.isPhysRegUsed(reg))
//       continue;

//     unsigned mapped_reg = reg - URCL::I0 + URCL::O0;

//     // Replace I register with O register.
//     MRI.replaceRegWith(reg, mapped_reg);

//     // Also replace register pair super-registers.
//     if ((reg - URCL::I0) % 2 == 0) {
//       unsigned preg = (reg - URCL::I0) / 2 + URCL::I0_I1;
//       unsigned mapped_preg = preg - URCL::I0_I1 + URCL::O0_O1;
//       MRI.replaceRegWith(preg, mapped_preg);
//     }
//   }

//   // Rewrite MBB's Live-ins.
//   for (MachineBasicBlock &MBB : MF) {
//     for (unsigned reg = URCL::I0_I1; reg <= URCL::I6_I7; ++reg) {
//       if (!MBB.isLiveIn(reg))
//         continue;
//       MBB.removeLiveIn(reg);
//       MBB.addLiveIn(reg - URCL::I0_I1 + URCL::O0_O1);
//     }
//     for (unsigned reg = URCL::I0; reg <= URCL::I7; ++reg) {
//       if (!MBB.isLiveIn(reg))
//         continue;
//       MBB.removeLiveIn(reg);
//       MBB.addLiveIn(reg - URCL::I0 + URCL::O0);
//     }
//   }

//   assert(verifyLeafProcRegUse(&MRI));
// #ifdef EXPENSIVE_CHECKS
//   MF.verify(0, "After LeafProc Remapping");
// #endif
// }

void URCLFrameLowering::determineCalleeSaves(MachineFunction &MF,
                                             BitVector &SavedRegs,
                                             RegScavenger *RS) const {
  TargetFrameLowering::determineCalleeSaves(MF, SavedRegs, RS);
  // if (!DisableLeafProc && isLeafProc(MF)) {
  //   URCLMachineFunctionInfo *MFI = MF.getInfo<URCLMachineFunctionInfo>();
  //   MFI->setLeafProc(true);

  //   remapRegsForLeafProc(MF);
  // }
}
