#include <cstdint>

#include "isa_spec/isa.h"
#include "simulator_core/core/register_file.h"
#include "tests/test_framework.h"

RISC201_TEST(regfile_initial_state) {
  RegisterFile rf;
  for (std::uint8_t i = 0; i < NUM_REGISTERS; ++i) {
    CHECK(rf.read(i) == 0u);
  }
  CHECK(rf.pc() == 0u);
  CHECK(rf.flags().E == false);
  CHECK(rf.flags().GT == false);
}

RISC201_TEST(regfile_read_write) {
  RegisterFile rf;
  rf.write(3, 0xABCDu);
  CHECK(rf.read(3) == 0xABCDu);
  rf.write(3, 7u);
  CHECK(rf.read(3) == 7u);
}

RISC201_TEST(regfile_pc) {
  RegisterFile rf;
  rf.set_pc(0x100u);
  CHECK(rf.pc() == 0x100u);
}

RISC201_TEST(regfile_sp_ra_aliases) {
  RegisterFile rf;
  rf.set_sp(0x1000u);
  rf.set_ra(0x40u);
  CHECK(rf.read(kRegSP) == 0x1000u);
  CHECK(rf.read(kRegRA) == 0x40u);
  CHECK(rf.sp() == 0x1000u);
  CHECK(rf.ra() == 0x40u);
}

RISC201_TEST(regfile_flags) {
  RegisterFile rf;
  rf.set_flags(true, false);
  CHECK(rf.flags().E == true);
  CHECK(rf.flags().GT == false);
}

RISC201_TEST(regfile_reset) {
  RegisterFile rf;
  rf.write(5, 99u);
  rf.set_pc(0x200u);
  rf.set_flags(true, true);
  rf.reset();
  CHECK(rf.read(5) == 0u);
  CHECK(rf.pc() == 0u);
  CHECK(rf.flags().E == false);
  CHECK(rf.flags().GT == false);
}
