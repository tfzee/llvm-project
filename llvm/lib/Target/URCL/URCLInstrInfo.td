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

def URCLToWordAddr : SDNode<"URCLISD::TO_WORD_ADDR", SDTIntUnaryOp>;
def URCLword_addr : SDNode<"URCLISD::WORD_ADDR", SDTIntUnaryOp>;
def URCLGLobalRef : SDNode<"URCLISD::GLOBAL_REF", SDTIntUnaryOp>;

def urcl_ptr_rc : RegClassByHwMode<[DefaultMode], [IntRegs]>;


def ptr_op : RegisterOperand<urcl_ptr_rc> {
  let DecoderMethod = "DecodeIntRegsRegisterClass";
}
def simm_generic : PatLeaf<(imm), [{
  // Query the subtarget dynamically for the current bit width
  unsigned Width = Subtarget->is8Bit() ? 8 : (Subtarget->is16Bit() ? 16 : 32);
  return isIntN(Width, N->getSExtValue());
}]>;

def oimm_generic : PatLeaf<(imm), [{
  unsigned Width = Subtarget->is8Bit() ? 8 : (Subtarget->is16Bit() ? 16 : 32);
  return isIntN(Width, N->getZExtValue());
}]>;



def simm_genericOp : Operand<iPTR> {
  let OperandType = "OPERAND_IMMEDIATE";
  let DecoderMethod = "DecodeSIMM32";
  let EncoderMethod = "getSImm32OpValue";
}
def imm_generic : Operand<DynamicRegType>;

def GetWordShiftAmount : SDNodeXForm<imm, [{
  // Query subtarget to get the right shift constant for word sizing
  uint32_t Shift = Subtarget->is8Bit() ? 0 : (Subtarget->is16Bit() ? 1 : 2);
  return CurDAG->getTargetConstant(Shift, SDLoc(N), MVT::i32);
}]>;

// Define a pattern leaf that matches the shift imm and applies the transform
def word_shift_amt : PatLeaf<(imm), [{ 
  uint32_t Shift = Subtarget->is8Bit() ? 0 : (Subtarget->is16Bit() ? 1 : 2);
  return N->getZExtValue() == Shift; 
}], GetWordShiftAmount>;


def SDT_SPCallSeqStart : SDCallSeqStart<[ SDTCisVT<0, DynamicRegType>,
                                          SDTCisVT<1, DynamicRegType> ]>;
def SDT_SPCallSeqEnd   : SDCallSeqEnd<[ SDTCisVT<0, DynamicRegType>,
                                        SDTCisVT<1, DynamicRegType> ]>;
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
def ADJCALLSTACKDOWN : Pseudo<(outs), (ins imm_generic:$amt1, imm_generic:$amt2),
                               "!ADJCALLSTACKDOWN $amt1, $amt2",
                               [(callseq_start timm:$amt1, timm:$amt2)]>;
def ADJCALLSTACKUP : Pseudo<(outs), (ins imm_generic:$amt1, imm_generic:$amt2),
                            "!ADJCALLSTACKUP $amt1",
                            [(callseq_end timm:$amt1, timm:$amt2)]>;
}

def to_tframeindex : SDNodeXForm<frameindex, [{
  return CurDAG->getTargetFrameIndex(N->getIndex(), N->getValueType(0));
}]>;

def calltarget : Operand<DynamicRegType> {
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
  def ri  : Base_Instr3<(outs RC:$rd), (ins RC:$rs1, immOp:$simm_generic),
                 !strconcat(OpcStr, " $rd, $rs1, $simm_generic"),
                 [(set Ty:$rd, (OpNode Ty:$rs1, (Ty simm_generic:$simm_generic)))],
                 itin>;
}

