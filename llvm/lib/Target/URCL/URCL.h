//===-- Sparc.h - Top-level interface for Sparc representation --*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains the entry points for global functions defined in the LLVM
// Sparc back-end.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_SPARC_SPARC_H
#define LLVM_LIB_TARGET_SPARC_SPARC_H

#include "MCTargetDesc/URCLMCTargetDesc.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Target/TargetMachine.h"

namespace llvm {
class AsmPrinter;
class FunctionPass;
class MCInst;
class MachineInstr;
class PassRegistry;
class URCLTargetMachine;

FunctionPass *createURCLISelDag(URCLTargetMachine &TM);
FunctionPass *createURCLCleanupPass();
void initializeURCLDAGToDAGISelLegacyPass(PassRegistry &);
void initializeURCLAsmPrinterPass(PassRegistry &);

namespace URCLISD {} // namespace URCLISD

} // end namespace llvm
#endif
