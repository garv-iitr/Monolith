#ifndef RISC201_CU_MICROCODE_PROGRAM_H
#define RISC201_CU_MICROCODE_PROGRAM_H

// ============================================================================
// The RISC201 microprogram: one routine per opcode, authored in the symbolic
// micro-assembly language.  This single table feeds both control memories.
// ============================================================================

#include <utility>
#include <vector>

#include "common/control_signals.h"
#include "isa_spec/isa.h"
#include "microcode/micro_asm.h"

namespace risc201::cu {

struct ProgramEntry {
  Opcode opcode;
  micro::Routine routine;
};

using Program = std::vector<ProgramEntry>;

// Build the RISC201 microprogram (25 active core integer/control opcodes).
Program build_program(StageLayout layout = StageLayout::SIX_STAGE);

}  // namespace risc201::cu

#endif  // RISC201_CU_MICROCODE_PROGRAM_H
