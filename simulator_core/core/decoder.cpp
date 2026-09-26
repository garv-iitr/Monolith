#include "simulator_core/core/decoder.h"

Word expand_immediate(uint8_t mod, uint16_t raw16) {
  switch (mod) {
    case 1:
      return static_cast<Word>(raw16);
    case 2:
      return static_cast<Word>(raw16) << 16;
    default:
      return static_cast<Word>(static_cast<Sword>(static_cast<int16_t>(raw16)));
  }
}

DecodedInstruction decode(Word instr) {
  DecodedInstruction d;
  d.opcode = get_opcode(instr);
  d.format = format_of(d.opcode);

  if (d.format == Format::ZERO_ADDR) {
    return d;
  }
  if (d.format == Format::BRANCH) {
    d.branch_offset = get_offset27(instr);
    return d;
  }

  d.is_immediate = get_ibit(instr) != 0;
  d.rd = get_rd(instr);
  d.rs1 = get_rs1(instr);

  if (d.is_immediate) {
    d.format = Format::IMMEDIATE;
    d.mod = get_mod(instr);
    d.imm16 = static_cast<uint16_t>(instr & 0xFFFF);
    d.immediate = expand_immediate(d.mod, d.imm16);
  } else {
    d.format = Format::REGISTER;
    d.rs2 = get_rs2(instr);
  }

  return d;
}