multiclass Instr2<string OpcStr, SDPatternOperator OpNode,
                 RegisterClass RC, ValueType Ty, Operand immOp,
                 InstrItinClass itin = NoItinerary> {
  def rr  : Base_Instr2<(outs RC:$rd), (ins RC:$rs1),
                 !strconcat(OpcStr, " $rd, $rs1"),
                 [(set Ty:$rd, (OpNode Ty:$rs1))],
                 itin>;
  def ri  : Base_Instr2<(outs RC:$rd), (ins immOp:$simm_generic),
                 !strconcat(OpcStr, " $rd, $simm_generic"),
                 [(set Ty:$rd, (OpNode simm_generic:$simm_generic))],
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
  def _ri : Base_Instr2<(outs RC:$rd), (ins RC:$rs1, immOp:$simm_generic),
                 !strconcat(OpcStr, " $rd, $rs1, $simm_generic"),
                 [(set Ty:$rd, (OpNode (URCLword_addr (addr_ri RC:$rs1, simm_generic:$simm_generic))))],
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
  def _ri : Base_Instr2<(outs), (ins RC:$rs1, immOp:$simm_generic, RC:$rd),
                 !strconcat(OpcStr, " $rs1, $simm_generic, $rd"),
                 [(OpNode Ty:$rd, (URCLword_addr (addr_ri RC:$rs1, simm_generic:$simm_generic)))],
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

  def _i  : Base_Instr2<(outs RC:$rd), (ins immOp:$simm_generic),
                 !strconcat(OpcStr, " $rd, $simm_generic"),
                 [(set Ty:$rd, (OpNode (URCLword_addr simm_generic:$simm_generic)))],
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
  def _i  : Base_Instr2<(outs), (ins RC:$rd, immOp:$simm_generic),
                 !strconcat(OpcStr, " $simm_generic, $rd"),
                 [(OpNode Ty:$rd, (URCLword_addr simm_generic:$simm_generic))],
                 itin>;
}
}

multiclass BR_CC<string OpcStr, CondCode CC,
                RegisterClass RC, Operand immOp, InstrItinClass itin = NoItinerary> { 
  def _rr  : Base_Instr3<(outs), (ins brtarget:$rd, RC:$rs1, RC:$rs2),
                 !strconcat(OpcStr, " $rd, $rs1, $rs2"),
                 [(brcc CC, RC:$rs1, RC:$rs2, bb:$rd)],
                 itin>;
  def _ri  : Base_Instr3<(outs), (ins brtarget:$rd, RC:$rs1, immOp:$simm_generic),
                 !strconcat(OpcStr, " $rd, $rs1, $simm_generic"),
                 [(brcc CC, RC:$rs1, simm_generic:$simm_generic, bb:$rd)],
                 itin>;
}

multiclass SET_CC<string OpcStr, CondCode CC,
                RegisterClass RC, ValueType Ty, Operand immOp, InstrItinClass itin = NoItinerary> { 
  def _r  : Base_Instr3<(outs RC:$rd), (ins RC:$rs1, RC:$rs2),
                 !strconcat(OpcStr, " $rd, $rs1, $rs2"),
                 [(set Ty:$rd, (setcc RC:$rs1, RC:$rs2, CC))],
                 itin>;
  def _i  : Base_Instr3<(outs RC:$rd), (ins RC:$rs1, immOp:$simm_generic),
                 !strconcat(OpcStr, " $rd, $rs1, $simm_generic"),
                 [(set Ty:$rd, (setcc RC:$rs1, simm_generic:$simm_generic, CC))],
                 itin>;
}

// schpeschial
let hasSideEffects = 0, isMoveReg = 1 in {
def MOVrr  : Base_Instr2<(outs IntRegs:$rd), (ins IntRegs:$rs1),
  "mov $rd, $rs1",
  [],
  NoItinerary>;
}

let hasSideEffects = 0, isReMaterializable = 0, isAsCheapAsAMove = 1, isMoveImm = 1 in {
def MOVri  : Base_Instr2<(outs IntRegs:$rd), (ins simm_genericOp:$simm_generic),
  "mov $rd, $simm_generic",
  [],
  // [(set i32:$rd, simm_generic:$simm_generic)],
  NoItinerary>;
}



