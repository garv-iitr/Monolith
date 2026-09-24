#include <cstdint>

#include "isa_spec/isa.h"
#include "tests/test_framework.h"

RISC201_TEST(isa_width_constants) {
  CHECK(kWordBits == RISC201_WORD_BITS);
  CHECK(kWordBits == 32 || kWordBits == 64);
  CHECK(NUM_REGISTERS == 16);
  CHECK(REG_WIDTH_BITS == kWordBits);
  CHECK(static_cast<int>(sizeof(Word)) * 8 == kWordBits);
}

RISC201_TEST(isa_opcode_values) {
  CHECK(OP_ADD == 0);
  CHECK(OP_SUB == 1);
  CHECK(OP_CMP == 5);
  CHECK(OP_RET == 20);
  CHECK(static_cast<int>(OP_BEQ) == 16);
}

// Worked encodings from the SimpleRISC chapter.
RISC201_TEST(isa_field_layout) {
  const Word sub = 0x0C480003u;  // sub r1, r2, 3
  CHECK(get_opcode(sub) == OP_SUB);
  CHECK(get_ibit(sub) == 1);
  CHECK(get_rd(sub) == 1);
  CHECK(get_rs1(sub) == 2);
  CHECK(get_mod(sub) == 0);

  const Word st = 0x7E080014u;  // st r8, 20[r2]
  CHECK(get_opcode(st) == OP_ST);
  CHECK(get_ibit(st) == 1);
  CHECK(get_rd(st) == 8);
  CHECK(get_rs1(st) == 2);
}

RISC201_TEST(isa_register_format) {
  // add r5, r6, r7 -> op=ADD, I=0, rd=5, rs1=6, rs2=7
  const Word w = (static_cast<Word>(OP_ADD) << 27) | (0u << 26) | (5u << 22) |
                 (6u << 18) | (7u << 14);
  CHECK(get_opcode(w) == OP_ADD);
  CHECK(get_ibit(w) == 0);
  CHECK(get_rd(w) == 5);
  CHECK(get_rs1(w) == 6);
  CHECK(get_rs2(w) == 7);
}

RISC201_TEST(isa_sign_extended_fields) {
  CHECK(get_imm18(0x00000003u) == 3);
  CHECK(get_imm18(0x0003FFFFu) == -1);
  CHECK(get_offset27(0x07FFFFFFu) == -1);

  // `b` with offset -1
  const Word b = (static_cast<Word>(OP_B) << 27) | 0x07FFFFFFu;
  CHECK(get_opcode(b) == OP_B);
  CHECK(get_offset27(b) == -1);
}

RISC201_TEST(isa_format_of) {
  CHECK(format_of(OP_NOP) == Format::ZERO_ADDR);
  CHECK(format_of(OP_RET) == Format::ZERO_ADDR);
  CHECK(format_of(OP_B) == Format::BRANCH);
  CHECK(format_of(OP_BEQ) == Format::BRANCH);
  CHECK(format_of(OP_CALL) == Format::BRANCH);
  CHECK(format_of(OP_ADD) == Format::REGISTER);
  CHECK(format_of(OP_LD) == Format::REGISTER);
}
