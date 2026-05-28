//===-- URCLAsmPrinter.cpp - URCL LLVM assembly writer ------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains a printer that converts from our internal representation
// of machine-dependent LLVM code to GAS-format URCL assembly language.
//
//===----------------------------------------------------------------------===//

#include "MCTargetDesc/URCLMCTargetDesc.h"
#include "MCTargetDesc/URCLTargetStreamer.h"
#include "TargetInfo/URCLTargetInfo.h"
#include "URCL.h"
#include "llvm/CodeGen/AsmPrinter.h"
#include "llvm/CodeGen/AsmPrinterHandler.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/MachineModuleInfoImpls.h"
#include "llvm/CodeGen/TargetLoweringObjectFileImpl.h"
#include "llvm/IR/Mangler.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/MCSymbol.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/raw_ostream.h"
using namespace llvm;

#define DEBUG_TYPE "asm-printer"

namespace {
class URCLAsmPrinter : public AsmPrinter {
private:
  URCLTargetStreamer &getTargetStreamer() {
    return static_cast<URCLTargetStreamer &>(*OutStreamer->getTargetStreamer());
  }

public:
  explicit URCLAsmPrinter(TargetMachine &TM,
                          std::unique_ptr<MCStreamer> Streamer)
      : AsmPrinter(TM, std::move(Streamer), ID) {}

  StringRef getPassName() const override { return "URCL Assembly Printer"; }

  void emitInstruction(const MachineInstr *MI) override;
  void emitBasicBlockStart(const MachineBasicBlock &MBB) override;
  void emitFunctionBodyStart() override;
  void emitFunctionHeader() override;
  bool doFinalization(llvm::Module &M) override;
  bool doInitialization(llvm::Module &M) override;
  void emitGlobalVariable(const GlobalVariable *GV) override;
  void emitAlignment(Align Alignment, const GlobalObject *GV = nullptr,
                     unsigned MaxBytesToEmit = 0);

private:
  void lowerToMCInst(const MachineInstr *MI, MCInst &OutMI);
  MCOperand lowerOperand(const MachineOperand &MO) const;
  std::map<const GlobalVariable *, uint64_t> GlobalOffsets;

public:
  static char ID;
};
} // namespace

struct URCLWord {
  uint32_t Data = 0;
  const Constant *Reloc = nullptr;
};

static void writeBytes(std::vector<uint8_t> &Buffer, unsigned Offset,
                       const uint8_t *Data, unsigned Size) {
  if (Offset + Size > Buffer.size()) {
    Buffer.resize(Offset + Size, 0);
  }
  std::memcpy(&Buffer[Offset], Data, Size);
}

static void flattenConstant(const Constant *CV, const DataLayout &DL,
                            unsigned Offset, std::vector<uint8_t> &Buffer,
                            std::map<unsigned, const Constant *> &Relocs) {

  if (CV->isNullValue())
    return;
  if (auto *CI = dyn_cast<ConstantInt>(CV)) {
    uint64_t Val = CI->getZExtValue();
    unsigned BitSize = CI->getType()->getIntegerBitWidth();
    unsigned ByteSize = (BitSize + 7) / 8;

    // Create a byte array of the integer value
    std::vector<uint8_t> Bytes(ByteSize);
    for (unsigned I = 0; I < ByteSize; ++I) {
      // Handles Endianness explicitly
      unsigned Shift = DL.isLittleEndian() ? (I * 8) : ((ByteSize - 1 - I) * 8);
      Bytes[I] = (Val >> Shift) & 0xFF;
    }
    writeBytes(Buffer, Offset, Bytes.data(), ByteSize);
    return;
  }

  if (auto *CDS = dyn_cast<ConstantDataSequential>(CV)) {
    StringRef RawData = CDS->getRawDataValues();
    writeBytes(Buffer, Offset, (const uint8_t *)RawData.data(), RawData.size());
    return;
  }

  if (auto *CA = dyn_cast<ConstantArray>(CV)) {
    unsigned ElemSize = DL.getTypeAllocSize(CA->getType()->getElementType());
    for (unsigned I = 0; I < CA->getNumOperands(); ++I) {
      flattenConstant(CA->getOperand(I), DL, Offset + (I * ElemSize), Buffer,
                      Relocs);
    }
    return;
  }

  if (auto *CS = dyn_cast<ConstantStruct>(CV)) {
    const StructLayout *SL = DL.getStructLayout(CS->getType());
    for (unsigned I = 0; I < CS->getNumOperands(); ++I) {
      flattenConstant(CS->getOperand(I), DL, Offset + SL->getElementOffset(I),
                      Buffer, Relocs);
    }
    return;
  }

  // pointers and shit
  if (CV->getType()->isPointerTy()) {
    Relocs[Offset] = CV;
    return;
  }
}

