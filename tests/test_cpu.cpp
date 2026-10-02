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
