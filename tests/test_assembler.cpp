#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <set>
#include <string>
#include <vector>

#include "isa_spec/isa.h"
#include "simulator_core/core/cpu.h"
#include "tests/test_framework.h"

namespace {

std::string find_risc201asm() {
  std::ifstream f1("risc201asm.exe");
  if (f1.is_open()) return "risc201asm.exe";
  std::ifstream f2("build\\risc201asm.exe");
  if (f2.is_open()) return "build\\risc201asm.exe";
  std::ifstream f3("./risc201asm");
  if (f3.is_open()) return "./risc201asm";
  std::ifstream f4("./build/risc201asm");
  if (f4.is_open()) return "./build/risc201asm";
  return "risc201asm";
}

std::string find_program_asm(const std::string& name) {
  std::ifstream f1("tests/programs/" + name);
  if (f1.is_open()) return "tests/programs/" + name;
  std::ifstream f2("../tests/programs/" + name);
  if (f2.is_open()) return "../tests/programs/" + name;
  return "tests/programs/" + name;
}

std::vector<Word> run_risc201asm(const std::string& asm_code) {
  const std::string asm_path = "test_temp.asm";
  const std::string hex_path = "test_temp.hex";
  std::ofstream out(asm_path);
  out << asm_code;
  out.close();

  const std::string asm_bin = find_risc201asm();
  const std::string cmd = asm_bin + " " + asm_path + " " + hex_path + " --hex";
  const int ret = std::system(cmd.c_str());
  if (ret != 0) {
    std::remove(asm_path.c_str());
    std::remove(hex_path.c_str());
    return {};
  }

  std::vector<Word> words;
  std::ifstream hex_in(hex_path);
  std::string line;
  while (std::getline(hex_in, line)) {
    const size_t first = line.find_first_not_of(" \t\r\n");
    if (first == std::string::npos || line[first] == '#') continue;
    try {
      const Word word = static_cast<Word>(std::stoul(line.substr(first, 8), nullptr, 16));
      words.push_back(word);
    } catch (...) {}
  }
  hex_in.close();
  std::remove(asm_path.c_str());
  std::remove(hex_path.c_str());
  return words;
}

std::vector<Word> assemble_file_to_words(const std::string& asm_filename, const std::string& hex_filename) {
  const std::string asm_path = find_program_asm(asm_filename);
  const std::string asm_bin = find_risc201asm();
  const std::string cmd = asm_bin + " " + asm_path + " " + hex_filename + " --hex";
  const int ret = std::system(cmd.c_str());
  if (ret != 0) return {};

  std::vector<Word> words;
  std::ifstream hex_in(hex_filename);
  std::string line;
  while (std::getline(hex_in, line)) {
    const size_t first = line.find_first_not_of(" \t\r\n");
    if (first == std::string::npos || line[first] == '#') continue;
    try {
      words.push_back(static_cast<Word>(std::stoul(line.substr(first, 8), nullptr, 16)));
    } catch (...) {}
  }
  hex_in.close();
  std::remove(hex_filename.c_str());
  return words;
}

}  // namespace

RISC201_TEST(assembler_straight_line_executes) {
  const auto code = run_risc201asm(
      "    mov r1, 6\n"
      "    mov r2, 7\n"
      "    mul r3, r1, r2\n"
      "    add r4, r1, r2\n"
      "    halt\n");
  CHECK_EQ(code.size(), 5u);

  risc201::Cpu cpu;
  cpu.load(code);
  cpu.run();

  CHECK_EQ(cpu.read_reg(1), Word{6});
  CHECK_EQ(cpu.read_reg(2), Word{7});
  CHECK_EQ(cpu.read_reg(3), Word{42});
  CHECK_EQ(cpu.read_reg(4), Word{13});
}

RISC201_TEST(assembler_branch_loop_executes) {
  const auto code = run_risc201asm(
      "    mov r1, 10\n"
      "    mov r2, 0\n"
      "loop:\n"
      "    cmp r1, 0\n"
      "    beq done\n"
      "    add r2, r2, r1\n"
      "    sub r1, r1, 1\n"
      "    b loop\n"
      "done:\n"
      "    halt\n");
  CHECK(!code.empty());

  risc201::Cpu cpu;
  cpu.load(code);
  cpu.run();

  CHECK_EQ(cpu.read_reg(2), Word{55});
}

RISC201_TEST(assembler_standalone_fib_executes) {
  const auto code = assemble_file_to_words("fib.asm", "test_fib.hex");
  CHECK(!code.empty());

  risc201::Cpu cpu;
  cpu.load(code);
  cpu.run();

  CHECK_EQ(cpu.read_reg(2), Word{55});
  CHECK_EQ(cpu.read_mem(0x10000), Word{55});
}

