//===-- URCLInstrFormats.td - URCL Instruction Formats ---*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

class InstURCL<dag outs, dag ins, string asmstr, list<dag> pattern,
             InstrItinClass itin = NoItinerary>
   : Instruction {
  field bits<32> Inst;

  let Namespace = "URCL";
  let Size = 4;

  bits<2> op;
  let Inst{31-30} = op;               // Top two bits are the 'op' field

  dag OutOperandList = outs;
  dag InOperandList = ins;
  let AsmString   = asmstr;
  let Pattern = pattern;

  let DecoderNamespace = "URCL";

  let Itinerary = itin;
}

class Base_Instr3<dag outs, dag ins, string asmstr, list<dag> pattern,
         InstrItinClass itin = NoItinerary>
   : InstURCL<outs, ins, asmstr, pattern, itin> {
  bits<5> rd;
  bits<6> rs1;
  bits<5> rs2;
  let op{0-1} = 1;
  let Inst{29-25} = rd;
  let Inst{24-19} = rs1;
  let Inst{18-14} = rs2;
}

class Base_Instr2<dag outs, dag ins, string asmstr, list<dag> pattern,
         InstrItinClass itin = NoItinerary>
   : InstURCL<outs, ins, asmstr, pattern, itin> {
  bits<5> rd;
  bits<6> rs1;
  let op{0-1} = 1;
  let Inst{29-25} = rd;
  let Inst{24-19} = rs1;
  let Inst{18-14} = 0;
}

class Base_Instr1<dag outs, dag ins, string asmstr, list<dag> pattern,
         InstrItinClass itin = NoItinerary>
   : InstURCL<outs, ins, asmstr, pattern, itin> {
  bits<6> rs1;
  let op{0-1} = 1;
  let Inst{29-25} = 0;
  let Inst{24-19} = rs1;
  let Inst{18-14} = 0;
}

