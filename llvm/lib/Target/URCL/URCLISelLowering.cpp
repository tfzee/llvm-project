//===-- URCLISelLowering.cpp - URCL DAG Lowering Implementation ---------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements the interfaces that URCL uses to lower LLVM code into a
// selection DAG.
//
//===----------------------------------------------------------------------===//

#include "URCLISelLowering.h"
#include "MCTargetDesc/URCLMCTargetDesc.h"
#include "URCLMachineFunctionInfo.h"
#include "URCLRegisterInfo.h"
#include "URCLSelectionDAGInfo.h"
#include "URCLTargetMachine.h"
#include "URCLTargetObjectFile.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/CodeGen/CallingConvLower.h"
#include "llvm/CodeGen/ISDOpcodes.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/SelectionDAG.h"
#include "llvm/CodeGen/SelectionDAGNodes.h"
#include "llvm/CodeGen/TargetLowering.h"
#include "llvm/CodeGen/TargetLoweringObjectFileImpl.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/DiagnosticInfo.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/KnownBits.h"
using namespace llvm;

SDValue URCLTargetLowering::LowerOperation(SDValue Op,
                                           SelectionDAG &DAG) const {
  switch (Op.getOpcode()) {
  case ISD::LOAD:
  case ISD::EXTLOAD:
    return LowerLOAD(Op, DAG);
  case ISD::STORE:
    return LowerSTORE(Op, DAG);
  case ISD::SELECT:
    return LowerSELECT(Op, DAG);
  case ISD::GlobalAddress:
    return LowerGLOBAL_REF(Op, DAG);
  default:
    return SDValue();
  }
}

