//===-- URCLMCTargetDesc.cpp - URCL Target Descriptions -----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file provides URCL specific target descriptions.
//
//===----------------------------------------------------------------------===//

#include "URCLMCTargetDesc.h"
#include "TargetInfo/URCLTargetInfo.h"
#include "URCLInstPrinter.h"
#include "URCLMCAsmInfo.h"
#include "URCLTargetStreamer.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/ErrorHandling.h"

namespace llvm {
namespace URCLASITag {
#define GET_ASITagsList_IMPL
#include "URCLGenSearchableTables.inc"
} // end namespace URCLASITag

namespace URCLPrefetchTag {
#define GET_PrefetchTagsList_IMPL
#include "URCLGenSearchableTables.inc"
} // end namespace URCLPrefetchTag
} // end namespace llvm

using namespace llvm;

#define GET_INSTRINFO_MC_DESC
#define ENABLE_INSTR_PREDICATE_VERIFIER
#include "URCLGenInstrInfo.inc"

#define GET_SUBTARGETINFO_MC_DESC
#include "URCLGenSubtargetInfo.inc"

#define GET_REGINFO_MC_DESC
#include "URCLGenRegisterInfo.inc"

static MCAsmInfo *createURCLMCAsmInfo(const MCRegisterInfo &MRI,
                                      const Triple &TT,
                                      const MCTargetOptions &Options) {
  MCAsmInfo *MAI = new URCLELFMCAsmInfo(TT, Options);
  unsigned Reg = MRI.getDwarfRegNum(URCL::SP, true);
  MCCFIInstruction Inst = MCCFIInstruction::cfiDefCfa(nullptr, Reg, 0);
  MAI->addInitialFrameState(Inst);
  return MAI;
}

static MCInstrInfo *createURCLMCInstrInfo() {
  MCInstrInfo *X = new MCInstrInfo();
  InitURCLMCInstrInfo(X);
  return X;
}

static MCRegisterInfo *createURCLMCRegisterInfo(const Triple &TT) {
  MCRegisterInfo *X = new MCRegisterInfo();
  InitURCLMCRegisterInfo(X, URCL::R16);
  return X;
}

static MCSubtargetInfo *createURCLMCSubtargetInfo(const Triple &TT,
                                                  StringRef CPU, StringRef FS) {
  MCSubtargetInfo *STI =
      createURCLMCSubtargetInfoImpl(TT, CPU, /*TuneCPU=*/CPU, FS);
  return STI;
}

// static MCTargetStreamer *
// createObjectTargetStreamer(MCStreamer &S, const MCSubtargetInfo &STI) {
//   return new URCLTargetELFStreamer(S, STI);
// }

static MCTargetStreamer *createTargetAsmStreamer(MCStreamer &S,
                                                 formatted_raw_ostream &OS,
                                                 MCInstPrinter *InstPrint) {
  return new URCLTargetAsmStreamer(S, OS);
}

static MCTargetStreamer *createNullTargetStreamer(MCStreamer &S) {
  return new URCLTargetStreamer(S);
}

static MCInstPrinter *createURCLMCInstPrinter(const Triple &T,
                                              unsigned SyntaxVariant,
                                              const MCAsmInfo &MAI,
                                              const MCInstrInfo &MII,
                                              const MCRegisterInfo &MRI) {
  return new URCLInstPrinter(MAI, MII, MRI);
}

extern "C" LLVM_ABI LLVM_EXTERNAL_VISIBILITY void LLVMInitializeURCLTargetMC() {
  // Register the MC asm info.
  RegisterMCAsmInfoFn X(getTheURCL8Target(), createURCLMCAsmInfo);
  RegisterMCAsmInfoFn Y(getTheURCL16Target(), createURCLMCAsmInfo);
  RegisterMCAsmInfoFn Z(getTheURCL32Target(), createURCLMCAsmInfo);

  for (Target *T : {&getTheURCL8Target(), &getTheURCL16Target(), &getTheURCL32Target()}) {
    // Register the MC instruction info.
    TargetRegistry::RegisterMCInstrInfo(*T, createURCLMCInstrInfo);

    // Register the MC register info.
    TargetRegistry::RegisterMCRegInfo(*T, createURCLMCRegisterInfo);

    // Register the MC subtarget info.
    TargetRegistry::RegisterMCSubtargetInfo(*T, createURCLMCSubtargetInfo);

    // Register the MC Code Emitter.
    // TargetRegistry::RegisterMCCodeEmitter(*T, createURCLMCCodeEmitter);

    // Register the asm backend.
    // TargetRegistry::RegisterMCAsmBackend(*T, createURCLAsmBackend);

    // Register the object target streamer.
    // TargetRegistry::RegisterObjectTargetStreamer(*T,
    //                                              createObjectTargetStreamer);

    // Register the asm streamer.
    TargetRegistry::RegisterAsmTargetStreamer(*T, createTargetAsmStreamer);

    // Register the null streamer.
    // TargetRegistry::RegisterNullTargetStreamer(*T, createNullTargetStreamer);

    // Register the MCInstPrinter
    TargetRegistry::RegisterMCInstPrinter(*T, createURCLMCInstPrinter);
  }
}
