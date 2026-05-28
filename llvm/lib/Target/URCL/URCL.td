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

class Proc<string Name, list<SubtargetFeature> Features,
           list<SubtargetFeature> TuneFeatures = []>
 : Processor<Name, NoItineraries, Features, TuneFeatures>;


def FeatureSoftFloat : SubtargetFeature<"soft-float", "UseSoftFloat", "true",
                              "Use software emulation for floating point">;
                           
def : Proc<"generic",         [FeatureSoftFloat]>;

def URCL : Target {
  // Pull in Instruction Info:
  let InstructionSet = URCLInstrInfo;
  // let AssemblyParsers  = [URCLAsmParser];
  // let AssemblyParserVariants = [URCLAsmParserVariant];
  let AssemblyWriters = [URCLAsmWriter];
  let AllowRegisterRenaming = 1;
}
