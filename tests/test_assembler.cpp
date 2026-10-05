#include "assembler.hpp"

#include "simulator_core/core/cpu.h"
#include "tests/test_framework.h"

RISC201_TEST(assembler_straight_line_executes) {
  Assembler assembler;
  AssemblyResult result = assembler.assemble(
      "    mov r1, #6\n"
      "    mov r2, #7\n"
      "    mul r3, r1, r2\n"
      "    add r4, r1, r2\n"
      "    halt\n");
  CHECK(!result.hasErrors());
  CHECK_EQ(result.code.size(), 5u);

  risc201::Cpu cpu;
  cpu.load(result.code);
  cpu.run();

  CHECK_EQ(cpu.read_reg(1), Word{6});
  CHECK_EQ(cpu.read_reg(2), Word{7});
  CHECK_EQ(cpu.read_reg(3), Word{42});
  CHECK_EQ(cpu.read_reg(4), Word{13});
}

RISC201_TEST(assembler_branch_loop_executes) {
  Assembler assembler;
  AssemblyResult result = assembler.assemble(
      "    mov r1, #10\n"
      "    mov r2, #0\n"
      "loop:\n"
      "    cmp r1, #0\n"
      "    beq done\n"
      "    add r2, r2, r1\n"
      "    sub r1, r1, #1\n"
      "    b loop\n"
      "done:\n"
      "    halt\n");
  CHECK(!result.hasErrors());

  risc201::Cpu cpu;
  cpu.load(result.code);
  cpu.run();

  CHECK_EQ(cpu.read_reg(2), Word{55});
}