URCLTargetLowering::URCLTargetLowering(const TargetMachine &TM,
                                       const URCLSubtarget &STI)
    : TargetLowering(TM, STI), Subtarget(&STI) {
  // MVT PtrVT = MVT::getIntegerVT(TM.getPointerSizeInBits(0));
  setBooleanVectorContents(ZeroOrNegativeOneBooleanContent);
  setBooleanContents(ZeroOrNegativeOneBooleanContent);

  setOperationAction(ISD::GlobalAddress, MVT::i32, Custom);

  addRegisterClass(MVT::i32, &URCL::IntRegsRegClass);

  for (unsigned Op = 0; Op < ISD::BUILTIN_OP_END; ++Op) {
    setOperationAction(Op, MVT::v2i32, Expand);
  }
  for (MVT VT : MVT::integer_fixedlen_vector_valuetypes()) {
    setLoadExtAction(ISD::SEXTLOAD, VT, MVT::v2i32, Expand);
    setLoadExtAction(ISD::ZEXTLOAD, VT, MVT::v2i32, Expand);
    setLoadExtAction(ISD::EXTLOAD, VT, MVT::v2i32, Expand);

    setLoadExtAction(ISD::SEXTLOAD, MVT::v2i32, VT, Expand);
    setLoadExtAction(ISD::ZEXTLOAD, MVT::v2i32, VT, Expand);
    setLoadExtAction(ISD::EXTLOAD, MVT::v2i32, VT, Expand);

    setTruncStoreAction(VT, MVT::v2i32, Expand);
    setTruncStoreAction(MVT::v2i32, VT, Expand);
  }
  setOperationAction(ISD::LOAD, MVT::v2i32, Expand);
  setOperationAction(ISD::STORE, MVT::v2i32, Expand);
  setOperationAction(ISD::EXTRACT_VECTOR_ELT, MVT::v2i32, Expand);
  setOperationAction(ISD::BUILD_VECTOR, MVT::v2i32, Expand);

  setOperationAction(ISD::LOAD, MVT::i64, Expand);
  setOperationAction(ISD::STORE, MVT::i64, Expand);

  setOperationAction(ISD::LOAD, MVT::i32, Custom);
  setOperationAction(ISD::STORE, MVT::i32, Custom);
  setLoadExtAction(ISD::EXTLOAD, MVT::i32, MVT::i8, Custom);
  setLoadExtAction(ISD::EXTLOAD, MVT::i32, MVT::i16, Custom);
  setLoadExtAction(ISD::ZEXTLOAD, MVT::i32, MVT::i8, Expand);
  setLoadExtAction(ISD::SEXTLOAD, MVT::i32, MVT::i8, Expand);
  setLoadExtAction(ISD::ZEXTLOAD, MVT::i32, MVT::i16, Expand);
  setLoadExtAction(ISD::SEXTLOAD, MVT::i32, MVT::i16, Expand);

  setOperationAction(ISD::STORE, MVT::i16, Expand);
  setOperationAction(ISD::STORE, MVT::i8, Expand);
  setTruncStoreAction(MVT::i32, MVT::i8, Custom);
  setTruncStoreAction(MVT::i32, MVT::i16, Custom);

  // setOperationAction(ISD::LOAD, MVT::i8,  Custom);
  // setOperationAction(ISD::STORE, MVT::i8, Custom);
  // setOperationAction(ISD::LOAD, MVT::i16, Custom);
  // setOperationAction(ISD::STORE, MVT::i16, Custom);
  // setOperationAction(ISD::LOAD, MVT::i32,  Custom);
  // setOperationAction(ISD::STORE, MVT::i32, Custom);

  // setLoadExtAction(ISD::SEXTLOAD, MVT::i32, MVT::i8, Expand);
  // setLoadExtAction(ISD::ZEXTLOAD, MVT::i32, MVT::i8, Expand);

  // for (MVT VT : MVT::fp_valuetypes()) {
  //   setLoadExtAction(ISD::EXTLOAD, VT, MVT::f16, Expand);
  //   setLoadExtAction(ISD::EXTLOAD, VT, MVT::f32, Expand);
  //   setLoadExtAction(ISD::EXTLOAD, VT, MVT::f64, Expand);
  // }

  for (MVT VT : MVT::integer_valuetypes())
    setLoadExtAction(ISD::SEXTLOAD, VT, MVT::i1, Promote);

  setTruncStoreAction(MVT::f32, MVT::f16, Expand);
  setTruncStoreAction(MVT::f64, MVT::f16, Expand);
  setTruncStoreAction(MVT::f64, MVT::f32, Expand);
  setTruncStoreAction(MVT::f128, MVT::f16, Expand);
  setTruncStoreAction(MVT::f128, MVT::f32, Expand);
  setTruncStoreAction(MVT::f128, MVT::f64, Expand);

  // setOperationAction(ISD::GlobalAddress, PtrVT, Custom);
  // setOperationAction(ISD::GlobalTLSAddress, PtrVT, Custom);
  // setOperationAction(ISD::ConstantPool, PtrVT, Custom);
  // setOperationAction(ISD::BlockAddress, PtrVT, Custom);

  // URCL doesn't have sext_inreg, replace them with shl/sra
  setOperationAction(ISD::SIGN_EXTEND_INREG, MVT::i16, Expand);
  setOperationAction(ISD::SIGN_EXTEND_INREG, MVT::i8, Expand);
  setOperationAction(ISD::SIGN_EXTEND_INREG, MVT::i1, Expand);

  // URCL has no REM or DIVREM operations.
  // setOperationAction(ISD::UREM, MVT::i32, Expand);
  // setOperationAction(ISD::SREM, MVT::i32, Expand);
  // setOperationAction(ISD::SDIVREM, MVT::i32, Expand);
  // setOperationAction(ISD::UDIVREM, MVT::i32, Expand);

  // Custom expand fp<->sint
  // setOperationAction(ISD::FP_TO_SINT, MVT::i32, Custom);
  // setOperationAction(ISD::SINT_TO_FP, MVT::i32, Custom);
  // setOperationAction(ISD::FP_TO_SINT, MVT::i64, Custom);
  // setOperationAction(ISD::SINT_TO_FP, MVT::i64, Custom);

  // Custom Expand fp<->uint
  // setOperationAction(ISD::FP_TO_UINT, MVT::i32, Custom);
  // setOperationAction(ISD::UINT_TO_FP, MVT::i32, Custom);
  // setOperationAction(ISD::FP_TO_UINT, MVT::i64, Custom);
  // setOperationAction(ISD::UINT_TO_FP, MVT::i64, Custom);

  // Lower f16 conversion operations into library calls
  // setOperationAction(ISD::FP16_TO_FP, MVT::f32, Expand);
  // setOperationAction(ISD::FP_TO_FP16, MVT::f32, Expand);
  // setOperationAction(ISD::FP16_TO_FP, MVT::f64, Expand);
  // setOperationAction(ISD::FP_TO_FP16, MVT::f64, Expand);
  // setOperationAction(ISD::FP16_TO_FP, MVT::f128, Expand);
  // setOperationAction(ISD::FP_TO_FP16, MVT::f128, Expand);

  setOperationAction(ISD::BITCAST, MVT::f32, Expand);
  setOperationAction(ISD::BITCAST, MVT::i32, Expand);

  // URCL has no select: expand to SELECT_CC.
  setOperationAction(ISD::SELECT, MVT::i32, Custom);
  // setOperationAction(ISD::SELECT, MVT::f32, Expand);
  // setOperationAction(ISD::SELECT, MVT::f64, Expand);
  // setOperationAction(ISD::SELECT, MVT::f128, Expand);

  setOperationAction(ISD::SETCC, MVT::i32, Legal);
  // setOperationAction(ISD::SETCC, MVT::f32, Expand);
  // setOperationAction(ISD::SETCC, MVT::f64, Expand);
  // setOperationAction(ISD::SETCC, MVT::f128, Expand);

  // URCL doesn't have BRCOND either, it has BR_CC.
  setOperationAction(ISD::BRCOND, MVT::Other, Expand);
  setOperationAction(ISD::BRIND, MVT::Other, Expand);

  setOperationAction(ISD::SELECT_CC, MVT::i32, Expand);
  // setOperationAction(ISD::SELECT_CC, MVT::f32, Custom);
  // setOperationAction(ISD::SELECT_CC, MVT::f64, Custom);
  // setOperationAction(ISD::SELECT_CC, MVT::f128, Custom);

  setOperationAction(ISD::ADDC, MVT::i32, Legal);
  setOperationAction(ISD::ADDE, MVT::i32, Legal);
  setOperationAction(ISD::SUBC, MVT::i32, Legal);
  setOperationAction(ISD::SUBE, MVT::i32, Legal);

  setMaxAtomicSizeInBitsSupported(0);

  setMinCmpXchgSizeInBits(32);

  // setOperationAction(ISD::ATOMIC_SWAP, MVT::i32, Legal);
  // setOperationAction(ISD::ATOMIC_FENCE, MVT::Other, Legal);
  // setOperationAction(ISD::ATOMIC_LOAD, MVT::i32, Custom);
  // setOperationAction(ISD::ATOMIC_STORE, MVT::i32, Custom);
  // setOperationAction(ISD::FNEG, MVT::f64, Custom);
  // setOperationAction(ISD::FABS, MVT::f64, Custom);

  setOperationAction(ISD::FSIN, MVT::f128, Expand);
  setOperationAction(ISD::FCOS, MVT::f128, Expand);
  setOperationAction(ISD::FSINCOS, MVT::f128, Expand);
  setOperationAction(ISD::FREM, MVT::f128, LibCall);
  setOperationAction(ISD::FMA, MVT::f128, Expand);
  setOperationAction(ISD::FSIN, MVT::f64, Expand);
  setOperationAction(ISD::FCOS, MVT::f64, Expand);
  setOperationAction(ISD::FSINCOS, MVT::f64, Expand);
  setOperationAction(ISD::FREM, MVT::f64, LibCall);
  setOperationAction(ISD::FMA, MVT::f64, Expand);
  setOperationAction(ISD::FSIN, MVT::f32, Expand);
  setOperationAction(ISD::FCOS, MVT::f32, Expand);
  setOperationAction(ISD::FSINCOS, MVT::f32, Expand);
  setOperationAction(ISD::FREM, MVT::f32, LibCall);
  setOperationAction(ISD::FMA, MVT::f32, Expand);
  setOperationAction(ISD::ROTL, MVT::i32, Expand);
  setOperationAction(ISD::ROTR, MVT::i32, Expand);
  setOperationAction(ISD::BSWAP, MVT::i32, Expand);
  setOperationAction(ISD::FCOPYSIGN, MVT::f128, Expand);
  setOperationAction(ISD::FCOPYSIGN, MVT::f64, Expand);
  setOperationAction(ISD::FCOPYSIGN, MVT::f32, Expand);
  setOperationAction(ISD::FPOW, MVT::f128, Expand);
  setOperationAction(ISD::FPOW, MVT::f64, Expand);
  setOperationAction(ISD::FPOW, MVT::f32, Expand);

  setOperationAction(ISD::SHL_PARTS, MVT::i32, Expand);
  setOperationAction(ISD::SRA_PARTS, MVT::i32, Expand);
  setOperationAction(ISD::SRL_PARTS, MVT::i32, Expand);

  // Expands to [SU]MUL_LOHI.
  setOperationAction(ISD::MULHU, MVT::i32, Legal);
  setOperationAction(ISD::MULHS, MVT::i32, Legal);
  setOperationAction(ISD::MUL, MVT::i32, Legal);

  // VASTART needs to be custom lowered to use the VarArgsFrameIndex.
  // setOperationAction(ISD::VASTART, MVT::Other, Custom);
  // VAARG needs to be lowered to not do unaligned accesses for doubles.
  // setOperationAction(ISD::VAARG, MVT::Other, Custom);

  setOperationAction(ISD::TRAP, MVT::Other, Legal);
  setOperationAction(ISD::DEBUGTRAP, MVT::Other, Legal);

  // Use the default implementation.
  setOperationAction(ISD::VACOPY, MVT::Other, Expand);
  setOperationAction(ISD::VAEND, MVT::Other, Expand);
  setOperationAction(ISD::STACKSAVE, MVT::Other, Expand);
  setOperationAction(ISD::STACKRESTORE, MVT::Other, Expand);
  // setOperationAction(ISD::DYNAMIC_STACKALLOC, MVT::i32, Custom);
  // setOperationAction(ISD::STACKADDRESS, MVT::Other, Custom);

  setStackPointerRegisterToSaveRestore(URCL::SP);

  setOperationAction(ISD::CTPOP, MVT::i32, Expand);

  setOperationAction(ISD::LOAD, MVT::f128, Expand);
  setOperationAction(ISD::STORE, MVT::f128, Expand);

  setOperationAction(ISD::FADD, MVT::f128, Expand);
  setOperationAction(ISD::FSUB, MVT::f128, Expand);
  setOperationAction(ISD::FMUL, MVT::f128, Expand);
  setOperationAction(ISD::FDIV, MVT::f128, Expand);
  setOperationAction(ISD::FSQRT, MVT::f128, Expand);
  setOperationAction(ISD::FNEG, MVT::f128, Expand);
  setOperationAction(ISD::FABS, MVT::f128, Expand);

  // setOperationAction(ISD::FP_EXTEND, MVT::f128, Custom);
  // setOperationAction(ISD::FP_ROUND, MVT::f64, Custom);
  // setOperationAction(ISD::FP_ROUND, MVT::f32, Custom);

  // Custom combine bitcast between f64 and v2i32
  // setTargetDAGCombine(ISD::BITCAST);

  setOperationAction(ISD::CTLZ, MVT::i32, Expand);
  setOperationAction(ISD::CTLZ, MVT::i64, Expand);
  setOperationAction(ISD::CTLZ_ZERO_POISON, MVT::i32, LibCall);
  setOperationAction(ISD::CTLZ_ZERO_POISON, MVT::i64, LibCall);

  setOperationAction(ISD::CTTZ, MVT::i32, Expand);
  setOperationAction(ISD::CTTZ, MVT::i64, Expand);
  setOperationAction(ISD::CTTZ_ZERO_POISON, MVT::i32, Expand);
  setOperationAction(ISD::CTTZ_ZERO_POISON, MVT::i64, Expand);

  // setOperationAction(ISD::INTRINSIC_WO_CHAIN, MVT::Other, Custom);

  // Some processors have no branch predictor and have pipelines longer than
  // what can be covered by the delay slot. This results in a stall, so mark
  // branches to be expensive on those processors.
  setJumpIsExpensive(false);
  // The high cost of branching means that using conditional moves will
  // still be profitable even if the condition is predictable.
  PredictableSelectIsExpensive = !isJumpExpensive();
  setMinFunctionAlignment(Align(4));

  computeRegisterProperties(Subtarget->getRegisterInfo());
}

