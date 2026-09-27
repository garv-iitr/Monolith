#pragma once

#include <cstdint>

#include "isa_spec/isa.h"

struct DecodedInstruction {
  uint8_t opcode = 0;
  Format  format = Format::REGISTER;
  bool    is_immediate = false;
  uint8_t rd = 0;
  uint8_t rs1 = 0;
  uint8_t rs2 = 0;
  uint8_t mod = 0;
  uint16_t imm16 = 0;
  Word    immediate = 0;
  int32_t branch_offset = 0;
};

Word expand_immediate(uint8_t mod, uint16_t raw16);

DecodedInstruction decode(Word instr);
