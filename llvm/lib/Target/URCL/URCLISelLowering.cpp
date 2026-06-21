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
#include "URCLSubtarget.h"
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
#include "llvm/Support/raw_ostream.h"
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
  case ISD::CopyToReg:
    return LowerCopyToReg(Op, DAG);
  case ISD::INTRINSIC_WO_CHAIN:
    return LowerINTRINSIC_WO_CHAIN(Op, DAG);
  default:
    return SDValue();
  }
}

SDValue URCLTargetLowering::LowerCopyToReg(SDValue Op,
                                           SelectionDAG &DAG) const {
  SDValue Src = Op.getOperand(2);

  if (auto *FI = dyn_cast<FrameIndexSDNode>(Src.getNode())) {
    SDValue Chain = Op.getOperand(0);
    SDLoc DL(Op);
    RegisterSDNode *R = cast<RegisterSDNode>(Op.getOperand(1));
    Register Reg = R->getReg();
    EVT VT = Src.getValueType();

    SDValue TFI = DAG.getTargetFrameIndex(FI->getIndex(), VT);
    SDValue ZeroImm = DAG.getTargetConstant(0, DL, VT);
    SDValue MaterializedFI(
        DAG.getMachineNode(URCL::ADDri, DL, VT, TFI, ZeroImm), 0);

    if (Op.getNode()->getNumValues() == 1) {
      return DAG.getCopyToReg(Chain, DL, Reg, MaterializedFI);
    }

    SDValue Glue = Op.getNumOperands() == 4 ? Op.getOperand(3) : SDValue();
    return DAG.getCopyToReg(Chain, DL, Reg, MaterializedFI, Glue);
  }

  return SDValue();
}

