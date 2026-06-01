include "llvm/Target/Target.td"
include "llvm/TableGen/SearchableTable.td"


def FeatureSoftFloat : SubtargetFeature<"soft-float", "UseSoftFloat", "true",
                              "Use software emulation for floating point">;

def IsURCL8  : Predicate<"Subtarget->getTargetTriple().getArch() == Triple::urcl8">;
def IsURCL16 : Predicate<"Subtarget->getTargetTriple().getArch() == Triple::urcl16">;
def IsURCL32 : Predicate<"Subtarget->getTargetTriple().getArch() == Triple::urcl32">;

defvar URCL32 = DefaultMode;
def URCL16 : HwMode<[IsURCL16]>;
def URCL8 : HwMode<[IsURCL8]>;




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


                           
def : Proc<"generic",         [FeatureSoftFloat]>;

def URCL : Target {
  // Pull in Instruction Info:
  let InstructionSet = URCLInstrInfo;
  // let AssemblyParsers  = [URCLAsmParser];
  // let AssemblyParserVariants = [URCLAsmParserVariant];
  let AssemblyWriters = [URCLAsmWriter];
  let AllowRegisterRenaming = 1;
}
