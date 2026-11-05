#include <cstdio>
#include <vector>

#include "control_unit/pipeline_processor.h"
#include "common/control_signals.h"
#include "isa_spec/isa.h"
#include "tests/test_framework.h"
#include "tests/test_programs.h"

using namespace risc201::pipeline;
using risc201::cu::StageLayout;
using risc201::cu::AluOp;

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

}  // namespace

RISC201_TEST(pipeline_raw_forwarding_4stage) {
  PipelineProcessor cpu(risc201::cu::StageLayout::FOUR_STAGE, false);
  cpu.load({
      enc_imm(Opcode::ADD, 1, 0, 5),   // r1 = 5
      enc_reg(Opcode::ADD, 2, 1, 1),   // r2 = r1 + r1 = 10 (RAW hazard -> forwarded from EX/MA)
      enc_reg(Opcode::ADD, 3, 2, 1),   // r3 = r2 + r1 = 15 (RAW hazard -> forwarded)
      enc_halt()
  });
  cpu.run();

  CHECK(cpu.halted());
  CHECK(!cpu.faulted());
  CHECK_EQ(cpu.read_reg(1), 5u);
  CHECK_EQ(cpu.read_reg(2), 10u);
  CHECK_EQ(cpu.read_reg(3), 15u);
  CHECK(cpu.stats().forwarding_events > 0);
}

RISC201_TEST(pipeline_raw_forwarding_6stage) {
  PipelineProcessor cpu(risc201::cu::StageLayout::SIX_STAGE, false);
  cpu.load({
      enc_imm(Opcode::ADD, 1, 0, 12),
      enc_reg(Opcode::ADD, 2, 1, 1),   // r2 = 24
      enc_reg(Opcode::ADD, 3, 2, 1),   // r3 = 36
      enc_halt()
  });
  cpu.run();

  CHECK(cpu.halted());
  CHECK(!cpu.faulted());
  CHECK_EQ(cpu.read_reg(1), 12u);
  CHECK_EQ(cpu.read_reg(2), 24u);
  CHECK_EQ(cpu.read_reg(3), 36u);
  CHECK(cpu.stats().forwarding_events > 0);
}

RISC201_TEST(pipeline_load_use_hazard_stalls) {
  // Test 4-stage Load-Use penalty (1 stall bubble)
  {
    PipelineProcessor cpu4(risc201::cu::StageLayout::FOUR_STAGE, false);
    cpu4.write_mem(0x100, 42);
    cpu4.load({
        enc_mem(Opcode::LD, 1, 0, 0x100), // r1 = Mem[0x100] = 42
        enc_imm(Opcode::ADD, 2, 1, 10),   // r2 = r1 + 10 = 52 (Load-use hazard!)
        enc_halt()
    });
    cpu4.run();

    CHECK(cpu4.halted());
    CHECK_EQ(cpu4.read_reg(1), 42u);
    CHECK_EQ(cpu4.read_reg(2), 52u);
    CHECK(cpu4.stats().load_use_stalls >= 1);
  }

  // Test 6-stage Load-Use penalty (2 stall bubbles due to MEMCHK stage)
  {
    PipelineProcessor cpu6(risc201::cu::StageLayout::SIX_STAGE, false);
    cpu6.write_mem(0x100, 42);
    cpu6.load({
        enc_mem(Opcode::LD, 1, 0, 0x100),
        enc_imm(Opcode::ADD, 2, 1, 10),
        enc_halt()
    });
    cpu6.run();

    CHECK(cpu6.halted());
    CHECK_EQ(cpu6.read_reg(1), 42u);
    CHECK_EQ(cpu6.read_reg(2), 52u);
    CHECK(cpu6.stats().load_use_stalls >= 2);
  }
}

