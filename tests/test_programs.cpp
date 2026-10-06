#include "tests/test_programs.h"

namespace risc201::testbench {
namespace {

Word enc_imm(Opcode op, int rd, int rs1, int imm) {
  return (static_cast<Word>(op) << 26) | (1u << 24) |
         (static_cast<Word>(rd & 0xF) << 20) | (static_cast<Word>(rs1 & 0xF) << 16) |
         static_cast<Word>(imm & 0xFFFF);
}
Word enc_reg(Opcode op, int rd, int rs1, int rs2) {
  return (static_cast<Word>(op) << 26) | (static_cast<Word>(rd & 0xF) << 20) |
         (static_cast<Word>(rs1 & 0xF) << 16) | (static_cast<Word>(rs2 & 0xF) << 12);
}
Word enc_mem(Opcode op, int rd, int rs1, int imm) {
  return (static_cast<Word>(op) << 26) | (static_cast<Word>(rd & 0xF) << 20) |
         (static_cast<Word>(rs1 & 0xF) << 16) | static_cast<Word>(imm & 0xFFFF);
}
Word enc_branch(Opcode op, int word_offset) {
  return (static_cast<Word>(op) << 26) | (static_cast<Word>(word_offset) & 0x03FFFFFF);
}
Word enc_halt() { return static_cast<Word>(Opcode::HALT) << 26; }
Word enc_zero(Opcode op) { return static_cast<Word>(op) << 26; }

}

// branch target is relative to pc+4, not the branch itself
TestProgram program_fib() {
  TestProgram p;
  p.name = "recursive fibonacci";
  p.code = {
      enc_imm(Opcode::MOV, REG_SP, 0, 0x300),
      enc_imm(Opcode::MOV, 1, 0, 10),
      enc_branch(Opcode::CALL, 8),
      enc_zero(Opcode::NOP),
      enc_mem(Opcode::ST, 2, 0, 0x100),
      enc_halt(),
      enc_zero(Opcode::NOP),
      enc_zero(Opcode::NOP),
      enc_zero(Opcode::NOP),
      enc_zero(Opcode::NOP),
      enc_zero(Opcode::NOP),
      enc_imm(Opcode::CMP, 0, 1, 1),
      enc_branch(Opcode::BGT, 5),
      enc_branch(Opcode::BNE, 2),
      enc_imm(Opcode::MOV, 2, 0, 1),
      enc_branch(Opcode::B, 1),
      enc_imm(Opcode::MOV, 2, 0, 0),
      enc_zero(Opcode::RET),
      enc_imm(Opcode::SUB, REG_SP, REG_SP, 4),
      enc_mem(Opcode::ST, 1, REG_SP, 0),
      enc_imm(Opcode::SUB, 1, 1, 1),
      enc_branch(Opcode::CALL, -11),
      enc_imm(Opcode::SUB, REG_SP, REG_SP, 4),
      enc_mem(Opcode::ST, 2, REG_SP, 0),
      enc_mem(Opcode::LD, 1, REG_SP, 4),
      enc_imm(Opcode::SUB, 1, 1, 2),
      enc_branch(Opcode::CALL, -16),
      enc_mem(Opcode::LD, 3, REG_SP, 0),
      enc_reg(Opcode::ADD, 2, 2, 3),
      enc_imm(Opcode::ADD, REG_SP, REG_SP, 8),
      enc_zero(Opcode::RET),
  };
  p.expected_mem = {{0x100, 55}};
  p.expected_regs = {{REG_SP, 0x300}};
  return p;
}

TestProgram program_matmul() {
  TestProgram p;
  p.name = "matrix multiply + fpu";
  p.code = {
      enc_imm(Opcode::MOV, 1, 0, 0),
      enc_imm(Opcode::CMP, 0, 1, 2),
      enc_branch(Opcode::BEQ, 36),
      enc_branch(Opcode::BLT, 1),
      enc_branch(Opcode::B, 34),
      enc_imm(Opcode::MOV, 2, 0, 0),
      enc_imm(Opcode::CMP, 0, 2, 2),
      enc_branch(Opcode::BEQ, 29),
      enc_branch(Opcode::BLT, 1),
      enc_branch(Opcode::B, 27),
      enc_reg(Opcode::XOR, 4, 4, 4),
      enc_imm(Opcode::MOV, 3, 0, 0),
      enc_imm(Opcode::CMP, 0, 3, 2),
      enc_branch(Opcode::BEQ, 16),
      enc_branch(Opcode::BLT, 1),
      enc_branch(Opcode::B, 14),
      enc_reg(Opcode::MOV, 5, 0, 1),
      enc_imm(Opcode::MUL, 5, 5, 2),
      enc_reg(Opcode::ADD, 5, 5, 3),
      enc_imm(Opcode::LSL, 5, 5, 2),
      enc_reg(Opcode::MOV, 6, 0, 3),
      enc_imm(Opcode::MUL, 6, 6, 2),
      enc_reg(Opcode::ADD, 6, 6, 2),
      enc_imm(Opcode::LSL, 6, 6, 2),
      enc_mem(Opcode::LD, 7, 5, 0x200),
      enc_mem(Opcode::LD, 8, 6, 0x220),
      enc_reg(Opcode::MUL, 7, 7, 8),
      enc_reg(Opcode::ADD, 4, 4, 7),
      enc_imm(Opcode::ADD, 3, 3, 1),
      enc_branch(Opcode::B, -18),
      enc_reg(Opcode::MOV, 9, 0, 1),
      enc_imm(Opcode::MUL, 9, 9, 2),
      enc_reg(Opcode::ADD, 9, 9, 2),
      enc_imm(Opcode::LSL, 9, 9, 2),
      enc_mem(Opcode::ST, 4, 9, 0x240),
      enc_imm(Opcode::ADD, 2, 2, 1),
      enc_branch(Opcode::B, -31),
      enc_imm(Opcode::ADD, 1, 1, 1),
      enc_branch(Opcode::B, -38),
      enc_mem(Opcode::FLD, 1, 0, 0x260),
      enc_mem(Opcode::FLD, 2, 0, 0x264),
      enc_reg(Opcode::FADD, 3, 1, 2),
      enc_mem(Opcode::FST, 3, 0, 0x270),
      enc_reg(Opcode::FSUB, 4, 1, 2),
      enc_mem(Opcode::FST, 4, 0, 0x274),
      enc_reg(Opcode::FMUL, 5, 1, 2),
      enc_mem(Opcode::FST, 5, 0, 0x278),
      enc_reg(Opcode::FDIV, 6, 1, 2),
      enc_mem(Opcode::FST, 6, 0, 0x27C),
      enc_reg(Opcode::FCMP, 0, 1, 2),
      enc_branch(Opcode::BGT, 5),
      enc_branch(Opcode::BLT, 1),
      enc_branch(Opcode::B, 3),
      enc_imm(Opcode::MOV, 10, 0, 7),
      enc_mem(Opcode::ST, 10, 0, 0x280),
      enc_branch(Opcode::B, 2),
      enc_imm(Opcode::MOV, 10, 0, 99),
      enc_mem(Opcode::ST, 10, 0, 0x280),
      enc_reg(Opcode::FMOV, 7, 0, 2),
      enc_mem(Opcode::FST, 7, 0, 0x284),
      enc_halt(),
  };
  p.data = {
      {0x200, 1}, {0x204, 2}, {0x208, 3}, {0x20C, 4},
      {0x220, 5}, {0x224, 6}, {0x228, 7}, {0x22C, 8},
      {0x260, 0x3FC00000},
      {0x264, 0x40100000},
  };
  p.expected_mem = {
      {0x240, 19}, {0x244, 22}, {0x248, 43}, {0x24C, 50},
      {0x270, 0x40700000},
      {0x274, 0xBF400000},
      {0x278, 0x40580000},
      {0x27C, 0x3F2AAAAB},
      {0x280, 7},
      {0x284, 0x40100000},
  };
  return p;
}

TestProgram program_sort() {
  TestProgram p;
  p.name = "bubble sort + checksum";
  p.code = {
      enc_imm(Opcode::MOV, 1, 0, 7),
      enc_imm(Opcode::CMP, 0, 1, 0),
      enc_branch(Opcode::BEQ, 26),
      enc_branch(Opcode::BLT, 25),
      enc_imm(Opcode::MOV, 2, 0, 0),
      enc_reg(Opcode::CMP, 0, 2, 1),
      enc_branch(Opcode::BEQ, 20),
      enc_branch(Opcode::BGT, 19),
      enc_reg(Opcode::MOV, 3, 0, 2),
      enc_imm(Opcode::LSL, 3, 3, 2),
      enc_mem(Opcode::LD, 4, 3, 0x300),
      enc_imm(Opcode::ADD, 3, 3, 4),
      enc_mem(Opcode::LD, 5, 3, 0x300),
      enc_reg(Opcode::CMP, 0, 4, 5),
      enc_branch(Opcode::BGT, 1),
      enc_branch(Opcode::B, 9),
      enc_imm(Opcode::SUB, 3, 3, 4),
      enc_mem(Opcode::LD, 4, 3, 0x300),
      enc_imm(Opcode::ADD, 6, 3, 4),
      enc_mem(Opcode::LD, 5, 6, 0x300),
      enc_reg(Opcode::XOR, 4, 4, 5),
      enc_reg(Opcode::XOR, 5, 5, 4),
      enc_reg(Opcode::XOR, 4, 4, 5),
      enc_mem(Opcode::ST, 4, 3, 0x300),
      enc_mem(Opcode::ST, 5, 6, 0x300),
      enc_imm(Opcode::ADD, 2, 2, 1),
      enc_branch(Opcode::B, -22),
      enc_imm(Opcode::SUB, 1, 1, 1),
      enc_branch(Opcode::B, -28),
      enc_mem(Opcode::BNDCHK, 0, 0, 0x300),
      enc_imm(Opcode::MOV, 6, 0, 0),
      enc_imm(Opcode::MOV, 7, 0, 0),
      enc_imm(Opcode::CMP, 0, 7, 8),
      enc_branch(Opcode::BEQ, 6),
      enc_reg(Opcode::MOV, 8, 0, 7),
      enc_imm(Opcode::LSL, 8, 8, 2),
      enc_mem(Opcode::LD, 4, 8, 0x300),
      enc_reg(Opcode::ADD, 6, 6, 4),
      enc_imm(Opcode::ADD, 7, 7, 1),
      enc_branch(Opcode::B, -8),
      enc_imm(Opcode::MOD, 6, 6, 10),
      enc_mem(Opcode::ST, 6, 0, 0x320),
      enc_imm(Opcode::MOV, 6, 0, 39),
      enc_imm(Opcode::DIV, 6, 6, 10),
      enc_imm(Opcode::AND, 6, 6, 0xFF),
      enc_imm(Opcode::OR, 6, 6, 0x100),
      enc_imm(Opcode::LSR, 6, 6, 4),
      enc_imm(Opcode::ASR, 6, 6, 1),
      enc_reg(Opcode::NOT, 6, 0, 6),
      enc_mem(Opcode::ST, 6, 0, 0x324),
      enc_halt(),
  };
  p.data = {
      {0x300, 5}, {0x304, 2}, {0x308, 9}, {0x30C, 1},
      {0x310, 7}, {0x314, 3}, {0x318, 8}, {0x31C, 4},
  };
  p.expected_mem = {
      {0x300, 1}, {0x304, 2}, {0x308, 3}, {0x30C, 4},
      {0x310, 5}, {0x314, 7}, {0x318, 8}, {0x31C, 9},
      {0x320, 9},
      {0x324, 0xFFFFFFF7},
  };
  return p;
}

std::set<std::uint8_t> opcodes_used(const std::vector<TestProgram>& programs) {
  std::set<std::uint8_t> used;
  for (const TestProgram& p : programs) {
    for (Word w : p.code) used.insert(get_opcode(w));
  }
  return used;
}

const std::set<std::uint8_t>& all_opcodes() {
  static const std::set<std::uint8_t> all = {
      0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
      0x0C, 0x0D, 0x10, 0x11, 0x12, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26,
      0x27, 0x28, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
  };
  return all;
}

}
