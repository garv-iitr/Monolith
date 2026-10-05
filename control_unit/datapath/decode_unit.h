#ifndef RISC201_CU_DECODE_UNIT_H
#define RISC201_CU_DECODE_UNIT_H

// ============================================================================
// Decode unit (Sarangi, sec. 9.5.2).
//
// Purely combinational: it slices the instruction register into the field
// registers I, rd, rs1, rs2, immx and branchTarget.  branchTarget is computed
// as pc + (offset26 << 2).  For memory-format instructions the immediate is
// always the signed imm16 field; for the register/immediate families the two
// mode bits select the expansion (sign / zero / high).
// ============================================================================

#include "common/internal_registers.h"
#include "isa_spec/isa.h"

namespace risc201::cu {

struct DecodeUnit {
  void eval(const InternalRegisters& in, InternalRegisters& out) const {
    const Word ir = in.ir;
    const Opcode op = opcode_of(ir);
    const Format fmt = format_of(op);

    out.rd  = get_rd(ir);
    out.rs1 = get_rs1(ir);
    out.rs2 = get_rs2(ir);

    switch (fmt) {
      case Format::MEMORY:
        out.I = true;  // base+offset addressing is always immediate-based
        out.immx = static_cast<Word>(get_imm16_signed(ir));
        break;

      case Format::BRANCH:
        out.I = false;
        out.immx = 0;
        out.branch_target = in.pc + (static_cast<Word>(get_offset26(ir)) << 2);
        break;

      case Format::ALU3:
      case Format::ALU2:
      case Format::COMPARE: {
        const std::uint8_t mode = get_mode(ir);
        out.I = (mode != 0);
        out.immx = expand_immediate(mode, get_imm16(ir));
        break;
      }

      case Format::NO_OPERAND:
      case Format::FPU3:
      case Format::FPU2:
      default:
        out.I = false;
        out.immx = 0;
        break;
    }
  }
};

}  // namespace risc201::cu

#endif  // RISC201_CU_DECODE_UNIT_H
