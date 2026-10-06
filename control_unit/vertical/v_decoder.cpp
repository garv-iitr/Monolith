#include "vertical/v_decoder.h"

#include "microcode/micro_asm.h"

namespace risc201::cu {

DecodedMicro VerticalDecoder::decode(const VerticalMicroWord& w) {
  // signed 12 bit imm, sign extend from bit 11
  const std::int32_t raw_imm = w.imm & 0xFFF;
  const std::int32_t imm = (raw_imm & 0x800) ? (raw_imm - 0x1000) : raw_imm;

  DecodedMicro d;
  micro::micro_semantics(w.op, w.src, w.dest, imm, static_cast<std::uint16_t>(w.target & 0x3FF),
                         static_cast<std::uint16_t>(w.args & 0x3FF), w.stage, d.sig, d.muimm);
  return d;
}

}
