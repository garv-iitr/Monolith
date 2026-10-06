#ifndef RISC201_CU_MICROINSTRUCTION_H
#define RISC201_CU_MICROINSTRUCTION_H

#include <cstdint>

#include "common/control_signals.h"
#include "isa_spec/isa.h"

namespace risc201::cu {

struct HorizontalMicroWord {
  ControlSignals sig;
  std::int32_t  imm = 0;
  std::uint16_t target = 0;
  MicroOp       op = MicroOp::MMOV;
};

// op and stage are just sim metadata, they arent part of the encoded word
struct VerticalMicroWord {
  MicroOp       op = MicroOp::MMOV;
  InternalReg   src = InternalReg::NONE;
  InternalReg   dest = InternalReg::NONE;
  std::int32_t  imm = 0;
  std::uint16_t target = 0;
  std::uint16_t args = 0;
  StageClass    stage = StageClass::EXECUTE;
};

struct DecodedMicro {
  ControlSignals sig;
  Word muimm = 0;
};

constexpr int kVerticalOpBits     = 3;
constexpr int kVerticalRegBits    = 5;
constexpr int kVerticalImmBits    = 12;
constexpr int kVerticalTargetBits = 10;
constexpr int kVerticalArgsBits   = 10;
constexpr int kVerticalWordBits   = kVerticalOpBits + 2 * kVerticalRegBits +
                                    kVerticalImmBits + kVerticalTargetBits + kVerticalArgsBits;

constexpr int kHorizontalWordBits = 33 + kVerticalImmBits + kVerticalTargetBits + kVerticalArgsBits;

}

#endif