// Arith
let hasSideEffects = 0 in {
  defm AND    : Instr3<"and",   and,       IntRegs, DynamicRegType, simm_genericOp>;
  defm OR     : Instr3<"or",    or,        IntRegs, DynamicRegType, simm_genericOp>;
  defm XOR    : Instr3<"xor",   xor,       IntRegs, DynamicRegType, simm_genericOp>;
  defm ADD    : Instr3<"add",   add,       IntRegs, DynamicRegType, simm_genericOp>;
  defm SUB    : Instr3<"sub",   sub,       IntRegs, DynamicRegType, simm_genericOp>;
  defm BSS    : Instr3<"bss",   sra,       IntRegs, DynamicRegType, simm_genericOp>;
  defm BSR    : Instr3<"bsr",   srl,       IntRegs, DynamicRegType, simm_genericOp>;
  defm BSL    : Instr3<"bsl",   shl,       IntRegs, DynamicRegType, simm_genericOp>;
  defm MOD    : Instr3<"mod",   urem,      IntRegs, DynamicRegType, simm_genericOp>;
  defm DIV    : Instr3<"div",   udiv,      IntRegs, DynamicRegType, simm_genericOp>;
  defm SDIV   : Instr3<"sdiv",  sdiv,      IntRegs, DynamicRegType, simm_genericOp>;
  defm MLT    : Instr3<"mlt",   mul,       IntRegs, DynamicRegType, simm_genericOp>;
  defm UMULT  : Instr3<"umlt",  mulhu,     IntRegs, DynamicRegType, simm_genericOp>;
  defm SUMLT  : Instr3<"sumlt", mulhs,     IntRegs, DynamicRegType, simm_genericOp>;
  
  defm NOT    : Instr2<"not",   not,       IntRegs, DynamicRegType, simm_genericOp>;
  def INC  : Base_Instr2<(outs IntRegs:$rd), (ins IntRegs:$rs1),
               "inc $rd, $rs1",
               [(set DynamicRegType:$rd, (add IntRegs:$rs1, (DynamicRegType 1)))],
                 NoItinerary>;
  def DEC  : Base_Instr2<(outs IntRegs:$rd), (ins IntRegs:$rs1),
               "dec $rd, $rs1",
               [(set DynamicRegType:$rd, (add IntRegs:$rs1, (DynamicRegType -1)))],
                 NoItinerary>;
  def NOT  : Base_Instr2<(outs IntRegs:$rd), (ins IntRegs:$rs1),
               "not $rd, $rs1",
               [(set DynamicRegType:$rd, (not IntRegs:$rs1))],
                 NoItinerary>;

}
    
def : Pat<(sub IntRegs:$rs1, (DynamicRegType 1)), (DEC IntRegs:$rs1)>;
def : Pat<(sub IntRegs:$rs1, (DynamicRegType -1)), (INC IntRegs:$rs1)>;

// def : Pat<(i32 0), (COPY (i32 R0))>;
def : Pat<(DynamicRegType simm_generic:$val),
          (ADDri (DynamicRegType R0), imm:$val)>;


defm LODB  : Load<"LOD",       extloadi8,  IntRegs, DynamicRegType, simm_genericOp>;
defm LODH  : Load<"LOD",       extloadi16, IntRegs, DynamicRegType, simm_genericOp>;
defm LOD   : Load<"LOD",       load,       IntRegs, DynamicRegType, simm_genericOp>;
defm LLODB : ListLoad<"LLOD",  extloadi8,  IntRegs, DynamicRegType, simm_genericOp>;
defm LLODH : ListLoad<"LLOD",  extloadi16, IntRegs, DynamicRegType, simm_genericOp>;
defm LLOD  : ListLoad<"LLOD",  load,       IntRegs, DynamicRegType, simm_genericOp>;

defm STR  : Store<"STR",       store, IntRegs, DynamicRegType, simm_genericOp>;
defm LSTR : ListStore<"LSTR",  store, IntRegs, DynamicRegType, simm_genericOp>;

//pointer stuff
//Frame index shenanigans
// def : Pat<(i32 (frameindex:$ptr)), 
//           (ADDri (i32 (to_tframeindex $ptr)), (i32 0))>;
def : Pat<(DynamicRegType (frameindex:$ptr)), 
          (DynamicRegType (to_tframeindex $ptr))>;


