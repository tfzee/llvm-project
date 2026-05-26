add_llvm_component_library(LLVMURCLDesc
  URCLAsmBackend.cpp
  # URCLELFObjectWriter.cpp
  URCLInstPrinter.cpp
  URCLMCAsmInfo.cpp
  # URCLMCCodeEmitter.cpp
  URCLMCTargetDesc.cpp
  # URCLMCExpr.cpp
  URCLTargetStreamer.cpp

  LINK_COMPONENTS
  MC
  URCLInfo
  Support
  TargetParser

  ADD_TO_COMPONENT
  URCL
  )

