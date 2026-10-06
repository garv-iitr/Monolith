#ifndef RISC201_CU_INTERNAL_REGISTERS_H
#define RISC201_CU_INTERNAL_REGISTERS_H

#include <cstdint>

#include "isa_spec/isa.h"

namespace risc201::cu {

struct InternalRegisters {
  Word pc = 0;
  Word ir = 0;

  bool      I = false;
  std::uint8_t rd = 0;
  std::uint8_t rs1 = 0;
  std::uint8_t rs2 = 0;
  Word      immx = 0;
  Word      branch_target = 0;

  std::uint8_t reg_src = 0;
  Word         reg_data = 0;
  Word         reg_val = 0;

  Word A = 0;
  Word B = 0;
  Word alu_result = 0;
  Flags flags{};
  bool  flags_E = false;
  bool  flags_GT = false;

  Word mar = 0;
  Word mdr = 0;
  Word ld_result = 0;

  std::uint8_t fp_src = 0;
  Word         fp_data = 0;
  Word         fp_val = 0;
  Word         fp_a = 0;
  Word         fp_b = 0;
  Word         fpu_result = 0;
  bool         fcc = false;

  Word memchk_addr = 0;
  bool memchk_status = false;

  std::uint16_t micro_pc = 0;

  void reset();

  Word get(InternalReg reg) const;
  void set(InternalReg reg, Word value);
};

}

#endif
