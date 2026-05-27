//===-- URCLInstrInfo.td - Target Description for URCL Target -----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file describes the URCL instructions in TableGen format.
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// Instruction format superclass
//===----------------------------------------------------------------------===//

include "URCLInstrFormats.td"

// def SDT_URCLRet : SDTypeProfile<0, 0, []>;
// def URCLretflag : SDNode<"URCLISD::RET_FLAG", SDT_URCLRet,
//                 [SDNPHasChain, SDNPOptInGlue, SDNPOutGlue, SDNPVariadic]>;
def URCL_RetProfile : SDTypeProfile<0, 0, []>;
def URCL_RetGlueProfile : SDTypeProfile<0, -1, []>;
def URCLret : SDNode<"URCLISD::RET", URCL_RetProfile,
                         [SDNPHasChain, SDNPOptInGlue]>;
def URCLret_glue : SDNode<"URCLISD::RET_GLUE", URCL_RetGlueProfile,
                         [SDNPHasChain, SDNPOptInGlue, SDNPOutGlue, SDNPVariadic]>;

def URCLword_addr : SDNode<"URCLISD::WORD_ADDR", SDTIntUnaryOp>;
def URCLGLobalRef : SDNode<"URCLISD::GLOBAL_REF", SDTIntUnaryOp>;

def urcl_ptr_rc : RegClassByHwMode<[DefaultMode], [IntRegs]>;


def ptr_op : RegisterOperand<urcl_ptr_rc> {
  let DecoderMethod = "DecodeIntRegsRegisterClass";
}
def simm32  : PatLeaf<(imm), [{ return isInt<32>(N->getSExtValue()); }]>;
def oimm32  : PatLeaf<(imm), [{ return isInt<32>(N->getZExtValue()); }]>;


def simm32Op : Operand<iPTR> {
  let OperandType = "OPERAND_IMMEDIATE";
  let DecoderMethod = "DecodeSIMM32";
  let EncoderMethod = "getSImm32OpValue";
}

def SDT_SPCallSeqStart : SDCallSeqStart<[ SDTCisVT<0, i32>,
                                          SDTCisVT<1, i32> ]>;
def SDT_SPCallSeqEnd   : SDCallSeqEnd<[ SDTCisVT<0, i32>,
                                        SDTCisVT<1, i32> ]>;
def callseq_start : SDNode<"ISD::CALLSEQ_START", SDT_SPCallSeqStart,
                           [SDNPHasChain, SDNPOutGlue]>;
def callseq_end   : SDNode<"ISD::CALLSEQ_END",   SDT_SPCallSeqEnd,
                           [SDNPHasChain, SDNPOptInGlue, SDNPOutGlue]>;

def SDT_URCLCall    : SDTypeProfile<0, -1, [SDTCisVT<0, iPTR>]>;
def URCLcall          : SDNode<"URCLISD::CALL", SDT_URCLCall,
                           [SDNPHasChain, SDNPOptInGlue, SDNPOutGlue,
                            SDNPVariadic]>;


class Pseudo<dag outs, dag ins, string asmstr, list<dag> pattern>
   : InstURCL<outs, ins, asmstr, pattern> {
  let isCodeGenOnly = 1;
  let isPseudo = 1;
}

let Defs = [SP], Uses = [SP] in {
def ADJCALLSTACKDOWN : Pseudo<(outs), (ins i32imm:$amt1, i32imm:$amt2),
                               "!ADJCALLSTACKDOWN $amt1, $amt2",
                               [(callseq_start timm:$amt1, timm:$amt2)]>;
def ADJCALLSTACKUP : Pseudo<(outs), (ins i32imm:$amt1, i32imm:$amt2),
                            "!ADJCALLSTACKUP $amt1",
                            [(callseq_end timm:$amt1, timm:$amt2)]>;
}

def to_tframeindex : SDNodeXForm<frameindex, [{
  return CurDAG->getTargetFrameIndex(N->getIndex(), N->getValueType(0));
}]>;

