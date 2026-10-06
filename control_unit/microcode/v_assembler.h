#ifndef RISC201_CU_V_ASSEMBLER_H
#define RISC201_CU_V_ASSEMBLER_H

#include "common/control_memory.h"
#include "common/microinstruction.h"
#include "microcode/microcode_program.h"

namespace risc201::cu {

ControlMemory<VerticalMicroWord> assemble_vertical(const Program& program);

}

#endif