#include "URCLGenCallingConv.inc"

static void emitReservedArgRegCallError(const MachineFunction &MF) {
  const Function &F = MF.getFunction();
  F.getContext().diagnose(DiagnosticInfoUnsupported{
      F, ("URCL doesn't support"
          " function calls if any of the argument registers is reserved.")});
}

static bool hasReturnsTwiceAttr(SelectionDAG &DAG, SDValue Callee,
                                const CallBase *Call) {
  if (Call)
    return Call->hasFnAttr(Attribute::ReturnsTwice);

  const Function *CalleeFn = nullptr;
  if (GlobalAddressSDNode *G = dyn_cast<GlobalAddressSDNode>(Callee)) {
    CalleeFn = dyn_cast<Function>(G->getGlobal());
  } else if (ExternalSymbolSDNode *E = dyn_cast<ExternalSymbolSDNode>(Callee)) {
    const Function &Fn = DAG.getMachineFunction().getFunction();
    const Module *M = Fn.getParent();
    const char *CalleeName = E->getSymbol();
    CalleeFn = M->getFunction(CalleeName);
  }

  if (!CalleeFn)
    return false;
  return CalleeFn->hasFnAttribute(Attribute::ReturnsTwice);
}

// Check whether any of the argument registers are reserved
static bool isAnyArgRegReserved(const URCLRegisterInfo *TRI,
                                const MachineFunction &MF) {
  // The register window design means that outgoing parameters at O*
  // will appear in the callee as I*.
  // Be conservative and check both sides of the register names.
  bool Outgoing =
      llvm::any_of(URCL::GPROutgoingArgRegClass, [TRI, &MF](MCPhysReg r) {
        return TRI->isReservedReg(MF, r);
      });
  bool Incoming =
      llvm::any_of(URCL::GPRIncomingArgRegClass, [TRI, &MF](MCPhysReg r) {
        return TRI->isReservedReg(MF, r);
      });
  return Outgoing || Incoming;
}