def calltarget : Operand<i32> {
  // let EncoderMethod = "getCallTargetOpValue";
}
def brtarget : Operand<OtherVT> {
  // let EncoderMethod = "getCallTargetOpValue";
}

// def brtarget : Operand<OtherVT> {
//   let EncoderMethod = "getBranchTargetOpValue";
//   let DecoderMethod = "DecodeDisp<22>";
//   let PrintMethod = "printCTILabel";
//   let OperandType = "OPERAND_PCREL";
// }


multiclass Instr3<string OpcStr, SDPatternOperator OpNode,
                 RegisterClass RC, ValueType Ty, Operand immOp,
                 InstrItinClass itin = NoItinerary> {
  def rr  : Base_Instr3<(outs RC:$rd), (ins RC:$rs1, RC:$rs2),
                 !strconcat(OpcStr, " $rd, $rs1, $rs2"),
                 [(set Ty:$rd, (OpNode Ty:$rs1, Ty:$rs2))],
                 itin>;
  def ri  : Base_Instr3<(outs RC:$rd), (ins RC:$rs1, immOp:$simm32),
                 !strconcat(OpcStr, " $rd, $rs1, $simm32"),
                 [(set Ty:$rd, (OpNode Ty:$rs1, (Ty simm32:$simm32)))],
                 itin>;
}

multiclass Instr2<string OpcStr, SDPatternOperator OpNode,
                 RegisterClass RC, ValueType Ty, Operand immOp,
                 InstrItinClass itin = NoItinerary> {
  def rr  : Base_Instr2<(outs RC:$rd), (ins RC:$rs1),
                 !strconcat(OpcStr, " $rd, $rs1"),
                 [(set Ty:$rd, (OpNode Ty:$rs1))],
                 itin>;
  def ri  : Base_Instr2<(outs RC:$rd), (ins immOp:$simm32),
                 !strconcat(OpcStr, " $rd, $simm32"),
                 [(set Ty:$rd, (OpNode simm32:$simm32))],
                 itin>;
}

def addr_rr : PatFrag<(ops node:$base, node:$index), 
                     (add node:$base, node:$index)>;
def addr_ri : PatFrag<(ops node:$base, node:$imm), 
                     (add node:$base, node:$imm)>;
multiclass ListLoad<string OpcStr, SDPatternOperator OpNode,
RegisterClass RC, ValueType Ty, Operand immOp, InstrItinClass itin = NoItinerary> { 
let mayLoad = 1,
    hasSideEffects = 0,
    isReMaterializable = 1 in {
  def _rr : Base_Instr2<(outs RC:$rd), (ins RC:$rs1, RC:$rs2),
                 !strconcat(OpcStr, " $rd, $rs1, $rs2"),
                 [(set Ty:$rd, (OpNode (URCLword_addr  (addr_rr RC:$rs1, RC:$rs2))))],
                 itin>;
  def _ri : Base_Instr2<(outs RC:$rd), (ins RC:$rs1, immOp:$simm32),
                 !strconcat(OpcStr, " $rd, $rs1, $simm32"),
                 [(set Ty:$rd, (OpNode (URCLword_addr (addr_ri RC:$rs1, simm32:$simm32))))],
                 itin>;
}
}

multiclass ListStore<string OpcStr, SDPatternOperator OpNode,
                    RegisterClass RC, ValueType Ty, Operand immOp, InstrItinClass itin = NoItinerary> {
let mayStore = 1,      
    hasSideEffects = 0 in {
  def _rr : Base_Instr2<(outs), (ins RC:$rs1, RC:$rs2, RC:$rd),
                 !strconcat(OpcStr, " $rs1, $rs2, $rd"),
                 [(OpNode Ty:$rd, (URCLword_addr (addr_rr RC:$rs1, RC:$rs2)))],
                 itin>;
  def _ri : Base_Instr2<(outs), (ins RC:$rs1, immOp:$simm32, RC:$rd),
                 !strconcat(OpcStr, " $rs1, $simm32, $rd"),
                 [(OpNode Ty:$rd, (URCLword_addr (addr_ri RC:$rs1, simm32:$simm32)))],
                 itin>;
}
}


