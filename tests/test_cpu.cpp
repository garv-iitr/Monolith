#include <array>
#include <cstdint>
#include <vector>

#include "isa_spec/isa.h"
#include "memory_safety/memory_map.h"
#include "memory_safety/stack_guard.h"
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

Word enc_mem(Opcode op, int rd, int rs1, int offset) {
  return (static_cast<Word>(op) << 26) |
         (static_cast<Word>(rd & 0xF) << 20) |
         (static_cast<Word>(rs1 & 0xF) << 16) |
         static_cast<Word>(offset & 0xFFFF);
}

Word enc_branch(Opcode op, int word_offset) {
  return (static_cast<Word>(op) << 26) | (static_cast<Word>(word_offset) & 0x03FFFFFF);
}

Word enc_zero(Opcode op) { return static_cast<Word>(op) << 26; }
Word enc_halt() { return static_cast<Word>(Opcode::HALT) << 26; }

}

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
  CHECK(cpu.read_reg(3) == 42u);
  CHECK(cpu.read_reg(4) == 13u);
  CHECK(cpu.read_reg(5) == 1u);
  CHECK(cpu.read_reg(6) == 1u);
  CHECK(cpu.read_reg(7) == 1u);
  CHECK(cpu.read_reg(8) == 6u);
  CHECK(cpu.read_reg(9) == 7u);
  CHECK(cpu.read_reg(10) == 1u);
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
  cpu.write_reg(1, 0);
  cpu.load({enc_mem(Opcode::LD, 2, 1, 0)});
  cpu.run();

  CHECK(cpu.halted());
  CHECK(cpu.faulted());
  CHECK(cpu.fault_code() == ExceptionCode::BAD_MEM_ACCESS);
  CHECK(cpu.fault_address() == 0u);
}

RISC201_TEST(cpu_stack_overflow_stops_execution) {
  risc201::Cpu cpu;
  cpu.write_reg(REG_SP, risc201::safety::StackGuard::kStackLimit);
  cpu.load({enc_branch(Opcode::CALL, 0)});
  cpu.run();

  CHECK(cpu.halted());
  CHECK(cpu.faulted());
  CHECK(cpu.fault_code() == ExceptionCode::STACK_OVERFLOW);
  CHECK(cpu.fault_address() == risc201::safety::StackGuard::kStackLimit);
}

RISC201_TEST(cpu_return_from_empty_stack_faults) {
  risc201::Cpu cpu;
  cpu.write_reg(REG_SP, risc201::safety::StackGuard::kStackBase);
  cpu.load({enc_zero(Opcode::RET)});
  cpu.run();

  CHECK(cpu.halted());
  CHECK(cpu.faulted());
  CHECK(cpu.fault_code() == ExceptionCode::STACK_UNDERFLOW);
  CHECK(cpu.fault_address() == risc201::safety::MemoryMap::kStackTop);
}

RISC201_TEST(cpu_invalid_data_access_faults_before_read) {
  risc201::Cpu cpu;
  cpu.write_reg(1, 0);
  cpu.load({enc_mem(Opcode::LD, 2, 1, 0), enc_halt()});
  cpu.run();

  CHECK(cpu.halted());
  CHECK(cpu.faulted());
  CHECK(cpu.fault_code() == ExceptionCode::BAD_MEM_ACCESS);
  CHECK(cpu.fault_address() == 0u);
}

RISC201_TEST(cpu_misaligned_data_access_faults_before_read) {
  risc201::Cpu cpu;
  cpu.write_reg(1, 0x10001);
  cpu.load({enc_mem(Opcode::LD, 2, 1, 0), enc_halt()});
  cpu.run();

  CHECK(cpu.halted());
  CHECK(cpu.faulted());
  CHECK(cpu.fault_code() == ExceptionCode::MISALIGNED_ACCESS);
  CHECK(cpu.fault_address() == 0x10001u);
}

RISC201_TEST(cpu_store_below_current_stack_pointer_faults) {
  risc201::Cpu cpu;
  cpu.write_reg(REG_SP, risc201::safety::MemoryMap::kStackTop - 4);
  cpu.write_reg(1, risc201::safety::MemoryMap::kStackTop - 8);
  cpu.write_reg(2, 0xABCD);
  cpu.load({enc_mem(Opcode::ST, 2, 1, 0)});
  cpu.run();

  CHECK(cpu.halted());
  CHECK(cpu.faulted());
  CHECK(cpu.fault_code() == ExceptionCode::STACK_OVERFLOW);
  CHECK(cpu.fault_address() == risc201::safety::MemoryMap::kStackTop - 8);
}

