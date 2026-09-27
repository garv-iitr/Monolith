#include <limits>

#include "alu/alu.h"
#include "isa_spec/isa.h"
#include "simulator_core/core/exception.h"
#include "tests/test_framework.h"

RISC201_TEST(alu_add_sub) {
  SimpleAlu alu;
  CHECK(alu.execute(ALUOp::ADD, 2u, 3u).value == 5u);

  const AluResult s = alu.execute(ALUOp::SUB, 5u, 3u);
  CHECK(s.value == 2u);
  CHECK(!s.zero);
  CHECK(!s.negative);

  CHECK(alu.execute(ALUOp::SUB, 5u, 5u).zero);
}

RISC201_TEST(alu_carry_and_overflow) {
  SimpleAlu alu;
  const Word wmax = std::numeric_limits<Word>::max();
  const Word smax = static_cast<Word>(std::numeric_limits<Sword>::max());
  const Word smin = static_cast<Word>(std::numeric_limits<Sword>::min());

  const AluResult wrap = alu.execute(ALUOp::ADD, wmax, 1u);
  CHECK(wrap.value == 0u);
  CHECK(wrap.zero);
  CHECK(wrap.carry);

  CHECK(alu.execute(ALUOp::ADD, smax, 1u).overflow);
  CHECK(alu.execute(ALUOp::SUB, smin, 1u).overflow);
  CHECK(!alu.execute(ALUOp::ADD, 1u, 1u).overflow);
}

RISC201_TEST(alu_mul_div_mod) {
  SimpleAlu alu;
  CHECK(alu.execute(ALUOp::MUL, 6u, 7u).value == 42u);
  CHECK(alu.execute(ALUOp::DIV, 20u, 3u).value == 6u);
  CHECK(alu.execute(ALUOp::MOD, 20u, 3u).value == 2u);

  bool threw = false;
  try {
    alu.execute(ALUOp::DIV, 1u, 0u);
  } catch (const RuntimeException& e) {
    threw = true;
    CHECK(e.code == ExceptionCode::DIV_BY_ZERO);
  }
  CHECK(threw);

  threw = false;
  try {
    alu.execute(ALUOp::MOD, 1u, 0u);
  } catch (const RuntimeException&) {
    threw = true;
  }
  CHECK(threw);
}

RISC201_TEST(alu_logical) {
  SimpleAlu alu;
  CHECK(alu.execute(ALUOp::AND, 0xF0u, 0x0Fu).value == 0u);
  CHECK(alu.execute(ALUOp::OR, 0xF0u, 0x0Fu).value == 0xFFu);
  CHECK(alu.execute(ALUOp::NOT, 0u, 0x0Fu).value == ~static_cast<Word>(0x0Fu));
}

RISC201_TEST(alu_shifts) {
  SimpleAlu alu;
  CHECK(alu.execute(ALUOp::LSL, 1u, 4u).value == 16u);
  CHECK(alu.execute(ALUOp::LSR, 16u, 4u).value == 1u);
  CHECK(alu.execute(ALUOp::ASR, static_cast<Word>(-8), 1u).value ==
        static_cast<Word>(-4));
}

RISC201_TEST(alu_compare) {
  SimpleAlu alu;
  CHECK(alu.execute(ALUOp::CMP, 5u, 5u).zero);

  const AluResult gt = alu.execute(ALUOp::CMP, 7u, 3u);
  CHECK(!gt.zero && !gt.negative);

  CHECK(alu.execute(ALUOp::CMP, 3u, 7u).negative);
}

RISC201_TEST(alu_passthrough) {
  SimpleAlu alu;
  CHECK(alu.execute(ALUOp::PASS_A, 42u, 7u).value == 42u);
  CHECK(alu.execute(ALUOp::NONE, 1u, 2u).value == 0u);
}
