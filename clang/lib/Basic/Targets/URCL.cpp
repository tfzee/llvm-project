//===--- URCL.cpp - Implement URCL target feature support ---------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements URCL TargetInfo objects.
//
//===----------------------------------------------------------------------===//

#include "URCL.h"
#include "Targets.h"
#include "clang/Basic/MacroBuilder.h"
#include "clang/Basic/TargetInfo.h"
#include "llvm/ADT/StringSwitch.h"

using namespace clang;
using namespace clang::targets;

const char *const URCLTargetInfo::GCCRegNames[] = {
    // clang-format off
    // Integer registers
    "r0",  "r1",  "r2",  "r3",  "r4",  "r5",  "r6",  "r7",  "r8",  "r9",  "r10",
    "r11", "r12", "r13", "r14", "r15", "r16", "SP"
    // clang-format on
};

ArrayRef<const char *> URCLTargetInfo::getGCCRegNames() const {
  return llvm::ArrayRef(GCCRegNames);
}

const TargetInfo::GCCRegAlias URCLTargetInfo::GCCRegAliases[] = {
    {{"r0"}, "r0"},   {{"r1"}, "r1"},   {{"r2"}, "r2"},   {{"r3"}, "r3"},
    {{"r4"}, "r4"},   {{"r5"}, "r5"},   {{"r6"}, "r6"},   {{"r7"}, "r7"},
    {{"r8"}, "r8"},   {{"r9"}, "r9"},   {{"r10"}, "r10"}, {{"r11"}, "r11"},
    {{"r12"}, "r12"}, {{"r13"}, "r13"}, {{"r14"}, "r14"}, {{"r15"}, "r15"},
    {{"r16"}, "r16"}, {{"SP"}, "SP"}};

ArrayRef<TargetInfo::GCCRegAlias> URCLTargetInfo::getGCCRegAliases() const {
  return llvm::ArrayRef(GCCRegAliases);
}

bool URCLTargetInfo::hasFeature(StringRef Feature) const {
  return llvm::StringSwitch<bool>(Feature)
      .Case("softfloat", SoftFloat)
      .Case("URCL", true)
      .Default(false);
}

// struct URCLCPUInfo {
//   llvm::StringLiteral Name;
//   URCLTargetInfo::CPUKind kind;
// };

// static constexpr URCLCPUInfo CPUInfo[] = {
//     {"URCL_8", URCLTargetInfo::CPUKind::CK_GENERIC8},
//     {"URCL_16", URCLTargetInfo::CPUKind::CK_GENERIC16},
//     {"URCL_32", URCLTargetInfo::CPUKind::CK_GENERIC32},
// };

// void URCLTargetInfo::fillValidCPUList(
//     SmallVectorImpl<StringRef> &Values) const {
//   for (const URCLCPUInfo &Info : CPUInfo)
//     Values.push_back(Info.Name);
// }

// URCLTargetInfo::CPUKind URCLTargetInfo::getCPUKind(StringRef Name) const {
//   const URCLCPUInfo *Item = llvm::find_if(
//       CPUInfo, [Name](const URCLCPUInfo &Info) { return Info.Name == Name; });

//   if (Item == std::end(CPUInfo))
//     return CK_GENERIC32;
//   return Item->kind;
// }

void URCLTargetInfo::getTargetDefines(const LangOptions &Opts,
                                      MacroBuilder &Builder) const {
  DefineStd(Builder, "URCL", Opts);
  
  if (SoftFloat)
    Builder.defineMacro("SOFT_FLOAT", "1");
  if(getTriple().isURCL32()){
    Builder.defineMacro("__URCL_BITWIDTH__", "32"); 
  }
  if(getTriple().isURCL16()){
    Builder.defineMacro("__URCL_BITWIDTH__", "16"); 
  }
  if(getTriple().isURCL8()){
    Builder.defineMacro("__URCL_BITWIDTH__", "8"); 
  }
}
