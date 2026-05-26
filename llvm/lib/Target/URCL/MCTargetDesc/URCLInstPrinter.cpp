//===-- URCLInstPrinter.cpp - Convert URCL MCInst to assembly syntax -----==//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// URCLDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This class prints an URCL MCInst to a .s file.
//
//===----------------------------------------------------------------------===//

#include "URCLInstPrinter.h"
#include "URCL.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/MCSymbol.h"
#include "llvm/Support/raw_ostream.h"
using namespace llvm;

#define DEBUG_TYPE "asm-printer"

// The generated AsmMatcher URCLGenAsmWriter uses "URCL" as the target
// namespace. But URCL backend uses "URCL" as its namespace.
namespace llvm {
namespace URCL {
using namespace URCL;
}
} // namespace llvm

#define GET_INSTRUCTION_NAME
#define PRINT_ALIAS_INSTR
#include "URCLGenAsmWriter.inc"

void URCLInstPrinter::printRegName(raw_ostream &OS, MCRegister Reg) {
  OS << getRegisterName(Reg);
}

void URCLInstPrinter::printRegName(raw_ostream &OS, MCRegister Reg,
                                   unsigned AltIdx) const {
  OS << getRegisterName(Reg, AltIdx);
}

void URCLInstPrinter::printInst(const MCInst *MI, uint64_t Address,
                                StringRef Annot, const MCSubtargetInfo &STI,
                                raw_ostream &O) {
  if (!printAliasInstr(MI, Address, STI, O) && !printURCLAliasInstr(MI, STI, O))
    printInstruction(MI, Address, STI, O);
  printAnnotation(O, Annot);
}

bool URCLInstPrinter::printURCLAliasInstr(const MCInst *MI,
                                          const MCSubtargetInfo &STI,
                                          raw_ostream &O) {
  switch (MI->getOpcode()) {
  default:
    return false;
    // case URCL::JMPLrr:
    // case URCL::JMPLri: {
    //   if (MI->getNumOperands() != 3)
    //     return false;
    //   if (!MI->getOperand(0).isReg())
    //     return false;
    //   switch (MI->getOperand(0).getReg().id()) {
    //   default:
    //     return false;
    //   case URCL::G0: // jmp $addr | ret | retl
    //     if (MI->getOperand(2).isImm() && MI->getOperand(2).getImm() == 8) {
    //       switch (MI->getOperand(1).getReg().id()) {
    //       default:
    //         break;
    //       case URCL::I7:
    //         O << "\tret";
    //         return true;
    //       case URCL::O7:
    //         O << "\tretl";
    //         return true;
    //       }
    //     }
    //     O << "\tjmp ";
    //     printMemOperand(MI, 1, STI, O);
    //     return true;
    //   case URCL::O7: // call $addr
    //     O << "\tcall ";
    //     printMemOperand(MI, 1, STI, O);
    //     return true;
    //   }
    // }
    // case URCL::V9FCMPS:
    // case URCL::V9FCMPD:
    // case URCL::V9FCMPQ:
    // case URCL::V9FCMPES:
    // case URCL::V9FCMPED:
    // case URCL::V9FCMPEQ: {
    //   if (isV9(STI) || (MI->getNumOperands() != 3) ||
    //       (!MI->getOperand(0).isReg()) ||
    //       (MI->getOperand(0).getReg() != URCL::FCC0))
    //     return false;
    //   // if V8, skip printing %fcc0.
    //   switch (MI->getOpcode()) {
    //   default:
    //   case URCL::V9FCMPS:
    //     O << "\tfcmps ";
    //     break;
    //   case URCL::V9FCMPD:
    //     O << "\tfcmpd ";
    //     break;
    //   case URCL::V9FCMPQ:
    //     O << "\tfcmpq ";
    //     break;
    //   case URCL::V9FCMPES:
    //     O << "\tfcmpes ";
    //     break;
    //   case URCL::V9FCMPED:
    //     O << "\tfcmped ";
    //     break;
    //   case URCL::V9FCMPEQ:
    //     O << "\tfcmpeq ";
    //     break;
    //   }
    //   printOperand(MI, 1, STI, O);
    //   O << ", ";
    //   printOperand(MI, 2, STI, O);
    //   return true;
    // }
  }
}

void URCLInstPrinter::printCTILabel(const MCInst *MI, uint64_t Address,
                                    unsigned OpNum, const MCSubtargetInfo &STI,
                                    raw_ostream &O) {
  const MCOperand &Op = MI->getOperand(OpNum);

  // If the label has already been resolved to an immediate offset (say, when
  // we're running the disassembler), just print the immediate.
  if (Op.isImm()) {
    int64_t Offset = Op.getImm();
    if (PrintBranchImmAsAddress) {
      uint64_t Target = Address + Offset;
      if (STI.getTargetTriple().isSPARC32())
        Target &= 0xffffffff;
      O << formatHex(Target);
    } else {
      O << ".";
      if (Offset >= 0)
        O << "+";
      O << Offset;
    }
    return;
  }

  // Otherwise, just print the expression.
  MAI.printExpr(O, *Op.getExpr());
}
void URCLInstPrinter::printOperand(const MCInst *MI, int opNum,
                                   const MCSubtargetInfo &STI, raw_ostream &O) {
  const MCOperand &MO = MI->getOperand(opNum);

  if (MO.isReg()) {
    MCRegister Reg = MO.getReg();
    printRegName(O, Reg);
    return;
  }

  if (MO.isImm()) {
    switch (MI->getOpcode()) {
    default:
      markup(O, Markup::Immediate) << formatImm(int32_t(MO.getImm()));
      return;

      // case URCL::TICCri: // Fall through
      // case URCL::TICCrr: // Fall through
      // case URCL::TRAPri: // Fall through
      // case URCL::TRAPrr: // Fall through
      // case URCL::TXCCri: // Fall through
      // case URCL::TXCCrr: // Fall through
      //   // Only seven-bit values up to 127.
      // O << ((int)MO.getImm() & 0x7f);
      // return;
    }
  }

  assert(MO.isExpr() && "Unknown operand kind in printOperand");

  const MCExpr *Expr = MO.getExpr();

  if (const MCSymbolRefExpr *SRE = dyn_cast<MCSymbolRefExpr>(Expr)) {
    if (SRE->getSymbol().getName().starts_with(".")) {
      O << SRE->getSymbol().getName();
    } else {
      O << "." << SRE->getSymbol().getName();
    }
  } else {
    MAI.printExpr(O, *Expr);
  }
}
