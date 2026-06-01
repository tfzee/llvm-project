//===-- URCLTargetInfo.cpp - URCL Target Implementation -----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "TargetInfo/URCLTargetInfo.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Compiler.h"
using namespace llvm;

Target &llvm::getTheURCL8Target() {
  static Target TheURCLTarget;
  return TheURCLTarget;
}

Target &llvm::getTheURCL16Target() {
  static Target TheURCLTarget;
  return TheURCLTarget;
}

Target &llvm::getTheURCL32Target() {
  static Target TheURCLTarget;
  return TheURCLTarget;
}

extern "C" LLVM_ABI LLVM_EXTERNAL_VISIBILITY void
LLVMInitializeURCLTargetInfo() {
  RegisterTarget<Triple::urcl8, /*HasJIT=*/false> X(getTheURCL8Target(), "urcl8",
                                                    "URCL8", "URCL8");
  RegisterTarget<Triple::urcl16, /*HasJIT=*/false> Y(getTheURCL16Target(), "urcl16",
                                                    "URCL16", "URCL16");
  RegisterTarget<Triple::urcl32, /*HasJIT=*/false> Z(getTheURCL32Target(), "urcl32",
                                                    "URCL32", "URCL32");
}