SDValue URCLTargetLowering::LowerCall(TargetLowering::CallLoweringInfo &CLI,
                                      SmallVectorImpl<SDValue> &InVals) const {
  SelectionDAG &DAG = CLI.DAG;
  SDLoc &dl = CLI.DL;
  SmallVectorImpl<ISD::OutputArg> &Outs = CLI.Outs;
  SmallVectorImpl<SDValue> &OutVals = CLI.OutVals;
  SmallVectorImpl<ISD::InputArg> &Ins = CLI.Ins;
  SDValue Chain = CLI.Chain;
  SDValue Callee = CLI.Callee;
  bool &isTailCall = CLI.IsTailCall;
  CallingConv::ID CallConv = CLI.CallConv;
  bool isVarArg = CLI.IsVarArg;
  MachineFunction &MF = DAG.getMachineFunction();
  LLVMContext &Ctx = *DAG.getContext();
  EVT PtrVT = getPointerTy(MF.getDataLayout());

  // Analyze operands of the call, assigning locations to each operand.
  SmallVector<CCValAssign, 16> ArgLocs;
  CCState CCInfo(CallConv, isVarArg, DAG.getMachineFunction(), ArgLocs,
                 *DAG.getContext());
  CCInfo.AnalyzeCallOperands(Outs, CC_URCL32);

  isTailCall = isTailCall && false; // IsEligibleForTailCallOptimization(CCInfo,
                                    // CLI, DAG.getMachineFunction());

  // Get the size of the outgoing arguments stack space requirement.
  unsigned ArgsSize = CCInfo.getStackSize();

  // Keep stack frames 8-byte aligned.
  ArgsSize = (ArgsSize + 7) & ~7;

  MachineFrameInfo &MFI = DAG.getMachineFunction().getFrameInfo();

  // Create local copies for byval args.
  SmallVector<SDValue, 8> ByValArgs;
  for (unsigned i = 0, e = Outs.size(); i != e; ++i) {
    ISD::ArgFlagsTy Flags = Outs[i].Flags;
    if (!Flags.isByVal())
      continue;

    SDValue Arg = OutVals[i];
    unsigned Size = Flags.getByValSize();
    Align Alignment = Flags.getNonZeroByValAlign();

    if (Size > 0U) {
      int FI = MFI.CreateStackObject(Size, Alignment, false);
      SDValue FIPtr = DAG.getFrameIndex(FI, getPointerTy(DAG.getDataLayout()));
      SDValue SizeNode = DAG.getConstant(Size, dl, MVT::i32);

      assert(false);
      Chain = DAG.getMemcpy(Chain, dl, FIPtr, Arg, SizeNode, Alignment,
                            false,        // isVolatile,
                            (Size <= 32), // AlwaysInline if size <= 32,
                            /*CI=*/nullptr, std::nullopt, MachinePointerInfo(),
                            MachinePointerInfo());
      ByValArgs.push_back(FIPtr);
    } else {
      SDValue nullVal;
      ByValArgs.push_back(nullVal);
    }
  }

  assert(!isTailCall || ArgsSize == 0);

  if (!isTailCall)
    Chain = DAG.getCALLSEQ_START(Chain, ArgsSize, 0, dl);

  SmallVector<std::pair<unsigned, SDValue>, 8> RegsToPass;
  SmallVector<SDValue, 8> MemOpChains;

  const unsigned StackOffset = 92;
  bool hasStructRetAttr = false;
  unsigned SRetArgSize = 0;
  // Walk the register/memloc assignments, inserting copies/loads.
  for (unsigned i = 0, realArgIdx = 0, byvalArgIdx = 0, e = ArgLocs.size();
       i != e; ++i, ++realArgIdx) {
    CCValAssign &VA = ArgLocs[i];
    SDValue Arg = OutVals[realArgIdx];

    ISD::ArgFlagsTy Flags = Outs[realArgIdx].Flags;

    // Use local copy if it is a byval arg.
    if (Flags.isByVal()) {
      Arg = ByValArgs[byvalArgIdx++];
      if (!Arg) {
        continue;
      }
    }

    // Promote the value if needed.
    switch (VA.getLocInfo()) {
    default:
      llvm_unreachable("Unknown loc info!");
    case CCValAssign::Full:
    case CCValAssign::Indirect:
      break;
    case CCValAssign::SExt:
      Arg = DAG.getNode(ISD::SIGN_EXTEND, dl, VA.getLocVT(), Arg);
      break;
    case CCValAssign::ZExt:
      Arg = DAG.getNode(ISD::ZERO_EXTEND, dl, VA.getLocVT(), Arg);
      break;
    case CCValAssign::AExt:
      Arg = DAG.getNode(ISD::ANY_EXTEND, dl, VA.getLocVT(), Arg);
      break;
    case CCValAssign::BCvt:
      Arg = DAG.getNode(ISD::BITCAST, dl, VA.getLocVT(), Arg);
      break;
    }

    if (Flags.isSRet()) {
      assert(VA.needsCustom());

      if (isTailCall)
        continue;

      assert(false);
      // store SRet argument in %sp+64
      SDValue StackPtr = DAG.getRegister(URCL::SP, MVT::i32);
      SDValue PtrOff = DAG.getIntPtrConstant(64, dl);
      PtrOff = DAG.getNode(ISD::ADD, dl, MVT::i32, StackPtr, PtrOff);
      assert(false);
      MemOpChains.push_back(
          DAG.getStore(Chain, dl, Arg, PtrOff, MachinePointerInfo()));
      hasStructRetAttr = true;
      // sret only allowed on first argument
      assert(Outs[realArgIdx].OrigArgIndex == 0);
      SRetArgSize =
          DAG.getDataLayout().getTypeAllocSize(CLI.getArgs()[0].IndirectType);
      continue;
    }

    if (VA.needsCustom()) {
      assert(false);
    }

    if (VA.getLocInfo() == CCValAssign::Indirect) {
      assert(false);
    }

    if (VA.isRegLoc()) {
      if (VA.getLocVT() != MVT::f32) {
        RegsToPass.push_back(std::make_pair(VA.getLocReg(), Arg));
        continue;
      }
      Arg = DAG.getNode(ISD::BITCAST, dl, MVT::i32, Arg);
      RegsToPass.push_back(std::make_pair(VA.getLocReg(), Arg));
      continue;
    }

    assert(VA.isMemLoc());

    SDValue StackPtr = DAG.getRegister(URCL::SP, MVT::i32);
    SDValue PtrOff =
        DAG.getIntPtrConstant(VA.getLocMemOffset() + StackOffset, dl);
    PtrOff = DAG.getNode(ISD::ADD, dl, MVT::i32, StackPtr, PtrOff);
    assert(false);
    MemOpChains.push_back(
        DAG.getStore(Chain, dl, Arg, PtrOff, MachinePointerInfo()));
  }

  if (!MemOpChains.empty())
    Chain = DAG.getNode(ISD::TokenFactor, dl, MVT::Other, MemOpChains);

  SDValue InGlue;
  for (const auto &[OrigReg, N] : RegsToPass) {
    Register Reg = isTailCall ? OrigReg : (OrigReg);
    Chain = DAG.getCopyToReg(Chain, dl, Reg, N, InGlue);
    InGlue = Chain.getValue(1);
  }

  bool hasReturnsTwice = hasReturnsTwiceAttr(DAG, Callee, CLI.CB);
  assert(!hasReturnsTwice);

  if (GlobalAddressSDNode *G = dyn_cast<GlobalAddressSDNode>(Callee))
    Callee = DAG.getTargetGlobalAddress(G->getGlobal(), dl, MVT::i32, 0);
  else if (ExternalSymbolSDNode *E = dyn_cast<ExternalSymbolSDNode>(Callee))
    Callee = DAG.getTargetExternalSymbol(E->getSymbol(), MVT::i32);

  SDVTList NodeTys = DAG.getVTList(MVT::Other, MVT::Glue);
  SmallVector<SDValue, 8> Ops;
  Ops.push_back(Chain);
  Ops.push_back(Callee);
  if (hasStructRetAttr)
    Ops.push_back(DAG.getTargetConstant(SRetArgSize, dl, MVT::i32));
  for (const auto &[OrigReg, N] : RegsToPass) {
    Register Reg = isTailCall ? OrigReg : (OrigReg);
    Ops.push_back(DAG.getRegister(Reg, N.getValueType()));
  }

  const URCLRegisterInfo *TRI = Subtarget->getRegisterInfo();
  assert(!hasReturnsTwice);
  const uint32_t *Mask =
      TRI->getCallPreservedMask(DAG.getMachineFunction(), CallConv);

  if (isAnyArgRegReserved(TRI, MF)) {
    emitReservedArgRegCallError(MF);
  }

  assert(Mask && "Missing call preserved mask for calling convention");
  Ops.push_back(DAG.getRegisterMask(Mask));

  if (InGlue.getNode()) {
    Ops.push_back(InGlue);
  }

  if (isTailCall) {
    assert(false);
  }

  Chain = DAG.getNode(URCLISD::CALL, dl, NodeTys, Ops);
  InGlue = Chain.getValue(1);

  Chain = DAG.getCALLSEQ_END(Chain, ArgsSize, 0, InGlue, dl);
  InGlue = Chain.getValue(1);

  SmallVector<CCValAssign, 16> RVLocs;
  CCState RVInfo(CallConv, isVarArg, DAG.getMachineFunction(), RVLocs,
                 *DAG.getContext());

  RVInfo.AnalyzeCallResult(Ins, RetCC_URCL32);

  for (unsigned I = 0; I != RVLocs.size(); ++I) {
    assert(RVLocs[I].isRegLoc() && "Can only return in registers!");
    if (RVLocs[I].getLocVT() == MVT::v2i32) {
      assert(false);
    } else {
      Chain = DAG.getCopyFromReg(Chain, dl, (RVLocs[I].getLocReg()),
                                 RVLocs[I].getValVT(), InGlue)
                  .getValue(1);
      InGlue = Chain.getValue(2);
      InVals.push_back(Chain.getValue(0));
    }
  }

  return Chain;
}
SDValue
URCLTargetLowering::LowerReturn(SDValue Chain, CallingConv::ID CallConv,
                                bool IsVarArg,
                                const SmallVectorImpl<ISD::OutputArg> &Outs,
                                const SmallVectorImpl<SDValue> &OutVals,
                                const SDLoc &DL, SelectionDAG &DAG) const {
  MachineFunction &MF = DAG.getMachineFunction();

  SmallVector<CCValAssign, 16> RVLocs;
  CCState CCInfo(CallConv, IsVarArg, DAG.getMachineFunction(), RVLocs,
                 *DAG.getContext());
  CCInfo.AnalyzeReturn(Outs, RetCC_URCL32);

  SDValue Glue;
  SmallVector<SDValue, 4> RetOps(1, Chain);
  for (unsigned i = 0, realRVLocIdx = 0; i != RVLocs.size();
       ++i, ++realRVLocIdx) {
    CCValAssign &VA = RVLocs[i];
    assert(VA.isRegLoc() && "Can only return in registers!");

    SDValue Arg = OutVals[realRVLocIdx];
    unsigned NumValues = Arg.getNode()->getNumValues();
    if (NumValues > 1 &&
        Arg.getNode()->getValueType(NumValues - 1) == MVT::Other) {
      SDValue ArgChain = Arg.getValue(NumValues - 1);
      Chain = DAG.getNode(ISD::TokenFactor, DL, MVT::Other, Chain, ArgChain);
    }

    if (VA.needsCustom()) {
      assert(false);
      // assert(VA.getLocVT() == MVT::v2i32);
      // SDValue Part0 = DAG.getNode(
      //     ISD::EXTRACT_VECTOR_ELT, DL, MVT::i32, Arg,
      //     DAG.getConstant(0, DL, getVectorIdxTy(DAG.getDataLayout())));
      // SDValue Part1 = DAG.getNode(
      //     ISD::EXTRACT_VECTOR_ELT, DL, MVT::i32, Arg,
      //     DAG.getConstant(1, DL, getVectorIdxTy(DAG.getDataLayout())));

      // Chain = DAG.getCopyToReg(Chain, DL, VA.getLocReg(), Part0, Glue);
      // Glue = Chain.getValue(1);
      // RetOps.push_back(DAG.getRegister(VA.getLocReg(), VA.getLocVT()));
      // VA = RVLocs[++i]; // skip ahead to next loc
      // Chain = DAG.getCopyToReg(Chain, DL, VA.getLocReg(), Part1, Glue);
    } else {
      Chain = DAG.getCopyToReg(Chain, DL, VA.getLocReg(), Arg, Glue);
    }

    // Guarantee that all emitted copies are stuck together with flags.
    Glue = Chain.getValue(1);
    RetOps.push_back(DAG.getRegister(VA.getLocReg(), VA.getLocVT()));
  }

  // If the function returns a struct, copy the SRetReturnReg to I0
  if (MF.getFunction().hasStructRetAttr()) {
    // URCLMachineFunctionInfo *SFI = MF.getInfo<URCLMachineFunctionInfo>();
    // Register Reg = SFI->getSRetReturnReg();
    // if (!Reg)
    //   llvm_unreachable("sret virtual register not created in the entry
    //   block");
    // auto PtrVT = getPointerTy(DAG.getDataLayout());
    // SDValue Val = DAG.getCopyFromReg(Chain, DL, Reg, PtrVT);
    // Chain = DAG.getCopyToReg(Chain, DL, SP::I0, Val, Glue);
    // Glue = Chain.getValue(1);
    // RetOps.push_back(DAG.getRegister(SP::I0, PtrVT));
    // RetAddrOffset = 12; // CallInst + Delay Slot + Unimp
    assert(false);
  }

  RetOps[0] = Chain; // Update chain.

  // Add the glue if we have it.

  SDVTList VTs;
  if (Glue.getNode()) {
    RetOps.push_back(Glue);
    VTs = DAG.getVTList(MVT::Other, MVT::Glue);
    return DAG.getNode(URCLISD::RET_GLUE, DL, VTs, RetOps);
  } else {
    VTs = DAG.getVTList(MVT::Other);
    return DAG.getNode(URCLISD::RET, DL, VTs, RetOps);
  }
}

