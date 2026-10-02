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

Word enc_branch(Opcode op, int word_offset) {
  return (static_cast<Word>(op) << 26) | (static_cast<Word>(word_offset) & 0x03FFFFFF);
}

Word enc_zero(Opcode op) { return static_cast<Word>(op) << 26; }
Word enc_halt() { return static_cast<Word>(Opcode::HALT) << 26; }

void print_state(const char* label, const risc201::Cpu& cpu) {
  const risc201::StateView v = cpu.snapshot();
  std::printf("\n[%s] pc=0x%X ir=0x%08X microPC=%u stage=%s\n", label,
              static_cast<unsigned>(v.pc), static_cast<unsigned>(v.ir),
              static_cast<unsigned>(v.micro_pc), v.stage);
  std::printf("flags Z=%d N=%d C=%d V=%d\n", v.flags.Z, v.flags.N, v.flags.C, v.flags.V);
  for (std::uint8_t i = 0; i < NUM_REGISTERS; ++i)
    std::printf(" r%u=0x%X", static_cast<unsigned>(i), static_cast<unsigned>(v.gprs[i]));
  std::printf("\n");
  if (v.fault_code != ExceptionCode::NONE)
    std::printf("FAULT: %s @ 0x%llX\n", to_string(v.fault_code),
                static_cast<unsigned long long>(v.fault_address));
  else
    std::printf("halted: %s\n", (v.halt_reason[0] != '\0') ? v.halt_reason : "halt");
}

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
  print_state("transfer", cpu);

  risc201::Cpu bad;
  bad.load({enc_branch(Opcode::CALL, 0)});
  bad.run();
  print_state("bad-access", bad);

  return 0;
}
