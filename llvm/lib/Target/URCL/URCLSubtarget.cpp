//===-- URCLSubtarget.cpp - URCL Subtarget Information ------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements the URCL specific subclass of TargetSubtargetInfo.
//
//===----------------------------------------------------------------------===//

#include "URCLSubtarget.h"
#include "URCLSelectionDAGInfo.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/MathExtras.h"
#include "llvm/Target/TargetMachine.h"

using namespace llvm;

#define DEBUG_TYPE "URCL-subtarget"

#define GET_SUBTARGETINFO_TARGET_DESC
#define GET_SUBTARGETINFO_CTOR
#include "URCLGenSubtargetInfo.inc"

void URCLSubtarget::anchor() {}

URCLSubtarget &URCLSubtarget::initializeSubtargetDependencies(StringRef CPU,
                                                              StringRef TuneCPU,
                                                              StringRef FS) {
  // const Triple &TT = getTargetTriple();
  std::string CPUName = std::string(CPU);
  if (CPUName.empty())
    CPUName = "generic";
  if (TuneCPU.empty())
    TuneCPU = CPUName;
  ParseSubtargetFeatures(CPUName, TuneCPU, FS);
  return *this;
}

URCLSubtarget::URCLSubtarget(const StringRef &CPU, const StringRef &TuneCPU,
                             const StringRef &FS, const TargetMachine &TM)
    : URCLGenSubtargetInfo(TM.getTargetTriple(), CPU, TuneCPU, FS),
      ReserveRegister(TM.getMCRegisterInfo().getNumRegs()),
      InstrInfo(initializeSubtargetDependencies(CPU, TuneCPU, FS)),
      TLInfo(TM, *this), FrameLowering(*this) {
  TSInfo = std::make_unique<URCLSelectionDAGInfo>();
}

URCLSubtarget::~URCLSubtarget() = default;