SDValue URCLTargetLowering::LowerFormalArguments(
    SDValue Chain, CallingConv::ID CallConv, bool isVarArg,
    const SmallVectorImpl<ISD::InputArg> &Ins, const SDLoc &dl,
    SelectionDAG &DAG, SmallVectorImpl<SDValue> &InVals) const {
  MachineFunction &MF = DAG.getMachineFunction();
  MachineRegisterInfo &RegInfo = MF.getRegInfo();
  URCLMachineFunctionInfo *FuncInfo = MF.getInfo<URCLMachineFunctionInfo>();
  EVT PtrVT = getPointerTy(DAG.getDataLayout());

  // Assign locations to all of the incoming arguments.
  SmallVector<CCValAssign, 16> ArgLocs;
  CCState CCInfo(CallConv, isVarArg, DAG.getMachineFunction(), ArgLocs,
                 *DAG.getContext());
  CCInfo.AnalyzeFormalArguments(Ins, CC_URCL32);

  const unsigned StackOffset = 92;
  bool IsLittleEndian = DAG.getDataLayout().isLittleEndian();

  unsigned InIdx = 0;
  for (unsigned i = 0, e = ArgLocs.size(); i != e; ++i, ++InIdx) {
    CCValAssign &VA = ArgLocs[i];
    EVT LocVT = VA.getLocVT();

    if (Ins[InIdx].Flags.isSRet()) {
      if (InIdx != 0)
        report_fatal_error("URCL only supports sret on the first parameter");
      // Get SRet from [%fp+64].
      int FrameIdx = MF.getFrameInfo().CreateFixedObject(4, 64, true);
      SDValue FIPtr = DAG.getFrameIndex(FrameIdx, MVT::i32);
      SDValue Arg =
          DAG.getLoad(MVT::i32, dl, Chain, FIPtr, MachinePointerInfo());
      InVals.push_back(Arg);
      continue;
    }

    SDValue Arg;
    if (VA.isRegLoc()) {
      if (VA.needsCustom()) {
        assert(false);
        // assert(VA.getLocVT() == MVT::f64 || VA.getLocVT() == MVT::v2i32);

        // Register VRegHi = RegInfo.createVirtualRegister(&URCL::IntRegsRegClass);
        // MF.getRegInfo().addLiveIn(VA.getLocReg(), VRegHi);
        // SDValue HiVal = DAG.getCopyFromReg(Chain, dl, VRegHi, MVT::i32);

        // assert(i + 1 < e);
        // CCValAssign &NextVA = ArgLocs[++i];

        // SDValue LoVal;
        // if (NextVA.isMemLoc()) {
        //   int FrameIdx = MF.getFrameInfo().CreateFixedObject(
        //       4, StackOffset + NextVA.getLocMemOffset(), true);
        //   SDValue FIPtr = DAG.getFrameIndex(FrameIdx, MVT::i32);
        //   LoVal = DAG.getLoad(MVT::i32, dl, Chain, FIPtr, MachinePointerInfo());
        // } else {
        //   Register loReg =
        //       MF.addLiveIn(NextVA.getLocReg(), &URCL::IntRegsRegClass);
        //   LoVal = DAG.getCopyFromReg(Chain, dl, loReg, MVT::i32);
        // }

        // if (IsLittleEndian)
        //   std::swap(LoVal, HiVal);

        // SDValue WholeValue =
        //     DAG.getNode(ISD::BUILD_PAIR, dl, MVT::i64, LoVal, HiVal);
        // WholeValue = DAG.getNode(ISD::BITCAST, dl, VA.getLocVT(), WholeValue);
        // InVals.push_back(WholeValue);
        // continue;
      }
      Register VReg = RegInfo.createVirtualRegister(&URCL::IntRegsRegClass);
      MF.getRegInfo().addLiveIn(VA.getLocReg(), VReg);
      Arg = DAG.getCopyFromReg(Chain, dl, VReg, MVT::i32);
      if (VA.getLocInfo() != CCValAssign::Indirect) {
        if (VA.getLocVT() == MVT::f32)
          Arg = DAG.getNode(ISD::BITCAST, dl, MVT::f32, Arg);
        else if (VA.getLocVT() != MVT::i32) {
          Arg = DAG.getNode(ISD::AssertSext, dl, MVT::i32, Arg,
                            DAG.getValueType(VA.getLocVT()));
          Arg = DAG.getNode(ISD::TRUNCATE, dl, VA.getLocVT(), Arg);
        }
        InVals.push_back(Arg);
        continue;
      }
    } else {
      assert(VA.isMemLoc());

      unsigned Offset = VA.getLocMemOffset() + StackOffset;

      if (VA.needsCustom()) {
        assert(false);
        // assert(VA.getValVT() == MVT::f64 || VA.getValVT() == MVT::v2i32);
        // // If it is double-word aligned, just load.
        // if (Offset % 8 == 0) {
        //   int FI = MF.getFrameInfo().CreateFixedObject(8, Offset, true);
        //   SDValue FIPtr = DAG.getFrameIndex(FI, PtrVT);
        //   SDValue Load = DAG.getLoad(VA.getValVT(), dl, Chain, FIPtr,
        //                              MachinePointerInfo());
        //   InVals.push_back(Load);
        //   continue;
        // }

        // int FI = MF.getFrameInfo().CreateFixedObject(4, Offset, true);
        // SDValue FIPtr = DAG.getFrameIndex(FI, PtrVT);
        // SDValue HiVal =
        //     DAG.getLoad(MVT::i32, dl, Chain, FIPtr, MachinePointerInfo());
        // int FI2 = MF.getFrameInfo().CreateFixedObject(4, Offset + 4, true);
        // SDValue FIPtr2 = DAG.getFrameIndex(FI2, PtrVT);

        // SDValue LoVal =
        //     DAG.getLoad(MVT::i32, dl, Chain, FIPtr2, MachinePointerInfo());

        // if (IsLittleEndian)
        //   std::swap(LoVal, HiVal);

        // SDValue WholeValue =
        //     DAG.getNode(ISD::BUILD_PAIR, dl, MVT::i64, LoVal, HiVal);
        // WholeValue = DAG.getNode(ISD::BITCAST, dl, VA.getValVT(), WholeValue);
        // InVals.push_back(WholeValue);
        // continue;
      }

      int FI = MF.getFrameInfo().CreateFixedObject(LocVT.getSizeInBits() / 8,
                                                   Offset, true);
      SDValue FIPtr = DAG.getFrameIndex(FI, PtrVT);
      SDValue Load = DAG.getLoad(LocVT, dl, Chain, FIPtr,
                                 MachinePointerInfo::getFixedStack(MF, FI));
      if (VA.getLocInfo() != CCValAssign::Indirect) {
        InVals.push_back(Load);
        continue;
      }
      Arg = Load;
    }

    assert(VA.getLocInfo() == CCValAssign::Indirect);

    SDValue ArgValue =
        DAG.getLoad(VA.getValVT(), dl, Chain, Arg, MachinePointerInfo());
    InVals.push_back(ArgValue);

    unsigned ArgIndex = Ins[InIdx].OrigArgIndex;
    assert(Ins[InIdx].PartOffset == 0);
    while (i + 1 != e && Ins[InIdx + 1].OrigArgIndex == ArgIndex) {
      CCValAssign &PartVA = ArgLocs[i + 1];
      unsigned PartOffset = Ins[InIdx + 1].PartOffset;
      SDValue Address = DAG.getMemBasePlusOffset(
          ArgValue, TypeSize::getFixed(PartOffset), dl);
      InVals.push_back(DAG.getLoad(PartVA.getValVT(), dl, Chain, Address,
                                   MachinePointerInfo()));
      ++i;
      ++InIdx;
    }
  }

  if (MF.getFunction().hasStructRetAttr()) {
    // Copy the SRet Argument to SRetReturnReg.
    URCLMachineFunctionInfo *SFI = MF.getInfo<URCLMachineFunctionInfo>();
    Register Reg = SFI->getSRetReturnReg();
    if (!Reg) {
      Reg = MF.getRegInfo().createVirtualRegister(&URCL::IntRegsRegClass);
      SFI->setSRetReturnReg(Reg);
    }
    SDValue Copy = DAG.getCopyToReg(DAG.getEntryNode(), dl, Reg, InVals[0]);
    Chain = DAG.getNode(ISD::TokenFactor, dl, MVT::Other, Copy, Chain);
  }

  // Store remaining ArgRegs to the stack if this is a varargs function.
  if (isVarArg) {
    static const MCPhysReg ArgRegs[] = {URCL::R1, URCL::R2, URCL::R3,
                                        URCL::R4, URCL::R5, URCL::R6};
    unsigned NumAllocated = CCInfo.getFirstUnallocated(ArgRegs);
    const MCPhysReg *CurArgReg = ArgRegs + NumAllocated,
                    *ArgRegEnd = ArgRegs + 6;
    unsigned ArgOffset = CCInfo.getStackSize();
    if (NumAllocated == 6)
      ArgOffset += StackOffset;
    else {
      assert(!ArgOffset);
      ArgOffset = 68 + 4 * NumAllocated;
    }

    // Remember the vararg offset for the va_start implementation.
    FuncInfo->setVarArgsFrameOffset(ArgOffset);

    std::vector<SDValue> OutChains;

    for (; CurArgReg != ArgRegEnd; ++CurArgReg) {
      Register VReg = RegInfo.createVirtualRegister(&URCL::IntRegsRegClass);
      MF.getRegInfo().addLiveIn(*CurArgReg, VReg);
      SDValue Arg = DAG.getCopyFromReg(DAG.getRoot(), dl, VReg, MVT::i32);

      int FrameIdx = MF.getFrameInfo().CreateFixedObject(4, ArgOffset, true);
      SDValue FIPtr = DAG.getFrameIndex(FrameIdx, MVT::i32);

      assert(false);
      OutChains.push_back(
          DAG.getStore(DAG.getRoot(), dl, Arg, FIPtr, MachinePointerInfo()));
      ArgOffset += 4;
    }

    if (!OutChains.empty()) {
      OutChains.push_back(Chain);
      Chain = DAG.getNode(ISD::TokenFactor, dl, MVT::Other, OutChains);
    }
  }

  return Chain;
}

