//===--- URCL.h - declare URCL target feature support ---------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file declares URCL TargetInfo objects.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_LIB_BASIC_TARGETS_URCL_H
#define LLVM_CLANG_LIB_BASIC_TARGETS_URCL_H
#include "clang/Basic/TargetInfo.h"
#include "clang/Basic/TargetOptions.h"
#include "llvm/Support/Compiler.h"
#include "llvm/TargetParser/Triple.h"
namespace clang {
namespace targets {
//  class for URCL (32-bit) .
class LLVM_LIBRARY_VISIBILITY URCLTargetInfo : public TargetInfo {
  static const TargetInfo::GCCRegAlias GCCRegAliases[];
  static const char *const GCCRegNames[];
  bool SoftFloat;

public:
  URCLTargetInfo(const llvm::Triple &Triple, const TargetOptions &)
      : TargetInfo(Triple), SoftFloat(false) {

    if (Triple.getArch() == llvm::Triple::urcl8) {
      PointerWidth = PointerAlign = 8;
      SuitableAlign = 8;

      IntWidth = IntAlign = 8;
      ShortWidth = 16;
      ShortAlign = 8;
      LongWidth = 16;
      LongAlign = 8;
      LongLongWidth = 32;
      LongLongAlign = 8;

      FloatWidth = 32;
      FloatAlign = 8;
      DoubleWidth = 64;
      DoubleAlign = 8;
      LongDoubleWidth = 64;
      LongDoubleAlign = 8;

      SizeType = UnsignedChar;
      PtrDiffType = SignedChar;
      IntPtrType = SignedChar;

    } else if (Triple.getArch() == llvm::Triple::urcl16) {
      PointerWidth = PointerAlign = 16;
      SuitableAlign = 16;

      IntWidth = IntAlign = 16;

      ShortWidth = 16;
      ShortAlign = 16;
      LongWidth = 32;
      LongAlign = 16;
      LongLongWidth = 64;
      LongLongAlign = 16;

      FloatWidth = 32;
      FloatAlign = 16;
      DoubleWidth = 64;
      DoubleAlign = 16;
      LongDoubleWidth = 64;
      LongDoubleAlign = 16;

      SizeType = UnsignedShort;
      PtrDiffType = SignedShort;
      IntPtrType = SignedShort;

    } else {
      // Default to 32-bit
      PointerWidth = PointerAlign = 32;
      SuitableAlign = 32;

      IntWidth = IntAlign = 32;
      ShortWidth = 16;
      ShortAlign = 16;
      LongWidth = LongAlign = 32;
      LongLongWidth = 64;
      LongLongAlign = 32;

      FloatWidth = 32;
      FloatAlign = 32;
      DoubleWidth = 64;
      DoubleAlign = 32;
      LongDoubleWidth = 64;
      LongDoubleAlign = 32;

      SizeType = UnsignedInt;
      PtrDiffType = SignedInt;
      IntPtrType = SignedInt;
    }

    resetDataLayout();
  }
  bool handleTargetFeatures(std::vector<std::string> &Features,
                            DiagnosticsEngine &Diags) override {
    // Check if software floating point is enabled
    if (llvm::is_contained(Features, "+soft-float")) {
      SoftFloat = true;
    }
    return true;
  }
  bool
  initFeatureMap(llvm::StringMap<bool> &Features, DiagnosticsEngine &Diags,
                 StringRef CPU,
                 const std::vector<std::string> &FeaturesVec) const override {
    return true;
  }
  bool hasFeature(StringRef Feature) const override;
  void getTargetDefines(const LangOptions &Opts,
                        MacroBuilder &Builder) const override;
  llvm::SmallVector<Builtin::InfosShard> getTargetBuiltins() const override {
    return {};
  }
  BuiltinVaListKind getBuiltinVaListKind() const override {
    return TargetInfo::VoidPtrBuiltinVaList;
  }
  bool validateAsmConstraint(const char *&Name,
                             TargetInfo::ConstraintInfo &info) const override {
    return false;
  }
  std::string_view getClobbers() const override { return ""; }
  ArrayRef<const char *> getGCCRegNames() const override;
  ArrayRef<TargetInfo::GCCRegAlias> getGCCRegAliases() const override;

  bool isValidCPUName(StringRef Name) const override {
    return Name == "generic";
  }
  bool setCPU(const std::string &Name) override { return isValidCPUName(Name); }
  void fillValidCPUList(SmallVectorImpl<StringRef> &Values) const override {
    Values.push_back("generic");
  }
  std::string_view getTargetCPU() const { return "generic"; }
};
} // namespace targets
} // namespace clang
#endif
