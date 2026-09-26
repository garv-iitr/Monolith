#include "simulator_core/core/register_file.h"

#include <cassert>

RegisterFile::RegisterFile() { reset(); }

void RegisterFile::reset() {
  regs_.fill(Word{0});
  pc_ = 0;
  flags_ = Flags{};
}

Word RegisterFile::read(std::uint8_t index) const {
  assert(index < NUM_REGISTERS);
  return regs_[index];
}

void RegisterFile::write(std::uint8_t index, Word value) {
  assert(index < NUM_REGISTERS);
  regs_[index] = value;
}
