#pragma once

#include <array>
#include <cstdint>

#include "isa_spec/isa.h"

constexpr std::uint8_t kRegSP = 14;
constexpr std::uint8_t kRegRA = 15;

struct Flags {
  bool E  = false;
  bool GT = false;
};

class RegisterFile {
 public:
  RegisterFile();

  void reset();

  Word read(std::uint8_t index) const;
  void write(std::uint8_t index, Word value);

  Word pc() const { return pc_; }
  void set_pc(Word value) { pc_ = value; }

  Word sp() const { return regs_[kRegSP]; }
  void set_sp(Word value) { regs_[kRegSP] = value; }
  Word ra() const { return regs_[kRegRA]; }
  void set_ra(Word value) { regs_[kRegRA] = value; }

  const Flags& flags() const { return flags_; }
  void set_flags(bool equal, bool greater_than) {
    flags_.E = equal;
    flags_.GT = greater_than;
  }

 private:
  std::array<Word, NUM_REGISTERS> regs_{};
  Word pc_ = 0;
  Flags flags_{};
};