RISC201_TEST(cpu_visualizes_live_stack_pointer_and_words) {
  risc201::Cpu cpu;
  const Word sp = risc201::safety::MemoryMap::kStackTop - 4;
  cpu.write_reg(REG_SP, sp);
  cpu.write_mem(sp, 0x1234ABCD);

  const std::string view = cpu.render_stack(5);
  CHECK(view.find("0x1234abcd") != std::string::npos);
  CHECK(view.find("SP / FRAME") != std::string::npos);
  CHECK(view.find("LIMIT") != std::string::npos);
  CHECK(view.find("BASE") != std::string::npos);
}

RISC201_TEST(cpu_initializes_stack_pointer_at_exclusive_top) {
  risc201::Cpu cpu;
  CHECK(cpu.read_reg(REG_SP) == risc201::safety::MemoryMap::kStackTop);
  cpu.reset();
  CHECK(cpu.read_reg(REG_SP) == risc201::safety::MemoryMap::kStackTop);
}

RISC201_TEST(cpu_call_and_return_use_guarded_stack) {
  risc201::Cpu cpu;
  cpu.load({enc_branch(Opcode::CALL, 1),
            enc_halt(),
            enc_zero(Opcode::RET)});
  cpu.run();

  CHECK(cpu.halted());
  CHECK(!cpu.faulted());
  CHECK(cpu.read_reg(REG_SP) == risc201::safety::MemoryMap::kStackTop);
  CHECK(cpu.read_mem(risc201::safety::MemoryMap::kStackTop - 4) == 4u);
}

RISC201_TEST(cpu_nested_calls_and_returns_restore_stack) {
  risc201::Cpu cpu;
  cpu.load({enc_branch(Opcode::CALL, 1),
            enc_halt(),
            enc_branch(Opcode::CALL, 1),
            enc_zero(Opcode::RET),
            enc_zero(Opcode::RET)});
  cpu.run();

  CHECK(cpu.halted());
  CHECK(!cpu.faulted());
  CHECK(cpu.read_reg(REG_SP) == risc201::safety::MemoryMap::kStackTop);
  CHECK(cpu.read_mem(risc201::safety::MemoryMap::kStackTop - 4) == 4u);
  CHECK(cpu.read_mem(risc201::safety::MemoryMap::kStackTop - 8) == 12u);
}

RISC201_TEST(cpu_checkpoint_restore_restores_shadow_return_stack) {
  risc201::Cpu cpu;
  cpu.load({enc_branch(Opcode::CALL, 1),
            enc_halt(),
            enc_zero(Opcode::RET)});

  for (int i = 0; i < 100 && cpu.stack_frame_boundaries().empty(); ++i)
    cpu.step();
  CHECK(cpu.stack_frame_boundaries().size() == 1u);

  const auto checkpoint = cpu.checkpoint();
  cpu.run();
  const auto memory =
      cpu.finish_step(checkpoint.memory_journal_mark);
  CHECK(cpu.halted());
  CHECK(!cpu.faulted());

  cpu.restore(checkpoint, memory);
  cpu.run();
  CHECK(cpu.halted());
  CHECK(!cpu.faulted());
  CHECK(cpu.read_reg(REG_SP) == risc201::safety::MemoryMap::kStackTop);
}

RISC201_TEST(cpu_reset_clears_shadow_return_stack) {
  risc201::Cpu cpu;
  cpu.load({enc_branch(Opcode::CALL, 1),
            enc_halt(),
            enc_zero(Opcode::RET)});
  for (int i = 0; i < 100 && cpu.stack_frame_boundaries().empty(); ++i)
    cpu.step();

  cpu.reset();
  constexpr Word return_slot =
      risc201::safety::MemoryMap::kStackTop - 4;
  cpu.write_reg(REG_SP, return_slot);
  cpu.write_mem(return_slot, 4u);
  cpu.load({enc_zero(Opcode::RET)});
  cpu.run();

  CHECK(cpu.faulted());
  CHECK(cpu.fault_code() == ExceptionCode::SHADOW_STACK_MISMATCH);
  CHECK(cpu.read_reg(REG_SP) == return_slot);
}

