
//===-- URCLRegisterInfo.td - URCL Register defs ---------*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
//  Declarations that describe the URCL register file
//===----------------------------------------------------------------------===//

class URCLReg<bits<16> Enc, string n> : Register<n> {
  let HWEncoding = Enc;
  let Namespace = "URCL";
}

class Ri<bits<16> Enc, string n> : URCLReg<Enc, n>;

let Namespace = "URCL",
    FallbackRegAltNameIndex = NoRegAltName in {
  def RegNamesStateReg : RegAltNameIndex;
}


// Integer registers
def R0 : Ri< 0, "r0">, DwarfRegNum<[0]> {
  let isConstant = true;
}
def R1 : Ri< 1, "r1">, DwarfRegNum<[1]>;
def R2 : Ri< 2, "r2">, DwarfRegNum<[2]>;
def R3 : Ri< 3, "r3">, DwarfRegNum<[3]>;
def R4 : Ri< 4, "r4">, DwarfRegNum<[4]>;
def R5 : Ri< 5, "r5">, DwarfRegNum<[5]>;
def R6 : Ri< 6, "r6">, DwarfRegNum<[6]>;
def R7 : Ri< 7, "r7">, DwarfRegNum<[7]>;
def R8 : Ri< 8, "r8">, DwarfRegNum<[8]>;
def R9 : Ri< 9, "r9">, DwarfRegNum<[9]>;
def R10 : Ri<10, "r10">, DwarfRegNum<[10]>;
def R11 : Ri<11, "r11">, DwarfRegNum<[11]>;
def R12 : Ri<12, "r12">, DwarfRegNum<[12]>;
def R13 : Ri<13, "r13">, DwarfRegNum<[13]>;
def R14 : Ri<14, "r14">, DwarfRegNum<[14]>;
def R15 : Ri<15, "r15">, DwarfRegNum<[15]>;
def R16 : Ri<16, "r16">, DwarfRegNum<[16]>;

def SP : Ri< 1, "SP">, DwarfRegNum<[17]>;

// FIXME: the register order should be defined in terms of the preferred
// allocation order...
def IntRegs : RegisterClass<"URCL", [i32], 32,
                            (sequence "R%u", 0, 16)>;

// GPR argument registers.
def GPROutgoingArg : RegisterClass<"URCL", [i32], 32, (sequence "R%u", 1, 6)>;
def GPRIncomingArg : RegisterClass<"URCL", [i32], 32, (sequence "R%u", 1, 6)>;

