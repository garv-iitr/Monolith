#include <cstdint>
#include <vector>

#include "isa_spec/isa.h"
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