RISC201_TEST(cpu_memory_safety_configuration_matrix) {
  struct Configuration {
    const char* name;
    risc201::cu::StageLayout layout;
    risc201::ControlEncoding encoding;
  };
  const std::array<Configuration, 4> configurations = {{
      {"4-stage horizontal", risc201::cu::StageLayout::FOUR_STAGE,
       risc201::ControlEncoding::HORIZONTAL},
      {"4-stage vertical", risc201::cu::StageLayout::FOUR_STAGE,
       risc201::ControlEncoding::VERTICAL},
      {"6-stage horizontal", risc201::cu::StageLayout::SIX_STAGE,
       risc201::ControlEncoding::HORIZONTAL},
      {"6-stage vertical", risc201::cu::StageLayout::SIX_STAGE,
       risc201::ControlEncoding::VERTICAL},
  }};
  const std::array<const char*, 20> scenarios = {{
      "CALL/RET round-trip", "CALL stack overflow", "empty RET underflow",
      "valid data store/load", "stack store below SP",
      "misaligned stack load",
      "integer load bad address", "integer load misaligned",
      "integer store bad address", "integer store misaligned",
      "FP load bad address", "FP load misaligned",
      "FP store bad address", "FP store misaligned",
      "corrupted return address", "nested CALL/RET", "unmatched RET",
      "BNDCHK valid address", "BNDCHK bad address", "BNDCHK misaligned",
  }};

  std::size_t passed = 0;
  const std::size_t total = configurations.size() * scenarios.size();
  for (const Configuration& config : configurations) {
    for (std::size_t scenario = 0; scenario < scenarios.size(); ++scenario) {
      risc201::Cpu cpu(risc201::cu::MemoryUnit::kDefaultWords, config.layout,
                       config.encoding);
      bool ok = false;
      switch (scenario) {
        case 0:
          cpu.load({enc_branch(Opcode::CALL, 1), enc_halt(),
                    enc_zero(Opcode::RET)});
          cpu.run();
          ok = cpu.halted() && !cpu.faulted() &&
               cpu.read_reg(REG_SP) == risc201::safety::MemoryMap::kStackTop &&
               cpu.read_mem(risc201::safety::MemoryMap::kStackTop - 4) == 4u;
          break;
        case 1:
          cpu.write_reg(REG_SP, risc201::safety::MemoryMap::kStackLimit);
          cpu.write_mem(risc201::safety::MemoryMap::kStackLimit - 4, 0xA55A);
          cpu.load({enc_branch(Opcode::CALL, 0)});
          cpu.run();
          ok = cpu.halted() && cpu.fault_code() == ExceptionCode::STACK_OVERFLOW &&
               cpu.fault_address() == risc201::safety::MemoryMap::kStackLimit &&
               cpu.read_reg(REG_SP) == risc201::safety::MemoryMap::kStackLimit &&
               cpu.read_mem(risc201::safety::MemoryMap::kStackLimit - 4) ==
                   0xA55A;
          break;
        case 2:
          cpu.write_reg(REG_SP, risc201::safety::MemoryMap::kStackTop);
          cpu.load({enc_zero(Opcode::RET)});
          cpu.run();
          ok = cpu.halted() &&
               cpu.fault_code() == ExceptionCode::STACK_UNDERFLOW &&
               cpu.fault_address() == risc201::safety::MemoryMap::kStackTop;
          break;
        case 3: {
          constexpr Word data_address = 0x10000;
          constexpr Word stack_address =
              risc201::safety::MemoryMap::kStackTop - 4;
          cpu.write_reg(REG_SP, stack_address);
          cpu.write_mem(stack_address, 0x5A5A1234);
          cpu.write_reg(1, data_address);
          cpu.write_reg(2, 0xDEADBEEF);
          cpu.load({enc_mem(Opcode::ST, 2, 1, 0),
                    enc_mem(Opcode::LD, 3, 1, 0)});
          cpu.run();
          ok = cpu.halted() && !cpu.faulted() &&
               cpu.read_mem(data_address) == 0xDEADBEEF &&
               cpu.read_reg(3) == 0xDEADBEEF &&
               cpu.read_mem(stack_address) == 0x5A5A1234;
          break;
        }
        case 4: {
          constexpr Word stack_address =
              risc201::safety::MemoryMap::kStackTop - 8;
          constexpr Word sentinel = 0xA55A1234;
          cpu.write_reg(REG_SP, stack_address + 4);
          cpu.write_reg(1, stack_address);
          cpu.write_reg(2, 0xDEADBEEF);
          cpu.write_mem(stack_address, sentinel);
          cpu.load({enc_mem(Opcode::ST, 2, 1, 0)});
          cpu.run();
          ok = cpu.halted() &&
               cpu.fault_code() == ExceptionCode::STACK_OVERFLOW &&
               cpu.fault_address() == stack_address &&
               cpu.read_reg(REG_SP) == stack_address + 4 &&
               cpu.read_mem(stack_address) == sentinel;
          break;
        }
        case 5: {
          const Word address =
              risc201::safety::MemoryMap::kStackTop - 5;
          cpu.write_reg(1, address);
          cpu.load({enc_mem(Opcode::LD, 2, 1, 0)});
          cpu.run();
          ok = cpu.halted() &&
               cpu.fault_code() == ExceptionCode::MISALIGNED_ACCESS &&
               cpu.fault_address() == address;
          break;
        }
        case 6:
          cpu.write_reg(1, 0);
          cpu.load({enc_mem(Opcode::LD, 2, 1, 0)});
          cpu.run();
          ok = cpu.halted() && cpu.fault_code() == ExceptionCode::BAD_MEM_ACCESS &&
               cpu.fault_address() == 0;
          break;
        case 7:
          cpu.write_reg(1, 0x10001);
          cpu.load({enc_mem(Opcode::LD, 2, 1, 0)});
          cpu.run();
          ok = cpu.halted() &&
               cpu.fault_code() == ExceptionCode::MISALIGNED_ACCESS &&
               cpu.fault_address() == 0x10001;
          break;
        case 8:
        case 9: {
          const Word address = scenario == 8 ? 0 : 0x10001;
          const Word aligned_address = scenario == 8 ? 0 : 0x10000;
          constexpr Word sentinel = 0xA55A1234;
          cpu.write_mem(aligned_address, sentinel);
          cpu.write_reg(1, address);
          cpu.write_reg(2, 0xDEADBEEF);
          cpu.load({enc_mem(Opcode::ST, 2, 1, 0)});
          const Word preserved_word = cpu.read_mem(aligned_address);
          cpu.run();
          const ExceptionCode expected =
              scenario == 8 ? ExceptionCode::BAD_MEM_ACCESS
                            : ExceptionCode::MISALIGNED_ACCESS;
          ok = cpu.halted() && cpu.fault_code() == expected &&
               cpu.fault_address() == address &&
               cpu.read_mem(aligned_address) == preserved_word;
          break;
        }
        case 10:
        case 11: {
          const Word address = scenario == 10 ? 0 : 0x10001;
          cpu.write_reg(1, address);
          cpu.load({enc_mem(Opcode::FLD, 2, 1, 0)});
          cpu.run();
          const ExceptionCode expected =
              scenario == 10 ? ExceptionCode::BAD_MEM_ACCESS
                            : ExceptionCode::MISALIGNED_ACCESS;
          ok = cpu.halted() && cpu.fault_code() == expected &&
               cpu.fault_address() == address;
          break;
        }
        case 12:
        case 13: {
          const Word address = scenario == 12 ? 0 : 0x10001;
          const Word aligned_address = scenario == 12 ? 0 : 0x10000;
          constexpr Word sentinel = 0xA55A1234;
          cpu.write_mem(aligned_address, sentinel);
          cpu.write_reg(1, address);
          cpu.load({enc_mem(Opcode::FST, 2, 1, 0)});
          const Word preserved_word = cpu.read_mem(aligned_address);
          cpu.run();
          const ExceptionCode expected =
              scenario == 12 ? ExceptionCode::BAD_MEM_ACCESS
                            : ExceptionCode::MISALIGNED_ACCESS;
          ok = cpu.halted() && cpu.fault_code() == expected &&
               cpu.fault_address() == address &&
               cpu.read_mem(aligned_address) == preserved_word;
          break;
        }
        case 14: {
          cpu.load({enc_branch(Opcode::CALL, 1),
                    enc_halt(),
                    enc_zero(Opcode::RET)});
          for (int i = 0; i < 100 && cpu.stack_frame_boundaries().empty(); ++i)
            cpu.step();
          constexpr Word return_slot =
              risc201::safety::MemoryMap::kStackTop - 4;
          constexpr Word corrupted_return_pc = 0x100u;
          cpu.write_mem(return_slot, corrupted_return_pc);
          cpu.run();
          ok = cpu.halted() &&
               cpu.fault_code() == ExceptionCode::SHADOW_STACK_MISMATCH &&
               cpu.fault_address() == return_slot &&
               cpu.pc() != corrupted_return_pc &&
               cpu.read_reg(REG_SP) == return_slot;
          break;
        }
        case 15:
          cpu.load({enc_branch(Opcode::CALL, 1),
                    enc_halt(),
                    enc_branch(Opcode::CALL, 1),
                    enc_zero(Opcode::RET),
                    enc_zero(Opcode::RET)});
          cpu.run();
          ok = cpu.halted() && !cpu.faulted() &&
               cpu.read_reg(REG_SP) == risc201::safety::MemoryMap::kStackTop &&
               cpu.read_mem(risc201::safety::MemoryMap::kStackTop - 4) == 4u &&
               cpu.read_mem(risc201::safety::MemoryMap::kStackTop - 8) == 12u;
          break;
        case 16: {
          constexpr Word return_slot =
              risc201::safety::MemoryMap::kStackTop - 4;
          cpu.write_reg(REG_SP, return_slot);
          cpu.write_mem(return_slot, 4u);
          cpu.load({enc_zero(Opcode::RET)});
          cpu.run();
          ok = cpu.halted() &&
               cpu.fault_code() == ExceptionCode::SHADOW_STACK_MISMATCH &&
               cpu.fault_address() == return_slot &&
               cpu.read_reg(REG_SP) == return_slot;
          break;
        }
        case 17:
        case 18:
        case 19: {
          const Word address = scenario == 17 ? 0x10000
                               : scenario == 18 ? 0x0FFC
                                                : 0x10001;
          cpu.write_reg(1, address);
          cpu.load({enc_mem(Opcode::BNDCHK, 0, 1, 0), enc_halt()});
          cpu.run();
          const ExceptionCode expected =
              scenario == 17 ? ExceptionCode::NONE
              : scenario == 18 ? ExceptionCode::BAD_MEM_ACCESS
                               : ExceptionCode::MISALIGNED_ACCESS;
          ok = cpu.halted() && cpu.fault_code() == expected;
          if (expected != ExceptionCode::NONE)
            ok = ok && cpu.fault_address() == address;
          break;
        }
      }
      std::printf("[SAFETY MATRIX] %s / %s: %s\n", config.name,
                  scenarios[scenario], ok ? "PASS" : "FAIL");
      passed += ok ? 1 : 0;
      CHECK(ok);
    }
  }
  std::printf("[SAFETY MATRIX] %zu/%zu cases passed\n", passed, total);
  CHECK(passed == total);
}

