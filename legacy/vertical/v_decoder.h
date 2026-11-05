// Major use: Hardware vertical microinstruction decoder translating compact 45-bit micro-words into discrete control signals.
#ifndef RISC201_CU_V_DECODER_H
#define RISC201_CU_V_DECODER_H

#include "common/control_signals.h"
#include "common/microinstruction.h"

namespace risc201::cu {

// Emulated combinational gate-level decoder required for vertical microprogramming
// this is the extra hw that horizontal microprogramming dosnt need
struct VerticalDecoder {
  // Decodes a 45-bit compressed vertical word into full control signals and sign-extended immediate
  static DecodedMicro decode(const VerticalMicroWord& w);
};

}

#endif
