#ifndef LLVM_LIB_TARGET_URCL_URCLTARGETMACHINE_H
#define LLVM_LIB_TARGET_URCL_URCLTARGETMACHINE_H

#include "URCLInstrInfo.h"
#include "URCLSubtarget.h"
#include "llvm/CodeGen/CodeGenTargetMachineImpl.h"
#include "llvm/CodeGen/TargetFrameLowering.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/CodeGen/TargetRegisterInfo.h"
#include "llvm/CodeGen/TargetSubtargetInfo.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/Target/TargetMachine.h"
#include <optional>

namespace llvm {

class Module;

class URCLTargetMachine : public CodeGenTargetMachineImpl {
  std::unique_ptr<TargetLoweringObjectFile> TLOF;
  mutable StringMap<std::unique_ptr<URCLSubtarget>> SubtargetMap;

public:
  URCLTargetMachine(const Target &T, const Triple &TT, StringRef CPU,
                    StringRef FS, const TargetOptions &Options,
                    std::optional<Reloc::Model> RM,
                    std::optional<CodeModel::Model> CM, CodeGenOptLevel OL,
                    bool JIT);
  ~URCLTargetMachine() override;

  const URCLSubtarget *getSubtargetImpl(const Function &F) const override;

  // Pass Pipeline Configuration
  TargetPassConfig *createPassConfig(PassManagerBase &PM) override;
  TargetLoweringObjectFile *getObjFileLowering() const override {
    return TLOF.get();
  }
  // TargetTransformInfo getTargetTransformInfo(const Function &F) const override;

  // MachineFunctionInfo *
  // createMachineFunctionInfo(BumpPtrAllocator &Allocator, const Function &F,
  //                           const TargetSubtargetInfo *STI) const override;
};

} // end namespace llvm

#endif
