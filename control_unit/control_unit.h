// Major use: Primary entry-point header exposing typed aliases for Horizontal and Vertical microprogrammed Control Units.
#ifndef RISC201_CONTROL_UNIT_H
#define RISC201_CONTROL_UNIT_H

#include "common/micro_control_unit.h"
#include "horizontal/h_codec.h"
#include "vertical/v_codec.h"
#include "microcode/h_assembler.h"
#include "microcode/v_assembler.h"

namespace risc201::cu {

// Instantiations of the generic MicroControlUnit template for both ROM micro-word representations:
// - HorizontalControlUnit: Wide, uncompressed micro-words for zero-latency direct-wire control signal decoding.
// - VerticalControlUnit: Compact, encoded micro-words requiring a combinational decoder stage.
// same program underneath, just two ways of encoding the word
using HorizontalControlUnit = MicroControlUnit<HorizontalMicroWord, HorizontalCodec>;
using VerticalControlUnit   = MicroControlUnit<VerticalMicroWord, VerticalCodec>;

}

#endif
