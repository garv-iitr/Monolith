#ifndef RISC201_CU_REGISTER_FILE_UNIT_H
#define RISC201_CU_REGISTER_FILE_UNIT_H

#include <array>
#include <cstdint>

#include "common/control_signals.h"
#include "common/internal_registers.h"
#include "isa_spec/isa.h"

namespace risc201::cu {

struct RegisterFileUnit {
  std::array<Word, NUM_REGISTERS> regs{};

  void reset() { regs.fill(Word{0}); }

  // reg_src is a 4-bit field, the % is just a safety mask
  void tick(const InternalRegisters& cur, const ControlSignals& sig, InternalRegisters& next) {
    if (sig.reg_op == RegOp::READ) {
      next.reg_val = regs[cur.reg_src % NUM_REGISTERS];
    }
    if (sig.reg_op == RegOp::WRITE) {
      regs[cur.reg_src % NUM_REGISTERS] = cur.reg_data;
    }
  }

  Word read(std::uint8_t index) const { return regs[index % NUM_REGISTERS]; }
  void write(std::uint8_t index, Word value) { regs[index % NUM_REGISTERS] = value; }
};

}

#endif
