#ifndef RISC201_CONTROL_UNIT_H
#define RISC201_CONTROL_UNIT_H

#include "common/micro_control_unit.h"
#include "horizontal/h_codec.h"
#include "vertical/v_codec.h"
#include "microcode/h_assembler.h"
#include "microcode/v_assembler.h"

namespace risc201::cu {

// same program underneath, just two ways of encoding the word
using HorizontalControlUnit = MicroControlUnit<HorizontalMicroWord, HorizontalCodec>;
using VerticalControlUnit   = MicroControlUnit<VerticalMicroWord, VerticalCodec>;

}

#endif
