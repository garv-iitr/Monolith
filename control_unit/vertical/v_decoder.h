#ifndef RISC201_CU_V_DECODER_H
#define RISC201_CU_V_DECODER_H

#include "common/control_signals.h"
#include "common/microinstruction.h"

namespace risc201::cu {

// this is the extra hw that horizontal microprogramming dosnt need
struct VerticalDecoder {
  static DecodedMicro decode(const VerticalMicroWord& w);
};

}

#endif
