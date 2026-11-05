// Major use: Implements 12-bit immediate sign extension and semantic translation for vertical micro-words.
#include "vertical/v_decoder.h"

#include "microcode/micro_asm.h"

namespace risc201::cu {

// Combinational decoding implementation: sign-extends immediate and reconstructs control signals via micro_semantics
DecodedMicro VerticalDecoder::decode(const VerticalMicroWord& w) {
  // Extract 12-bit immediate field and sign-extend from bit 11 to 32 bits
  // signed 12 bit imm, sign extend from bit 11
  const std::int32_t raw_imm = w.imm & 0xFFF;
  const std::int32_t imm = (raw_imm & 0x800) ? (raw_imm - 0x1000) : raw_imm;

  // Synthesize control signals and expanded 32-bit immediate using the microcode semantics rules
  DecodedMicro d;
  micro::micro_semantics(w.op, w.src, w.dest, imm, static_cast<std::uint16_t>(w.target & 0x3FF),
                         static_cast<std::uint16_t>(w.args & 0x3FF), w.stage, d.sig, d.muimm);
  return d;
}

}
