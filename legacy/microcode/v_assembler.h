// Major use: Assembler interface compiling microcode programs into compressed vertical ControlMemory ROM images.
#ifndef RISC201_CU_V_ASSEMBLER_H
#define RISC201_CU_V_ASSEMBLER_H

#include "common/control_memory.h"
#include "common/microinstruction.h"
#include "microcode/microcode_program.h"

namespace risc201::cu {

// Compiles and links microcode routines into a compressed 45-bit vertical Control Memory ROM image
ControlMemory<VerticalMicroWord> assemble_vertical(const Program& program);

}

#endif
