#ifndef RISC201_CU_MICROCODE_PROGRAM_H
#define RISC201_CU_MICROCODE_PROGRAM_H

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

Program build_program(StageLayout layout = StageLayout::SIX_STAGE);

}

#endif
