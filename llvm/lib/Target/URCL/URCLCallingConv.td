//===-- URCLCallingConv.td - Calling Conventions URCL ----*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This describes the calling conventions for the URCL architectures.
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// URCL 32-bit.
//===----------------------------------------------------------------------===//

def CC_URCL32 : CallingConv<[
  // // Custom assign SRet to [sp+64].
  // CCIfSRet<CCCustom<"CC_URCL_Assign_SRet">>,
  // f128 arguments are passed indirectly, using i32 pointers.
  CCIfType<[f128], CCPassIndirect<i32>>,
  // i32 f32 arguments get passed in integer registers if there is space.
  CCIfType<[i32, f32], CCAssignToReg<[R1, R2, R3, R4, R5]>>,
  // Alternatively, they are assigned to the stack in 4-byte aligned units.
  CCAssignToStack<4, 4>
]>;

def CC_URCL16 : CallingConv<[
  // // Custom assign SRet to [sp+64].
  // CCIfSRet<CCCustom<"CC_URCL_Assign_SRet">>,
  // f128 arguments are passed indirectly, using i16 pointers.
  CCIfType<[f128], CCPassIndirect<i16>>,
  // i16 f16 arguments get passed in integer registers if there is space.
  CCIfType<[i16, f16], CCAssignToReg<[R1, R2, R3, R4, R5]>>,
  // Alternatively, they are assigned to the stack in 2-byte aligned units.
  CCAssignToStack<2, 2>
]>;

def CC_URCL8 : CallingConv<[
  // // Custom assign SRet to [sp+64].
  // CCIfSRet<CCCustom<"CC_URCL_Assign_SRet">>,
  // f128 arguments are passed indirectly, using i8 pointers.
  CCIfType<[f128], CCPassIndirect<i8>>,
  // i8 f8 arguments get passed in integer registers if there is space.
  CCIfType<[i8], CCAssignToReg<[R1, R2, R3, R4, R5]>>,
  // Alternatively, they are assigned to the stack in 1-byte aligned units.
  CCAssignToStack<1, 1>
]>;


def RetCC_URCL32 : CallingConv<[
  CCIfType<[i32, f32], CCAssignToReg<[R1, R1, R2, R3, R4, R5]>>,
]>;
def RetCC_URCL16 : CallingConv<[
  CCIfType<[i16, f16], CCAssignToReg<[R1, R1, R2, R3, R4, R5]>>,
]>;
def RetCC_URCL8 : CallingConv<[
  CCIfType<[i8], CCAssignToReg<[R1, R1, R2, R3, R4, R5]>>,
]>;


def CSR : CalleeSavedRegs<(add)> {
  let OtherPreserved = (add (sequence "R%u", 6, 16), SP);
}

// def RTCSR : CalleeSavedRegs<(add)> {
//   let OtherPreserved = (add I6, I7);
// }
