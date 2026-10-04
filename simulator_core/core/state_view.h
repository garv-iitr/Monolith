#pragma once

#include <array>
#include <cstdint>

#include "isa_spec/isa.h"
#include "simulator_core/core/exception.h"

namespace risc201 {

struct StateView {
  Word pc = 0;
  Word ir = 0;
  std::array<Word, NUM_REGISTERS> gprs{};
  ::Flags flags{};
  std::uint16_t micro_pc = 0;
  const char* stage = "";
  bool halted = false;
  const char* halt_reason = "";
  ExceptionCode fault_code = ExceptionCode::NONE;
  std::uint64_t fault_address = 0;
};

}  // namespace risc201
