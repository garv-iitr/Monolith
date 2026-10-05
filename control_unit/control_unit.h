#ifndef RISC201_CONTROL_UNIT_H
#define RISC201_CONTROL_UNIT_H

// ============================================================================
// Public facade of the RISC201 control unit.
//
//   * HorizontalControlUnit -- control signals embedded in the control word.
//   * VerticalControlUnit   -- encoded control word + micro-decoder.
//
// Both share the microprogram (microcode/microcode_program.h), the datapath
// and the execute unit, and both can be run under the 4-stage or the 6-stage
// layout (StageLayout::FOUR_STAGE / SIX_STAGE).
// ============================================================================

#include "common/micro_control_unit.h"
#include "horizontal/h_codec.h"
#include "vertical/v_codec.h"
#include "microcode/h_assembler.h"
#include "microcode/v_assembler.h"

namespace risc201::cu {

using HorizontalControlUnit = MicroControlUnit<HorizontalMicroWord, HorizontalCodec>;
using VerticalControlUnit   = MicroControlUnit<VerticalMicroWord, VerticalCodec>;

}  // namespace risc201::cu

#endif  // RISC201_CONTROL_UNIT_H
