// Major use: Bridge interface routing vertical micro-words into the hardware vertical micro-decoder.
#ifndef RISC201_CU_V_CODEC_H
#define RISC201_CU_V_CODEC_H

#include "vertical/v_decoder.h"

namespace risc201::cu {

// Codec adapter delegating decoding of compressed vertical micro-words to VerticalDecoder
struct VerticalCodec {
  // Invokes combinational decoder to expand a 45-bit vertical word into active control signals
  static DecodedMicro decode(const VerticalMicroWord& w) { return VerticalDecoder::decode(w); }
};

}

#endif