//for global

let Predicates = [IsURCL32] in {
  let hasSideEffects = 0, isReMaterializable = 1 in {
    def GET_GLOBAL_ADDR_i32 : Base_Instr2<(outs IntRegs:$rd), (ins simm_genericOp:$simm),
                           "mov $rd, $simm\n\tbsl $rd, $rd, 2",
                           []>;
  }
  def : Pat<(i32 (load (URCLword_addr (i32 (srl (URCLGLobalRef i32:$g), (i32 2)))))), 
            (LOD_g i32:$g)>;
  def : Pat<(URCLGLobalRef i32:$g), (GET_GLOBAL_ADDR_i32 i32:$g)>;
  def : Pat<(URCLToWordAddr IntRegs:$rs1), (BSRri IntRegs:$rs1, (DynamicRegType 2))>;
}

let Predicates = [IsURCL16] in {
  let hasSideEffects = 0, isReMaterializable = 1 in {
    def GET_GLOBAL_ADDR_i16 : Base_Instr2<(outs IntRegs:$rd), (ins simm_genericOp:$simm),
                           "mov $rd, $simm\n\tbsl $rd, $rd, 1",
                           []>;
  }
  def : Pat<(i16 (load (URCLword_addr (i16 (srl (URCLGLobalRef i16:$g), (i16 1)))))), 
            (LOD_g i16:$g)>;
  def : Pat<(URCLGLobalRef i16:$g), (GET_GLOBAL_ADDR_i16 i16:$g)>;
  def : Pat<(URCLToWordAddr IntRegs:$rs1), (BSRri IntRegs:$rs1, (DynamicRegType 1))>;
}

let Predicates = [IsURCL8] in {
  let hasSideEffects = 0, isReMaterializable = 1 in {
    def GET_GLOBAL_ADDR_i8 : Base_Instr2<(outs IntRegs:$rd), (ins simm_genericOp:$simm),
                           "mov $rd, $simm",
                           []>;
  }
  def : Pat<(i8 (load (URCLword_addr (URCLGLobalRef i8:$g)))), 
            (LOD_g i8:$g)>;
  def : Pat<(URCLGLobalRef i8:$g), (GET_GLOBAL_ADDR_i8 i8:$g)>;
  def : Pat<(URCLToWordAddr IntRegs:$rs1), (COPY IntRegs:$rs1)>;
}



let isBranch = 1, isTerminator = 1, isBarrier = 1 in {
  def JMP : Base_Instr1<(outs), (ins brtarget:$rd),
                    "jmp $rd",
                    [(br bb:$rd)]>;
  defm SBGE : BR_CC<"sbge", SETGE, IntRegs, simm_genericOp>;
  defm SBRG : BR_CC<"sbrg", SETGT, IntRegs, simm_genericOp>;
  defm SBLE : BR_CC<"sble", SETLE, IntRegs, simm_genericOp>;
  defm SBRL : BR_CC<"sbrl", SETLT, IntRegs, simm_genericOp>;
  defm BGE : BR_CC<"bge", SETUGE, IntRegs, simm_genericOp>;
  defm BRG : BR_CC<"brg", SETUGT, IntRegs, simm_genericOp>;
  defm BLE : BR_CC<"ble", SETULE, IntRegs, simm_genericOp>;
  defm BRL : BR_CC<"brl", SETULT, IntRegs, simm_genericOp>;

  defm BRE : BR_CC<"bre", SETEQ, IntRegs, simm_genericOp>;
  defm BNE : BR_CC<"bne", SETNE, IntRegs, simm_genericOp>;
}

defm SETE  : SET_CC<"sete",  SETEQ, IntRegs, DynamicRegType, simm_genericOp>;
defm SETNE : SET_CC<"setne", SETNE, IntRegs, DynamicRegType, simm_genericOp>;


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
                    [(URCLcall oimm_generic:$rs1)]> {
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