multiclass Load<string OpcStr, SDPatternOperator OpNode,
               RegisterClass RC, ValueType Ty, Operand immOp, InstrItinClass itin = NoItinerary> {
let mayLoad = 1,
    hasSideEffects = 0,
    isReMaterializable = 1 in {
  def _r  : Base_Instr2<(outs RC:$rd), (ins RC:$rs1),
                 !strconcat(OpcStr, " $rd, $rs1"),
                 [(set Ty:$rd, (OpNode (URCLword_addr RC:$rs1)))],
                 itin>;

  def _i  : Base_Instr2<(outs RC:$rd), (ins immOp:$simm32),
                 !strconcat(OpcStr, " $rd, $simm32"),
                 [(set Ty:$rd, (OpNode (URCLword_addr simm32:$simm32)))],
                 itin>;

  def _g  : Base_Instr2<(outs RC:$rd), (ins immOp:$glob),
                 !strconcat(OpcStr, " $rd, $glob"),
                 [(set Ty:$rd, (OpNode tglobaladdr:$glob))],
                 itin>;
}
}

multiclass Store<string OpcStr, SDPatternOperator OpNode,
                RegisterClass RC, ValueType Ty, Operand immOp, InstrItinClass itin = NoItinerary> { 
let mayStore = 1,      
    hasSideEffects = 0 in {
  def _r  : Base_Instr2<(outs), (ins RC:$rd, RC:$rs1),
                 !strconcat(OpcStr, " $rs1, $rd"),
                 [(OpNode Ty:$rd, (URCLword_addr RC:$rs1))],
                 itin>;
  def _i  : Base_Instr2<(outs), (ins RC:$rd, immOp:$simm32),
                 !strconcat(OpcStr, " $simm32, $rd"),
                 [(OpNode Ty:$rd, (URCLword_addr simm32:$simm32))],
                 itin>;
}
}

multiclass BR_CC<string OpcStr, CondCode CC,
                RegisterClass RC, Operand immOp, InstrItinClass itin = NoItinerary> { 
  def _r  : Base_Instr3<(outs), (ins brtarget:$rd, RC:$rs1, RC:$rs2),
                 !strconcat(OpcStr, " $rd, $rs1, $rs2"),
                 [(brcc CC, RC:$rs1, RC:$rs2, bb:$rd)],
                 itin>;
  def _i  : Base_Instr3<(outs), (ins brtarget:$rd, RC:$rs1, immOp:$simm32),
                 !strconcat(OpcStr, " $rd, $rs1, $simm32"),
                 [(brcc CC, RC:$rs1, simm32:$simm32, bb:$rd)],
                 itin>;
}

multiclass SET_CC<string OpcStr, CondCode CC,
                RegisterClass RC, ValueType Ty, Operand immOp, InstrItinClass itin = NoItinerary> { 
  def _r  : Base_Instr3<(outs RC:$rd), (ins RC:$rs1, RC:$rs2),
                 !strconcat(OpcStr, " $rd, $rs1, $rs2"),
                 [(set Ty:$rd, (setcc RC:$rs1, RC:$rs2, CC))],
                 itin>;
  def _i  : Base_Instr3<(outs RC:$rd), (ins RC:$rs1, immOp:$simm32),
                 !strconcat(OpcStr, " $rd, $rs1, $simm32"),
                 [(set Ty:$rd, (setcc RC:$rs1, simm32:$simm32, CC))],
                 itin>;
}

// schpeschial
let hasSideEffects = 0, isMoveReg = 1 in {
def MOVrr  : Base_Instr2<(outs IntRegs:$rd), (ins IntRegs:$rs1),
  "mov $rd, $rs1",
  [],
  NoItinerary>;
}

