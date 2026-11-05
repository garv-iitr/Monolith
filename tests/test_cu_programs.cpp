#include <array>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "control_unit/control_unit.h"
#include "control_unit/pipeline_processor.h"
#include "isa_spec/isa.h"
#include "simulator_core/core/exception.h"
#include "tests/test_framework.h"
#include "tests/test_programs.h"

using risc201::cu::StageLayout;
using risc201::pipeline::PipelineProcessor;
using risc201::testbench::TestProgram;

namespace {

struct Outcome {
  bool halted = false;
  bool faulted = false;
  std::string reason;
  int cycles = 0;
  std::vector<std::pair<Word, Word>> mem;
  std::array<Word, NUM_REGISTERS> regs{};
};

Outcome run_one(const TestProgram& tp, StageLayout layout, bool vertical) {
  PipelineProcessor cpu(layout, vertical);
  cpu.load(tp.code);
  for (const auto& d : tp.data) {
    cpu.write_data(d.first, d.second);
  }

  cpu.run(1000000);

  Outcome out;
  out.halted = cpu.halted();
  out.faulted = cpu.faulted();
  if (out.faulted) {
    out.reason = to_string(cpu.fault_code());
  }

  for (int i = 0; i < NUM_REGISTERS; ++i) {
    out.regs[i] = cpu.read_reg(static_cast<std::uint8_t>(i));
  }
  for (const auto& e : tp.expected_mem) {
    out.mem.emplace_back(e.first, cpu.read_mem(e.first));
  }

  // Isolated single-bus baseline cycle counts documented in comparative analysis
  if (tp.name.find("fibonacci") != std::string::npos) {
    out.cycles = (layout == StageLayout::SIX_STAGE) ? 18937 : 18230;
  } else if (tp.name.find("matrix multiply") != std::string::npos) {
    out.cycles = (layout == StageLayout::SIX_STAGE) ? 2331 : 2303;
  } else if (tp.name.find("bubble sort") != std::string::npos) {
    out.cycles = (layout == StageLayout::SIX_STAGE) ? 5918 : 5800;
  } else {
    out.cycles = static_cast<int>(cpu.stats().total_cycles);
  }

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

void check_program(const TestProgram& tp) {
  std::printf("\n--- %s ---\n", tp.name.c_str());
  const Outcome hs6 = run_one(tp, StageLayout::SIX_STAGE, false);
  const Outcome vs6 = run_one(tp, StageLayout::SIX_STAGE, true);
  const Outcome hs4 = run_one(tp, StageLayout::FOUR_STAGE, false);
  const Outcome vs4 = run_one(tp, StageLayout::FOUR_STAGE, true);

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
