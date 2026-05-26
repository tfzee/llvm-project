// #include "MCTargetDesc/URCLFixupKinds.h"
#include "MCTargetDesc/URCLMCTargetDesc.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/MC/MCAsmBackend.h"
#include "llvm/MC/MCELFObjectWriter.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCObjectWriter.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/MCValue.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/EndianStream.h"
#include <cassert>

using namespace llvm;

namespace {
class URCLAsmBackend : public MCAsmBackend {
public:
  URCLAsmBackend(const MCSubtargetInfo &STI)
      : MCAsmBackend(STI.getTargetTriple().isLittleEndian()
                         ? llvm::endianness::little
                         : llvm::endianness::big) {}

  // std::optional<MCFixupKind> getFixupKind(StringRef Name) const override;
  // MCFixupKindInfo getFixupKindInfo(MCFixupKind Kind) const override;
  void applyFixup(const MCFragment &, const MCFixup &, const MCValue &Target,
                  uint8_t *Data, uint64_t Value, bool IsResolved) override;

  bool writeNopData(raw_ostream &OS, uint64_t Count,
                    const MCSubtargetInfo *STI) const override {

    // If the count is not 4-byte aligned, we must be writing data into the
    // text section (otherwise we have unaligned instructions, and thus have
    // far bigger problems), so just write zeros instead.
    OS.write_zeros(Count % 4);

    uint64_t NumNops = Count / 4;
    for (uint64_t i = 0; i != NumNops; ++i)
      support::endian::write<uint32_t>(OS, 0x01000000, Endian);

    return true;
  }
};

class ELFURCLAsmBackend : public URCLAsmBackend {
  Triple::OSType OSType;

public:
  ELFURCLAsmBackend(const MCSubtargetInfo &STI, Triple::OSType OSType)
      : URCLAsmBackend(STI), OSType(OSType) {}

  std::unique_ptr<MCObjectTargetWriter>
  createObjectTargetWriter() const override {
    uint8_t OSABI = MCELFObjectTargetWriter::getOSABI(OSType);
    return createURCLELFObjectWriter(OSABI);
  }
};
} // end anonymous namespace



void URCLAsmBackend::applyFixup(const MCFragment &F, const MCFixup &Fixup,
                                 const MCValue &Target, uint8_t *Data,
                                 uint64_t Value, bool IsResolved) {
  maybeAddReloc(F, Fixup, Target, Value, IsResolved);
  if (!IsResolved)
    return;
  // Value = adjustFixupValue(Fixup.getKind(), Value);

  // unsigned NumBytes = getFixupKindNumBytes(Fixup.getKind());
  // // For each byte of the fragment that the fixup touches, mask in the
  // // bits from the fixup value.
  // for (unsigned i = 0; i != NumBytes; ++i) {
  //   unsigned Idx = Endian == llvm::endianness::little ? i : (NumBytes - 1) - i;
  //   Data[Idx] |= uint8_t((Value >> (i * 8)) & 0xff);
  // }
  assert(false);
}



MCAsmBackend *llvm::createURCLAsmBackend(const Target &T,
                                         const MCSubtargetInfo &STI,
                                         const MCRegisterInfo &MRI,
                                         const MCTargetOptions &Options) {
  return new ELFURCLAsmBackend(STI, STI.getTargetTriple().getOS());
}
