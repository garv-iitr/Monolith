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

RISC201_TEST(assembler_source_word_mapping_tracks_expansion_and_sections) {
  Assembler assembler;
  const AssemblyResult result = assembler.assemble(
      ".text\n"
      "start:\n"
      "    li r1, 0x12345678\n"
      "    add r2, r1, r1\n"
      "    .org 0x20\n"
      "    halt\n"
      ".data\n"
      "value: .word 7, 8\n"
      "    .space 8\n");

  CHECK(!result.hasErrors());
  CHECK_EQ(result.sourceWords.size(), 8u);
  CHECK_EQ(result.sourceWords[0].address, 0u);
  CHECK_EQ(result.sourceWords[0].sourceLine, 3);
  CHECK(result.sourceWords[0].executable);
  CHECK_EQ(result.sourceWords[1].address, 4u);
  CHECK_EQ(result.sourceWords[1].sourceLine, 3);
  CHECK(result.sourceWords[1].executable);
  CHECK_EQ(result.sourceWords[2].address, 8u);
  CHECK_EQ(result.sourceWords[2].sourceLine, 4);
  CHECK(result.sourceWords[2].executable);
  CHECK_EQ(result.sourceWords[3].address, 0x20u);
  CHECK_EQ(result.sourceWords[3].sourceLine, 6);
  CHECK(result.sourceWords[3].executable);
  CHECK_EQ(result.sourceWords[4].address, 0x10000u);
  CHECK_EQ(result.sourceWords[4].sourceLine, 8);
  CHECK(!result.sourceWords[4].executable);
  CHECK_EQ(result.sourceWords[5].address, 0x10004u);
  CHECK(!result.sourceWords[5].executable);
  CHECK_EQ(result.sourceWords[6].address, 0x10008u);
  CHECK_EQ(result.sourceWords[6].sourceLine, 9);
  CHECK(!result.sourceWords[6].executable);
  CHECK_EQ(result.sourceWords[7].address, 0x1000Cu);
  CHECK_EQ(result.sourceWords[7].sourceLine, 9);
  CHECK(!result.sourceWords[7].executable);
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
