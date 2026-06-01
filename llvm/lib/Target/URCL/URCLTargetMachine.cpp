
#include "URCLTargetMachine.h"
#include "TargetInfo/URCLTargetInfo.h"
#include "URCL.h"
#include "URCLMachineFunctionInfo.h"
#include "URCLTargetObjectFile.h"
// #include "URCLTargetTransformInfo.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/CodeGen/TargetPassConfig.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/CodeGen.h"
#include "llvm/Support/Compiler.h"
#include <memory>
#include <optional>
using namespace llvm;

extern "C" LLVM_ABI LLVM_EXTERNAL_VISIBILITY void LLVMInitializeURCLTarget() {
  // Register the target.
  RegisterTargetMachine<URCLTargetMachine> X(getTheURCL8Target());
  RegisterTargetMachine<URCLTargetMachine> Y(getTheURCL16Target());
  RegisterTargetMachine<URCLTargetMachine> Z(getTheURCL32Target());

  PassRegistry &PR = *PassRegistry::getPassRegistry();
  initializeURCLAsmPrinterPass(PR);
  initializeURCLDAGToDAGISelLegacyPass(PR);
}

static Reloc::Model getEffectiveRelocModel(std::optional<Reloc::Model> RM) {
  return RM.value_or(Reloc::Static);
}
static CodeModel::Model
getEffectiveURCLCodeModel(std::optional<CodeModel::Model> CM, Reloc::Model RM,
                          bool Is64Bit, bool JIT) {
  if (CM) {
    if (*CM == CodeModel::Tiny)
      report_fatal_error("Target does not support the tiny CodeModel", false);
    if (*CM == CodeModel::Kernel)
      report_fatal_error("Target does not support the kernel CodeModel", false);
    return *CM;
  }
  if (Is64Bit) {
    if (JIT)
      return CodeModel::Large;
    return RM == Reloc::PIC_ ? CodeModel::Small : CodeModel::Medium;
  }
  return CodeModel::Small;
}

namespace {
/// Sparc Code Generator Pass Configuration Options.
class URCLPassConfig : public TargetPassConfig {
public:
  URCLPassConfig(URCLTargetMachine &TM, PassManagerBase &PM)
      : TargetPassConfig(TM, PM) {}

  URCLTargetMachine &getURCLTargetMachine() const {
    return getTM<URCLTargetMachine>();
  }

  void addIRPasses() override;
  bool addInstSelector() override;
  void addPreEmitPass() override;
};
} // namespace

TargetPassConfig *
URCLTargetMachine::URCLTargetMachine::createPassConfig(PassManagerBase &PM) {
  return new URCLPassConfig(*this, PM);
}

void URCLPassConfig::addIRPasses() {
  addPass(createAtomicExpandLegacyPass());
  TargetPassConfig::addIRPasses();
}

bool URCLPassConfig::addInstSelector() {
  addPass(createURCLISelDag(getURCLTargetMachine()));
  return false;
}

void URCLPassConfig::addPreEmitPass() { addPass(createURCLCleanupPass()); }

URCLTargetMachine::URCLTargetMachine(const Target &T, const Triple &TT,
                                     StringRef CPU, StringRef FS,
                                     const TargetOptions &Options,
                                     std::optional<Reloc::Model> RM,
                                     std::optional<CodeModel::Model> CM,
                                     CodeGenOptLevel OL, bool JIT)
    : CodeGenTargetMachineImpl(
          T, TT.computeDataLayout(), TT, CPU, FS, Options,
          getEffectiveRelocModel(RM),
          getEffectiveURCLCodeModel(CM, getEffectiveRelocModel(RM), false, JIT),
          OL),
      TLOF(std::make_unique<URCLELFTargetObjectFile>()) {

  if (Options.FloatABIType == FloatABI::Default) {
    this->Options.FloatABIType = FloatABI::Soft;
  }
  if (Options.EABIVersion == EABI::Default ||
      Options.EABIVersion == EABI::Unknown) {
    this->Options.EABIVersion = EABI::GNU;
  }

  initAsmInfo();
}

URCLTargetMachine::~URCLTargetMachine() = default;

std::unique_ptr<URCLSubtarget> subtarget;

const URCLSubtarget *
URCLTargetMachine::getSubtargetImpl(const Function &F) const {
  Attribute CPUAttr = F.getFnAttribute("target-cpu");
  Attribute TuneAttr = F.getFnAttribute("tune-cpu");
  Attribute FSAttr = F.getFnAttribute("target-features");

  std::string CPU =
      CPUAttr.isValid() ? CPUAttr.getValueAsString().str() : TargetCPU;
  std::string TuneCPU =
      TuneAttr.isValid() ? TuneAttr.getValueAsString().str() : CPU;
  std::string FS =
      FSAttr.isValid() ? FSAttr.getValueAsString().str() : TargetFS;

  resetTargetOptions(F);
  auto &I = subtarget;
  if (!I) {
    resetTargetOptions(F);
    I = std::make_unique<URCLSubtarget>(CPU, TuneCPU, FS, *this);
  }
  return I.get();
}
