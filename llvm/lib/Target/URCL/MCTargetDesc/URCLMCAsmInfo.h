//===- URCLMCAsmInfo.h - URCL asm properties -----------------*- C++ -*--===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains the declaration of the URCLMCAsmInfo class.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_URCL_MCTARGETDESC_URCLMCASMINFO_H
#define LLVM_LIB_TARGET_URCL_MCTARGETDESC_URCLMCASMINFO_H

#include "llvm/IR/GlobalObject.h"
#include "llvm/MC/MCAsmInfoELF.h"

namespace llvm {

class Triple;

class URCLELFMCAsmInfo : public MCAsmInfoELF {
  void anchor() override;

public:
  explicit URCLELFMCAsmInfo(const Triple &TheTriple,
                            const MCTargetOptions &Options);

  const MCExpr *
  getExprForPersonalitySymbol(const MCSymbol *Sym, unsigned Encoding,
                              MCStreamer &Streamer) const override;
  const MCExpr *getExprForFDESymbol(const MCSymbol *Sym, unsigned Encoding,
                                    MCStreamer &Streamer) const override;

  void printSpecifierExpr(raw_ostream &OS,
                          const MCSpecifierExpr &Expr) const override;
};

namespace URCL {
// uint16_t parseSpecifier(StringRef name);
// StringRef getSpecifierName(uint16_t S);
} // namespace URCL

} // end namespace llvm

#endif // LLVM_LIB_TARGET_URCL_MCTARGETDESC_URCLMCASMINFO_H
