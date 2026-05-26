//===-- URCLISelDAGToDAG.cpp - A dag to dag inst selector for URCL ------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines an instruction selector for the URCL target.
//
//===----------------------------------------------------------------------===//

#include "URCLSelectionDAGInfo.h"
#include "URCLTargetMachine.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/SelectionDAGISel.h"
#include "llvm/Support/ErrorHandling.h"
using namespace llvm;

#define DEBUG_TYPE "urcl-isel"
#define PASS_NAME "URCL DAG->DAG Pattern Instruction Selection"

//===----------------------------------------------------------------------===//
// Instruction Selector Implementation
//===----------------------------------------------------------------------===//

//===--------------------------------------------------------------------===//
/// URCLDAGToDAGISel - URCL specific code to select URCL machine
/// instructions for SelectionDAG operations.
///

namespace {
class URCLDAGToDAGISel : public SelectionDAGISel {
  /// Subtarget - Keep a pointer to the URCL Subtarget around so that we can
  /// make the right decision when generating code for different targets.
  const URCLSubtarget *Subtarget = nullptr;

public:
  URCLDAGToDAGISel() = delete;

  explicit URCLDAGToDAGISel(URCLTargetMachine &tm) : SelectionDAGISel(tm) {}

  bool runOnMachineFunction(MachineFunction &MF) override {
    Subtarget = &MF.getSubtarget<URCLSubtarget>();
    return SelectionDAGISel::runOnMachineFunction(MF);
  }

#include "URCLGenDAGISel.inc"

  void Select(SDNode *N) override;
};

class URCLDAGToDAGISelLegacy : public SelectionDAGISelLegacy {
public:
  static char ID;
  explicit URCLDAGToDAGISelLegacy(URCLTargetMachine &tm)
      : SelectionDAGISelLegacy(ID, std::make_unique<URCLDAGToDAGISel>(tm)) {}
};
} // end anonymous namespace

char URCLDAGToDAGISelLegacy::ID = 0;

INITIALIZE_PASS(URCLDAGToDAGISelLegacy, DEBUG_TYPE, PASS_NAME, false, false)

void URCLDAGToDAGISel::Select(SDNode *N) {
  SDLoc DL(N);
  if (N->isMachineOpcode()) {
    N->setNodeId(-1);
    return;
  }

  // unsigned Opcode = N->getOpcode();
  // if (Opcode == ISD::GlobalAddress) {
  //   auto *GA = cast<GlobalAddressSDNode>(N);
  //   EVT VT = N->getValueType(0);

  //   SDValue TGA = CurDAG->getTargetGlobalAddress(GA->getGlobal(), DL, VT,
  //                                                GA->getOffset());
  //   SDNode *ResNode =
  //       CurDAG->getMachineNode(URCLISD::GLOBAL_REF, DL, VT, TGA);

  //   ReplaceNode(N, ResNode);
  //   return;
  // }

  SelectCode(N);
}

FunctionPass *llvm::createURCLISelDag(URCLTargetMachine &TM) {
  return new URCLDAGToDAGISelLegacy(TM);
}
