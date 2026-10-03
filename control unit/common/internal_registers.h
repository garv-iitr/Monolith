#ifndef RISC201_CU_INTERNAL_REGISTERS_H
#define RISC201_CU_INTERNAL_REGISTERS_H

// ============================================================================
// Every register visible to a microprogram (Sarangi, Table 9.7) plus the
// RISC201 floating-point and memory-check extensions.
//
// These registers are the latches that sit between the shared bus and the
// functional units: a microinstruction can move a value from one of them to
// another, and the functional units read/write them to do real work.
// ============================================================================

#include <cstdint>

#include "isa_spec/isa.h"

namespace risc201::cu {

struct InternalRegisters {
  // --- fetch unit -----------------------------------------------------------
  Word pc = 0;
  Word ir = 0;

  // --- decode unit (combinational outputs, refreshed from `ir`) -------------
  bool      I = false;
  std::uint8_t rd = 0;
  std::uint8_t rs1 = 0;
  std::uint8_t rs2 = 0;
  Word      immx = 0;
  Word      branch_target = 0;

  // --- register-file interface ---------------------------------------------
  std::uint8_t reg_src = 0;
  Word         reg_data = 0;
  Word         reg_val = 0;

  // --- ALU ------------------------------------------------------------------
  Word A = 0;
  Word B = 0;
  Word alu_result = 0;
  Flags flags{};                 // canonical architectural flags (Z N C V)
  bool  flags_E = false;         // == flags.Z            (branch on equal)
  bool  flags_GT = false;        // == !N && !Z           (branch on greater)

  // --- memory unit ----------------------------------------------------------
  Word mar = 0;
  Word mdr = 0;
  Word ld_result = 0;

  // --- FPU ------------------------------------------------------------------
  std::uint8_t fp_src = 0;       // FPR index being accessed
  Word         fp_data = 0;      // value to write into an FPR
  Word         fp_val = 0;       // value read out of an FPR
  Word         fp_a = 0;         // first FP operand
  Word         fp_b = 0;         // second FP operand
  Word         fpu_result = 0;
  bool         fcc = false;      // floating-point condition code

  // --- memory-check unit (6-stage) -----------------------------------------
  Word memchk_addr = 0;
  bool memchk_status = false;    // true == address present / legal

  // --- micro-sequencer state ------------------------------------------------
  std::uint16_t micro_pc = 0;

  void reset();

  // Generic, width-agnostic access used by mmov / mmovi / madd / mbeq.
  Word get(InternalReg reg) const;
  void set(InternalReg reg, Word value);
};

}  // namespace risc201::cu

#endif  // RISC201_CU_INTERNAL_REGISTERS_H