let hasSideEffects = 0, isReMaterializable = 1, isAsCheapAsAMove = 1, isMoveImm = 1 in {
def MOVri  : Base_Instr2<(outs IntRegs:$rd), (ins simm32Op:$simm32),
  "mov $rd, $simm32",
  [(set i32:$rd, simm32:$simm32)],
  NoItinerary>;
}
// def : Pat<(i32 0), (COPY (i32 R0))>;
def : Pat<(i32 simm32:$val),
          (MOVri imm:$val)>;

// Arith
let hasSideEffects = 0 in {
  defm AND    : Instr3<"and",   and,       IntRegs, i32, simm32Op>;
  defm OR     : Instr3<"or",    or,        IntRegs, i32, simm32Op>;
  defm XOR    : Instr3<"xor",   xor,       IntRegs, i32, simm32Op>;
  defm ADD    : Instr3<"add",   add,       IntRegs, i32, simm32Op>;
  defm SUB    : Instr3<"sub",   sub,       IntRegs, i32, simm32Op>;
  defm BSS    : Instr3<"bss",   sra,       IntRegs, i32, simm32Op>;
  defm BSR    : Instr3<"bsr",   srl,       IntRegs, i32, simm32Op>;
  defm BSL    : Instr3<"bsl",   shl,       IntRegs, i32, simm32Op>;
  defm MOD    : Instr3<"mod",   urem,      IntRegs, i32, simm32Op>;
  defm DIV    : Instr3<"div",   udiv,      IntRegs, i32, simm32Op>;
  defm SDIV   : Instr3<"sdiv",  sdiv,      IntRegs, i32, simm32Op>;
  defm MLT    : Instr3<"mlt",   mul,       IntRegs, i32, simm32Op>;
  defm UMULT  : Instr3<"umlt",  mulhu,     IntRegs, i32, simm32Op>;
  defm SUMLT  : Instr3<"sumlt", mulhs,     IntRegs, i32, simm32Op>;
  
  defm NOT    : Instr2<"not",   not,       IntRegs, i32, simm32Op>;
  def INC  : Base_Instr2<(outs IntRegs:$rd), (ins IntRegs:$rs1),
               "inc $rd, $rs1",
               [(set i32:$rd, (add IntRegs:$rs1, (i32 1)))],
                 NoItinerary>;
  def DEC  : Base_Instr2<(outs IntRegs:$rd), (ins IntRegs:$rs1),
               "dec $rd, $rs1",
               [(set i32:$rd, (add IntRegs:$rs1, (i32 -1)))],
                 NoItinerary>;

}
  
  
def : Pat<(sub IntRegs:$rs1, (i32 1)), (DEC IntRegs:$rs1)>;
def : Pat<(sub IntRegs:$rs1, (i32 -1)), (INC IntRegs:$rs1)>;


defm LODB  : Load<"LOD",       extloadi8,  IntRegs, i32, simm32Op>;
defm LODH  : Load<"LOD",       extloadi16, IntRegs, i32, simm32Op>;
defm LOD   : Load<"LOD",       load,       IntRegs, i32, simm32Op>;
defm LLODB : ListLoad<"LLOD",  extloadi8,  IntRegs, i32, simm32Op>;
defm LLODH : ListLoad<"LLOD",  extloadi16, IntRegs, i32, simm32Op>;
defm LLOD  : ListLoad<"LLOD",  load,       IntRegs, i32, simm32Op>;

defm STR  : Store<"STR",       store, IntRegs, i32, simm32Op>;
defm LSTR : ListStore<"LSTR",  store, IntRegs, i32, simm32Op>;

//pointer stuff
//Frame index shenanigans
// def : Pat<(i32 (frameindex:$ptr)), 
//           (ADDri (i32 (to_tframeindex $ptr)), (i32 0))>;
def : Pat<(i32 (frameindex:$ptr)), 
          (i32 (to_tframeindex $ptr))>;


