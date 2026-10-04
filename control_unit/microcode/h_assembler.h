#ifndef RISC201_CU_H_ASSEMBLER_H
#define RISC201_CU_H_ASSEMBLER_H

#include "common/control_memory.h"
#include "common/microinstruction.h"
#include "microcode/microcode_program.h"

namespace risc201::cu {

// Translate the symbolic program into a horizontal control memory: every
// microinstruction carries its control signals literally, with no decode stage.
ControlMemory<HorizontalMicroWord> assemble_horizontal(const Program& program);

}  // namespace risc201::cu

#endif  // RISC201_CU_H_ASSEMBLER_H
