//===- URCLMCAsmInfo.cpp - URCL asm properties --------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains the declarations of the URCLMCAsmInfo properties.
//
//===----------------------------------------------------------------------===//

#include "URCLMCAsmInfo.h"
#include "llvm/BinaryFormat/Dwarf.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/MCTargetOptions.h"
#include "llvm/TableGen/DirectiveEmitter.h"
#include "llvm/TargetParser/Triple.h"

using namespace llvm;

void URCLELFMCAsmInfo::anchor() {}

URCLELFMCAsmInfo::URCLELFMCAsmInfo(const Triple &TheTriple,
                                   const MCTargetOptions &Options)
    : MCAsmInfoELF(Options) {

  CodePointerSize = CalleeSaveStackSlotSize = 4;

  CommentString = "//";
  HasIdentDirective = false;

  Data8bitsDirective = "\tdw\t";
  Data16bitsDirective = "\tdw\t";
  Data32bitsDirective = "\tdw\t";
  ZeroDirective = "\t.skip\t";
  GlobalDirective = "\t.global\t."; 
  LabelSuffix = "";
  SupportsDebugInformation = true;

  HasDotTypeDotSizeDirective = false;
  HasSingleParameterDotFile = false;

  GlobalDirective = nullptr;

  ExceptionsType = ExceptionHandling::None;

  UsesELFSectionDirectiveForBSS = false;
}


const MCExpr *URCLELFMCAsmInfo::getExprForPersonalitySymbol(
    const MCSymbol *Sym, unsigned Encoding, MCStreamer &Streamer) const {
  if (Encoding & dwarf::DW_EH_PE_pcrel) {
    MCContext &Ctx = Streamer.getContext();
    return MCSpecifierExpr::create(Sym, ELF::R_SPARC_DISP32, Ctx);
  }

  return MCAsmInfo::getExprForPersonalitySymbol(Sym, Encoding, Streamer);
}
const MCExpr *
URCLELFMCAsmInfo::getExprForFDESymbol(const MCSymbol *Sym, unsigned Encoding,
                                      MCStreamer &Streamer) const {
  if (Encoding & dwarf::DW_EH_PE_pcrel) {
    MCContext &Ctx = Streamer.getContext();
    return MCSpecifierExpr::create(Sym, ELF::R_SPARC_DISP32, Ctx);
  }
  return MCAsmInfo::getExprForFDESymbol(Sym, Encoding, Streamer);
}

void URCLELFMCAsmInfo::printSpecifierExpr(raw_ostream &OS,
                                          const MCSpecifierExpr &Expr) const {
  // StringRef S = URCL::getSpecifierName(Expr.getSpecifier());
  // if (!S.empty())
  // OS << '%' << S << '(';
  OS << "%PRINT_SPEC_TODO(";
  printExpr(OS, *Expr.getSubExpr());
  // if (!S.empty())
  //   OS << ')';
}