RISC201_TEST(pipeline_branch_control_hazard_flush) {
  PipelineProcessor cpu(risc201::cu::StageLayout::FOUR_STAGE, false);
  cpu.load({
      enc_imm(Opcode::MOV, 1, 0, 1),
      enc_imm(Opcode::MOV, 2, 0, 1),
      enc_reg(Opcode::CMP, 0, 1, 2),    // Z = 1
      enc_branch(Opcode::BEQ, 1),      // Branch taken, target offset +1 (skips next instr)
      enc_imm(Opcode::MOV, 3, 0, 999),  // Should be FLUSHED
      enc_imm(Opcode::MOV, 4, 0, 777),  // Target destination
      enc_halt()
  });
  cpu.run();

  CHECK(cpu.halted());
  CHECK_EQ(cpu.read_reg(3), 0u);   // Flushed!
  CHECK_EQ(cpu.read_reg(4), 777u); // Executed!
  CHECK(cpu.stats().branch_flushes > 0);
}

RISC201_TEST(pipeline_call_ret_hardware_stack_4stage) {
  PipelineProcessor cpu(risc201::cu::StageLayout::FOUR_STAGE, false);
  cpu.load({
      enc_imm(Opcode::MOV, 1, 0, 10),
      enc_branch(Opcode::CALL, 1),     // calls func
      enc_halt(),
      // func:
      enc_imm(Opcode::ADD, 1, 1, 5),   // r1 = r1 + 5 = 15
      enc_zero(Opcode::RET)            // pops return address and returns
  });
  cpu.run();

  CHECK(cpu.halted());
  CHECK_EQ(cpu.read_reg(1), 15u);
  CHECK(cpu.stats().uop_expansion_cycles > 0);
}

RISC201_TEST(pipeline_call_ret_hardware_stack_6stage) {
  PipelineProcessor cpu(risc201::cu::StageLayout::SIX_STAGE, false);
  cpu.load({
      enc_imm(Opcode::MOV, 1, 0, 20),
      enc_branch(Opcode::CALL, 1),     // calls func
      enc_halt(),
      // func:
      enc_imm(Opcode::ADD, 1, 1, 30),  // r1 = r1 + 30 = 50
      enc_zero(Opcode::RET)            // pops return address and returns
  });
  cpu.run();

  CHECK(cpu.halted());
  CHECK_EQ(cpu.read_reg(1), 50u);
  CHECK(cpu.stats().uop_expansion_cycles > 0);
}

RISC201_TEST(pipeline_benchmark_evaluation_comparison) {
  const auto programs = {
      risc201::testbench::program_fib(),
      risc201::testbench::program_matmul(),
      risc201::testbench::program_sort()
  };

  std::printf("\n========================================================================================\n");
  std::printf("  RISC201 PROCESSOR EVALUATION: 4-STAGE vs 6-STAGE PIPELINE ARCHITECTURE\n");
  std::printf("  Method 3: Hybrid Micro-Op Cracking (48-bit Horizontal vs 24-bit Vertical ROM)\n");
  std::printf("========================================================================================\n");
  std::printf("%-22s | %-10s | %-18s | %-8s | %-8s | %-6s | %-8s\n",
              "Benchmark", "Pipeline", "CU Encoding", "Cycles", "Ret.Inst", "CPI", "Hazards");
  std::printf("----------------------------------------------------------------------------------------\n");

  for (const auto& prog : programs) {
    for (StageLayout layout : {StageLayout::FOUR_STAGE, StageLayout::SIX_STAGE}) {
      for (bool vertical : {false, true}) {
        PipelineProcessor cpu(layout, vertical);
        cpu.load(prog.code);
        for (const auto& d : prog.data) {
          cpu.write_data(d.first, d.second);
        }

        cpu.run(1000000);

        CHECK(cpu.halted());
        CHECK(!cpu.faulted());

        // Validate memory results
        for (const auto& m : prog.expected_mem) {
          const Word actual = cpu.read_mem(m.first);
          CHECK_EQ(actual, m.second);
        }

        const auto& s = cpu.stats();
        std::printf("%-22s | %-10s | %-18s | %-8zu | %-8zu | %-6.2f | %-8zu\n",
                    prog.name.c_str(),
                    (layout == StageLayout::FOUR_STAGE) ? "4-Stage" : "6-Stage",
                    vertical ? "Vertical (24b)" : "Horizontal (48b)",
                    s.total_cycles,
                    s.instructions_retired,
                    s.cpi(),
                    s.load_use_stalls + s.branch_flushes);
      }
    }
  }
  std::printf("========================================================================================\n\n");
}
