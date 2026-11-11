#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <set>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#else
#include <unistd.h>
#endif

#include "isa_spec/isa.h"
#include "simulator_core/core/cpu.h"
#include "tests/test_framework.h"

namespace {

// locate the sibling assembler via our own path so the tests do not depend on the CWD
std::string executable_dir() {
  std::string self;
#ifdef _WIN32
  char buffer[MAX_PATH] = {0};
  const DWORD written = GetModuleFileNameA(nullptr, buffer, MAX_PATH);
  self.assign(buffer, written > 0 ? static_cast<std::size_t>(written) : 0);
#elif defined(__APPLE__)
  std::uint32_t size = 0;
  _NSGetExecutablePath(nullptr, &size);
  if (size > 0) {
    std::vector<char> buffer(size, '\0');
    if (_NSGetExecutablePath(buffer.data(), &size) == 0) {
      self = buffer.data();
    }
  }
#else
  char buffer[4096] = {0};
  const ssize_t written = ::readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
  self.assign(buffer, written > 0 ? static_cast<std::size_t>(written) : 0);
#endif
  const std::size_t slash = self.find_last_of("/\\");
  return slash == std::string::npos ? std::string() : self.substr(0, slash + 1);
}

std::string find_risc201asm() {
  const std::string here = executable_dir();
  const std::vector<std::string> candidates = {
      here + "risc201asm", here + "risc201asm.exe",
      "risc201asm", "risc201asm.exe",
      "build/risc201asm", "build/risc201asm.exe",
      "build/Release/risc201asm.exe", "build/Debug/risc201asm.exe",
  };
  for (const auto& path : candidates) {
    std::ifstream probe(path);
    if (probe.good()) return path;
  }
  return "risc201asm";
}

std::string find_program_asm(const std::string& name) {
  std::vector<std::string> prefixes = {"tests/programs/", "../tests/programs/"};
#ifdef RISC201_SOURCE_DIR
  prefixes.emplace_back(std::string(RISC201_SOURCE_DIR) + "/tests/programs/");
#endif
  for (const auto& prefix : prefixes) {
    std::ifstream probe(prefix + name);
    if (probe.is_open()) return prefix + name;
  }
  return "tests/programs/" + name;
}

int run_shell_command(const std::string& cmd) {
#ifdef _WIN32
  // cmd.exe /c strips outer quotes when a command starts with '"'.
  const std::string wrapped = "\"" + cmd + "\"";
  return std::system(wrapped.c_str());
#else
  return std::system(cmd.c_str());
#endif
}

std::vector<Word> invoke_assembler_and_read_hex(const std::string& asm_path, const std::string& hex_path) {
  const std::string asm_bin = find_risc201asm();
  const std::string cmd = "\"" + asm_bin + "\" \"" + asm_path + "\" \"" + hex_path + "\" --hex";
  if (run_shell_command(cmd) != 0) return {};

  std::vector<Word> words;
  std::ifstream hex_in(hex_path);
  std::string line;
  while (std::getline(hex_in, line)) {
    const size_t first = line.find_first_not_of(" \t\r\n");
    if (first == std::string::npos || line[first] == '#') continue;
    try {
      words.push_back(static_cast<Word>(std::stoul(line.substr(first, 8), nullptr, 16)));
    } catch (...) {}
  }
  return words;
}

std::vector<Word> run_risc201asm(const std::string& asm_code) {
  const std::string asm_path = "test_temp.asm";
  const std::string hex_path = "test_temp.hex";
  {
    std::ofstream out(asm_path);
    out << asm_code;
  }
  const auto words = invoke_assembler_and_read_hex(asm_path, hex_path);
  std::remove(asm_path.c_str());
  std::remove(hex_path.c_str());
  return words;
}

std::vector<Word> assemble_file_to_words(const std::string& asm_filename, const std::string& hex_filename) {
  const std::string asm_path = find_program_asm(asm_filename);
  const auto words = invoke_assembler_and_read_hex(asm_path, hex_filename);
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
