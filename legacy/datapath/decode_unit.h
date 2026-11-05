// Hardware instruction decoder unpacking 32-bit RISC201 instruction words into register indices and immediate formats.
#ifndef RISC201_CU_DECODE_UNIT_H
#define RISC201_CU_DECODE_UNIT_H

#include "common/internal_registers.h"
#include "isa_spec/isa.h"

namespace risc201::cu {

// Combinational instruction decoder modeling hardware bitfield extraction from IR
struct DecodeUnit {
  // Unpacks register specifiers (rd, rs1, rs2) and expands immediate values according to instruction format
  void eval(const InternalRegisters& in, InternalRegisters& out) const {
    const Word ir = in.ir;
    const Opcode op = opcode_of(ir);
    const Format fmt = format_of(op);

    // Extract standard register operand specifiers: rd [23:20], rs1 [19:16], rs2 [15:12]
    out.rd  = get_rd(ir);
    out.rs1 = get_rs1(ir);
    out.rs2 = get_rs2(ir);

    // Format-specific immediate decoding and branch target calculation
    switch (fmt) {
      // Memory format: 16-bit signed displacement for base + offset addressing
      case Format::MEMORY:
        out.mode = 1;  
        out.I = true;  
        out.immx = static_cast<Word>(get_imm16_signed(ir));
        break;

      // Branch format: 26-bit PC-relative target offset word-aligned (target = PC + offset*4)
      case Format::BRANCH:
        out.mode = 0;
        out.I = false;
        out.immx = 0;
        out.branch_target = in.pc + (static_cast<Word>(get_offset26(ir)) << 2);
        break;

      // Arithmetic, logic, and comparison formats: 2-bit mode controls immediate extension
      // (mode 1 = sign-extended 16-bit, mode 2 = zero-extended 16-bit, mode 3 = upper 16-bit)
      case Format::ALU3:
      case Format::ALU2:
      case Format::COMPARE: {
        const std::uint8_t mode = get_mode(ir);
        out.mode = mode;
        out.I = (mode != 0);
        out.immx = expand_immediate(mode, get_imm16(ir));
        break;
      }

      // Default/zero-operand and FPU instructions with no immediate constants
      case Format::NO_OPERAND:
      case Format::FPU3:
      case Format::FPU2:
      default:
        out.mode = 0;
        out.I = false;
        out.immx = 0;
        break;
    }
  }
};

}

#endif
