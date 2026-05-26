//===-- URCLTargetStreamer.cpp - URCL Target Streamer Methods -----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file provides URCL specific target streamer methods.
//
//===----------------------------------------------------------------------===//

#include "URCLTargetStreamer.h"
#include "URCLInstPrinter.h"
#include "URCLMCTargetDesc.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/MC/MCELFObjectWriter.h"
#include "llvm/MC/MCRegister.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/Support/FormattedStream.h"

using namespace llvm;

// static unsigned getEFlagsForFeatureSet(const MCSubtargetInfo &STI) {
//   unsigned EFlags = 0;
//   return EFlags;
// }

// pin vtable to this file
URCLTargetStreamer::URCLTargetStreamer(MCStreamer &S) : MCTargetStreamer(S) {}

void URCLTargetStreamer::anchor() {}

URCLTargetAsmStreamer::URCLTargetAsmStreamer(MCStreamer &S,
                                             formatted_raw_ostream &OS)
    : URCLTargetStreamer(S), OS(OS) {}

void URCLTargetAsmStreamer::emitURCLRegisterIgnore(unsigned reg) {
  OS << "\t.register "
     << "%" << StringRef(URCLInstPrinter::getRegisterName(reg)).lower()
     << ", #ignore\n";
}

void URCLTargetAsmStreamer::emitURCLRegisterScratch(unsigned reg) {
  OS << "\t.register "
     << "%" << StringRef(URCLInstPrinter::getRegisterName(reg)).lower()
     << ", #scratch\n";
}

// URCLTargetELFStreamer::URCLTargetELFStreamer(MCStreamer &S,
//                                              const MCSubtargetInfo &STI)
//     : URCLTargetStreamer(S) {
//   ELFObjectWriter &W = getStreamer().getWriter();
//   unsigned EFlags = W.getELFHeaderEFlags();

//   EFlags |= getEFlagsForFeatureSet(STI);

//   W.setELFHeaderEFlags(EFlags);
// }

// MCELFStreamer &URCLTargetELFStreamer::getStreamer() {
//   return static_cast<MCELFStreamer &>(Streamer);
// }
