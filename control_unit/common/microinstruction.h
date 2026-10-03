#ifndef RISC201_CU_MICROINSTRUCTION_H
#define RISC201_CU_MICROINSTRUCTION_H

// ============================================================================
// The two physical forms of a microinstruction.
//
//   HorizontalMicroWord -- control signals embedded in the word (sec. 9.8.2).
//                          The execute unit drives the datapath straight from
//                          these bits; there is no separate decode stage.
//
//   VerticalMicroWord   -- a compact encoded word (sec. 9.8.1).  The fields
//                          (type, source, destination, immediate, branch
//                          target, args) are decoded into ControlSignals by
//                          the micro-decoder before execution.
//
// Both are produced from the same symbolic microcode (microcode/micro_asm.h),
// so the horizontal and vertical control units differ only in how they store
// and decode a microprogram.
// ============================================================================

#include <cstdint>

#include "common/control_signals.h"
#include "isa_spec/isa.h"

namespace risc201::cu {

struct HorizontalMicroWord {
  ControlSignals sig;
  std::int32_t  imm = 0;     // micro-immediate (muimm), sign-extended
  std::uint16_t target = 0;  // micro-branch target (microPC address)
  MicroOp       op = MicroOp::MMOV;  // simulation metadata (not encoded)
};

struct VerticalMicroWord {
  MicroOp       op = MicroOp::MMOV;
  InternalReg   src = InternalReg::NONE;   // 5-bit register id
  InternalReg   dest = InternalReg::NONE;  // 5-bit register id
  std::int32_t  imm = 0;                   // 12-bit immediate
  std::uint16_t target = 0;                // 10-bit branch target
  std::uint16_t args = 0;                  // 10-bit unit args (unit[3] | op[7])
  StageClass    stage = StageClass::EXECUTE;  // simulation metadata (not encoded)
};

// The result of materialising a microinstruction into control signals.
struct DecodedMicro {
  ControlSignals sig;
  Word muimm = 0;
};

// Widths of the encoded vertical microinstruction (Sarangi, fig. 9.24).
constexpr int kVerticalOpBits     = 3;
constexpr int kVerticalRegBits    = 5;
constexpr int kVerticalImmBits    = 12;
constexpr int kVerticalTargetBits = 10;
constexpr int kVerticalArgsBits   = 10;
constexpr int kVerticalWordBits   = kVerticalOpBits + 2 * kVerticalRegBits +
                                    kVerticalImmBits + kVerticalTargetBits + kVerticalArgsBits;

// Horizontal encoding (Sarangi, fig. 9.26): 33 control signals + 12-bit
// immediate + 10-bit target + 10-bit args.
constexpr int kHorizontalWordBits = 33 + kVerticalImmBits + kVerticalTargetBits + kVerticalArgsBits;

}  // namespace risc201::cu

#endif  // RISC201_CU_MICROINSTRUCTION_H