URCLTargetLowering::URCLTargetLowering(const TargetMachine &TM,
                                       const URCLSubtarget &STI)
    : TargetLowering(TM, STI), Subtarget(&STI) {
  // MVT PtrVT = MVT::getIntegerVT(TM.getPointerSizeInBits(0));
  setBooleanVectorContents(ZeroOrNegativeOneBooleanContent);
  setBooleanContents(ZeroOrNegativeOneBooleanContent);

  uint32_t WordSize;
  MVT WordType = Subtarget->getWordType();
  MVT ExtWordType;
  addRegisterClass(WordType, &URCL::IntRegsRegClass);
  switch (Subtarget->getWordSize()) {
  case llvm::URCLSubtarget::WordSize::Word32:
    ExtWordType = MVT::v2i32;
    WordSize = 32;
    break;
  case llvm::URCLSubtarget::WordSize::Word16:
    ExtWordType = MVT::v2i16;
    WordSize = 16;
    setOperationAction(ISD::LOAD, MVT::i32, Expand);
    setOperationAction(ISD::STORE, MVT::i32, Expand);
    break;
  case llvm::URCLSubtarget::WordSize::Word8:
    ExtWordType = MVT::v2i8;
    WordSize = 8;
    setOperationAction(ISD::LOAD, MVT::i16, Expand);
    setOperationAction(ISD::STORE, MVT::i16, Expand);
    setOperationAction(ISD::LOAD, MVT::i32, Expand);
    setOperationAction(ISD::STORE, MVT::i32, Expand);
    break;
  }
  llvm::errs() << "Compiling for WordSize:" << WordSize << "\n";

  setOperationAction(ISD::GlobalAddress, WordType, Custom);

  for (unsigned Op = 0; Op < ISD::BUILTIN_OP_END; ++Op) {
    setOperationAction(Op, ExtWordType, Expand);
  }
  for (MVT VT : MVT::integer_fixedlen_vector_valuetypes()) {
    setLoadExtAction(ISD::SEXTLOAD, VT, ExtWordType, Expand);
    setLoadExtAction(ISD::ZEXTLOAD, VT, ExtWordType, Expand);
    setLoadExtAction(ISD::EXTLOAD, VT, ExtWordType, Expand);

    setLoadExtAction(ISD::SEXTLOAD, ExtWordType, VT, Expand);
    setLoadExtAction(ISD::ZEXTLOAD, ExtWordType, VT, Expand);
    setLoadExtAction(ISD::EXTLOAD, ExtWordType, VT, Expand);

    setTruncStoreAction(VT, ExtWordType, Expand);
    setTruncStoreAction(ExtWordType, VT, Expand);
  }
  setOperationAction(ISD::LOAD, ExtWordType, Expand);
  setOperationAction(ISD::STORE, ExtWordType, Expand);
  setOperationAction(ISD::EXTRACT_VECTOR_ELT, MVT::v2i32, Expand);
  setOperationAction(ISD::BUILD_VECTOR, MVT::v2i32, Expand);

  setOperationAction(ISD::LOAD, MVT::i64, Expand);
  setOperationAction(ISD::STORE, MVT::i64, Expand);
  if (WordSize < 32) {
    setOperationAction(ISD::LOAD, MVT::i32, Expand);
    setOperationAction(ISD::STORE, MVT::i32, Expand);
  }
  if (WordSize < 16) {
    setOperationAction(ISD::LOAD, MVT::i16, Expand);
    setOperationAction(ISD::STORE, MVT::i16, Expand);
  }

  setOperationAction(ISD::LOAD, WordType, Custom);
  setOperationAction(ISD::STORE, WordType, Custom);
  if (WordSize > 8) {
    setLoadExtAction(ISD::EXTLOAD, WordType, MVT::i8, Custom);
    setLoadExtAction(ISD::ZEXTLOAD, WordType, MVT::i8, Expand);
    setLoadExtAction(ISD::SEXTLOAD, WordType, MVT::i8, Expand);
  }
  if (WordSize > 16) {
    setLoadExtAction(ISD::EXTLOAD, WordType, MVT::i16, Custom);
    setLoadExtAction(ISD::ZEXTLOAD, WordType, MVT::i16, Expand);
    setLoadExtAction(ISD::SEXTLOAD, WordType, MVT::i16, Expand);
  }

  if (WordSize > 8) {
    setOperationAction(ISD::STORE, MVT::i8, Expand);
    setTruncStoreAction(MVT::i32, MVT::i8, Custom);
  }
  if (WordSize > 16) {
    setOperationAction(ISD::STORE, MVT::i16, Expand);
    setTruncStoreAction(MVT::i32, MVT::i16, Custom);
  }

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
  setOperationAction(ISD::SIGN_EXTEND_INREG, MVT::i32, Expand);
  setOperationAction(ISD::SIGN_EXTEND_INREG, MVT::i16, Expand);
  setOperationAction(ISD::SIGN_EXTEND_INREG, MVT::i8, Expand);
  setOperationAction(ISD::SIGN_EXTEND_INREG, MVT::i1, Expand);

  // setOperationAction(ISD::BITCAST, MVT::f32, Expand);
  // setOperationAction(ISD::BITCAST, MVT::i32, Expand);

  // URCL has no select: expand to SELECT_CC.
  setOperationAction(ISD::SELECT, WordType, Custom);
  setOperationAction(ISD::SETCC, WordType, Legal);

  // URCL doesn't have BRCOND either, it has BR_CC.
  setOperationAction(ISD::BRCOND, MVT::Other, Expand);
  setOperationAction(ISD::BRIND, MVT::Other, Expand);

  setOperationAction(ISD::CopyToReg, MVT::Other, Custom);
  setOperationAction(ISD::SELECT_CC, WordType, Expand);

  // setOperationAction(ISD::ADDC, MVT::i32, Legal);
  // setOperationAction(ISD::ADDE, MVT::i32, Legal);
  // setOperationAction(ISD::SUBC, MVT::i32, Legal);
  // setOperationAction(ISD::SUBE, MVT::i32, Legal);

  setMaxAtomicSizeInBitsSupported(0);
  setMinCmpXchgSizeInBits(WordSize);

  // setOperationAction(ISD::ATOMIC_SWAP, MVT::i32, Legal);
  // setOperationAction(ISD::ATOMIC_FENCE, MVT::Other, Legal);
  // setOperationAction(ISD::ATOMIC_LOAD, MVT::i32, Custom);
  // setOperationAction(ISD::ATOMIC_STORE, MVT::i32, Custom);
  // setOperationAction(ISD::FNEG, MVT::f64, Custom);
  // setOperationAction(ISD::FABS, MVT::f64, Custom);

  // setOperationAction(ISD::FSIN, MVT::f128, Expand);
  // setOperationAction(ISD::FCOS, MVT::f128, Expand);
  // setOperationAction(ISD::FSINCOS, MVT::f128, Expand);
  // setOperationAction(ISD::FREM, MVT::f128, LibCall);
  // setOperationAction(ISD::FMA, MVT::f128, Expand);
  // setOperationAction(ISD::FSIN, MVT::f64, Expand);
  // setOperationAction(ISD::FCOS, MVT::f64, Expand);
  // setOperationAction(ISD::FSINCOS, MVT::f64, Expand);
  // setOperationAction(ISD::FREM, MVT::f64, LibCall);
  // setOperationAction(ISD::FMA, MVT::f64, Expand);
  // setOperationAction(ISD::FSIN, MVT::f32, Expand);
  // setOperationAction(ISD::FCOS, MVT::f32, Expand);
  // setOperationAction(ISD::FSINCOS, MVT::f32, Expand);
  // setOperationAction(ISD::FREM, MVT::f32, LibCall);
  // setOperationAction(ISD::FMA, MVT::f32, Expand);
  // setOperationAction(ISD::ROTL, MVT::i32, Expand);
  // setOperationAction(ISD::ROTR, MVT::i32, Expand);
  // setOperationAction(ISD::BSWAP, MVT::i32, Expand);
  // setOperationAction(ISD::FCOPYSIGN, MVT::f128, Expand);
  // setOperationAction(ISD::FCOPYSIGN, MVT::f64, Expand);
  // setOperationAction(ISD::FCOPYSIGN, MVT::f32, Expand);
  // setOperationAction(ISD::FPOW, MVT::f128, Expand);
  // setOperationAction(ISD::FPOW, MVT::f64, Expand);
  // setOperationAction(ISD::FPOW, MVT::f32, Expand);

  setOperationAction(ISD::SHL_PARTS, WordType, Expand);
  setOperationAction(ISD::SRA_PARTS, WordType, Expand);
  setOperationAction(ISD::SRL_PARTS, WordType, Expand);

  // Expands to [SU]MUL_LOHI.
  setOperationAction(ISD::MULHU, WordType, Legal);
  setOperationAction(ISD::MULHS, WordType, Legal);
  setOperationAction(ISD::UMUL_LOHI, WordType, Expand);
  setOperationAction(ISD::SMUL_LOHI, WordType, Expand);
  setOperationAction(ISD::MUL, WordType, Legal);

  // VASTART needs to be custom lowered to use the VarArgsFrameIndex.
  // setOperationAction(ISD::VASTART, MVT::Other, Custom);
  // VAARG needs to be lowered to not do unaligned accesses for doubles.
  // setOperationAction(ISD::VAARG, MVT::Other, Custom);

  setOperationAction(ISD::TRAP, MVT::Other, Legal);
  setOperationAction(ISD::DEBUGTRAP, MVT::Other, Legal);

  setOperationAction(ISD::VACOPY, MVT::Other, Expand);
  setOperationAction(ISD::VAEND, MVT::Other, Expand);
  setOperationAction(ISD::STACKSAVE, MVT::Other, Expand);
  setOperationAction(ISD::STACKRESTORE, MVT::Other, Expand);

  setStackPointerRegisterToSaveRestore(URCL::SP);

  setOperationAction(ISD::CTPOP, WordType, Expand);

  // setOperationAction(ISD::LOAD, MVT::f128, Expand);
  // setOperationAction(ISD::STORE, MVT::f128, Expand);
  // setOperationAction(ISD::FADD, MVT::f128, Expand);
  // setOperationAction(ISD::FSUB, MVT::f128, Expand);
  // setOperationAction(ISD::FMUL, MVT::f128, Expand);
  // setOperationAction(ISD::FDIV, MVT::f128, Expand);
  // setOperationAction(ISD::FSQRT, MVT::f128, Expand);
  // setOperationAction(ISD::FNEG, MVT::f128, Expand);
  // setOperationAction(ISD::FABS, MVT::f128, Expand);

  // setOperationAction(ISD::FP_EXTEND, MVT::f128, Custom);
  // setOperationAction(ISD::FP_ROUND, MVT::f64, Custom);
  // setOperationAction(ISD::FP_ROUND, MVT::f32, Custom);

  setOperationAction(ISD::CTLZ, MVT::i8, Expand);
  setOperationAction(ISD::CTLZ, MVT::i16, Expand);
  setOperationAction(ISD::CTLZ, MVT::i32, Expand);
  setOperationAction(ISD::CTLZ, MVT::i64, Expand);
  setOperationAction(ISD::CTLZ_ZERO_POISON, MVT::i8, Expand);
  setOperationAction(ISD::CTLZ_ZERO_POISON, MVT::i16, Expand);
  setOperationAction(ISD::CTLZ_ZERO_POISON, MVT::i32, Expand);
  setOperationAction(ISD::CTLZ_ZERO_POISON, MVT::i64, Expand);

  setOperationAction(ISD::CTTZ, MVT::i8, Expand);
  setOperationAction(ISD::CTTZ, MVT::i16, Expand);
  setOperationAction(ISD::CTTZ, MVT::i32, Expand);
  setOperationAction(ISD::CTTZ, MVT::i64, Expand);
  setOperationAction(ISD::CTTZ_ZERO_POISON, MVT::i8, Expand);
  setOperationAction(ISD::CTTZ_ZERO_POISON, MVT::i16, Expand);
  setOperationAction(ISD::CTTZ_ZERO_POISON, MVT::i32, Expand);
  setOperationAction(ISD::CTTZ_ZERO_POISON, MVT::i64, Expand);

  setJumpIsExpensive(false);
  PredictableSelectIsExpensive = !isJumpExpensive();

  assert(WordSize % 4 == 0);
  setMinFunctionAlignment(Align(WordSize / 4));

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

SDValue URCLTargetLowering::LowerINTRINSIC_WO_CHAIN(SDValue Op,
                                                    SelectionDAG &DAG) const {
  unsigned IntNo = Op.getConstantOperandVal(0);
  switch (IntNo) {
  default:
    return SDValue();
  case Intrinsic::thread_pointer: {
    // TODO: not sure
    assert(false);
  }
  }
}

bool URCLTargetLowering::CanLowerReturn(
    CallingConv::ID CallConv, MachineFunction &MF, bool isVarArg,
    const SmallVectorImpl<ISD::OutputArg> &Outs, LLVMContext &Context,
    const Type *RetTy) const {
  SmallVector<CCValAssign, 16> RVLocs;
  CCState CCInfo(CallConv, isVarArg, MF, RVLocs, Context);
  switch (MF.getSubtarget<URCLSubtarget>().getWordSize()) {
  case URCLSubtarget::WordSize::Word32:
    return CCInfo.CheckReturn(Outs, RetCC_URCL32);
  case URCLSubtarget::WordSize::Word16:
    return CCInfo.CheckReturn(Outs, RetCC_URCL16);
  case URCLSubtarget::WordSize::Word8:
    return CCInfo.CheckReturn(Outs, RetCC_URCL8);
  }
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
  bool &IsTailCall = CLI.IsTailCall;
  CallingConv::ID CallConv = CLI.CallConv;
  bool IsVarArg = CLI.IsVarArg;
  MachineFunction &MF = DAG.getMachineFunction();
  // LLVMContext &Ctx = *DAG.getContext();
  EVT PtrVT = getPointerTy(MF.getDataLayout());

  // Analyze operands of the call, assigning locations to each operand.
  SmallVector<CCValAssign, 16> ArgLocs;
  CCState CCInfo(CallConv, IsVarArg, DAG.getMachineFunction(), ArgLocs,
                 *DAG.getContext());

  MVT WordType = Subtarget->getWordType();
  switch (Subtarget->getWordSize()) {
  case llvm::URCLSubtarget::WordSize::Word32:
    CCInfo.AnalyzeCallOperands(Outs, CC_URCL32);
    break;
  case llvm::URCLSubtarget::WordSize::Word16:
    CCInfo.AnalyzeCallOperands(Outs, CC_URCL16);
    break;
  case llvm::URCLSubtarget::WordSize::Word8:
    CCInfo.AnalyzeCallOperands(Outs, CC_URCL8);
    break;
  }

  IsTailCall = IsTailCall && false; // IsEligibleForTailCallOptimization(CCInfo,
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
      SDValue SizeNode = DAG.getConstant(Size, dl, WordType);

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

  assert(!IsTailCall || ArgsSize == 0);

  if (!IsTailCall)
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

      if (IsTailCall)
        continue;

      assert(false);
      // store SRet argument in %sp+64
      // SDValue StackPtr = DAG.getRegister(URCL::SP, WordType);
      // SDValue PtrOff = DAG.getIntPtrConstant(64, dl);
      // PtrOff = DAG.getNode(ISD::ADD, dl, MVT::i32, StackPtr, PtrOff);
      // assert(false);
      // MemOpChains.push_back(
      //     DAG.getStore(Chain, dl, Arg, PtrOff, MachinePointerInfo()));
      // hasStructRetAttr = true;
      // // sret only allowed on first argument
      // assert(Outs[realArgIdx].OrigArgIndex == 0);
      // SRetArgSize =
      //     DAG.getDataLayout().getTypeAllocSize(CLI.getArgs()[0].IndirectType);
      // continue;
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
      Arg = DAG.getNode(ISD::BITCAST, dl, WordType, Arg);
      RegsToPass.push_back(std::make_pair(VA.getLocReg(), Arg));
      continue;
    }

    assert(VA.isMemLoc());

    SDValue StackPtr = DAG.getRegister(URCL::SP, WordType);
    SDValue PtrOff =
        DAG.getIntPtrConstant(VA.getLocMemOffset() + StackOffset, dl);
    PtrOff = DAG.getNode(ISD::ADD, dl, WordType, StackPtr, PtrOff);
    MemOpChains.push_back(
        DAG.getStore(Chain, dl, Arg, PtrOff, MachinePointerInfo()));
  }

  if (!MemOpChains.empty())
    Chain = DAG.getNode(ISD::TokenFactor, dl, MVT::Other, MemOpChains);

  SDValue InGlue;
  for (const auto &[OrigReg, N] : RegsToPass) {
    Register Reg = IsTailCall ? OrigReg : (OrigReg);
    Chain = DAG.getCopyToReg(Chain, dl, Reg, N, InGlue);
    InGlue = Chain.getValue(1);
  }

  bool hasReturnsTwice = hasReturnsTwiceAttr(DAG, Callee, CLI.CB);
  assert(!hasReturnsTwice);

  if (GlobalAddressSDNode *G = dyn_cast<GlobalAddressSDNode>(Callee))
    Callee = DAG.getTargetGlobalAddress(G->getGlobal(), dl, WordType, 0);
  else if (ExternalSymbolSDNode *E = dyn_cast<ExternalSymbolSDNode>(Callee))
    Callee = DAG.getTargetExternalSymbol(E->getSymbol(), WordType);

  SDVTList NodeTys = DAG.getVTList(MVT::Other, MVT::Glue);
  SmallVector<SDValue, 8> Ops;
  Ops.push_back(Chain);
  Ops.push_back(Callee);
  if (hasStructRetAttr)
    Ops.push_back(DAG.getTargetConstant(SRetArgSize, dl, WordType));
  for (const auto &[OrigReg, N] : RegsToPass) {
    Register Reg = IsTailCall ? OrigReg : (OrigReg);
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

  if (IsTailCall) {
    assert(false);
  }

  Chain = DAG.getNode(URCLISD::CALL, dl, NodeTys, Ops);
  InGlue = Chain.getValue(1);

  Chain = DAG.getCALLSEQ_END(Chain, ArgsSize, 0, InGlue, dl);
  InGlue = Chain.getValue(1);

  SmallVector<CCValAssign, 16> RVLocs;
  CCState RVInfo(CallConv, IsVarArg, DAG.getMachineFunction(), RVLocs,
                 *DAG.getContext());

  switch (Subtarget->getWordSize()) {
  case llvm::URCLSubtarget::WordSize::Word32:
    RVInfo.AnalyzeCallResult(Ins, RetCC_URCL32);
    break;
  case llvm::URCLSubtarget::WordSize::Word16:
    RVInfo.AnalyzeCallResult(Ins, RetCC_URCL16);
    break;
  case llvm::URCLSubtarget::WordSize::Word8:
    RVInfo.AnalyzeCallResult(Ins, RetCC_URCL8);
    break;
  }

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

  switch (Subtarget->getWordSize()) {
  case llvm::URCLSubtarget::WordSize::Word32:
    CCInfo.AnalyzeReturn(Outs, RetCC_URCL32);
    break;
  case llvm::URCLSubtarget::WordSize::Word16:
    CCInfo.AnalyzeReturn(Outs, RetCC_URCL16);
    break;
  case llvm::URCLSubtarget::WordSize::Word8:
    CCInfo.AnalyzeReturn(Outs, RetCC_URCL8);
    break;
  }

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

  MVT WordType = Subtarget->getWordType();
  auto WordSize = Subtarget->getWordSizeBytes();

  // Assign locations to all of the incoming arguments.
  SmallVector<CCValAssign, 16> ArgLocs;
  CCState CCInfo(CallConv, isVarArg, DAG.getMachineFunction(), ArgLocs,
                 *DAG.getContext());
  MVT FloatWordType;
  switch (Subtarget->getWordSize()) {
  case llvm::URCLSubtarget::WordSize::Word32:
    CCInfo.AnalyzeFormalArguments(Ins, CC_URCL32);
    FloatWordType = MVT::f32;
    break;
  case llvm::URCLSubtarget::WordSize::Word16:
    CCInfo.AnalyzeFormalArguments(Ins, CC_URCL16);
    FloatWordType = MVT::f16;
    break;
  case llvm::URCLSubtarget::WordSize::Word8:
    CCInfo.AnalyzeFormalArguments(Ins, CC_URCL8);
    // kinda illegal
    FloatWordType = MVT::f16;
    break;
  }

  const unsigned StackOffset = 92;
  // bool IsLittleEndian = DAG.getDataLayout().isLittleEndian();

  unsigned InIdx = 0;
  for (unsigned i = 0, e = ArgLocs.size(); i != e; ++i, ++InIdx) {
    CCValAssign &VA = ArgLocs[i];
    EVT LocVT = VA.getLocVT();

    if (Ins[InIdx].Flags.isSRet()) {
      if (InIdx != 0)
        report_fatal_error("URCL only supports sret on the first parameter");
      // Get SRet from [%fp+64].
      int FrameIdx = MF.getFrameInfo().CreateFixedObject(WordSize, 64, true);
      SDValue FIPtr = DAG.getFrameIndex(FrameIdx, WordType);
      SDValue Arg =
          DAG.getLoad(WordType, dl, Chain, FIPtr, MachinePointerInfo());
      InVals.push_back(Arg);
      continue;
    }

    SDValue Arg;
    if (VA.isRegLoc()) {
      if (VA.needsCustom()) {
        assert(false);
        // assert(VA.getLocVT() == MVT::f64 || VA.getLocVT() == MVT::v2i32);

        // Register VRegHi =
        // RegInfo.createVirtualRegister(&URCL::IntRegsRegClass);
        // MF.getRegInfo().addLiveIn(VA.getLocReg(), VRegHi);
        // SDValue HiVal = DAG.getCopyFromReg(Chain, dl, VRegHi, MVT::i32);

        // assert(i + 1 < e);
        // CCValAssign &NextVA = ArgLocs[++i];

        // SDValue LoVal;
        // if (NextVA.isMemLoc()) {
        //   int FrameIdx = MF.getFrameInfo().CreateFixedObject(
        //       4, StackOffset + NextVA.getLocMemOffset(), true);
        //   SDValue FIPtr = DAG.getFrameIndex(FrameIdx, MVT::i32);
        //   LoVal = DAG.getLoad(MVT::i32, dl, Chain, FIPtr,
        //   MachinePointerInfo());
        // } else {
        //   Register loReg =
        //       MF.addLiveIn(NextVA.getLocReg(), &URCL::IntRegsRegClass);
        //   LoVal = DAG.getCopyFromReg(Chain, dl, loReg, MVT::i32);
        // }

        // if (IsLittleEndian)
        //   std::swap(LoVal, HiVal);

        // SDValue WholeValue =
        //     DAG.getNode(ISD::BUILD_PAIR, dl, MVT::i64, LoVal, HiVal);
        // WholeValue = DAG.getNode(ISD::BITCAST, dl, VA.getLocVT(),
        // WholeValue); InVals.push_back(WholeValue); continue;
      }
      Register VReg = RegInfo.createVirtualRegister(&URCL::IntRegsRegClass);
      MF.getRegInfo().addLiveIn(VA.getLocReg(), VReg);
      Arg = DAG.getCopyFromReg(Chain, dl, VReg, WordType);
      if (VA.getLocInfo() != CCValAssign::Indirect) {
        if (VA.getLocVT() == FloatWordType)
          Arg = DAG.getNode(ISD::BITCAST, dl, FloatWordType, Arg);
        else if (VA.getLocVT() != MVT::i32) {
          Arg = DAG.getNode(ISD::AssertSext, dl, WordType, Arg,
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
        // WholeValue = DAG.getNode(ISD::BITCAST, dl, VA.getValVT(),
        // WholeValue); InVals.push_back(WholeValue); continue;
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
      SDValue Arg = DAG.getCopyFromReg(DAG.getRoot(), dl, VReg, WordType);

      int FrameIdx =
          MF.getFrameInfo().CreateFixedObject(WordSize, ArgOffset, true);
      SDValue FIPtr = DAG.getFrameIndex(FrameIdx, WordType);

      assert(false);
      OutChains.push_back(
          DAG.getStore(DAG.getRoot(), dl, Arg, FIPtr, MachinePointerInfo()));
      ArgOffset += WordSize;
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

  MVT WordType = Subtarget->getWordType();
  uint32_t PtrMask = 0;
  switch (Subtarget->getWordSize()) {
  case llvm::URCLSubtarget::WordSize::Word32:
    PtrMask = 3;
    break;
  case llvm::URCLSubtarget::WordSize::Word16:
    PtrMask = 1;
    break;
  case llvm::URCLSubtarget::WordSize::Word8:
    PtrMask = 0;
    break;
  }

  if (Ptr.getOpcode() == URCLISD::WORD_ADDR) {
    return SDValue();
  }

  if (isa<FrameIndexSDNode>(Ptr)) {
    return SDValue();
  }

  SDValue WordAddr = DAG.getNode(URCLISD::TO_WORD_ADDR, DL, WordType, Ptr);

  SDValue WrappedPtr = DAG.getNode(URCLISD::WORD_ADDR, DL, WordType, WordAddr);

  SDValue FullWord =
      DAG.getLoad(WordType, DL, LN->getChain(), WrappedPtr,
                  MachinePointerInfo(LN->getMemOperand()->getValue()));
  SDValue LoadChain = FullWord.getValue(1);

  if (MemVT == WordType)
    return DAG.getMergeValues({FullWord, LoadChain}, DL);

  SDValue ByteOffset = DAG.getNode(ISD::AND, DL, WordType, Ptr,
                                   DAG.getConstant(PtrMask, DL, WordType));

  SDValue ShiftAmt = DAG.getNode(ISD::SHL, DL, WordType, ByteOffset,
                                 DAG.getConstant(PtrMask, DL, WordType));

  SDValue Shifted = DAG.getNode(ISD::SRL, DL, WordType, FullWord, ShiftAmt);

  uint32_t MaskVal = (MemVT == MVT::i8) ? 0xFF : 0xFFFF;
  SDValue Extracted = DAG.getNode(ISD::AND, DL, WordType, Shifted,
                                  DAG.getConstant(MaskVal, DL, WordType));

  return DAG.getMergeValues({Extracted, LoadChain}, DL);
}

SDValue URCLTargetLowering::LowerSTORE(SDValue Op, SelectionDAG &DAG) const {
  StoreSDNode *SN = cast<StoreSDNode>(Op);
  SDLoc DL(Op);
  SDValue Ptr = SN->getBasePtr();
  SDValue Value = SN->getValue();
  EVT MemVT = SN->getMemoryVT();

  MVT WordType = Subtarget->getWordType();
  uint32_t PtrMask = 0;
  switch (Subtarget->getWordSize()) {
  case llvm::URCLSubtarget::WordSize::Word32:
    PtrMask = 3;
    break;
  case llvm::URCLSubtarget::WordSize::Word16:
    PtrMask = 1;
    break;
  case llvm::URCLSubtarget::WordSize::Word8:
    PtrMask = 0;
    break;
  }

  if (Ptr.getOpcode() == URCLISD::WORD_ADDR) {
    return SDValue();
  }

  if (isa<FrameIndexSDNode>(Ptr)) {
    return SDValue();
  }

  SDValue WordAddr = DAG.getNode(URCLISD::TO_WORD_ADDR, DL, WordType, Ptr);

  SDValue WrappedPtr = DAG.getNode(URCLISD::WORD_ADDR, DL, WordType, WordAddr);

  if (MemVT == WordType) {
    return DAG.getStore(SN->getChain(), DL, Value, WrappedPtr,
                        SN->getMemOperand());
  }

  SDValue LoadExisting =
      DAG.getLoad(WordType, DL, SN->getChain(), WrappedPtr,
                  MachinePointerInfo(SN->getMemOperand()->getValue()));

  SDValue ByteOffset = DAG.getNode(ISD::AND, DL, WordType, Ptr,
                                   DAG.getConstant(PtrMask, DL, WordType));
  SDValue ShiftAmt = DAG.getNode(ISD::SHL, DL, WordType, ByteOffset,
                                 DAG.getConstant(PtrMask, DL, WordType));

  uint32_t MaskVal = (MemVT == MVT::i8) ? 0xFF : 0xFFFF;
  SDValue MaskedVal = DAG.getNode(ISD::AND, DL, WordType, Value,
                                  DAG.getConstant(MaskVal, DL, WordType));

  SDValue ShiftedNewVal =
      DAG.getNode(ISD::SHL, DL, WordType, MaskedVal, ShiftAmt);

  SDValue ClearMask = DAG.getNode(
      ISD::SHL, DL, WordType, DAG.getConstant(MaskVal, DL, WordType), ShiftAmt);
  ClearMask = DAG.getNOT(DL, ClearMask, WordType);

  SDValue ClearedOldVal =
      DAG.getNode(ISD::AND, DL, WordType, LoadExisting, ClearMask);

  SDValue FinalWord =
      DAG.getNode(ISD::OR, DL, WordType, ClearedOldVal, ShiftedNewVal);

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
  SDValue AllOnes;

  switch (Subtarget->getWordSize()) {
  case llvm::URCLSubtarget::WordSize::Word32:
    AllOnes = DAG.getConstant(bit_cast<uint32_t>((int32_t)-1), DL, VT);
    break;
  case llvm::URCLSubtarget::WordSize::Word16:
    AllOnes = DAG.getConstant(bit_cast<uint16_t>((int16_t)-1), DL, VT);
    break;
  case llvm::URCLSubtarget::WordSize::Word8:
    AllOnes = DAG.getConstant(bit_cast<uint8_t>((int8_t)-1), DL, VT);
    break;
  }

  SDValue NotMask = DAG.getNode(ISD::XOR, DL, VT, Mask, AllOnes);

  SDValue TruePart = DAG.getNode(ISD::AND, DL, VT, TrueV, Mask);
  SDValue FalsePart = DAG.getNode(ISD::AND, DL, VT, FalseV, NotMask);

  return DAG.getNode(ISD::OR, DL, VT, TruePart, FalsePart);
}

SDValue URCLTargetLowering::PerformDAGCombine(SDNode *N,
                                              DAGCombinerInfo &DCI) const {
  switch (N->getOpcode()) {
  default:
    break;
  case URCLISD::TO_WORD_ADDR:
    return DagCombineToWordAddrSimplifier(N, DCI);
  }
  return SDValue();
}

SDValue
URCLTargetLowering::DagCombineToWordAddrSimplifier(SDNode *N,
                                                   DAGCombinerInfo &DCI) const {
  SelectionDAG &DAG = DCI.DAG;
  uint32_t PtrMask;
  uint32_t PtrShiftAmount;
  switch (Subtarget->getWordSize()) {
  case llvm::URCLSubtarget::WordSize::Word32:
    PtrMask = 4;
    PtrShiftAmount = 2;
    break;
  case llvm::URCLSubtarget::WordSize::Word16:
    PtrMask = 2;
    PtrShiftAmount = 1;
    break;
  case llvm::URCLSubtarget::WordSize::Word8:
    return SDValue();
  }

  SDValue Op = N->getOperand(0);
  if (Op.getOpcode() == ISD::ADD) {
    SDValue Arg0 = Op.getOperand(0);
    SDValue Arg1 = Op.getOperand(1);
    if (auto *C = dyn_cast<ConstantSDNode>(Arg1)) {
      int64_t Imm = C->getSExtValue();
      // could do it aswell but mostlikely doesnt make sense since we then have
      // mostlikely a unaligned load and want to resuse the lower bits
      if (Imm % PtrMask == 0) {
        SDLoc DL(N);
        EVT VT = N->getValueType(0);
        SDValue NewToWordAddr =
            DAG.getNode(URCLISD::TO_WORD_ADDR, DL, VT, Arg0);
        int64_t NewImm = Imm / PtrMask;
        SDValue NewConst = DAG.getSignedConstant(NewImm, DL, VT);
        return DAG.getNode(ISD::ADD, DL, VT, NewToWordAddr, NewConst);
      }
    } else if (Op->hasOneUse()) {
      // TODO: can only do this if we show that the previous values are 4byte
      // aligned

      // SDLoc DL(N); EVT VT = N->getValueType(0); SDValue
      // NewToWordAddr1 = DAG.getNode(URCLISD::TO_WORD_ADDR, DL, VT, Arg0);
      // SDValue NewToWordAddr2 = DAG.getNode(URCLISD::TO_WORD_ADDR, DL, VT,
      // Arg1); return DAG.getNode(ISD::ADD, DL, VT, NewToWordAddr1,
      // NewToWordAddr2);
    }
  } else if (Op.getOpcode() == ISD::SHL) {
    auto *ShiftConstant = dyn_cast<ConstantSDNode>(Op->getOperand(1));
    if (ShiftConstant && ShiftConstant->getZExtValue() == PtrShiftAmount) {
      SDValue Arg0 = Op.getOperand(0);
      return Arg0;
    }
  }

  return SDValue();
}
