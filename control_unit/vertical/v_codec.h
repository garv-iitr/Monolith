#ifndef RISC201_CU_V_CODEC_H
#define RISC201_CU_V_CODEC_H

#include "vertical/v_decoder.h"

namespace risc201::cu {

struct VerticalCodec {
  static DecodedMicro decode(const VerticalMicroWord& w) { return VerticalDecoder::decode(w); }
};

}

#endif
