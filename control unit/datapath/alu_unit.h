#ifndef RISC201_CU_ALU_UNIT_H
#define RISC201_CU_ALU_UNIT_H

// ============================================================================
// ALU unit (Sarangi, sec. 9.5.4).
//
// Two input registers A and B feed the arithmetic core; the result lands in
// aluResult and, on a compare (or a flag-setting op), the Z/N/C/V flags are
// committed.  The operation is selected by the ALU operation code carried in
// the micro-instruction args.
// ============================================================================

#include "common/control_signals.h"
#include "common/internal_registers.h"
#include "isa_spec/isa.h"

namespace risc201::cu {

struct AluResult {
  Word value = 0;
  bool zero = false;
  bool negative = false;
  bool carry = false;
  bool overflow = false;
};

struct AluUnit {
  // Pure arithmetic/logic core (1 cycle).
  static AluResult execute(AluOp op, Word a, Word b);

  void tick(const InternalRegisters& cur, const ControlSignals& sig, InternalRegisters& next) const {
    if (sig.alu_op == AluOp::NOP) return;

    const AluResult r = execute(sig.alu_op, cur.A, cur.B);
    next.alu_result = r.value;

    if (sig.set_flags) {
      next.flags.Z = r.zero;
      next.flags.N = r.negative;
      next.flags.C = r.carry;
      next.flags.V = r.overflow;
      next.flags_E = r.zero;
      next.flags_GT = !r.negative && !r.zero;
    }
  }
};

}  // namespace risc201::cu

#endif  // RISC201_CU_ALU_UNIT_H
