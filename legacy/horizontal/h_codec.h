// Fast direct-wire decoder for horizontal micro-words with zero combinational gate latency.
#ifndef RISC201_CU_H_CODEC_H
#define RISC201_CU_H_CODEC_H

#include "common/microinstruction.h"

namespace risc201::cu {

// Codec for horizontal micro-words: extracts pre-decoded control lines with zero combinational gate delay
struct HorizontalCodec {
  // Pass-through decoder copying control signals directly and sign-extending the 32-bit immediate
  // signals are already in the word here, only the imm needs sign extending
  static DecodedMicro decode(const HorizontalMicroWord& w) {
    DecodedMicro d;
    d.sig = w.sig;
    d.muimm = static_cast<Word>(static_cast<Sword>(w.imm));
    return d;
  }
};

}

#endif
