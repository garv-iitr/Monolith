#include "common/internal_registers.h"

namespace risc201::cu {

void InternalRegisters::reset() {
  pc = 0;
  ir = 0;
  I = false;
  rd = rs1 = rs2 = 0;
  immx = 0;
  branch_target = 0;
  reg_src = 0;
  reg_data = reg_val = 0;
  A = B = alu_result = 0;
  flags = Flags{};
  flags_E = false;
  flags_GT = false;
  mar = mdr = ld_result = 0;
  fp_src = 0;
  fp_data = fp_val = 0;
  fp_a = fp_b = fpu_result = 0;
  fcc = false;
  memchk_addr = 0;
  memchk_status = false;
  micro_pc = 0;
}

Word InternalRegisters::get(InternalReg reg) const {
  switch (reg) {
    case InternalReg::NONE:          return 0;
    case InternalReg::PC:            return pc;
    case InternalReg::IR:            return ir;
    case InternalReg::I:             return I ? 1 : 0;
    case InternalReg::RD:            return rd;
    case InternalReg::RS1:           return rs1;
    case InternalReg::RS2:           return rs2;
    case InternalReg::IMMX:          return immx;
    case InternalReg::BRANCH_TARGET: return branch_target;
    case InternalReg::REG_SRC:       return reg_src;
    case InternalReg::REG_DATA:      return reg_data;
    case InternalReg::REG_VAL:       return reg_val;
    case InternalReg::A:             return A;
    case InternalReg::B:             return B;
    case InternalReg::FLAGS_E:       return flags.Z ? 1 : 0;
    case InternalReg::FLAGS_GT:      return (!flags.N && !flags.Z) ? 1 : 0;
    case InternalReg::ALU_RESULT:    return alu_result;
    case InternalReg::MAR:           return mar;
    case InternalReg::MDR:           return mdr;
    case InternalReg::LD_RESULT:     return ld_result;
    case InternalReg::FP_SRC:        return fp_src;
    case InternalReg::FP_DATA:       return fp_data;
    case InternalReg::FP_VAL:        return fp_val;
    case InternalReg::FP_A:          return fp_a;
    case InternalReg::FP_B:          return fp_b;
    case InternalReg::FPU_RESULT:    return fpu_result;
    case InternalReg::FCC:           return fcc ? 1 : 0;
    case InternalReg::MEMCHK_STATUS: return memchk_status ? 1 : 0;
    case InternalReg::MEMCHK_ADDR:   return memchk_addr;
    case InternalReg::FLAGS_N:       return flags.N ? 1 : 0;
  }
  return 0;
}

void InternalRegisters::set(InternalReg reg, Word value) {
  switch (reg) {
    case InternalReg::NONE:          break;
    case InternalReg::PC:            pc = value; break;
    case InternalReg::IR:            ir = value; break;
    case InternalReg::I:             I = (value != 0); break;
    case InternalReg::RD:            rd = static_cast<std::uint8_t>(value & 0xF); break;
    case InternalReg::RS1:           rs1 = static_cast<std::uint8_t>(value & 0xF); break;
    case InternalReg::RS2:           rs2 = static_cast<std::uint8_t>(value & 0xF); break;
    case InternalReg::IMMX:          immx = value; break;
    case InternalReg::BRANCH_TARGET: branch_target = value; break;
    case InternalReg::REG_SRC:       reg_src = static_cast<std::uint8_t>(value & 0xF); break;
    case InternalReg::REG_DATA:      reg_data = value; break;
    case InternalReg::REG_VAL:       reg_val = value; break;
    case InternalReg::A:             A = value; break;
    case InternalReg::B:             B = value; break;
    case InternalReg::FLAGS_E:       flags.Z = (value != 0); break;
    case InternalReg::FLAGS_GT:      flags.Z = (value == 0); break;
    case InternalReg::ALU_RESULT:    alu_result = value; break;
    case InternalReg::MAR:           mar = value; break;
    case InternalReg::MDR:           mdr = value; break;
    case InternalReg::LD_RESULT:     ld_result = value; break;
    case InternalReg::FP_SRC:        fp_src = static_cast<std::uint8_t>(value & 0xF); break;
    case InternalReg::FP_DATA:       fp_data = value; break;
    case InternalReg::FP_VAL:        fp_val = value; break;
    case InternalReg::FP_A:          fp_a = value; break;
    case InternalReg::FP_B:          fp_b = value; break;
    case InternalReg::FPU_RESULT:    fpu_result = value; break;
    case InternalReg::FCC:           fcc = (value != 0); break;
    case InternalReg::MEMCHK_STATUS: memchk_status = (value != 0); break;
    case InternalReg::MEMCHK_ADDR:   memchk_addr = value; break;
    case InternalReg::FLAGS_N:       flags.N = (value != 0); break;
  }
  // Keep the 1-bit flag mirrors coherent with the architectural flags.
  flags_E = flags.Z;
  flags_GT = !flags.N && !flags.Z;
}

}  // namespace risc201::cu
