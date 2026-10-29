#include <cstdint>
#include <vector>

#include "isa_spec/isa.h"
#include "memory_safety/memory_map.h"
#include "simulator_core/core/debugger.h"
#include "tests/test_framework.h"

namespace {

Word enc_imm(Opcode op, int rd, int rs1, int imm) {
  return (static_cast<Word>(op) << 26) | (1u << 24) |
         (static_cast<Word>(rd & 0xF) << 20) | (static_cast<Word>(rs1 & 0xF) << 16) |
         static_cast<Word>(imm & 0xFFFF);
}

Word enc_halt() { return static_cast<Word>(Opcode::HALT) << 26; }

}

RISC201_TEST(debugger_load_program_resets_state_and_loads_both_sections) {
  risc201::Debugger dbg;
  dbg.load({enc_imm(Opcode::MOV, 1, 0, 5), enc_halt()});
  dbg.step_instruction();
  dbg.load_program(0, {enc_imm(Opcode::MOV, 2, 0, 7), enc_halt()},
                   0x10000, {0xA5A5u});

  CHECK(dbg.cpu().pc() == 0u);
  CHECK(dbg.cpu().read_reg(1) == 0u);
  CHECK(dbg.cpu().read_mem(0x10000) == 0xA5A5u);
  dbg.step_instruction();
  CHECK(dbg.cpu().read_reg(2) == 7u);
}

RISC201_TEST(debugger_step_advances_one_instruction) {
  risc201::Debugger dbg;
  dbg.load({enc_imm(Opcode::MOV, 1, 0, 5),
            enc_imm(Opcode::MOV, 2, 0, 7),
            enc_halt()});

  dbg.step_instruction();
  CHECK(dbg.cpu().pc() == 4u);
  CHECK(dbg.cpu().read_reg(1) == 5u);
  CHECK(!dbg.cpu().halted());
}

RISC201_TEST(debugger_step_back_instruction_restores_registers) {
  risc201::Debugger dbg;
  dbg.load({enc_imm(Opcode::MOV, 1, 0, 5), enc_imm(Opcode::MOV, 2, 0, 9), enc_halt()});
  dbg.step_instruction();
  dbg.step_instruction();

  CHECK(dbg.cpu().read_reg(2) == 9u);
  CHECK(dbg.step_back_instruction());
  CHECK(dbg.cpu().pc() == 4u);
  CHECK(dbg.cpu().read_reg(1) == 5u);
  CHECK(dbg.cpu().read_reg(2) == 0u);
  CHECK(!dbg.cpu().halted());
}

RISC201_TEST(debugger_step_back_micro_restores_partial_instruction) {
  risc201::Debugger dbg;
  dbg.load({enc_imm(Opcode::MOV, 1, 0, 5), enc_halt()});
  dbg.step_micro();
  const auto before = dbg.view();
  dbg.step_micro();

  CHECK(dbg.view().micro_pc != before.micro_pc);
  CHECK(dbg.step_back_micro());
  CHECK(dbg.view().micro_pc == before.micro_pc);
  CHECK(dbg.view().pc == before.pc);
  CHECK(dbg.view().ir == before.ir);
  CHECK(dbg.view().micro.A == before.micro.A);
}

RISC201_TEST(debugger_step_back_restores_store_memory) {
  risc201::Debugger dbg;
  dbg.write_reg(1, 0x10000);
  dbg.write_reg(2, 0xAABBCCDD);
  dbg.write_mem(0x10000, 0x12345678);
  dbg.load({(static_cast<Word>(Opcode::ST) << 26) | (2u << 20) | (1u << 16), enc_halt()});
  dbg.step_instruction();

  CHECK(dbg.cpu().read_mem(0x10000) == 0xAABBCCDDu);
  CHECK(dbg.step_back_instruction());
  CHECK(dbg.cpu().read_mem(0x10000) == 0x12345678u);
  CHECK(dbg.cpu().pc() == 0u);
}