SDValue URCLTargetLowering::LowerLOAD(SDValue Op, SelectionDAG &DAG) const {
  LoadSDNode *LN = cast<LoadSDNode>(Op);
  SDLoc DL(Op);
  SDValue Ptr = LN->getBasePtr();
  EVT MemVT = LN->getMemoryVT();

  if (Ptr.getOpcode() == URCLISD::WORD_ADDR) {
    return SDValue();
  }

  if (isa<FrameIndexSDNode>(Ptr)) {
    return SDValue();
  }

  SDValue WordAddr = DAG.getNode(ISD::SRL, DL, MVT::i32, Ptr,
                                 DAG.getConstant(2, DL, MVT::i32));

  SDValue WrappedPtr = DAG.getNode(URCLISD::WORD_ADDR, DL, MVT::i32, WordAddr);

  SDValue FullWord =
      DAG.getLoad(MVT::i32, DL, LN->getChain(), WrappedPtr,
                  MachinePointerInfo(LN->getMemOperand()->getValue()));
  SDValue LoadChain = FullWord.getValue(1);

  if (MemVT == MVT::i32)
    return DAG.getMergeValues({FullWord, LoadChain}, DL);

  SDValue ByteOffset = DAG.getNode(ISD::AND, DL, MVT::i32, Ptr,
                                   DAG.getConstant(3, DL, MVT::i32));

  SDValue ShiftAmt = DAG.getNode(ISD::SHL, DL, MVT::i32, ByteOffset,
                                 DAG.getConstant(3, DL, MVT::i32));

  SDValue Shifted = DAG.getNode(ISD::SRL, DL, MVT::i32, FullWord, ShiftAmt);

  uint32_t MaskVal = (MemVT == MVT::i8) ? 0xFF : 0xFFFF;
  SDValue Extracted = DAG.getNode(ISD::AND, DL, MVT::i32, Shifted,
                                  DAG.getConstant(MaskVal, DL, MVT::i32));

  return DAG.getMergeValues({Extracted, LoadChain}, DL);
}

