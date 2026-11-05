// Architectural register file unit providing synchronous read and write ports for 16 general-purpose registers.
#ifndef RISC201_CU_REGISTER_FILE_UNIT_H
#define RISC201_CU_REGISTER_FILE_UNIT_H

#include <array>
#include <cstdint>

#include "common/control_signals.h"
#include "common/internal_registers.h"
#include "isa_spec/isa.h"

namespace risc201::cu {

// Emulated 16-entry general-purpose register file (r0 through r15)
struct RegisterFileUnit {
  std::array<Word, NUM_REGISTERS> regs{}; // Storage array for registers r0..r15

  // Zeroes all 16 general-purpose registers
  void reset() { regs.fill(Word{0}); }

  // Hardware clock cycle step for register read and write strobes:
  // - READ: Reads register at index cur.reg_src into next.reg_val
  // - WRITE: Commits data from cur.reg_data into register at index cur.reg_src
  // reg_src is a 4-bit field, the % is just a safety mask
  void tick(const InternalRegisters& cur, const ControlSignals& sig, InternalRegisters& next) {
    if (sig.reg_op == RegOp::READ) {
      next.reg_val = regs[cur.reg_src % NUM_REGISTERS];
    }
    if (sig.reg_op == RegOp::WRITE) {
      regs[cur.reg_src % NUM_REGISTERS] = cur.reg_data;
    }
  }

  // Direct read accessor by register index (used for debuggers and state verification)
  Word read(std::uint8_t index) const { return regs[index % NUM_REGISTERS]; }
  // Direct write mutator by register index (used for initial state seeding and debugging)
  void write(std::uint8_t index, Word value) { regs[index % NUM_REGISTERS] = value; }
};

}

#endif
