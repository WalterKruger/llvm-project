//===- llvm/CodeGen/MachineBlockHashInfo.cpp---------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Compute the hashes of basic blocks.
//
//===----------------------------------------------------------------------===//

#include "llvm/CodeGen/MachineBlockHashInfo.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/InitializePasses.h"
#include "llvm/Target/TargetMachine.h"

using namespace llvm;

template <typename T>
std::enable_if_t<is_integral_or_enum<T>::value, uint64_t>
hashValue(const T &Val) {
  return static_cast<uint64_t>(Val);
}

static uint64_t hashValue(StringRef S) {
  uint64_t Hash = 0;
  for (char C : S)
    Hash = hashing::detail::hash_16_bytes(Hash, C);
  return Hash;
}

template <typename T> uint64_t hashValue(ArrayRef<T> A) {
  uint64_t Hash = 0;
  for (const auto &C : A)
    Hash = hashing::detail::hash_16_bytes(Hash, C);
  return Hash;
}

static uint64_t hashValue(const APInt &I) {
  return hashValue(ArrayRef(I.getRawData(), I.getNumWords()));
}

template <typename T> static uint64_t ignoreValue(const T *) { return 1; }

static uint64_t hashCombine() { return 0; }

template <typename T, typename... Ts>
uint64_t hashCombine(const T &Hash, const Ts &...Args) {
  return hashing::detail::hash_16_bytes(hashValue(Hash), hashCombine(Args...));
}

// Similar to hash_code llvm::hash_value(const MachineOperand &MO) but stable.
static uint64_t hashOperand(const MachineOperand &MO) {
  switch (MO.getType()) {
  case MachineOperand::MO_Register:
    // Register operands don't have target flags.
    return hashCombine(MO.getType(), MO.getReg().id(), MO.getSubReg(),
                       MO.isDef());
  case MachineOperand::MO_Immediate:
    return hashCombine(MO.getType(), MO.getTargetFlags(), MO.getImm());
  case MachineOperand::MO_CImmediate:
    return hashCombine(MO.getType(), MO.getTargetFlags(),
                       MO.getCImm()->getValue());
  case MachineOperand::MO_FPImmediate:
    return hashCombine(MO.getType(), MO.getTargetFlags(),
                       MO.getFPImm()->getValue().bitcastToAPInt());
  case MachineOperand::MO_MachineBasicBlock:
    return hashCombine(MO.getType(), MO.getTargetFlags(),
                       ignoreValue(MO.getMBB()));
  case MachineOperand::MO_FrameIndex:
    return hashCombine(MO.getType(), MO.getTargetFlags(), MO.getIndex());
  case MachineOperand::MO_ConstantPoolIndex:
  case MachineOperand::MO_TargetIndex:
    return hashCombine(MO.getType(), MO.getTargetFlags(), MO.getIndex(),
                       MO.getOffset());
  case MachineOperand::MO_JumpTableIndex:
    return hashCombine(MO.getType(), MO.getTargetFlags(), MO.getIndex());
  case MachineOperand::MO_ExternalSymbol:
    return hashCombine(MO.getType(), MO.getTargetFlags(), MO.getOffset(),
                       StringRef(MO.getSymbolName()));
  case MachineOperand::MO_GlobalAddress:
    return hashCombine(MO.getType(), MO.getTargetFlags(),
                       ignoreValue(MO.getGlobal()), MO.getOffset());
  case MachineOperand::MO_BlockAddress:
    return hashCombine(MO.getType(), MO.getTargetFlags(),
                       ignoreValue(MO.getBlockAddress()), MO.getOffset());
  case MachineOperand::MO_RegisterMask:
  case MachineOperand::MO_RegisterLiveOut:
    return hashCombine(MO.getType(), MO.getTargetFlags(),
                       ignoreValue(MO.getRegMask()));
  case MachineOperand::MO_Metadata:
    return hashCombine(MO.getType(), MO.getTargetFlags(),
                       ignoreValue(MO.getMetadata()));
  case MachineOperand::MO_MCSymbol:
    return hashCombine(MO.getType(), MO.getTargetFlags(),
                       ignoreValue(MO.getMCSymbol()));
  case MachineOperand::MO_DbgInstrRef:
    return hashCombine(MO.getType(), MO.getTargetFlags(),
                       MO.getInstrRefInstrIndex(), MO.getInstrRefOpIndex());
  case MachineOperand::MO_CFIIndex:
    return hashCombine(MO.getType(), MO.getTargetFlags(), MO.getCFIIndex());
  case MachineOperand::MO_IntrinsicID:
    return hashCombine(MO.getType(), MO.getTargetFlags(), MO.getIntrinsicID());
  case MachineOperand::MO_Predicate:
    return hashCombine(MO.getType(), MO.getTargetFlags(), MO.getPredicate());
  case MachineOperand::MO_ShuffleMask:
    return hashCombine(MO.getType(), MO.getTargetFlags(), MO.getShuffleMask());
  case MachineOperand::MO_LaneMask:
    return hashCombine(MO.getType(), MO.getTargetFlags(),
                       MO.getLaneMask().getAsInteger());
  }
  llvm_unreachable("Invalid machine operand type");
}

