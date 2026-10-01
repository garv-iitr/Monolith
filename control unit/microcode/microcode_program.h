#ifndef RISC201_CU_MICROCODE_PROGRAM_H
#define RISC201_CU_MICROCODE_PROGRAM_H

// ============================================================================
// The RISC201 microprogram: one routine per opcode, authored in the symbolic
// micro-assembly language.  This single table feeds both control memories.
// ============================================================================

#include <utility>
#include <vector>

#include "isa_spec/isa.h"
#include "microcode/micro_asm.h"

namespace risc201::cu {

struct ProgramEntry {
  Opcode opcode;
  micro::Routine routine;
};

using Program = std::vector<ProgramEntry>;

// Build the complete RISC201 microprogram (all 37 opcodes).
Program build_program();

}  // namespace risc201::cu

#endif  // RISC201_CU_MICROCODE_PROGRAM_H