RISC201_TEST(debugger_step_back_restores_fault_state) {
  risc201::Debugger dbg;
  dbg.write_reg(1, 0);
  dbg.load({(static_cast<Word>(Opcode::LD) << 26) | (2u << 20) | (1u << 16)});
  dbg.step_instruction();

  CHECK(dbg.cpu().faulted());
  CHECK(dbg.step_back_instruction());
  CHECK(!dbg.cpu().faulted());
  CHECK(!dbg.cpu().halted());
  CHECK(dbg.cpu().pc() == 0u);
}

RISC201_TEST(debugger_step_back_restores_call_stack_markers) {
  risc201::Debugger dbg;
  dbg.write_mem(risc201::safety::MemoryMap::kStackTop - 4, 0xDEADBEEFu);
  dbg.load({(static_cast<Word>(Opcode::CALL) << 26) | 1u,
            enc_halt(), static_cast<Word>(Opcode::RET) << 26});
  while (dbg.cpu().stack_frame_boundaries().empty() && !dbg.cpu().halted())
    dbg.step_micro();

  CHECK(dbg.cpu().stack_frame_boundaries().size() == 1u);
  CHECK(dbg.cpu().read_reg(REG_SP) == risc201::safety::MemoryMap::kStackTop - 4);
  CHECK(dbg.step_back_instruction());
  CHECK(dbg.cpu().stack_frame_boundaries().empty());
  CHECK(dbg.cpu().read_reg(REG_SP) == risc201::safety::MemoryMap::kStackTop);
  CHECK(dbg.cpu().read_mem(risc201::safety::MemoryMap::kStackTop - 4) == 0xDEADBEEFu);
  CHECK(dbg.cpu().pc() == 0u);
}

RISC201_TEST(debugger_reverse_history_is_bounded) {
  risc201::Debugger dbg;
  std::vector<Word> code(risc201::Debugger::kHistoryLimit + 8,
                         static_cast<Word>(Opcode::NOP) << 26);
  dbg.load(code);
  for (std::size_t i = 0; i < risc201::Debugger::kHistoryLimit + 2; ++i)
    dbg.step_micro();

  std::size_t reversed = 0;
  while (dbg.step_back_micro()) ++reversed;
  CHECK(reversed == risc201::Debugger::kHistoryLimit);
  CHECK(!dbg.step_back_instruction());
}

RISC201_TEST(debugger_reset_clears_reverse_history) {
  risc201::Debugger dbg;
  dbg.load({enc_imm(Opcode::MOV, 1, 0, 5), enc_halt()});
  dbg.step_instruction();
  dbg.reset();
  CHECK(!dbg.step_back_instruction());
}

RISC201_TEST(debugger_run_to_breakpoint) {
  risc201::Debugger dbg;
  dbg.load({enc_imm(Opcode::MOV, 1, 0, 1),
            enc_imm(Opcode::MOV, 2, 0, 2),
            enc_imm(Opcode::MOV, 3, 0, 3),
            enc_halt()});
  dbg.add_breakpoint(8);
  dbg.run_to_breakpoint();

  CHECK(dbg.at_breakpoint());
  CHECK(dbg.cpu().pc() == 8u);
  CHECK(!dbg.cpu().halted());
}

RISC201_TEST(debugger_run_to_completion) {
  risc201::Debugger dbg;
  dbg.load({enc_imm(Opcode::MOV, 1, 0, 9), enc_halt()});
  dbg.run();

  CHECK(dbg.cpu().halted());
  CHECK(!dbg.cpu().faulted());
  CHECK(dbg.cpu().read_reg(1) == 9u);
}

RISC201_TEST(debugger_watch_detects_change) {
  risc201::Debugger dbg;
  dbg.load({enc_imm(Opcode::MOV, 1, 0, 5), enc_halt()});
  dbg.watch(1);

  dbg.step_instruction();
  const auto changed = dbg.changed_registers();
  CHECK(changed.size() == 1);
  CHECK(changed[0] == 1);
}
