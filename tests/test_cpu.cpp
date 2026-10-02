#include <cstdint>
#include <vector>

#include "isa_spec/isa.h"
#include "simulator_core/core/cpu.h"
#include "tests/test_framework.h"

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

Word enc_branch(Opcode op, int word_offset) {
  return (static_cast<Word>(op) << 26) | (static_cast<Word>(word_offset) & 0x03FFFFFF);
}

Word enc_zero(Opcode op) { return static_cast<Word>(op) << 26; }
Word enc_halt() { return static_cast<Word>(Opcode::HALT) << 26; }

}  // namespace

RISC201_TEST(cpu_runs_transfer_program) {
  risc201::Cpu cpu;
  cpu.load({enc_imm(Opcode::MOV, 1, 0, 5),
            enc_reg(Opcode::MOV, 2, 0, 1),
            enc_imm(Opcode::MOV, 3, 0, 0x1234),
            enc_zero(Opcode::NOP),
            enc_halt()});
  cpu.run();

  CHECK(cpu.halted());
  CHECK(!cpu.faulted());
  CHECK(cpu.read_reg(1) == 5u);
  CHECK(cpu.read_reg(2) == 5u);
  CHECK(cpu.read_reg(3) == 0x1234u);
}

RISC201_TEST(cpu_executes_alu_ops) {
  risc201::Cpu cpu;
  cpu.load({enc_imm(Opcode::MOV, 1, 0, 6),
            enc_imm(Opcode::MOV, 2, 0, 7),
            enc_reg(Opcode::MUL, 3, 1, 2),
            enc_reg(Opcode::ADD, 4, 1, 2),
            enc_reg(Opcode::SUB, 5, 2, 1),
            enc_reg(Opcode::DIV, 6, 2, 1),
            enc_reg(Opcode::MOD, 7, 2, 1),
            enc_reg(Opcode::AND, 8, 1, 2),
            enc_reg(Opcode::OR, 9, 1, 2),
            enc_reg(Opcode::XOR, 10, 1, 2),
            enc_halt()});
  cpu.run();

  CHECK(!cpu.faulted());
  CHECK(cpu.read_reg(3) == 42u);  // 6 * 7
  CHECK(cpu.read_reg(4) == 13u);  // 6 + 7
  CHECK(cpu.read_reg(5) == 1u);   // 7 - 6
  CHECK(cpu.read_reg(6) == 1u);   // 7 / 6
  CHECK(cpu.read_reg(7) == 1u);   // 7 % 6
  CHECK(cpu.read_reg(8) == 6u);   // 6 & 7
  CHECK(cpu.read_reg(9) == 7u);   // 6 | 7
  CHECK(cpu.read_reg(10) == 1u);  // 6 ^ 7
}

RISC201_TEST(cpu_cmp_sets_flags) {
  risc201::Cpu cpu;
  cpu.load({enc_imm(Opcode::MOV, 1, 0, 5),
            enc_imm(Opcode::MOV, 2, 0, 5),
            enc_reg(Opcode::CMP, 0, 1, 2),
            enc_halt()});
  cpu.run();

  CHECK(cpu.flags().Z);
  CHECK(!cpu.flags().N);
}

RISC201_TEST(cpu_bad_memory_access_faults) {
  risc201::Cpu cpu;
  cpu.load({enc_branch(Opcode::CALL, 0)});
  cpu.run();

  CHECK(cpu.halted());
  CHECK(cpu.faulted());
  CHECK(cpu.fault_code() == ExceptionCode::BAD_MEM_ACCESS);
}

RISC201_TEST(cpu_snapshot_reflects_state) {
  risc201::Cpu cpu;
  cpu.load({enc_imm(Opcode::MOV, 1, 0, 7), enc_halt()});
  cpu.run();

  const risc201::StateView v = cpu.snapshot();
  CHECK(v.halted);
  CHECK(v.gprs[1] == 7u);
  CHECK(v.fault_code == ExceptionCode::NONE);
}
