#pragma once

#include <cstdint>
#include "control_unit/control_rom.h"
#include "isa_spec/isa.h"

namespace risc201::pipeline {

// =============================================================================
// PIPELINE MICROCODE SEQUENCER STATE MACHINE
// =============================================================================
// Implements Method 3: Hybrid u-Op Cracking Sequencer.
// - Single-cycle RISC instructions pass through with zero latency (CPI = 1.0).
// - Multi-uop instructions (e.g. CALL, RET) assert 'freeze_if_of' and sequence
//   through microcode dispatch steps until 'is_last_uop' is asserted.
// =============================================================================

class MicroSequencer {
 public:
  MicroSequencer() { reset(); }

  void reset() {
    cracking_ = false;
    freeze_active_ = false;
    active_op_ = Opcode::NOP;
    uop_step_ = 0;
    micro_pc_ = 0;
    saved_instruction_ = 0;
  }

  // Tests if an opcode requires multi-uop cracking sequence
  static constexpr bool is_complex_op(Opcode op) {
    return (op == Opcode::CALL || op == Opcode::RET);
  }

  // Dispatches a new macro-instruction into the sequencer
  void dispatch(Opcode op, Word inst, bool is_last_uop) {
    if (is_complex_op(op) && !is_last_uop) {
      cracking_ = true;
      freeze_active_ = true;
      active_op_ = op;
      saved_instruction_ = inst;
      uop_step_ = 1;
      micro_pc_ = static_cast<std::uint16_t>(rom_address(op, 1));
    } else {
      cracking_ = false;
      freeze_active_ = false;
      active_op_ = Opcode::NOP;
      uop_step_ = 0;
      saved_instruction_ = 0;
      micro_pc_ = static_cast<std::uint16_t>(rom_address(op, 0));
    }
  }

  // Advances the sequencer by one clock cycle
  void step_cycle(bool is_last_uop) {
    if (!cracking_) return;

    if (is_last_uop) {
      // Completed last micro-op of cracked instruction; release pipeline freeze
      cracking_ = false;
      freeze_active_ = false;
      active_op_ = Opcode::NOP;
      uop_step_ = 0;
      saved_instruction_ = 0;
    } else {
      // Continue cracking sequence; keep IF/OF frozen
      uop_step_++;
      freeze_active_ = true;
      micro_pc_ = static_cast<std::uint16_t>(rom_address(active_op_, uop_step_));
    }
  }

  // Inspection accessors
  bool is_cracking() const { return cracking_; }
  bool freeze_pipeline() const { return freeze_active_; }
  Opcode active_op() const { return active_op_; }
  std::uint16_t uop_step() const { return uop_step_; }
  std::uint16_t micro_pc() const { return micro_pc_; }
  Word saved_instruction() const { return saved_instruction_; }

 private:
  bool cracking_ = false;
  bool freeze_active_ = false;
  Opcode active_op_ = Opcode::NOP;
  std::uint16_t uop_step_ = 0;
  std::uint16_t micro_pc_ = 0;
  Word saved_instruction_ = 0;
};

}  // namespace risc201::pipeline
