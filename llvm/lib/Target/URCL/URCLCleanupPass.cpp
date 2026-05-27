// #include "URCLMCTargetDesc.h"
#include "MCTargetDesc/URCLMCTargetDesc.h"
#define DEBUG_TYPE "urcl-stack-cleanup"
#include "URCL.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"

using namespace llvm;

namespace {
struct URCLCleanup : public MachineFunctionPass {
  static char ID;
  URCLCleanup() : MachineFunctionPass(ID) {}

  bool runOnMachineFunction(MachineFunction &MF) override {
    bool Changed = false;
    const auto *TII = MF.getSubtarget().getInstrInfo();

    for (MachineBasicBlock &MBB : MF) {
      for (auto MII = MBB.rbegin(), MIE = MBB.rend(); MII != MIE;) {
        MachineInstr &MI = *MII;
        ++MII;

        // cleanup identity copies generated through adds: add r1, r1, 0
        if ((MI.getOpcode() == URCL::ADDri || MI.getOpcode() == URCL::ADDrr) &&
            MI.getOperand(0).isReg() && MI.getOperand(1).isReg() &&
            MI.getOperand(0).getReg() == MI.getOperand(1).getReg() &&
            ((MI.getOperand(2).isImm() && MI.getOperand(2).getImm() == 0) ||
             (MI.getOperand(2).isReg() &&
              MI.getOperand(2).getReg() == URCL::R0))) {
          MI.eraseFromParent();
          Changed = true;
          continue;
        }

        // replace identity copies with explicit move
        if ((MI.getOpcode() == URCL::ADDri || MI.getOpcode() == URCL::ADDrr) &&
            MI.getOperand(0).isReg() && MI.getOperand(1).isReg() &&
            MI.getOperand(0).getReg() != MI.getOperand(1).getReg() &&
            ((MI.getOperand(2).isImm() && MI.getOperand(2).getImm() == 0) ||
             (MI.getOperand(2).isReg() &&
              MI.getOperand(2).getReg() == URCL::R0))) {
          Register DstReg = MI.getOperand(0).getReg();
          Register SrcReg = MI.getOperand(1).getReg();

          bool IsKill = MI.getOperand(1).isKill();
          bool IsRenamableDst = MI.getOperand(0).isRenamable();
          bool IsRenamableSrc = MI.getOperand(1).isRenamable();

          BuildMI(MBB, MI, MI.getDebugLoc(), TII->get(URCL::MOVrr))
              .addReg(DstReg,
                      RegState::Define | (IsRenamableDst ? RegState::Renamable
                                                         : RegState::NoFlags))
              .addReg(SrcReg, (IsKill ? RegState::Kill : RegState::NoFlags) |
                                  (IsRenamableSrc ? RegState::Renamable
                                                  : RegState::NoFlags));
          MI.eraseFromParent();
          Changed = true;
          continue;
        }
        // common pattern of (SP << 2 >> 2) since SP 32bit aligned we can
        // elimniate it
        if (MI.getOpcode() == URCL::BSLri && MI.getOperand(0).isReg() &&
            MI.getOperand(1).isReg() && MI.getOperand(1).getReg() == URCL::SP &&
            MI.getOperand(2).isImm() && MI.getOperand(2).getImm() == 2) {
          Register ShiftedReg = MI.getOperand(0).getReg();
          if (ShiftedReg == URCL::SP)
            continue;
          // find the matching right shift
          MachineBasicBlock::iterator NextNI = std::next(MI.getIterator());
          if (NextNI != MBB.end() && NextNI->getOpcode() == URCL::BSRri) {
            MachineInstr &NextMI = *NextNI;
            // match the right shift
            if (NextMI.getOperand(1).isReg() &&
                NextMI.getOperand(1).getReg() == ShiftedReg &&
                NextMI.getOperand(2).isImm() &&
                NextMI.getOperand(2).getImm() == 2) {
              Register FinalDestReg = NextMI.getOperand(0).getReg();
              if (FinalDestReg != URCL::SP) {
                BuildMI(MBB, MI, MI.getDebugLoc(), TII->get(URCL::MOVrr))
                    .addReg(FinalDestReg, RegState::Define)
                    .addReg(URCL::SP);
              }
              NextMI.eraseFromParent();
              MI.eraseFromParent();

              Changed = true;
              continue;
            }
          }
        }
      }
    }
    return Changed;
  }
};
} // namespace

char URCLCleanup::ID = 0;
FunctionPass *llvm::createURCLCleanupPass() { return new URCLCleanup(); }
