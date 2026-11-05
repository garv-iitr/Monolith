#include <cstdint>
#include <cstring>
#include <vector>

#include "alu/alu.h"
#include "fpu/fpu.h"
#include "control_unit/pipeline_processor.h"
#include "control_unit/cu_types.h"
#include "simulator_core/core/cpu.h"
#include "simulator_core/core/debugger.h"
#include "memory_safety/memory_map.h"
#include "tests/test_framework.h"

using namespace risc201::cu;
using namespace risc201::pipeline;
using risc201::safety::MemoryMap;

namespace {

Word float_to_word(float f) {
  Word w;
  std::memcpy(&w, &f, sizeof(float));
  return w;
}

float word_to_float(Word w) {
  float f;
  std::memcpy(&f, &w, sizeof(float));
  return f;
}

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

// =============================================================================
// 1. ALU EDGE CASES
// =============================================================================

RISC201_TEST(alu_division_and_modulo_by_zero_throws) {
  ALU alu;
  bool div_threw = false;
  try {
    alu.execute(AluOp::DIV, 100, 0);
  } catch (const RuntimeException& e) {
    if (e.code == ExceptionCode::DIV_BY_ZERO) div_threw = true;
  }
  CHECK(div_threw);

  bool mod_threw = false;
  try {
    alu.execute(AluOp::MOD, 100, 0);
  } catch (const RuntimeException& e) {
    if (e.code == ExceptionCode::DIV_BY_ZERO) mod_threw = true;
  }
  CHECK(mod_threw);
}

RISC201_TEST(alu_signed_division_and_modulo_edge_cases) {
  ALU alu;

  // Zero dividend
  CHECK_EQ(alu.execute(AluOp::DIV, 0, 5).value, 0u);
  CHECK_EQ(alu.execute(AluOp::MOD, 0, 5).value, 0u);

  // Positive / Positive
  CHECK_EQ(alu.execute(AluOp::DIV, 100, 10).value, 10u);
  CHECK_EQ(alu.execute(AluOp::MOD, 100, 10).value, 0u);
  CHECK_EQ(alu.execute(AluOp::DIV, 7, 3).value, 2u);
  CHECK_EQ(alu.execute(AluOp::MOD, 7, 3).value, 1u);

  // Negative / Positive
  CHECK_EQ(static_cast<int32_t>(alu.execute(AluOp::DIV, static_cast<Word>(-100), 10).value), -10);
  CHECK_EQ(static_cast<int32_t>(alu.execute(AluOp::DIV, static_cast<Word>(-7), 3).value), -2);
  CHECK_EQ(static_cast<int32_t>(alu.execute(AluOp::MOD, static_cast<Word>(-7), 3).value), -1);

  // Positive / Negative
  CHECK_EQ(static_cast<int32_t>(alu.execute(AluOp::DIV, 100, static_cast<Word>(-10)).value), -10);
  CHECK_EQ(static_cast<int32_t>(alu.execute(AluOp::DIV, 7, static_cast<Word>(-3)).value), -2);
  CHECK_EQ(static_cast<int32_t>(alu.execute(AluOp::MOD, 7, static_cast<Word>(-3)).value), 1);

  // Negative / Negative
  CHECK_EQ(static_cast<int32_t>(alu.execute(AluOp::DIV, static_cast<Word>(-100), static_cast<Word>(-10)).value), 10);
  CHECK_EQ(static_cast<int32_t>(alu.execute(AluOp::DIV, static_cast<Word>(-7), static_cast<Word>(-3)).value), 2);
  CHECK_EQ(static_cast<int32_t>(alu.execute(AluOp::MOD, static_cast<Word>(-7), static_cast<Word>(-3)).value), -1);

  // INT32_MIN corner cases
  constexpr Word kInt32Min = 0x80000000u;
  CHECK_EQ(alu.execute(AluOp::DIV, kInt32Min, 1).value, kInt32Min);
  CHECK_EQ(alu.execute(AluOp::MOD, kInt32Min, 1).value, 0u);
  CHECK_EQ(alu.execute(AluOp::DIV, kInt32Min, static_cast<Word>(-1)).value, kInt32Min);
  CHECK_EQ(alu.execute(AluOp::MOD, kInt32Min, static_cast<Word>(-1)).value, 0u);
}

RISC201_TEST(alu_radix4_booth_multiplication_edge_cases) {
  ALU alu;

  CHECK_EQ(alu.execute(AluOp::MUL, 0, 12345).value, 0u);
  CHECK_EQ(alu.execute(AluOp::MUL, 12345, 0).value, 0u);
  CHECK_EQ(alu.execute(AluOp::MUL, 1, 12345).value, 12345u);
  CHECK_EQ(alu.execute(AluOp::MUL, 6, 7).value, 42u);

  // Signed multiplication
  CHECK_EQ(static_cast<int32_t>(alu.execute(AluOp::MUL, static_cast<Word>(-6), 7).value), -42);
  CHECK_EQ(static_cast<int32_t>(alu.execute(AluOp::MUL, 6, static_cast<Word>(-7)).value), -42);
  CHECK_EQ(static_cast<int32_t>(alu.execute(AluOp::MUL, static_cast<Word>(-6), static_cast<Word>(-7)).value), 42);

  // Larger values
  CHECK_EQ(alu.execute(AluOp::MUL, 12345, 67890).value, 12345u * 67890u);

  // Signed boundary
  constexpr Word kInt32Min = 0x80000000u;
  constexpr Word kInt32Max = 0x7FFFFFFFu;
  CHECK_EQ(alu.execute(AluOp::MUL, kInt32Min, 1).value, kInt32Min);
  CHECK_EQ(alu.execute(AluOp::MUL, kInt32Max, 1).value, kInt32Max);
}

RISC201_TEST(alu_barrel_shifter_edge_cases) {
  ALU alu;
  constexpr Word val = 0x12345678u;

  // Shift by 0
  CHECK_EQ(alu.execute(AluOp::LSL, val, 0).value, val);
  CHECK_EQ(alu.execute(AluOp::LSR, val, 0).value, val);
  CHECK_EQ(alu.execute(AluOp::ASR, val, 0).value, val);

  // Shift by 4
  CHECK_EQ(alu.execute(AluOp::LSL, val, 4).value, 0x23456780u);
  CHECK_EQ(alu.execute(AluOp::LSR, val, 4).value, 0x01234567u);
  CHECK_EQ(alu.execute(AluOp::ASR, val, 4).value, 0x01234567u);

  // Shift by 31
  CHECK_EQ(alu.execute(AluOp::LSL, 1, 31).value, 0x80000000u);
  CHECK_EQ(alu.execute(AluOp::LSR, 0x80000000u, 31).value, 1u);

  // Shift by >= 32 (masked by 0x1F)
  CHECK_EQ(alu.execute(AluOp::LSL, val, 32).value, val);
  CHECK_EQ(alu.execute(AluOp::LSL, val, 36).value, alu.execute(AluOp::LSL, val, 4).value);

  // ASR vs LSR on negative value
  constexpr Word neg = 0x80000000u;
  CHECK_EQ(alu.execute(AluOp::ASR, neg, 4).value, 0xF8000000u); // Sign bit propagated
  CHECK_EQ(alu.execute(AluOp::LSR, neg, 4).value, 0x08000000u); // Zeroes inserted
}

RISC201_TEST(alu_flags_edge_cases) {
  ALU alu;

  // Zero flag
  auto res_zero = alu.execute(AluOp::SUB, 42, 42);
  CHECK(res_zero.zero);
  CHECK(!res_zero.negative);

  // Negative flag
  auto res_neg = alu.execute(AluOp::SUB, 10, 20);
  CHECK(!res_neg.zero);
  CHECK(res_neg.negative);

  // Carry on addition
  auto res_carry = alu.execute(AluOp::ADD, 0xFFFFFFFFu, 1);
  CHECK(res_carry.carry);
  CHECK(res_carry.zero);
  CHECK(!res_carry.overflow);

  // Overflow on signed addition
  auto res_ovf = alu.execute(AluOp::ADD, 0x7FFFFFFFu, 1);
  CHECK(!res_ovf.carry);
  CHECK(res_ovf.overflow);
  CHECK(res_ovf.negative);

  // CMP sets flags without modifying output value (value = 0)
  auto res_cmp = alu.execute(AluOp::CMP, 100, 100);
  CHECK_EQ(res_cmp.value, 0u);
  CHECK(res_cmp.zero);
  CHECK(!res_cmp.negative);

  auto res_cmp_lt = alu.execute(AluOp::CMP, 50, 100);
  CHECK_EQ(res_cmp_lt.value, 0u);
  CHECK(!res_cmp_lt.zero);
  CHECK(res_cmp_lt.negative);
}

// =============================================================================
// 2. FPU EDGE CASES
// =============================================================================

RISC201_TEST(fpu_division_by_zero_throws) {
  FPU fpu;
  Word one = float_to_word(1.0f);
  Word pos_zero = float_to_word(0.0f);
  Word neg_zero = float_to_word(-0.0f);

  bool pos_threw = false;
  try {
    fpu.execute(FpuOp::FDIV, one, pos_zero);
  } catch (const RuntimeException& e) {
    if (e.code == ExceptionCode::DIV_BY_ZERO) pos_threw = true;
  }
  CHECK(pos_threw);

  bool neg_threw = false;
  try {
    fpu.execute(FpuOp::FDIV, one, neg_zero);
  } catch (const RuntimeException& e) {
    if (e.code == ExceptionCode::DIV_BY_ZERO) neg_threw = true;
  }
  CHECK(neg_threw);
}

RISC201_TEST(fpu_operations_and_flags_edge_cases) {
  FPU fpu;

  // Addition & Subtraction
  Word f1 = float_to_word(2.5f);
  Word f2 = float_to_word(1.25f);
  Word res_add = fpu.execute(FpuOp::FADD, f1, f2).value;
  CHECK_EQ(word_to_float(res_add), 3.75f);

  Word res_sub = fpu.execute(FpuOp::FSUB, f1, f2).value;
  CHECK_EQ(word_to_float(res_sub), 1.25f);

  // Subtraction to zero
  auto res_zero = fpu.execute(FpuOp::FSUB, f1, f1);
  CHECK(res_zero.zero);
  CHECK_EQ(word_to_float(res_zero.value), 0.0f);

  // Multiplication
  Word f_neg = float_to_word(-3.0f);
  Word f_pos = float_to_word(4.0f);
  auto res_mul = fpu.execute(FpuOp::FMUL, f_neg, f_pos);
  CHECK_EQ(word_to_float(res_mul.value), -12.0f);
  CHECK(res_mul.negative);
  CHECK(!res_mul.zero);

  // Comparison: FCMP (+0.0 vs -0.0) -> EQ
  Word pos_zero = float_to_word(0.0f);
  Word neg_zero = float_to_word(-0.0f);
  auto cmp_zero = fpu.execute(FpuOp::FCMP, pos_zero, neg_zero);
  CHECK_EQ(cmp_zero.fcc, 0u); // 0 = EQ
  CHECK(cmp_zero.zero);

  // Comparison: FCMP (1.0 vs 2.0) -> LT
  auto cmp_lt = fpu.execute(FpuOp::FCMP, float_to_word(1.0f), float_to_word(2.0f));
  CHECK_EQ(cmp_lt.fcc, 1u); // 1 = LT

  // Comparison: FCMP (2.0 vs 1.0) -> GT
  auto cmp_gt = fpu.execute(FpuOp::FCMP, float_to_word(2.0f), float_to_word(1.0f));
  CHECK_EQ(cmp_gt.fcc, 2u); // 2 = GT

  // Comparison: FCMP (-5.0 vs -2.0) -> LT
  auto cmp_neg = fpu.execute(FpuOp::FCMP, float_to_word(-5.0f), float_to_word(-2.0f));
  CHECK_EQ(cmp_neg.fcc, 1u); // -5.0 < -2.0
}

// =============================================================================
// 3. PIPELINE & HAZARDS EDGE CASES
// =============================================================================

RISC201_TEST(pipeline_r0_hardwired_zero_protection) {
  // Test across both 4-stage and 6-stage
  for (auto layout : {StageLayout::FOUR_STAGE, StageLayout::SIX_STAGE}) {
    PipelineProcessor cpu(layout, false);
    cpu.load({
        enc_imm(Opcode::ADD, 1, 0, 100),  // r1 = 100
        enc_reg(Opcode::ADD, 0, 1, 1),    // r0 = r1 + r1 = 200 (DISCARDED!)
        enc_reg(Opcode::ADD, 2, 0, 1),    // r2 = r0 + r1 -> MUST BE 0 + 100 = 100!
        enc_halt()
    });
    cpu.run();

    CHECK(cpu.halted());
    CHECK(!cpu.faulted());
    CHECK_EQ(cpu.read_reg(0), 0u);   // r0 remains 0
    CHECK_EQ(cpu.read_reg(1), 100u);
    CHECK_EQ(cpu.read_reg(2), 100u); // Forwarding did NOT bypass 200 into r0!
  }
}

RISC201_TEST(pipeline_double_raw_forwarding_both_operands) {
  // Test both operands forwarded simultaneously from different pipeline stages
  for (auto layout : {StageLayout::FOUR_STAGE, StageLayout::SIX_STAGE}) {
    PipelineProcessor cpu(layout, false);
    cpu.load({
        enc_imm(Opcode::ADD, 1, 0, 10),   // r1 = 10
        enc_imm(Opcode::ADD, 2, 0, 20),   // r2 = 20
        enc_reg(Opcode::ADD, 3, 1, 2),   // r3 = r1 + r2 = 30 (r2 from EX, r1 from CHK/MA)
        enc_halt()
    });
    cpu.run();

    CHECK(cpu.halted());
    CHECK(!cpu.faulted());
    CHECK_EQ(cpu.read_reg(1), 10u);
    CHECK_EQ(cpu.read_reg(2), 20u);
    CHECK_EQ(cpu.read_reg(3), 30u);
  }
}

RISC201_TEST(pipeline_load_use_hazard_before_conditional_branch) {
  // Load followed immediately by CMP then BEQ
  for (auto layout : {StageLayout::FOUR_STAGE, StageLayout::SIX_STAGE}) {
    PipelineProcessor cpu(layout, false);
    cpu.write_mem(0x100, 77);
    cpu.load({
        enc_mem(Opcode::LD, 1, 0, 0x100),  // r1 = 77
        enc_imm(Opcode::CMP, 0, 1, 77),    // CMP r1, 77 (Load-use hazard on r1!)
        enc_branch(Opcode::BEQ, 1),        // If equal, skip 1 instr (+4 bytes past next)
        enc_imm(Opcode::MOV, 2, 0, 999),   // Skipped if branch taken
        enc_imm(Opcode::MOV, 3, 0, 555),   // Target branch
        enc_halt()
    });
    cpu.run();

    CHECK(cpu.halted());
    CHECK(!cpu.faulted());
    CHECK_EQ(cpu.read_reg(1), 77u);
    CHECK_EQ(cpu.read_reg(2), 0u);   // Skipped!
    CHECK_EQ(cpu.read_reg(3), 555u); // Executed branch target!
  }
}

// =============================================================================
// 4. MEMORY SAFETY & SHADOW STACK EDGE CASES
// =============================================================================

RISC201_TEST(memory_safety_misaligned_access_throws) {
  risc201::Cpu cpu;
  // Odd byte address 0x10001 for word load
  cpu.write_reg(1, 0x10001);
  cpu.load({enc_mem(Opcode::LD, 2, 1, 0), enc_halt()});
  cpu.run();

  CHECK(cpu.halted());
  CHECK(cpu.faulted());
  CHECK(cpu.fault_code() == ExceptionCode::MISALIGNED_ACCESS);
}

RISC201_TEST(memory_safety_stack_overflow_exact_boundary) {
  // When SP == kStackLimit, a CALL attempts to push return address past stack limit -> STACK_OVERFLOW
  risc201::Cpu cpu;
  cpu.write_reg(REG_SP, MemoryMap::kStackLimit);
  cpu.load({enc_branch(Opcode::CALL, 0)});
  cpu.run();

  CHECK(cpu.halted());
  CHECK(cpu.faulted());
  CHECK(cpu.fault_code() == ExceptionCode::STACK_OVERFLOW);
  CHECK(cpu.fault_address() == MemoryMap::kStackLimit);
}

RISC201_TEST(memory_safety_shadow_return_stack_mismatch_detection) {
  risc201::Cpu cpu;
  // A CALL writes return address to stack memory AND pushes to shadow return stack.
  // Then we maliciously overwrite the return address on the stack before RET!
  cpu.load({
      enc_branch(Opcode::CALL, 2),        // 0x00: calls func at offset 2
      enc_halt(),                         // 0x04: return point
      // func:
      enc_imm(Opcode::MOV, 1, 0, 0x1234), // 0x08: bogus address
      enc_mem(Opcode::ST, 1, REG_SP, 0),  // 0x0C: corrupt return address in memory!
      enc_zero(Opcode::RET)               // 0x10: RET checks memory against shadow stack -> FAULT!
  });
  cpu.run();

  CHECK(cpu.halted());
  CHECK(cpu.faulted());
  CHECK(cpu.fault_code() == ExceptionCode::SHADOW_STACK_MISMATCH);
}

// =============================================================================
// 5. DEBUGGER TIME-TRAVEL & REVERSE STEPPING
// =============================================================================

RISC201_TEST(debugger_time_travel_full_fidelity) {
  risc201::Debugger dbg;
  dbg.write_reg(1, 0x10000);
  dbg.write_reg(2, 42);
  dbg.write_mem(0x10000, 0x11223344);
  dbg.load({
      enc_mem(Opcode::ST, 2, 1, 0),
      enc_halt()
  });

  // Step instruction: executes ST
  dbg.step_instruction();
  CHECK_EQ(dbg.cpu().read_mem(0x10000), 42u);

  // Step backward: undos ST, restoring memory
  CHECK(dbg.step_back_instruction());
  CHECK_EQ(dbg.cpu().read_mem(0x10000), 0x11223344u);

  // Forward step again
  dbg.step_instruction();
  CHECK_EQ(dbg.cpu().read_mem(0x10000), 42u);
}