RISC201_TEST(assembler_standalone_fac_executes) {
  const auto code = assemble_file_to_words("fac.asm", "test_fac.hex");
  CHECK(!code.empty());

  risc201::Cpu cpu;
  cpu.load(code);
  cpu.run();

  // factorial(12) = 479,001,600 (0x1C8CFC00)
  CHECK_EQ(cpu.read_reg(2), Word{479001600});
  CHECK_EQ(cpu.read_mem(0x10000), Word{479001600});
}

RISC201_TEST(assembler_standalone_alu_ops_executes) {
  const auto code = assemble_file_to_words("alu_ops.asm", "test_alu_ops.hex");
  CHECK(!code.empty());

  risc201::Cpu cpu;
  cpu.load(code);
  cpu.run();

  // Verify memory outputs: all 12 arithmetic/shift results equal 42 (or -42 for ASR)
  CHECK_EQ(cpu.read_mem(0x1000), Word{42});  // ADD
  CHECK_EQ(cpu.read_mem(0x1004), Word{42});  // SUB
  CHECK_EQ(cpu.read_mem(0x1008), Word{42});  // MUL
  CHECK_EQ(cpu.read_mem(0x100C), Word{42});  // DIV
  CHECK_EQ(cpu.read_mem(0x1010), Word{42});  // MOD
  CHECK_EQ(cpu.read_mem(0x1014), Word{42});  // AND
  CHECK_EQ(cpu.read_mem(0x1018), Word{42});  // OR
  CHECK_EQ(cpu.read_mem(0x101C), Word{42});  // XOR
  CHECK_EQ(cpu.read_mem(0x1020), Word{42});  // NOT
  CHECK_EQ(cpu.read_mem(0x1024), Word{42});  // LSL
  CHECK_EQ(cpu.read_mem(0x1028), Word{42});  // LSR
  CHECK_EQ(cpu.read_mem(0x102C), static_cast<Word>(-42));  // ASR
  CHECK_EQ(cpu.read_mem(0x1030), Word{1});   // CMP pass sentinel
}

RISC201_TEST(assembler_standalone_control_flow_executes) {
  const auto code = assemble_file_to_words("control_flow.asm", "test_cf.hex");
  CHECK(!code.empty());

  risc201::Cpu cpu;
  cpu.load(code);
  cpu.run();

  CHECK_EQ(cpu.read_mem(0x1000), Word{0xCAFE});
}

RISC201_TEST(assembler_standalone_fpu_ops_executes) {
  const auto code = assemble_file_to_words("fpu_ops.asm", "test_fpu.hex");
  CHECK(!code.empty());

  risc201::Cpu cpu;
  cpu.load(code);
  cpu.run();

  CHECK_EQ(cpu.read_mem(0x100C), Word{0x41600000});  // FADD: 14.0f
  CHECK_EQ(cpu.read_mem(0x1010), Word{0x40C00000});  // FSUB: 6.0f
  CHECK_EQ(cpu.read_mem(0x1014), Word{0x41000000});  // FMUL: 8.0f
  CHECK_EQ(cpu.read_mem(0x1018), Word{0x40A00000});  // FDIV: 5.0f
  CHECK_EQ(cpu.read_mem(0x101C), Word{0x41600000});  // FMOV: 14.0f
  CHECK_EQ(cpu.read_mem(0x1020), Word{0xCAFE});      // FCMP / PASS
}

RISC201_TEST(assembler_standalone_memory_ops_executes) {
  const auto code = assemble_file_to_words("memory_ops.asm", "test_mem.hex");
  CHECK(!code.empty());

  risc201::Cpu cpu;
  cpu.load(code);
  cpu.run();

  CHECK_EQ(cpu.read_mem(0x1020), Word{10});
  CHECK_EQ(cpu.read_mem(0x102C), Word{40});
  CHECK_EQ(cpu.read_mem(0x1040), Word{0xCAFE});
}

RISC201_TEST(assembler_standalone_all_34_opcodes_executes) {
  const auto code = assemble_file_to_words("all_34_opcodes.asm", "test_all34.hex");
  CHECK(!code.empty());

  // Verify that all 34 opcodes are present in the assembled machine code
  std::set<std::uint8_t> used_opcodes;
  for (Word w : code) {
    used_opcodes.insert(get_opcode(w));
  }
  CHECK_EQ(used_opcodes.size(), static_cast<std::size_t>(34));

  risc201::Cpu cpu;
  cpu.load(code);
  cpu.run();

  CHECK_EQ(cpu.read_mem(0x1020), Word{0xCAFE});
}
