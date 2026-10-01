#ifndef RISC201_CU_V_DECODER_H
#define RISC201_CU_V_DECODER_H

// ============================================================================
// Micro-decoder for the vertically microprogrammed control unit (Sarangi,
// sec. 9.8.1).  It performs exactly the job of the "decode unit" in fig. 9.25:
//   * extracts the fields (type, source, destination, immediate, branch
//     target, args) from the compact microinstruction,
//   * sign-extends the 12-bit immediate,
//   * and generates the control signals by dispatching on the micro-op type.
//
// This is the extra hardware that horizontal microprogramming eliminates.
// ============================================================================

#include "common/control_signals.h"
#include "common/microinstruction.h"

namespace risc201::cu {

struct VerticalDecoder {
  static DecodedMicro decode(const VerticalMicroWord& w);
};

}  // namespace risc201::cu

#endif  // RISC201_CU_V_DECODER_H