void URCLAsmPrinter::emitGlobalVariable(const GlobalVariable *GV) {
  if (!GV->hasInitializer())
    return;

  MCSection *TextSec = getObjFileLowering().getTextSection();
  OutStreamer->switchSection(TextSec);

  MCSymbol *Sym = getSymbol(GV);
  OutStreamer->emitLabel(Sym);

  const DataLayout &DL = GV->getParent()->getDataLayout();

  std::vector<uint8_t> Buffer;
  std::map<unsigned, const Constant *> Relocs;

  flattenConstant(GV->getInitializer(), DL, 0, Buffer, Relocs);

  // padd end
  if (Buffer.size() % 4 != 0) {
    Buffer.resize((Buffer.size() + 3) & ~3, 0);
  }

  // emit as 32-bit Words
  for (unsigned I = 0; I < Buffer.size(); I += 4) {
    bool EmittedReloc = false;
    for (auto const &[RelocOffset, Val] : Relocs) {
      if (RelocOffset >= I && RelocOffset < I + 4) {
        const GlobalValue *GV = dyn_cast<GlobalValue>(Val);
        if (GV && GlobalOffsets.count(dyn_cast<GlobalVariable>(GV))) {
          uint64_t ByteAddr = GlobalOffsets[dyn_cast<GlobalVariable>(GV)];
          assert(ByteAddr % 4 == 0);
          OutStreamer->emitIntValue(ByteAddr / 4, 4);
        } else {
          emitGlobalConstant(DL, Val);
        }

        EmittedReloc = true;
        break;
      }
    }

    if (!EmittedReloc) {
      uint32_t Word = (uint32_t)Buffer[I] | ((uint32_t)Buffer[I + 1] << 8) |
                      ((uint32_t)Buffer[I + 2] << 16) |
                      ((uint32_t)Buffer[I + 3] << 24);

      OutStreamer->emitIntValue(Word, 4);
    }
  }

  OutStreamer->addBlankLine();
}

bool URCLAsmPrinter::doInitialization(Module &M) {
  uint64_t CurrentOffset = 0;
  const DataLayout &DL = M.getDataLayout();

  for (const GlobalVariable &GV : M.globals()) {
    if (!GV.hasInitializer())
      continue;

    // Calculate alignment/padding (your architecture is 4-byte aligned)
    // If you have specific alignment rules, calculate them here
    uint64_t Size = DL.getTypeAllocSize(GV.getInitializer()->getType());

    // Align to 4 bytes if needed
    if (CurrentOffset % 4 != 0) {
      CurrentOffset = (CurrentOffset + 3) & ~3;
    }

    // Store the offset (in BYTES)
    GlobalOffsets[&GV] = CurrentOffset;

    // Increment offset
    CurrentOffset += Size;
  }

  for (GlobalVariable &GV : M.globals()) {
    if (GV.hasName() && !GV.getName().starts_with(".")) {
      GV.setName(Twine(".") + GV.getName());
    }
  }

  for (Function &F : M) {
    if (!F.isIntrinsic() && F.hasName() && !F.getName().starts_with(".")) {
      F.setName(Twine(".") + F.getName());
    }
  }

  return AsmPrinter::doInitialization(M);
}

void URCLAsmPrinter::emitAlignment(Align Alignment, const GlobalObject *GV,
                                   unsigned MaxBytesToEmit) {}

bool URCLAsmPrinter::doFinalization(llvm::Module &M) {
  for (const GlobalVariable &GV : M.globals()) {
    emitGlobalVariable(&GV);
  }
  return false;
}

void URCLAsmPrinter::emitFunctionHeader() {
  const Function &F = MF->getFunction();

  auto *Section = getObjFileLowering().SectionForGlobal(&F, TM);
  MF->setSection(Section);
  for (auto &Handler : Handlers) {
    Handler->beginFunction(MF);
    Handler->beginBasicBlockSection(MF->front());
  }
}

