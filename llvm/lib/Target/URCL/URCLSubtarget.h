//===-- URCLSubtarget.h - Define Subtarget for the URCL -------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file declares the URCL specific subclass of TargetSubtargetInfo.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_URCL_URCLSUBTARGET_H
#define LLVM_LIB_TARGET_URCL_URCLSUBTARGET_H

#include "MCTargetDesc/URCLMCTargetDesc.h"
#include "URCLFrameLowering.h"
#include "URCLISelLowering.h"
#include "URCLInstrInfo.h"
#include "llvm/CodeGen/SelectionDAGTargetInfo.h"
#include "llvm/CodeGen/TargetSubtargetInfo.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/TargetParser/Triple.h"

#define GET_SUBTARGETINFO_HEADER
#include "URCLGenSubtargetInfo.inc"

namespace llvm {
class StringRef;
class URCLSubtarget : public URCLGenSubtargetInfo {
  // ReserveRegister[i] - Register #i is not available as a general purpose
  // register.
  BitVector ReserveRegister;

  virtual void anchor();

#define GET_SUBTARGETINFO_MACRO(ATTRIBUTE, DEFAULT, GETTER)                    \
  bool ATTRIBUTE = DEFAULT;
#include "URCLGenSubtargetInfo.inc"

  URCLInstrInfo InstrInfo;
  URCLTargetLowering TLInfo;
  std::unique_ptr<const SelectionDAGTargetInfo> TSInfo;
  URCLFrameLowering FrameLowering;

  URCLSubtarget &initializeSubtargetDependencies(StringRef CPU,
                                                 StringRef TuneCPU,
                                                 StringRef FS);

public:
  URCLSubtarget(const StringRef &CPU, const StringRef &TuneCPU,
                const StringRef &FS, const TargetMachine &TM);

  ~URCLSubtarget() override;
  const URCLInstrInfo *getInstrInfo() const override { return &InstrInfo; }
  const TargetFrameLowering *getFrameLowering() const override {
    return &FrameLowering;
  }
  const URCLRegisterInfo *getRegisterInfo() const override {
    return &InstrInfo.getRegisterInfo();
  }
  const SelectionDAGTargetInfo *getSelectionDAGInfo() const override {
    return TSInfo.get();
  }
  // void initLibcallLoweringInfo(LibcallLoweringInfo &Info) const override;
  // bool enableMachineScheduler() const override;

  void ParseSubtargetFeatures(StringRef CPU, StringRef TuneCPU, StringRef FS);
  const URCLTargetLowering *getTargetLowering() const override {
    return &TLInfo;
  }
};

} // namespace llvm
#endif
