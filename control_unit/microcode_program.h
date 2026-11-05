// Major use: Declares program routine structures and the factory building microprograms for all 34 RISC201 opcodes.
#ifndef RISC201_CU_MICROCODE_PROGRAM_H
#define RISC201_CU_MICROCODE_PROGRAM_H

#include <utility>
#include <vector>

#include "control_unit/cu_types.h"
#include "isa_spec/isa.h"
#include "control_unit/micro_asm.h"

namespace risc201::cu {

// Associates an architectural ISA Opcode with its corresponding microcode execution subroutine
struct ProgramEntry {
  Opcode opcode;          // Architectural instruction opcode
  micro::Routine routine; // Sequence of microinstructions executing this opcode
};

// Represents the complete microcode specification for all supported instructions
using Program = std::vector<ProgramEntry>;

// Constructs and returns the full microprogram suite for all 34 opcodes, specialized for the given pipeline layout
Program build_program(StageLayout layout = StageLayout::SIX_STAGE);

}

#endif
