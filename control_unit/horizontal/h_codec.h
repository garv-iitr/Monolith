#ifndef RISC201_CU_H_CODEC_H
#define RISC201_CU_H_CODEC_H

// ============================================================================
// Horizontal codec: the "materialise" path of a horizontally microprogrammed
// control unit.  The control signals are already in the word, so this is just
// a wire-through (plus sign-extending the micro-immediate).  There is no
// micro-decoder -- that is the whole point of horizontal microprogramming.
// ============================================================================

#include "common/microinstruction.h"

namespace risc201::cu {

struct HorizontalCodec {
  static DecodedMicro decode(const HorizontalMicroWord& w) {
    DecodedMicro d;
    d.sig = w.sig;
    d.muimm = static_cast<Word>(static_cast<Sword>(w.imm));
    return d;
  }
};

}  // namespace risc201::cu

#endif  // RISC201_CU_H_CODEC_H
