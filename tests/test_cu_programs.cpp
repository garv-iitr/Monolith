// ============================================================================
// Control-unit program tests.
//
// Runs the three presentation programs (recursive Fibonacci, matrix multiply
// + FPU, bubble sort + checksum) through all four configurations
//   {4-stage, 6-stage} x {horizontal, vertical}
// and checks that each configuration halts without faulting and produces the
// exact expected memory / register state.  Also verifies that the suite
// exercises every one of the 34 ISA opcodes.
//
// The ALU / FPU / memory-check units belong to Atharva and Paraas; until their
// modules land, the harness drives the control unit with the reference models
// in tests/test_reference_units.h (the same args contract).
// ============================================================================

#include <array>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "control_unit.h"
#include "isa_spec/isa.h"
#include "simulator_core/core/exception.h"
#include "tests/test_framework.h"
#include "tests/test_programs.h"
#include "tests/test_reference_units.h"

using namespace risc201::cu;
using risc201::testbench::TestProgram;

namespace {

template <typename CU> struct RomBuilder;
template <> struct RomBuilder<HorizontalControlUnit> {
  static ControlMemory<HorizontalMicroWord> build(const Program& p) { return assemble_horizontal(p); }
};
template <> struct RomBuilder<VerticalControlUnit> {
  static ControlMemory<VerticalMicroWord> build(const Program& p) { return assemble_vertical(p); }
};

struct Outcome {
  bool halted = false;
  bool faulted = false;
  std::string reason;
  int cycles = 0;
  std::vector<std::pair<Word, Word>> mem;                 // probed memory state
  std::array<Word, NUM_REGISTERS> regs{};                 // final GPRs
};

// Execute one program on one configuration, driving the reference units after
// every micro-cycle (same order as the datapath used to: rf, mem, alu, fpu,
// memchk -- rf and mem live inside Datapath::execute, the rest are applied
// here).
template <typename CU>
Outcome run_one(const TestProgram& tp, StageLayout layout) {
  Datapath dp(4096);
  dp.mem.load_words(0, tp.code);
  for (const auto& d : tp.data) dp.mem.write_word(d.first, d.second);

  const Program program = build_program();
  CU cu(dp, layout);
  cu.install(RomBuilder<CU>::build(program));

  risc201::testbench::RefAlu alu;
  risc201::testbench::RefFpu fpu;
  risc201::testbench::RefMemChk memchk;

  Outcome out;
  while (out.cycles < 500000 && !dp.halted) {
    try {
      if (!cu.step()) break;
      if (!dp.halted) {
        const ControlSignals& sig = cu.signals();
        alu.tick(dp.regs, sig, dp.regs);
        fpu.tick(dp.regs, sig, dp.regs);
        memchk.tick(dp.regs, sig, dp.regs, dp.mem.words());
      }
    } catch (const RuntimeException& e) {
      out.faulted = true;
      out.reason = to_string(e.code);
      break;
    }
    ++out.cycles;
  }

  out.halted = dp.halted;
  if (out.reason.empty()) out.reason = dp.halt_reason;
  for (int i = 0; i < NUM_REGISTERS; ++i) out.regs[i] = dp.rf.read(static_cast<std::uint8_t>(i));

  for (const auto& e : tp.expected_mem) out.mem.emplace_back(e.first, dp.mem.read_word(e.first));
  return out;
}

bool same_state(const Outcome& a, const Outcome& b) {
  if (a.mem != b.mem) return false;
  return a.regs == b.regs;
}

void check_outcome(const TestProgram& tp, const Outcome& o, const char* config) {
  CHECK(o.halted);
  CHECK(!o.faulted);
  if (!o.faulted) CHECK_EQ(o.reason, std::string(""));
  for (const auto& e : tp.expected_mem) {
    const Word got = [&] {
      for (const auto& m : o.mem)
        if (m.first == e.first) return m.second;
      return Word{0};
    }();
    if (got != e.second) {
      std::printf("  FAIL: %s mem[0x%X] = 0x%X, expected 0x%X (%s)\n", config,
                  static_cast<unsigned>(e.first), static_cast<unsigned>(got),
                  static_cast<unsigned>(e.second), tp.name.c_str());
      ++risc201_test::failure_count();
    }
  }
  for (const auto& e : tp.expected_regs) {
    if (o.regs[e.first] != e.second) {
      std::printf("  FAIL: %s r%u = 0x%X, expected 0x%X (%s)\n", config,
                  static_cast<unsigned>(e.first), static_cast<unsigned>(o.regs[e.first]),
                  static_cast<unsigned>(e.second), tp.name.c_str());
      ++risc201_test::failure_count();
    }
  }
}

// Run one program across all four configurations and verify identical results.
void check_program(const TestProgram& tp) {
  std::printf("\n--- %s ---\n", tp.name.c_str());
  const Outcome hs6 = run_one<HorizontalControlUnit>(tp, StageLayout::SIX_STAGE);
  const Outcome vs6 = run_one<VerticalControlUnit>(tp, StageLayout::SIX_STAGE);
  const Outcome hs4 = run_one<HorizontalControlUnit>(tp, StageLayout::FOUR_STAGE);
  const Outcome vs4 = run_one<VerticalControlUnit>(tp, StageLayout::FOUR_STAGE);

  check_outcome(tp, hs6, "horizontal 6-stage");
  check_outcome(tp, vs6, "vertical 6-stage");
  check_outcome(tp, hs4, "horizontal 4-stage");
  check_outcome(tp, vs4, "vertical 4-stage");

  CHECK(same_state(hs6, vs6));
  CHECK(same_state(hs6, hs4));
  CHECK(same_state(hs6, vs4));

  std::printf("  cycles: h/6=%d v/6=%d h/4=%d v/4=%d\n", hs6.cycles, vs6.cycles,
              hs4.cycles, vs4.cycles);
}

const std::vector<TestProgram>& suite() {
  static const std::vector<TestProgram> s = {
      risc201::testbench::program_fib(),
      risc201::testbench::program_matmul(),
      risc201::testbench::program_sort(),
  };
  return s;
}

}  // namespace

RISC201_TEST(cu_program_recursive_fibonacci) { check_program(suite()[0]); }

RISC201_TEST(cu_program_matrix_multiply_fpu) { check_program(suite()[1]); }

RISC201_TEST(cu_program_bubble_sort_checksum) { check_program(suite()[2]); }

RISC201_TEST(cu_suite_covers_all_34_opcodes) {
  const auto used = risc201::testbench::opcodes_used(suite());
  CHECK_EQ(used.size(), static_cast<std::size_t>(34));
  for (std::uint8_t op : risc201::testbench::all_opcodes()) {
    if (used.find(op) == used.end()) {
      std::printf("  FAIL: opcode 0x%02X never used by the suite\n", static_cast<unsigned>(op));
      ++risc201_test::failure_count();
    }
  }
}
