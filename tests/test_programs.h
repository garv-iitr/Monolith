#ifndef RISC201_TEST_PROGRAMS_H
#define RISC201_TEST_PROGRAMS_H


#include <cstdint>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "isa_spec/isa.h"

namespace risc201::testbench {

struct TestProgram {
  std::string name;
  std::vector<Word> code;
  std::vector<std::pair<Word, Word>> data;
  std::vector<std::pair<Word, Word>> expected_mem;
  std::vector<std::pair<std::uint8_t, Word>> expected_regs;
};

TestProgram program_fib();

TestProgram program_matmul();

TestProgram program_sort();

std::set<std::uint8_t> opcodes_used(const std::vector<TestProgram>& programs);

const std::set<std::uint8_t>& all_opcodes();

}

#endif