static uint64_t hashBlock(const MachineBasicBlock &MBB, bool HashOperands) {
  uint64_t Hash = 0;
  for (const MachineInstr &MI : MBB) {
    if (MI.isMetaInstruction() || MI.isTerminator())
      continue;
    Hash = hashing::detail::hash_16_bytes(Hash, MI.getOpcode());
    if (HashOperands) {
      for (unsigned i = 0; i < MI.getNumOperands(); i++) {
        Hash =
            hashing::detail::hash_16_bytes(Hash, hashOperand(MI.getOperand(i)));
      }
    }
  }
  return Hash;
}

/// Fold a 64-bit integer to a 16-bit one.
static uint16_t fold_64_to_16(const uint64_t Value) {
  uint16_t Res = static_cast<uint16_t>(Value);
  Res ^= static_cast<uint16_t>(Value >> 16);
  Res ^= static_cast<uint16_t>(Value >> 32);
  Res ^= static_cast<uint16_t>(Value >> 48);
  return Res;
}

INITIALIZE_PASS(MachineBlockHashInfo, "machine-block-hash",
                "Machine Block Hash Analysis", true, true)

char MachineBlockHashInfo::ID = 0;

MachineBlockHashInfo::MachineBlockHashInfo() : MachineFunctionPass(ID) {}

void MachineBlockHashInfo::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.setPreservesAll();
  MachineFunctionPass::getAnalysisUsage(AU);
}

struct CollectHashInfo {
  uint64_t Offset;
  uint64_t OpcodeHash;
  uint64_t InstrHash;
  uint64_t NeighborHash;
};

bool MachineBlockHashInfo::runOnMachineFunction(MachineFunction &F) {
  DenseMap<const MachineBasicBlock *, CollectHashInfo> HashInfos;
  uint16_t Offset = 0;
  // Initialize hash components
  for (const MachineBasicBlock &MBB : F) {
    auto &HashInfo = HashInfos[&MBB];
    // offset of the machine basic block
    HashInfo.Offset = Offset + MBB.size();
    // Hashing opcodes
    HashInfo.OpcodeHash = hashBlock(MBB, /*HashOperands=*/false);
    // Hash complete instructions
    HashInfo.InstrHash = hashBlock(MBB, /*HashOperands=*/true);
  }

  // Initialize neighbor hash
  for (const MachineBasicBlock &MBB : F) {
    auto &HashInfo = HashInfos[&MBB];
    uint64_t Hash = HashInfo.OpcodeHash;
    // Append hashes of successors
    for (const MachineBasicBlock *SuccMBB : MBB.successors()) {
      uint64_t SuccHash = HashInfos[SuccMBB].OpcodeHash;
      Hash = hashing::detail::hash_16_bytes(Hash, SuccHash);
    }
    // Append hashes of predecessors
    for (const MachineBasicBlock *PredMBB : MBB.predecessors()) {
      uint64_t PredHash = HashInfos[PredMBB].OpcodeHash;
      Hash = hashing::detail::hash_16_bytes(Hash, PredHash);
    }
    HashInfo.NeighborHash = Hash;
  }

  // Assign hashes
  for (const MachineBasicBlock &MBB : F) {
    const auto &HashInfo = HashInfos[&MBB];
    BlendedBlockHash BlendedHash(fold_64_to_16(HashInfo.Offset),
                                 fold_64_to_16(HashInfo.OpcodeHash),
                                 fold_64_to_16(HashInfo.InstrHash),
                                 fold_64_to_16(HashInfo.NeighborHash));
    MBBHashInfo[&MBB] = BlendedHash.combine();
  }

  return false;
}

uint64_t MachineBlockHashInfo::getMBBHash(const MachineBasicBlock &MBB) {
  return MBBHashInfo[&MBB];
}

MachineFunctionPass *llvm::createMachineBlockHashInfoPass() {
  return new MachineBlockHashInfo();
}
