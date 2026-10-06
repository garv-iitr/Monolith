#ifndef RISC201_CU_H_CODEC_H
#define RISC201_CU_H_CODEC_H

#include "common/microinstruction.h"

namespace risc201::cu {

struct HorizontalCodec {
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
