#include <cstdint>

#include "isa_spec/isa.h"
#include "simulator_core/core/decoder.h"
#include "tests/test_framework.h"

namespace {

Word make_r(uint8_t op, uint8_t rd, uint8_t rs1, uint8_t rs2) {
  return (static_cast<Word>(op) << 27) | (static_cast<Word>(rd) << 22) |
         (static_cast<Word>(rs1) << 18) | (static_cast<Word>(rs2) << 14);
}

Word make_i(uint8_t op, uint8_t rd, uint8_t rs1, uint8_t mod, uint16_t imm) {
  return (static_cast<Word>(op) << 27) | (1u << 26) |
         (static_cast<Word>(rd) << 22) | (static_cast<Word>(rs1) << 18) |
         (static_cast<Word>(mod) << 16) | static_cast<Word>(imm);
}

Word make_b(uint8_t op, int32_t off) {
  return (static_cast<Word>(op) << 27) | (static_cast<Word>(off) & 0x7FFFFFFu);
}

}  // namespace

RISC201_TEST(decoder_sub_immediate) {
  const DecodedInstruction d = decode(0x0C480003u);
  CHECK(d.opcode == OP_SUB);
  CHECK(d.format == Format::IMMEDIATE);
  CHECK(d.is_immediate);
  CHECK(d.rd == 1);
  CHECK(d.rs1 == 2);
  CHECK(d.mod == 0);
  CHECK(d.imm16 == 3);
  CHECK(d.immediate == 3u);
}

RISC201_TEST(decoder_st_immediate) {
  const DecodedInstruction d = decode(0x7E080014u);
  CHECK(d.opcode == OP_ST);
  CHECK(d.format == Format::IMMEDIATE);
  CHECK(d.is_immediate);
  CHECK(d.rd == 8);
  CHECK(d.rs1 == 2);
  CHECK(d.immediate == 20u);
}

RISC201_TEST(decoder_register_format) {
  const DecodedInstruction d = decode(make_r(OP_ADD, 5, 6, 7));
  CHECK(d.opcode == OP_ADD);
  CHECK(d.format == Format::REGISTER);
  CHECK(!d.is_immediate);
  CHECK(d.rd == 5);
  CHECK(d.rs1 == 6);
  CHECK(d.rs2 == 7);
}

RISC201_TEST(decoder_branch_format) {
  const DecodedInstruction d = decode(make_b(OP_B, -1));
  CHECK(d.opcode == OP_B);
  CHECK(d.format == Format::BRANCH);
  CHECK(d.branch_offset == -1);
}

RISC201_TEST(decoder_zero_addr) {
  const DecodedInstruction nop = decode(static_cast<Word>(OP_NOP) << 27);
  CHECK(nop.opcode == OP_NOP);
  CHECK(nop.format == Format::ZERO_ADDR);

  const DecodedInstruction ret = decode(static_cast<Word>(OP_RET) << 27);
  CHECK(ret.opcode == OP_RET);
  CHECK(ret.format == Format::ZERO_ADDR);
}

RISC201_TEST(decoder_immediate_modifiers) {
  const DecodedInstruction def = decode(make_i(OP_MOV, 0, 0, 0, 0xFFFE));
  CHECK(def.mod == 0);
  CHECK(def.imm16 == 0xFFFE);
  CHECK(def.immediate == static_cast<Word>(-2));

  const DecodedInstruction uns = decode(make_i(OP_MOV, 0, 0, 1, 0xFEAB));
  CHECK(uns.mod == 1);
  CHECK(uns.immediate == 0xFEABu);

  const DecodedInstruction high = decode(make_i(OP_MOV, 0, 0, 2, 0xFEAB));
  CHECK(high.mod == 2);
  CHECK(high.immediate == (static_cast<Word>(0xFEABu) << 16));
}

RISC201_TEST(decoder_ld_and_cmp) {
  const DecodedInstruction ld = decode(make_i(OP_LD, 1, 2, 0, 12));
  CHECK(ld.opcode == OP_LD);
  CHECK(ld.format == Format::IMMEDIATE);
  CHECK(ld.rd == 1);
  CHECK(ld.rs1 == 2);
  CHECK(ld.immediate == 12u);

  const DecodedInstruction cmp_r = decode(make_r(OP_CMP, 0, 1, 2));
  CHECK(cmp_r.opcode == OP_CMP);
  CHECK(cmp_r.format == Format::REGISTER);
  CHECK(cmp_r.rs1 == 1);
  CHECK(cmp_r.rs2 == 2);

  const DecodedInstruction cmp_i = decode(make_i(OP_CMP, 0, 1, 0, 9));
  CHECK(cmp_i.opcode == OP_CMP);
  CHECK(cmp_i.is_immediate);
  CHECK(cmp_i.rs1 == 1);
  CHECK(cmp_i.immediate == 9u);
}