SDValue URCLTargetLowering::LowerSTORE(SDValue Op, SelectionDAG &DAG) const {
  StoreSDNode *SN = cast<StoreSDNode>(Op);
  SDLoc DL(Op);
  SDValue Ptr = SN->getBasePtr();
  SDValue Value = SN->getValue();
  EVT MemVT = SN->getMemoryVT();

  if (Ptr.getOpcode() == URCLISD::WORD_ADDR) {
    return SDValue();
  }

  if (isa<FrameIndexSDNode>(Ptr)) {
    return SDValue();
  }

  SDValue WordAddr = DAG.getNode(ISD::SRL, DL, MVT::i32, Ptr,
                                 DAG.getConstant(2, DL, MVT::i32));

  SDValue WrappedPtr = DAG.getNode(URCLISD::WORD_ADDR, DL, MVT::i32, WordAddr);

  if (MemVT == MVT::i32) {
    return DAG.getStore(SN->getChain(), DL, Value, WrappedPtr,
                        SN->getMemOperand());
  }

  SDValue LoadExisting =
      DAG.getLoad(MVT::i32, DL, SN->getChain(), WrappedPtr,
                  MachinePointerInfo(SN->getMemOperand()->getValue()));

  SDValue ByteOffset = DAG.getNode(ISD::AND, DL, MVT::i32, Ptr,
                                   DAG.getConstant(3, DL, MVT::i32));
  SDValue ShiftAmt = DAG.getNode(ISD::SHL, DL, MVT::i32, ByteOffset,
                                 DAG.getConstant(3, DL, MVT::i32));

  uint32_t MaskVal = (MemVT == MVT::i8) ? 0xFF : 0xFFFF;
  SDValue MaskedVal = DAG.getNode(ISD::AND, DL, MVT::i32, Value,
                                  DAG.getConstant(MaskVal, DL, MVT::i32));

  SDValue ShiftedNewVal =
      DAG.getNode(ISD::SHL, DL, MVT::i32, MaskedVal, ShiftAmt);

  SDValue ClearMask = DAG.getNode(
      ISD::SHL, DL, MVT::i32, DAG.getConstant(MaskVal, DL, MVT::i32), ShiftAmt);
  ClearMask = DAG.getNOT(DL, ClearMask, MVT::i32);

  SDValue ClearedOldVal =
      DAG.getNode(ISD::AND, DL, MVT::i32, LoadExisting, ClearMask);

  SDValue FinalWord =
      DAG.getNode(ISD::OR, DL, MVT::i32, ClearedOldVal, ShiftedNewVal);

  return DAG.getStore(LoadExisting.getValue(1), DL, FinalWord, WrappedPtr,
                      MachinePointerInfo(SN->getMemOperand()->getValue()));
}

