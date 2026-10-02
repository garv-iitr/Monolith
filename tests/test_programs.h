#ifndef RISC201_TEST_PROGRAMS_H
#define RISC201_TEST_PROGRAMS_H

// ============================================================================
// The three presentation test programs, encoded as RISC201 machine words.
// Assembly listings with expected outputs live in tests/programs/*.asm.
//
// Together they exercise all 34 opcodes of the ISA:
//   fib     : MOV CALL NOP ST HALT CMP BGT BNE B RET SUB LD ADD
//   matmul  : + BEQ BLT XOR MUL LSL FLD FST FADD FSUB FMUL FDIV FCMP FMOV
//   sort    : + BNDCHK MOD DIV AND OR LSR ASR NOT
// ============================================================================

#include <cstdint>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "isa_spec/isa.h"

namespace risc201::testbench {

struct TestProgram {
  std::string name;
  std::vector<Word> code;                             // instruction words, PC starts at 0
  std::vector<std::pair<Word, Word>> data;            // byte addr -> initial value
  std::vector<std::pair<Word, Word>> expected_mem;    // byte addr -> expected value
  std::vector<std::pair<std::uint8_t, Word>> expected_regs;  // GPR -> expected value
};

// Recursive Fibonacci: fib(10) = 55 (full-descending stack, sp = r14).
TestProgram program_fib();

// 2x2 integer matrix multiply + an FPU kernel (fadd/fsub/fmul/fdiv/fcmp/fmov).
TestProgram program_matmul();

// Bubble sort of 8 words with xor-swap + a post-sort checksum using
// mod/div/and/or/lsr/asr/not and a bndchk on the array range.
TestProgram program_sort();

// Union of opcodes used across the given programs.
std::set<std::uint8_t> opcodes_used(const std::vector<TestProgram>& programs);

// All 34 ISA opcodes (the coverage target for the suite).
const std::set<std::uint8_t>& all_opcodes();

}  // namespace risc201::testbench

#endif  // RISC201_TEST_PROGRAMS_H
