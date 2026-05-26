add_llvm_component_group(URCL)

set(LLVM_TARGET_DEFINITIONS URCL.td)

# tablegen(LLVM URCLGenAsmMatcher.inc -gen-asm-matcher)
tablegen(LLVM URCLGenAsmWriter.inc -gen-asm-writer)
tablegen(LLVM URCLGenCallingConv.inc -gen-callingconv)
tablegen(LLVM URCLGenDAGISel.inc -gen-dag-isel)
# tablegen(LLVM URCLGenDisassemblerTables.inc -gen-disassembler)
tablegen(LLVM URCLGenInstrInfo.inc -gen-instr-info)
# tablegen(LLVM URCLGenMCCodeEmitter.inc -gen-emitter)
tablegen(LLVM URCLGenRegisterInfo.inc -gen-register-info)
tablegen(LLVM URCLGenSDNodeInfo.inc -gen-sd-node-info -sdnode-namespace=URCLISD)
tablegen(LLVM URCLGenSearchableTables.inc -gen-searchable-tables)
tablegen(LLVM URCLGenSubtargetInfo.inc -gen-subtarget)

add_public_tablegen_target(URCLCommonTableGen)

add_llvm_target(URCLCodeGen
  # DelaySlotFiller.cpp
  # LeonPasses.cpp
  URCLAsmPrinter.cpp
  URCLInstrInfo.cpp
  URCLISelDAGToDAG.cpp
  URCLISelLowering.cpp
  URCLFrameLowering.cpp
  URCLMachineFunctionInfo.cpp
  URCLRegisterInfo.cpp
  URCLSelectionDAGInfo.cpp
  URCLSubtarget.cpp
  URCLTargetMachine.cpp
  URCLTargetObjectFile.cpp
  # SparcTargetTransformInfo.cpp

  LINK_COMPONENTS
  Analysis
  AsmPrinter
  CodeGen
  CodeGenTypes
  Core
  MC
  SelectionDAG
  URCLInfo
  Support
  Target
  TargetParser
  TransformUtils

  ADD_TO_COMPONENT
  URCL  
  )

# add_subdirectory(AsmParser)
# add_subdirectory(Disassembler)
add_subdirectory(MCTargetDesc)
add_subdirectory(TargetInfo)
