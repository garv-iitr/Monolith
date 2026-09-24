#include <cstdint>
#include <string>

#include "isa_spec/isa.h"
#include "tests/test_framework.h"

using namespace risc201;

RISC201_TEST(isa_width_constants) {
  CHECK(kWordBits == RISC201_WORD_BITS);
  CHECK(kWordBytes * 8 == kWordBits);
  CHECK(kNumRegs == 16);
  CHECK(kNumOpcodes == 21);
  CHECK(kInstrBits == 32);
  static_assert(kWordBits == 32 || kWordBits == 64, "width must be 32 or 64");
}

RISC201_TEST(isa_sign_extend) {
  CHECK(sign_extend<16>(0x0001) == 1);
  CHECK(sign_extend<16>(0x7FFF) == 32767);
  CHECK(sign_extend<16>(0x8000) == -32768);
  CHECK(sign_extend<16>(0xFFFF) == -1);
}

RISC201_TEST(isa_expand_immediate) {
  CHECK(expand_immediate(kModDefault, 0x0003) == 3);
  CHECK(expand_immediate(kModDefault, 0xFFFE) == static_cast<word_t>(-2));
  CHECK(expand_immediate(kModUnsigned, 0xFEAB) == 0xFEAB);
  CHECK(expand_immediate(kModHigh, 0xFEAB) ==
        static_cast<word_t>(std::uint64_t{0xFEAB} << 16));
}

// Worked encodings straight from the SimpleRISC chapter.
RISC201_TEST(isa_field_layout) {
  const instr_t sub = 0x0C480003u;  // sub r1, r2, 3
  CHECK(field(sub, kOpShift, kOpMask) == static_cast<instr_t>(Op::SUB));
  CHECK(field(sub, kIBitShift, kIMask) == 1);
  CHECK(field(sub, kRdShift, kRegMask) == 1);
  CHECK(field(sub, kRs1Shift, kRegMask) == 2);
  CHECK(field(sub, kConstShift, kConstMask) == 3);

  const instr_t st = 0x7E080014u;  // st r8, 20[r2]
  CHECK(field(st, kOpShift, kOpMask) == static_cast<instr_t>(Op::ST));
  CHECK(field(st, kIBitShift, kIMask) == 1);
  CHECK(field(st, kRdShift, kRegMask) == 8);
  CHECK(field(st, kRs1Shift, kRegMask) == 2);
  CHECK(field(st, kConstShift, kConstMask) == 20);
}

RISC201_TEST(isa_opcode_metadata) {
  CHECK(std::string(mnemonic(Op::ADD)) == "add");
  CHECK(std::string(mnemonic(Op::ST)) == "st");
  CHECK(std::string(mnemonic(Op::RET)) == "ret");

  CHECK(is_branch_format(Op::B));
  CHECK(is_branch_format(Op::BEQ));
  CHECK(is_branch_format(Op::RET));
  CHECK(is_branch_format(Op::NOP));
  CHECK(!is_branch_format(Op::ADD));
  CHECK(!is_branch_format(Op::LD));

  CHECK(kStackPointer == Reg::R14);
  CHECK(kReturnAddress == Reg::R15);
}
