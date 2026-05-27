
//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_URCL_URCLSELECTIONDAGINFO_H
#define LLVM_LIB_TARGET_URCL_URCLSELECTIONDAGINFO_H

#include "llvm/CodeGen/SelectionDAGTargetInfo.h"

#define GET_SDNODE_ENUM
#include "URCLGenSDNodeInfo.inc"

namespace llvm {

class URCLSelectionDAGInfo : public SelectionDAGGenTargetInfo {
public:
  URCLSelectionDAGInfo();
  ~URCLSelectionDAGInfo() override;

  const char *getTargetNodeName(unsigned Opcode) const override {
#define CASE(NAME)                                                             \
  case URCLISD::NAME:                                                          \
    return "URCLISD::" #NAME

    // These nodes don't have corresponding entries in *.td files yet.
    switch (static_cast<URCLISD::GenNodeType>(Opcode)) {
      CASE(WORD_ADDR);
      CASE(CALL);
      CASE(GLOBAL_REF);
      CASE(RET);
      CASE(RET_GLUE);
    }
#undef CASE
    return SelectionDAGGenTargetInfo::getTargetNodeName(Opcode);
  }
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_URCL_URCLSELECTIONDAGINFO_H
