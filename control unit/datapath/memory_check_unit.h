#ifndef RISC201_CU_MEMORY_CHECK_UNIT_H
#define RISC201_CU_MEMORY_CHECK_UNIT_H

// ============================================================================
// Memory-check unit (RISC201 6-stage extension).
//
// The 6-stage pipeline inserts a dedicated stage between EX and MA.  This unit
// consumes the effective address produced by the ALU and decides whether the
// referenced location is actually present: word-aligned and inside the
// physical array.  The result is latched in memChkStatus, which the memory
// stage (and the BNDCHK microcode) branch on.
//
// It is purely a checker -- it never touches memory -- so it can be bypassed
// entirely in the 4-stage layout, where the check is folded into MA.
// ============================================================================

#include <cstddef>

#include "common/control_signals.h"
#include "common/internal_registers.h"
#include "isa_spec/isa.h"
#include "simulator_core/core/exception.h"

namespace risc201::cu {

struct MemoryCheckUnit {
  bool enabled = true;
  bool trap_on_fail = true;

  static bool present(Word byte_addr, std::size_t words) {
    if (byte_addr % 4 != 0) return false;
    return static_cast<std::size_t>(byte_addr / 4) < words;
  }

  void tick(const InternalRegisters& cur, const ControlSignals& sig,
            InternalRegisters& next, std::size_t words) const {
    (void)cur;
    if (sig.memchk_op != MemChkOp::CHECK) return;
    next.memchk_status = present(next.memchk_addr, words);
    if (!next.memchk_status && trap_on_fail) {
      throw RuntimeException{ExceptionCode::BAD_MEM_ACCESS,
                             static_cast<std::uint64_t>(next.memchk_addr)};
    }
  }
};

}  // namespace risc201::cu

#endif  // RISC201_CU_MEMORY_CHECK_UNIT_H