void URCLAsmPrinter::emitFunctionBodyStart() {
  StringRef FuncName = MF->getFunction().getName();
  OutStreamer->emitRawText("//-- Begin function " +
                           GlobalValue::dropLLVMManglingEscape(FuncName) +
                           "\n");
  OutStreamer->emitRawText(FuncName + "\n");
}

void URCLAsmPrinter::emitBasicBlockStart(const MachineBasicBlock &MBB) {
  MCSymbol *Sym = MBB.getSymbol();
  OutStreamer->emitLabel(Sym);
}

MCOperand URCLAsmPrinter::lowerOperand(const MachineOperand &MO) const {
  switch (MO.getType()) {
  default:
    errs() << "Operand:" << MO.getType() << "   " << MO;
    llvm_unreachable("unknown operand type");
    break;
  case MachineOperand::MO_Register:
    if (MO.isImplicit())
      break;
    return MCOperand::createReg(MO.getReg());

  case MachineOperand::MO_Immediate:
    return MCOperand::createImm(MO.getImm());

  case MachineOperand::MO_MachineBasicBlock:
  case MachineOperand::MO_GlobalAddress:
  case MachineOperand::MO_BlockAddress:
  case MachineOperand::MO_ExternalSymbol:
  case MachineOperand::MO_ConstantPoolIndex: {
    auto RelType = MO.getTargetFlags();
    const MCSymbol *Symbol = nullptr;
    switch (MO.getType()) {
    default:
      errs() << MO.getType() << "   " << MO;
      llvm_unreachable("");
    case MachineOperand::MO_MachineBasicBlock:
      Symbol = MO.getMBB()->getSymbol();
      break;
    case MachineOperand::MO_GlobalAddress:
      Symbol = getSymbol(MO.getGlobal());
      break;
    case MachineOperand::MO_BlockAddress:
      Symbol = GetBlockAddressSymbol(MO.getBlockAddress());
      break;
    case MachineOperand::MO_ExternalSymbol:
      Symbol = GetExternalSymbolSymbol(MO.getSymbolName());
      break;
    case MachineOperand::MO_ConstantPoolIndex:
      Symbol = GetCPISymbol(MO.getIndex());
      break;
    }

    const MCExpr *Expr = MCSymbolRefExpr::create(Symbol, OutContext);
    if (RelType)
      Expr = MCSpecifierExpr::create(Expr, RelType, OutContext);
    return MCOperand::createExpr(Expr);
  }

  case MachineOperand::MO_RegisterMask:
    break;
  }
  return MCOperand();
}

void URCLAsmPrinter::lowerToMCInst(const MachineInstr *MI, MCInst &OutMI) {
  OutMI.setOpcode(MI->getOpcode());

  for (const MachineOperand &MO : MI->operands()) {
    MCOperand MCOp = lowerOperand(MO);
    if (MCOp.isValid())
      OutMI.addOperand(MCOp);
  }
}

void URCLAsmPrinter::emitInstruction(const MachineInstr *MI) {
  URCL_MC::verifyInstructionPredicates(MI->getOpcode(),
                                       getSubtargetInfo().getFeatureBits());
  if (MI->isBundle()) {
    const MachineBasicBlock *MBB = MI->getParent();
    MachineBasicBlock::const_instr_iterator I = ++MI->getIterator();
    while (I != MBB->instr_end() && I->isInsideBundle()) {
      emitInstruction(&*I);
      ++I;
    }
    return;
  }

  switch (MI->getOpcode()) {
  default:
    break;
  case TargetOpcode::DBG_VALUE:
    // FIXME: Debug Value.
    return;
  }
  MachineBasicBlock::const_instr_iterator I = MI->getIterator();
  MachineBasicBlock::const_instr_iterator E = MI->getParent()->instr_end();
  do {
    MCInst TmpInst;
    lowerToMCInst(&*I, TmpInst);
    EmitToStreamer(*OutStreamer, TmpInst);
  } while ((++I != E) && I->isInsideBundle());
}

char URCLAsmPrinter::ID = 0;

INITIALIZE_PASS(URCLAsmPrinter, "urcl-asm-printer", "URCL Assembly Printer",
                false, false)

extern "C" LLVM_ABI LLVM_EXTERNAL_VISIBILITY void
LLVMInitializeURCLAsmPrinter() {
  RegisterAsmPrinter<URCLAsmPrinter> X(getTheURCLTarget());
}
