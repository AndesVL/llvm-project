#include "Mips.h"
#include "MipsInstrInfo.h"
#include "MipsTargetMachine.h"
#include "llvm/CodeGen/MachineDominators.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineLoopInfo.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/Support/CommandLine.h"
#include <llvm/CodeGen/MachineOperand.h>
#include <llvm/IR/DebugLoc.h>
#include "llvm/CodeGen/MachineInstrBundleIterator.h"
#include "MCTargetDesc/MipsBaseInfo.h"


using namespace llvm;

unsigned int uStoreEquivalentOpcode(MachineInstr &MI) {
  switch (MI.getOpcode()) {
    case Mips::CAPSTORE8: return Mips::UCAPSTORE8;
    case Mips::CAPSTORE832: return Mips::UCAPSTORE832;
    case Mips::CAPSTORE16: return Mips::UCAPSTORE16;
    case Mips::CAPSTORE1632: return Mips::UCAPSTORE1632;
    case Mips::CAPSTORE32: return Mips::UCAPSTORE32;
    case Mips::CAPSTORE3264: return Mips::UCAPSTORE3264;
    case Mips::CAPSTORE64: return Mips::UCAPSTORE64;
    default: return 0;
  }
}

// TODO add special rule for the stack stores to avoid the offset

MachineInstr* newUninitializedStore(MachineInstr &OldMi, MachineBasicBlock::iterator I, unsigned int Opc, MachineBasicBlock &MBB) {
  const TargetInstrInfo *TII = MBB.getParent()->getSubtarget().getInstrInfo();
  MachineInstr* MI = BuildMI(MBB, I, DebugLoc(), TII->get(Opc))
                         .add(OldMi.getOperand(3)) // cb     (hardcoded operand mapping, see tablegen)
                         .add(OldMi.getOperand(2)) // offset
                         .add(OldMi.getOperand(0)) // rs
                         .add(OldMi.getOperand(3)); // cb
  return MI;
}

MachineInstr* newStoreOffset(MachineInstr &OldMi, MachineBasicBlock::iterator I,  MachineBasicBlock &MBB) {
  const TargetInstrInfo *TII = MBB.getParent()->getSubtarget().getInstrInfo();
  MachineInstr* MI = BuildMI(MBB, I, DebugLoc(), TII->get(Mips::CIncOffset))
                      .add(OldMi.getOperand(1)) // rt    (hardcoded operand mapping, see tablegen)
                      .add(OldMi.getOperand(3)) // cb
                      .add(OldMi.getOperand(3)); // cb
  return MI;
}

namespace {

class UStorePass : public MachineFunctionPass {
public: 
  static char ID;
  const MipsTargetMachine *TargetMachine = nullptr;
  const MipsInstrInfo *InstrInfo = nullptr;

  UStorePass(): MachineFunctionPass(ID) {};
  UStorePass(const MipsTargetMachine &TM) : MachineFunctionPass(ID) {  
    InstrInfo = TM.getSubtargetImpl()->getInstrInfo();
    TargetMachine = &TM;
  }

  StringRef getPassName() const override { return "UStorePass"; }

  bool runOnMachineFunction(MachineFunction &MF) override {
    bool IsStoreReplaced = false;
    for (MachineBasicBlock &MBB: MF) {
      for (auto I = MBB.begin(); I != MBB.end(); ++I) {
        MachineInstr & MI = *I;
        unsigned int UOpc = uStoreEquivalentOpcode(MI);
        if(UOpc) {
          IsStoreReplaced = true;
          newStoreOffset(MI, I, MBB);
          newUninitializedStore(MI, I, UOpc, MBB);
          I = MBB.erase(MI);
          I--;
        }
      }
    }
    return IsStoreReplaced;
  }
};
} // namespace

namespace llvm {

void initializeUStorePassPass(PassRegistry &);
} // namespace llvm

char UStorePass::ID;
INITIALIZE_PASS_BEGIN(UStorePass, "ustorepass",
                      "Replace stores with uninitialized stores", false,
                      false) // is_cfg_only, is_analysis
INITIALIZE_PASS_END(UStorePass, "ustorepass",
                    "Replace stores with uninitialized stores", false, false)

MachineFunctionPass *llvm::createUStorePass(const MipsTargetMachine &TM) { return (MachineFunctionPass*) new UStorePass(TM); }
