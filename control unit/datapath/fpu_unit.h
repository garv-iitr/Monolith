#ifndef RISC201_CU_FPU_UNIT_H
#define RISC201_CU_FPU_UNIT_H

// ============================================================================
// Floating-point unit.
//
// Structurally identical to the register file (fpSrc / fpData / fpVal access
// the 16 FPRs) with an arithmetic core on top (fpA, fpB -> fpuResult).  A
// float compare commits the float condition code and mirrors Z / N into the
// architectural flags so the branch microcode can reuse flags.E / flags.GT.
// ============================================================================

#include <array>
#include <cstdint>

#include "common/control_signals.h"
#include "common/internal_registers.h"
#include "isa_spec/isa.h"

namespace risc201::cu {

struct FpuUnit {
  std::array<Word, NUM_FLOAT_REGISTERS> fprs{};

  void reset() { fprs.fill(Word{0}); }

  void tick(const InternalRegisters& cur, const ControlSignals& sig, InternalRegisters& next);

  static Word pack(float f);
  static float unpack(Word w);
};

}  // namespace risc201::cu

#endif  // RISC201_CU_FPU_UNIT_H