RISC201_TEST(cpu_stack_visualizer_tracks_call_frames) {
  risc201::Cpu cpu;
  cpu.load({enc_branch(Opcode::CALL, 1),
            enc_halt(),
            enc_zero(Opcode::RET)});

  for (int i = 0; i < 100 && cpu.stack_frame_boundaries().empty(); ++i) {
    cpu.step();
  }
  CHECK(cpu.stack_frame_boundaries().size() == 1u);
  CHECK(cpu.render_stack().find("CALL FRAME") != std::string::npos);

  for (int i = 0; i < 100 && !cpu.stack_frame_boundaries().empty(); ++i) {
    cpu.step();
  }
  CHECK(cpu.stack_frame_boundaries().empty());
  CHECK(!cpu.faulted());
}

RISC201_TEST(cpu_stack_snapshot_diff_tracks_call_and_return) {
  risc201::Cpu cpu;
  const auto empty_stack = cpu.stack_memory_snapshot();
  const Word empty_sp = cpu.stack_pointer_word();
  cpu.load({enc_branch(Opcode::CALL, 1), enc_halt(),
            enc_zero(Opcode::RET)});

  for (int i = 0; i < 100 && cpu.stack_frame_boundaries().empty(); ++i) {
    cpu.step();
  }
  const std::string push_diff =
      cpu.render_stack_diff(empty_stack, empty_sp);
  CHECK(push_diff.find("(push)") != std::string::npos);
  CHECK(push_diff.find("word 0x7fff") != std::string::npos);
  CHECK(cpu.render_stack().find("CALL FRAME") != std::string::npos);

  const auto active_stack = cpu.stack_memory_snapshot();
  const Word active_sp = cpu.stack_pointer_word();
  for (int i = 0; i < 100 && !cpu.stack_frame_boundaries().empty(); ++i) {
    cpu.step();
  }
  for (int i = 0; i < 100 && cpu.micro_pc() != 0; ++i) {
    cpu.step();
  }
  const std::string pop_diff = cpu.render_stack_diff(active_stack, active_sp);
  CHECK(pop_diff.find("(pop)") != std::string::npos);
  CHECK(cpu.stack_frame_boundaries().empty());
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

RISC201_TEST(cpu_signed_overflow_comparison_branches) {
  // Test case 1: Positive large minus negative one overflows (+2147483647 > -1)
  // CMP 0x7FFFFFFF, 0xFFFFFFFF -> A - B = 0x80000000 (N=1, V=1).
  // BGT must be taken because (N == V) && !Z. BLT must NOT be taken because N == V.
  {
    risc201::Cpu cpu;
    cpu.write_reg(1, 0x7FFFFFFFu);
    cpu.write_reg(2, 0xFFFFFFFFu);
    cpu.load({
      enc_reg(Opcode::CMP, 0, 1, 2),
      enc_branch(Opcode::BGT, 2),
      enc_imm(Opcode::MOV, 3, 0, 0), // Should be skipped
      enc_halt(),
      enc_imm(Opcode::MOV, 3, 0, 1), // Should execute
      enc_halt()
    });
    cpu.run();
    CHECK(!cpu.faulted());
    CHECK(cpu.read_reg(3) == 1u);
  }

  // Test case 2: Negative large minus positive one overflows (-2147483648 < 1)
  // CMP 0x80000000, 1 -> A - B = 0x7FFFFFFF (N=0, V=1).
  // BLT must be taken because N != V. BGT must NOT be taken.
  {
    risc201::Cpu cpu;
    cpu.write_reg(1, 0x80000000u);
    cpu.write_reg(2, 1u);
    cpu.load({
      enc_reg(Opcode::CMP, 0, 1, 2),
      enc_branch(Opcode::BLT, 2),
      enc_imm(Opcode::MOV, 3, 0, 0), // Should be skipped
      enc_halt(),
      enc_imm(Opcode::MOV, 3, 0, 1), // Should execute
      enc_halt()
    });
    cpu.run();
    CHECK(!cpu.faulted());
    CHECK(cpu.read_reg(3) == 1u);
  }
}

RISC201_TEST(cpu_consecutive_ret_shadow_stack) {
  risc201::Cpu cpu;
  // Address 0: CALL fn1 (target addr 8)
  // Address 4: HALT
  // Address 8 (fn1): CALL fn2 (target addr 16)
  // Address 12 (fn1 ret): RET (returns to 4)
  // Address 16 (fn2): RET (returns to 12, which is immediately another RET)
  cpu.load({
    enc_branch(Opcode::CALL, 1),
    enc_halt(),
    enc_branch(Opcode::CALL, 1),
    enc_zero(Opcode::RET),
    enc_zero(Opcode::RET)
  });
  cpu.run();
  CHECK(!cpu.faulted());
  CHECK(cpu.halted());
  CHECK(cpu.read_reg(REG_SP) == risc201::safety::MemoryMap::kStackTop);
}

RISC201_TEST(cpu_bndchk_stack_spush_spop_safe) {
  risc201::Cpu cpu;
  // Simulates SPUSH / SPOP macro sequence:
  // BNDCHK SP, -4
  // SUBI SP, SP, 4
  // ST r1, 0[SP]
  // BNDCHK SP, 4
  // LD r2, 0[SP]
  // ADDI SP, SP, 4
  // HALT
  cpu.write_reg(1, 0x12345678u);
  cpu.load({
    enc_mem(Opcode::BNDCHK, 0, REG_SP, static_cast<int16_t>(-4)),
    enc_imm(Opcode::SUB, REG_SP, REG_SP, 4),
    enc_mem(Opcode::ST, 1, REG_SP, 0),
    enc_mem(Opcode::BNDCHK, 0, REG_SP, 4),
    enc_mem(Opcode::LD, 2, REG_SP, 0),
    enc_imm(Opcode::ADD, REG_SP, REG_SP, 4),
    enc_halt()
  });
  cpu.run();
  CHECK(!cpu.faulted());
  CHECK(cpu.halted());
  CHECK(cpu.read_reg(2) == 0x12345678u);
  CHECK(cpu.read_reg(REG_SP) == risc201::safety::MemoryMap::kStackTop);
}

RISC201_TEST(cpu_committed_pc_preserves_branch_target) {
  risc201::Cpu cpu;
  // Address 0: B to target at offset 2 (address 12)
  // Address 4: NOP
  // Address 8: NOP
  // Address 12: HALT
  cpu.load({
    enc_branch(Opcode::B, 2),
    enc_zero(Opcode::NOP),
    enc_zero(Opcode::NOP),
    enc_halt()
  });
  cpu.run();
  CHECK(!cpu.faulted());
  CHECK(cpu.halted());
  CHECK(cpu.pc() == 12u);
}
