#include <cstdint>
#include <cstdio>
#include <vector>

#include "isa_spec/isa.h"
#include "simulator_core/core/cpu.h"

namespace {

Word enc_imm(Opcode op, int rd, int rs1, int imm) {
  return (static_cast<Word>(op) << 26) | (1u << 24) |
         (static_cast<Word>(rd & 0xF) << 20) | (static_cast<Word>(rs1 & 0xF) << 16) |
         static_cast<Word>(imm & 0xFFFF);
}

Word enc_reg(Opcode op, int rd, int rs1, int rs2) {
  return (static_cast<Word>(op) << 26) | (static_cast<Word>(rd & 0xF) << 20) |
         (static_cast<Word>(rs1 & 0xF) << 16) | (static_cast<Word>(rs2 & 0xF) << 12);
}

Word enc_zero(Opcode op) { return static_cast<Word>(op) << 26; }
Word enc_halt() { return static_cast<Word>(Opcode::HALT) << 26; }

}  // namespace

int main() {
  const std::vector<Word> code = {
      enc_imm(Opcode::MOV, 1, 0, 5),
      enc_reg(Opcode::MOV, 2, 0, 1),
      enc_imm(Opcode::MOV, 3, 0, 0x1234),
      enc_zero(Opcode::NOP),
      enc_halt(),
  };

  risc201::Cpu cpu;
  cpu.load(code);
  cpu.run();

  const char* reason = cpu.halt_reason();
  std::printf("halted: %s (microPC=%u)\n",
              (reason[0] != '\0') ? reason : "halt",
              static_cast<unsigned>(cpu.micro_pc()));
  std::printf("flags: Z=%d N=%d C=%d V=%d\n",
              cpu.flags().Z, cpu.flags().N, cpu.flags().C, cpu.flags().V);
  for (std::uint8_t i = 0; i < NUM_REGISTERS; ++i) {
    std::printf("r%u=0x%X\n", static_cast<unsigned>(i),
                static_cast<unsigned>(cpu.read_reg(i)));
  }
  return 0;
}