SDValue URCLTargetLowering::LowerGLOBAL_REF(SDValue Op,
                                            SelectionDAG &DAG) const {
  SDLoc DL(Op);
  auto *GA = cast<GlobalAddressSDNode>(Op);
  EVT VT = Op.getValueType();

  SDValue TGA =
      DAG.getTargetGlobalAddress(GA->getGlobal(), DL, VT, GA->getOffset());
  return DAG.getNode(URCLISD::GLOBAL_REF, DL, VT, TGA);
}

SDValue URCLTargetLowering::LowerSELECT(SDValue Op, SelectionDAG &DAG) const {
  SDValue Cond = Op.getOperand(0);
  SDValue TrueV = Op.getOperand(1);
  SDValue FalseV = Op.getOperand(2);
  SDLoc DL(Op);
  EVT VT = Op.getValueType();

  // Because of ZeroOrNegativeOneBooleanContent, it should always be -1 or 0
  SDValue Mask = Cond;

  SDValue AllOnes = DAG.getConstant(bit_cast<uint32_t>((int32_t)-1), DL, VT);
  SDValue NotMask = DAG.getNode(ISD::XOR, DL, VT, Mask, AllOnes);

  SDValue TruePart = DAG.getNode(ISD::AND, DL, VT, TrueV, Mask);
  SDValue FalsePart = DAG.getNode(ISD::AND, DL, VT, FalseV, NotMask);

  return DAG.getNode(ISD::OR, DL, VT, TruePart, FalsePart);
}
