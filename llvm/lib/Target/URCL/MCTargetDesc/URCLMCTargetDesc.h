//===-- URCLMCTargetDesc.h - URCL Target Descriptions ---------*- C++ -*-===//
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

#ifndef LLVM_LIB_TARGET_URCL_MCTARGETDESC_URCLMCTARGETDESC_H
#define LLVM_LIB_TARGET_URCL_MCTARGETDESC_URCLMCTARGETDESC_H

#include "llvm/ADT/StringRef.h"
#include "llvm/Support/DataTypes.h"

#include <memory>

namespace llvm {
class MCAsmBackend;
class MCCodeEmitter;
class MCContext;
class MCInstrInfo;
class MCObjectTargetWriter;
class MCRegisterInfo;
class MCSubtargetInfo;
class MCTargetOptions;
class Target;
class Triple;

MCCodeEmitter *createURCLMCCodeEmitter(const MCInstrInfo &MCII, MCContext &Ctx);
MCAsmBackend *createURCLAsmBackend(const Target &T, const MCSubtargetInfo &STI,
                                   const MCRegisterInfo &MRI,
                                   const MCTargetOptions &Options);
std::unique_ptr<MCObjectTargetWriter>
createURCLELFObjectWriter(uint8_t OSABI);

// Defines symbolic names for URCL v9 ASI tag names.
namespace URCLASITag {
struct ASITag {
  const char *Name;
  const char *AltName;
  unsigned Encoding;
};

#define GET_ASITagsList_DECL
#include "URCLGenSearchableTables.inc"
} // end namespace URCLASITag

// Defines symbolic names for URCL v9 prefetch tag names.
namespace URCLPrefetchTag {
struct PrefetchTag {
  const char *Name;
  unsigned Encoding;
};

#define GET_PrefetchTagsList_DECL
#include "URCLGenSearchableTables.inc"
} // end namespace URCLPrefetchTag
} // namespace llvm

// Defines symbolic names for URCL registers.  This defines a mapping from
// register name to register number.
//
#define GET_REGINFO_ENUM
#include "URCLGenRegisterInfo.inc"

// Defines symbolic names for the URCL instructions.
//
#define GET_INSTRINFO_ENUM
#define GET_INSTRINFO_MC_HELPER_DECLS
#include "URCLGenInstrInfo.inc"

#define GET_SUBTARGETINFO_ENUM
#include "URCLGenSubtargetInfo.inc"

#endif
