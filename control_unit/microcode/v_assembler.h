#ifndef RISC201_CU_V_ASSEMBLER_H
#define RISC201_CU_V_ASSEMBLER_H

#include "common/control_memory.h"
#include "common/microinstruction.h"
#include "microcode/microcode_program.h"

namespace risc201::cu {

// Translate the symbolic program into a vertical control memory: each
// microinstruction is stored as (type, src, dest, imm, target, args) and is
// decoded into control signals at run time by the micro-decoder.
ControlMemory<VerticalMicroWord> assemble_vertical(const Program& program);

}  // namespace risc201::cu

#endif  // RISC201_CU_V_ASSEMBLER_H
