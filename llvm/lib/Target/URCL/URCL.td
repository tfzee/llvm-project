include "llvm/Target/Target.td"
include "llvm/TableGen/SearchableTable.td"

//===----------------------------------------------------------------------===//
// Register File, Calling Conv, Instruction Descriptions
//===----------------------------------------------------------------------===//

include "URCLRegisterInfo.td"
include "URCLCallingConv.td"
include "URCLInstrInfo.td"

defm : RemapAllTargetPseudoPointerOperands<urcl_ptr_rc>;

def URCLInstrInfo : InstrInfo;

def URCLAsmParser : AsmParser {
  let ShouldEmitMatchRegisterAltName = true;
  let AllowDuplicateRegisterNames = true;
}

//===----------------------------------------------------------------------===//
// Declare the target which we are implementing
//===----------------------------------------------------------------------===//

def URCLAsmWriter : AsmWriter {
  string AsmWriterClassName  = "InstPrinter";
  int PassSubtarget = 1;
  int Variant = 0;
}

def URCL : Target {
  // Pull in Instruction Info:
  let InstructionSet = URCLInstrInfo;
  // let AssemblyParsers  = [URCLAsmParser];
  // let AssemblyParserVariants = [URCLAsmParserVariant];
  let AssemblyWriters = [URCLAsmWriter];
  let AllowRegisterRenaming = 1;
}