//for global
let hasSideEffects = 0, isReMaterializable = 1 in {
def GET_GLOBAL_ADDR : Base_Instr2<(outs IntRegs:$rd), (ins simm32Op:$simm),
                       "mov $rd, $simm\n\tbsl $rd, $rd, 2",
                       []>;
}

def : Pat<(load (URCLword_addr (srl (URCLGLobalRef i32:$g), 2))), (LOD_g i32:$g)>;
def : Pat<(URCLGLobalRef i32:$g), (GET_GLOBAL_ADDR i32:$g)>;



let isBranch = 1, isTerminator = 1, isBarrier = 1 in {
  def JMP : Base_Instr1<(outs), (ins brtarget:$rd),
                    "jmp $rd",
                    [(br bb:$rd)]>;
  defm SBGE : BR_CC<"sbge", SETGE, IntRegs, simm32Op>;
  defm SBRG : BR_CC<"sbrg", SETGT, IntRegs, simm32Op>;
  defm SBLE : BR_CC<"sble", SETLE, IntRegs, simm32Op>;
  defm SBRL : BR_CC<"sbrl", SETLT, IntRegs, simm32Op>;
  defm BGE : BR_CC<"bge", SETUGE, IntRegs, simm32Op>;
  defm BRG : BR_CC<"brg", SETUGT, IntRegs, simm32Op>;
  defm BLE : BR_CC<"ble", SETULE, IntRegs, simm32Op>;
  defm BRL : BR_CC<"brl", SETULT, IntRegs, simm32Op>;

  defm BRE : BR_CC<"bre", SETEQ, IntRegs, simm32Op>;
  defm BNE : BR_CC<"bne", SETNE, IntRegs, simm32Op>;
}

defm SETE  : SET_CC<"sete",  SETEQ, IntRegs, i32, simm32Op>;
defm SETNE : SET_CC<"setne", SETNE, IntRegs, i32, simm32Op>;


let isReturn = 1, isTerminator = 1, isBarrier = 1, 
    Uses = [SP], Defs = [SP] in {
  def RET : InstURCL<(outs), (ins),
                 "ret",
                 []>; 
}
def : Pat<(URCLret), (RET)>;
def : Pat<(URCLret_glue), (RET)>;




let Uses = [SP], Defs = [SP], isCall = 1 in {
  def CALL : Base_Instr1<(outs), (ins calltarget:$rs1, variable_ops),
                    "cal $rs1",
                    [(URCLcall oimm32:$rs1)]> {
    bits<30> disp;
    let op = 1;
    let Inst{29-0} = disp;
  }

  // indirect calls
  // let isCodeGenOnly = 1, rd = 15 in {
  //   def CALLrr : F3_1<2, 0b111000,
  //                     (outs), (ins (MEMrr $rs1, $rs2):$addr, variable_ops),
  //                     "call $addr",
  //                     [(call ADDRrr:$addr)],
  //                     IIC_jmp_or_call>;
  //   def CALLri : F3_2<2, 0b111000,
  //                     (outs), (ins (MEMri $rs1, $simm13):$addr, variable_ops),
  //                     "call $addr",
  //                     [(call ADDRri:$addr)],
  //                     IIC_jmp_or_call>;
  // }

  // let isAsmParserOnly = 1, rd = 15 in {
  //   def CALLrri : F3_1<2, 0b111000,
  //                     (outs), (ins (MEMrr $rs1, $rs2):$addr, i32imm:$imm),
  //                     "call $addr, $imm", []>;
  //   def CALLrii : F3_2<2, 0b111000,
  //                     (outs), (ins (MEMri $rs1, $simm13):$addr, i32imm:$imm),
  //                     "call $addr, $imm", []>;
  // }
}
def : Pat<(URCLcall tglobaladdr:$dst), (CALL tglobaladdr:$dst)>;
def : Pat<(URCLcall texternalsym:$dst), (CALL texternalsym:$dst)>;

