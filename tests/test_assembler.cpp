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
