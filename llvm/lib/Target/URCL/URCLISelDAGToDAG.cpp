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
#include "llvm/CodeGen/ISDOpcodes.h"
#include "llvm/CodeGen/SelectionDAGISel.h"
#include "llvm/CodeGen/SelectionDAGNodes.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"
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
  bool selectLoadStackSlotSimplifier(SDNode *N);
  bool selectStoreStackSlotSimplifier(SDNode *N);

  bool SelectAddrFI(SDValue Addr, SDValue &Base) {
    if (auto *FIN = dyn_cast<FrameIndexSDNode>(Addr)) {
      Base = CurDAG->getTargetFrameIndex(
          FIN->getIndex(), TLI->getPointerTy(CurDAG->getDataLayout()));
      return true;
    }
    return false;
  }
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

bool URCLDAGToDAGISel::selectLoadStackSlotSimplifier(SDNode *N) {
  if (auto *ST = dyn_cast<LoadSDNode>(N)) {
    SDValue Address = ST->getBasePtr();
    if (!isa<FrameIndexSDNode>(Address)) {
      return false;
    }
    SDLoc DL(N);
    SDValue Ops[] = {Address, CurDAG->getTargetConstant(0, DL, MVT::i32), ST->getChain()};
    llvm::errs() << "N=================================\n";
    CurDAG->dump(true);
    llvm::errs() << "Rsults: \n";
    auto *NewNode = CurDAG->SelectNodeTo(N, URCL::LLOD_ri, ST->getValueType(0), MVT::Other, Ops);
    llvm::errs() << "OrDoesIt?: \n";
    CurDAG->dump(true);
    NewNode->dump(CurDAG);
    llvm::errs() << "\n";
    return true;
  }
  return false;
}

bool URCLDAGToDAGISel::selectStoreStackSlotSimplifier(SDNode *N) {
  if (auto *ST = dyn_cast<StoreSDNode>(N)) {
    SDValue Address = ST->getBasePtr();
    if (!isa<FrameIndexSDNode>(Address)) {
      return false;
    }
    SDLoc DL(N);
    SDValue Ops[] = {Address, CurDAG->getTargetConstant(0, DL, MVT::i32),
                     ST->getValue(), ST->getChain()};
    CurDAG->SelectNodeTo(N, URCL::LSTR_ri, MVT::Other, Ops);
    return true;
  }
  return false;
}

void URCLDAGToDAGISel::Select(SDNode *N) {
  SDLoc DL(N);
  if (N->isMachineOpcode()) {
    N->setNodeId(-1);
    return;
  }

  // loads/stores from SP *will* be aligned
  if (selectLoadStackSlotSimplifier(N)) {
    return;
  }
  if (selectStoreStackSlotSimplifier(N)) {
    return;
  }
  SelectCode(N);
}

FunctionPass *llvm::createURCLISelDag(URCLTargetMachine &TM) {
  return new URCLDAGToDAGISelLegacy(TM);
}
