#ifndef RISC201_CU_V_CODEC_H
#define RISC201_CU_V_CODEC_H

// Vertical codec: materialise a control word by running it through the
// micro-decoder.  Plugs into the shared MicroControlUnit template.

#include "vertical/v_decoder.h"

namespace risc201::cu {

struct VerticalCodec {
  static DecodedMicro decode(const VerticalMicroWord& w) { return VerticalDecoder::decode(w); }
};

}  // namespace risc201::cu

#endif  // RISC201_CU_V_CODEC_H
