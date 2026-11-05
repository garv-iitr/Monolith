#include "control_unit/control_unit.h"

namespace risc201::pipeline {

namespace {

inline cu::AluOp map_alu_op(Opcode op) {
  switch (op) {
    case Opcode::ADD: return cu::AluOp::ADD;
    case Opcode::SUB: return cu::AluOp::SUB;
    case Opcode::MUL: return cu::AluOp::MUL;
    case Opcode::DIV: return cu::AluOp::DIV;
    case Opcode::MOD: return cu::AluOp::MOD;
    case Opcode::AND: return cu::AluOp::AND;
    case Opcode::OR:  return cu::AluOp::OR;
    case Opcode::XOR: return cu::AluOp::XOR;
    case Opcode::LSL: return cu::AluOp::LSL;
    case Opcode::LSR: return cu::AluOp::LSR;
    case Opcode::ASR: return cu::AluOp::ASR;
    default:          return cu::AluOp::NOP;
  }
}

inline cu::FpuOp map_fpu_op(Opcode op) {
  switch (op) {
    case Opcode::FADD: return cu::FpuOp::FADD;
    case Opcode::FSUB: return cu::FpuOp::FSUB;
    case Opcode::FMUL: return cu::FpuOp::FMUL;
    case Opcode::FDIV: return cu::FpuOp::FDIV;
    case Opcode::FCMP: return cu::FpuOp::FCMP;
    case Opcode::FMOV: return cu::FpuOp::FMOV;
    default:           return cu::FpuOp::NOP;
  }
}

inline Cond map_branch_cond(Opcode op) {
  switch (op) {
    case Opcode::BEQ: return Cond::EQ;
    case Opcode::BNE: return Cond::NE;
    case Opcode::BGT: return Cond::GT;
    case Opcode::BLT: return Cond::LT;
    default:          return Cond::EQ;
  }
}

}  // namespace

// =============================================================================
// Horizontal Control Unit Implementation
// =============================================================================

HorizontalPipelineControlUnit::HorizontalPipelineControlUnit() { reset(); }

void HorizontalPipelineControlUnit::reset() {
  sequencer_.reset();
}

static PipelineControlBundle bundle_from_horizontal_rom(const HorizontalControlWord& hw, Word inst, Opcode op, std::uint16_t step) {
  PipelineControlBundle b{};
  b.micro_pc = static_cast<std::uint16_t>(hw.next_micro_pc);
  b.is_multi_uop = (hw.is_multi_uop != 0);
  b.is_last_uop = (hw.is_last_uop != 0);
  b.freeze_if_of = (hw.freeze_if_of != 0);
  b.halt = (hw.halt != 0);

  b.ex.alu_op = static_cast<cu::AluOp>(hw.alu_op);
  b.ex.operand_a_src = static_cast<OperandASrc>(hw.op_a_src);
  b.ex.operand_b_src = static_cast<OperandBSrc>(hw.op_b_src);
  b.ex.set_flags = (hw.set_flags != 0);
  b.ex.branch_cond = static_cast<Cond>(hw.branch_cond);
  b.ex.is_branch = (hw.is_branch != 0);
  b.ex.is_call = (hw.is_call != 0);
  b.ex.is_ret = (hw.is_ret != 0);
  b.ex.is_fpu = (hw.is_fpu != 0);
  b.ex.fpu_op = static_cast<cu::FpuOp>(hw.fpu_op);

  b.chk.memchk_enable = (hw.chk_enable != 0);
  b.chk.chk_mode = static_cast<ChkMode>(hw.chk_mode);
  b.ma.mem_read = (hw.mem_read != 0);
  b.ma.mem_write = (hw.mem_write != 0);
  b.ma.is_fpu = (hw.ma_is_fpu != 0);

  b.wb.reg_write_en = (hw.reg_write_en != 0);
  b.wb.reg_write_src = static_cast<RegWriteSrc>(hw.reg_write_src);
  b.wb.is_fpu = (hw.wb_is_fpu != 0);

  const bool ibit = get_ibit(inst);
  const std::uint8_t rd = get_rd(inst);
  const std::uint8_t rs1 = get_rs1(inst);
  const std::uint8_t rs2 = get_rs2(inst);

  if (op == Opcode::CALL) {
    if (step == 0) {
      b.src_reg_a = REG_SP;
      b.wb.dest_reg = REG_SP;
      b.imm = 4;
    } else {
      b.src_reg_a = REG_SP;
      b.imm = get_offset26(inst);
    }
  } else if (op == Opcode::RET) {
    if (step == 0) {
      b.src_reg_a = REG_SP;
      b.wb.dest_reg = REG_RA;
    } else {
      b.src_reg_a = REG_SP;
      b.src_reg_b = REG_RA;
      b.wb.dest_reg = REG_SP;
      b.imm = 4;
    }
  } else {
    const Format fmt = format_of(op);
    if (fmt == Format::ALU3 || fmt == Format::COMPARE) {
      b.src_reg_a = rs1;
      b.src_reg_b = ibit ? 0 : rs2;
      if (ibit) b.ex.operand_b_src = OperandBSrc::IMM;
      b.imm = expand_immediate(get_mode(inst), get_imm16(inst));
      b.wb.dest_reg = rd;
    } else if (fmt == Format::ALU2) {
      b.src_reg_b = (op == Opcode::MOV && ibit) ? 0 : rs2;
      if (op == Opcode::MOV && ibit) b.ex.operand_b_src = OperandBSrc::IMM;
      b.imm = expand_immediate(get_mode(inst), get_imm16(inst));
      b.wb.dest_reg = rd;
    } else if (fmt == Format::MEMORY) {
      b.src_reg_a = rs1;
      b.imm = static_cast<Word>(get_imm16_signed(inst));
      if (op == Opcode::ST || op == Opcode::FST) {
        b.src_reg_b = rd;
        b.src_reg_b_is_fpu = (op == Opcode::FST);
      } else {
        b.wb.dest_reg = rd;
      }
    } else if (fmt == Format::BRANCH) {
      b.imm = get_offset26(inst);
    } else if (fmt == Format::FPU3) {
      b.src_reg_a = rs1;
      b.src_reg_b = rs2;
      b.src_reg_a_is_fpu = true;
      b.src_reg_b_is_fpu = true;
      b.wb.dest_reg = rd;
    } else if (fmt == Format::FPU2) {
      b.src_reg_b = rs2;
      b.src_reg_b_is_fpu = true;
      if (op == Opcode::FCMP) {
        b.src_reg_a = rs1;
        b.src_reg_a_is_fpu = true;
      } else {
        b.wb.dest_reg = rd;
      }
    }
  }
  return b;
}

PipelineControlBundle HorizontalPipelineControlUnit::crack_single_cycle(Opcode op, Word inst) {
  const HorizontalControlWord hw = lookup_horizontal_rom(op, 0);
  return bundle_from_horizontal_rom(hw, inst, op, 0);
}

PipelineControlBundle HorizontalPipelineControlUnit::crack_multi_uop(Opcode op, Word inst, std::uint16_t step) {
  const HorizontalControlWord hw = lookup_horizontal_rom(op, step);
  return bundle_from_horizontal_rom(hw, inst, op, step);
}

PipelineControlBundle HorizontalPipelineControlUnit::peek_bundle(Word instruction, const Flags& flags) const {
  (void)flags;
  if (sequencer_.is_cracking()) {
    return crack_multi_uop(sequencer_.active_op(), sequencer_.saved_instruction(), sequencer_.uop_step());
  }
  const Opcode op = opcode_of(instruction);
  if (op == Opcode::CALL || op == Opcode::RET) {
    return crack_multi_uop(op, instruction, 0);
  }
  return crack_single_cycle(op, instruction);
}

void HorizontalPipelineControlUnit::advance(Word instruction) {
  if (sequencer_.is_cracking()) {
    const PipelineControlBundle b = crack_multi_uop(sequencer_.active_op(),
                                                    sequencer_.saved_instruction(),
                                                    sequencer_.uop_step());
    sequencer_.step_cycle(b.is_last_uop);
  } else {
    const Opcode op = opcode_of(instruction);
    if (op == Opcode::CALL || op == Opcode::RET) {
      const PipelineControlBundle b = crack_multi_uop(op, instruction, 0);
      sequencer_.dispatch(op, instruction, b.is_last_uop);
    } else {
      sequencer_.dispatch(op, instruction, true);
    }
  }
}

// =============================================================================
// Vertical Control Unit Implementation
// =============================================================================

VerticalPipelineControlUnit::VerticalPipelineControlUnit() { reset(); }

void VerticalPipelineControlUnit::reset() {
  sequencer_.reset();
}

VerticalControlWord VerticalPipelineControlUnit::fetch_vertical_rom(Opcode op, std::uint16_t step) {
  return lookup_vertical_rom(op, step);
}

PipelineControlBundle VerticalPipelineControlUnit::decode_compact(
    const VerticalControlWord& vw, Word inst, std::uint16_t step) const {
  PipelineControlBundle b{};
  const Opcode op = opcode_of(inst);
  b.micro_pc = static_cast<std::uint16_t>(rom_address(op, step));
  b.is_multi_uop = (vw.is_multi_uop != 0);
  b.is_last_uop = (vw.is_last_uop != 0);
  b.freeze_if_of = (vw.freeze_if_of != 0);
  b.halt = (vw.halt != 0);
  b.ex.set_flags = (vw.set_flags != 0);

  const bool ibit = get_ibit(inst);
  const std::uint8_t rd = get_rd(inst);
  const std::uint8_t rs1 = get_rs1(inst);
  const std::uint8_t rs2 = get_rs2(inst);

  // 1. Combinational Operation Class & Sub-Op Decoding
  switch (vw.op_class) {
    case 0: // ALU Class
      switch (vw.sub_op) {
        case 1:  b.ex.alu_op = cu::AluOp::ADD; break;
        case 2:  b.ex.alu_op = cu::AluOp::SUB; break;
        case 3:  b.ex.alu_op = cu::AluOp::MUL; break;
        case 4:  b.ex.alu_op = cu::AluOp::DIV; break;
        case 5:  b.ex.alu_op = cu::AluOp::MOD; break;
        case 6:  b.ex.alu_op = cu::AluOp::AND; break;
        case 7:  b.ex.alu_op = cu::AluOp::OR;  break;
        case 8:  b.ex.alu_op = cu::AluOp::NOT; break;
        case 9:  b.ex.alu_op = cu::AluOp::XOR; break;
        case 10: b.ex.alu_op = cu::AluOp::LSL; break;
        case 11: b.ex.alu_op = cu::AluOp::LSR; break;
        case 12: b.ex.alu_op = cu::AluOp::ASR; break;
        case 13: b.ex.alu_op = cu::AluOp::SUB; b.ex.set_flags = true; break; // CMP
        case 14: b.ex.alu_op = cu::AluOp::PASS_B; break; // MOV
        default: b.ex.alu_op = cu::AluOp::NOP; break;
      }
      break;

    case 1: // FPU Class
      b.ex.is_fpu = true;
      switch (vw.sub_op) {
        case 1: b.ex.fpu_op = cu::FpuOp::FADD; break;
        case 2: b.ex.fpu_op = cu::FpuOp::FSUB; break;
        case 3: b.ex.fpu_op = cu::FpuOp::FMUL; break;
        case 4: b.ex.fpu_op = cu::FpuOp::FDIV; break;
        case 5: b.ex.fpu_op = cu::FpuOp::FCMP; b.ex.set_flags = true; break;
        case 6: b.ex.fpu_op = cu::FpuOp::FMOV; break;
        default: b.ex.fpu_op = cu::FpuOp::NOP; break;
      }
      break;

    case 2: // MEM Class
      b.ex.alu_op = cu::AluOp::ADD;
      b.chk.memchk_enable = true;
      b.chk.chk_mode = (vw.sub_op == 5) ? ChkMode::BNDCHK_INSTR : ChkMode::MEMORY_ADDR;
      break;

    case 3: // BRANCH Class
      b.ex.is_branch = true;
      b.ex.branch_cond = static_cast<Cond>(vw.sub_op);
      break;

    case 4: // SYSTEM Class
      if (vw.sub_op == 1) b.halt = true;
      break;

    case 5: // FLOW Class (CALL / RET)
      if (vw.sub_op == 1) { // CALL
        if (step == 0) {
          b.ex.alu_op = cu::AluOp::SUB;
          b.chk.memchk_enable = true;
          b.chk.chk_mode = ChkMode::STACK_BOUNDS;
        } else {
          b.ex.is_branch = true;
          b.ex.is_call = true;
        }
      } else { // RET
        if (step == 0) {
          b.ex.alu_op = cu::AluOp::PASS_A;
          b.chk.memchk_enable = true;
          b.chk.chk_mode = ChkMode::STACK_BOUNDS;
        } else {
          b.ex.alu_op = cu::AluOp::ADD;
          b.ex.is_branch = true;
          b.ex.is_ret = true;
        }
      }
      break;
  }

  // 2. Combinational Operand Mux Decoding
  switch (vw.operand_mode) {
    case 0: // RS1 and RS2 (overridden to IMM if ibit is set)
      b.ex.operand_a_src = OperandASrc::RS1;
      b.ex.operand_b_src = ibit ? OperandBSrc::IMM : OperandBSrc::RS2;
      break;
    case 1: // RS1 and IMM
      b.ex.operand_a_src = OperandASrc::RS1;
      b.ex.operand_b_src = OperandBSrc::IMM;
      break;
    case 2: // ZERO and RS2
      b.ex.operand_a_src = OperandASrc::ZERO;
      b.ex.operand_b_src = (op == Opcode::MOV && ibit) ? OperandBSrc::IMM : OperandBSrc::RS2;
      break;
    case 3: // ZERO and IMM
      b.ex.operand_a_src = OperandASrc::ZERO;
      b.ex.operand_b_src = OperandBSrc::IMM;
      break;
    case 4: // RS1 and ZERO
      b.ex.operand_a_src = OperandASrc::RS1;
      b.ex.operand_b_src = OperandBSrc::ZERO;
      break;
    case 5: // PC and IMM
      b.ex.operand_a_src = OperandASrc::PC;
      b.ex.operand_b_src = OperandBSrc::IMM;
      break;
  }

  // 3. Combinational Memory Action Decoding
  switch (vw.mem_action) {
    case 0: b.ma.mem_read = false; b.ma.mem_write = false; break;
    case 1: b.ma.mem_read = true;  b.ma.mem_write = false; break;
    case 2: b.ma.mem_read = false; b.ma.mem_write = true;  break;
    case 3: b.ma.mem_read = false; b.ma.mem_write = false; b.chk.memchk_enable = true; break;
  }
  b.ma.is_fpu = (vw.is_fpu_domain != 0);

  // 4. Combinational Writeback Target Decoding
  switch (vw.wb_target) {
    case 0: b.wb.reg_write_en = false; break;
    case 1: b.wb.reg_write_en = true; b.wb.reg_write_src = RegWriteSrc::ALU_OUT; break;
    case 2: b.wb.reg_write_en = true; b.wb.reg_write_src = RegWriteSrc::MEM_DATA; break;
    case 3: b.wb.reg_write_en = true; b.wb.reg_write_src = RegWriteSrc::PC_PLUS4; break;
    case 4: b.wb.reg_write_en = true; b.wb.reg_write_src = RegWriteSrc::FPU_OUT; break;
  }
  b.wb.is_fpu = (vw.is_fpu_domain != 0);

  // 5. Register Operands and Immediates Extraction
  if (op == Opcode::CALL) {
    if (step == 0) {
      b.src_reg_a = REG_SP;
      b.wb.dest_reg = REG_SP;
      b.imm = 4;
    } else {
      b.src_reg_a = REG_SP;
      b.imm = get_offset26(inst);
    }
  } else if (op == Opcode::RET) {
    if (step == 0) {
      b.src_reg_a = REG_SP;
      b.wb.dest_reg = REG_RA;
    } else {
      b.src_reg_a = REG_SP;
      b.src_reg_b = REG_RA;
      b.wb.dest_reg = REG_SP;
      b.imm = 4;
    }
  } else {
    const Format fmt = format_of(op);
    if (fmt == Format::ALU3 || fmt == Format::COMPARE) {
      b.src_reg_a = rs1;
      b.src_reg_b = ibit ? 0 : rs2;
      b.imm = expand_immediate(get_mode(inst), get_imm16(inst));
      b.wb.dest_reg = rd;
    } else if (fmt == Format::ALU2) {
      b.src_reg_b = (op == Opcode::MOV && ibit) ? 0 : rs2;
      b.imm = expand_immediate(get_mode(inst), get_imm16(inst));
      b.wb.dest_reg = rd;
    } else if (fmt == Format::MEMORY) {
      b.src_reg_a = rs1;
      b.imm = static_cast<Word>(get_imm16_signed(inst));
      if (op == Opcode::ST || op == Opcode::FST) {
        b.src_reg_b = rd;
        b.src_reg_b_is_fpu = (op == Opcode::FST);
      } else {
        b.wb.dest_reg = rd;
      }
    } else if (fmt == Format::BRANCH) {
      b.imm = get_offset26(inst);
    } else if (fmt == Format::FPU3) {
      b.src_reg_a = rs1;
      b.src_reg_b = rs2;
      b.src_reg_a_is_fpu = true;
      b.src_reg_b_is_fpu = true;
      b.wb.dest_reg = rd;
    } else if (fmt == Format::FPU2) {
      b.src_reg_b = rs2;
      b.src_reg_b_is_fpu = true;
      if (op == Opcode::FCMP) {
        b.src_reg_a = rs1;
        b.src_reg_a_is_fpu = true;
      } else {
        b.wb.dest_reg = rd;
      }
    }
  }
  return b;
}

PipelineControlBundle VerticalPipelineControlUnit::peek_bundle(Word instruction, const Flags& flags) const {
  (void)flags;
  if (sequencer_.is_cracking()) {
    const VerticalControlWord vw = fetch_vertical_rom(sequencer_.active_op(), sequencer_.uop_step());
    return decode_compact(vw, sequencer_.saved_instruction(), sequencer_.uop_step());
  }
  const Opcode op = opcode_of(instruction);
  const VerticalControlWord vw = fetch_vertical_rom(op, 0);
  return decode_compact(vw, instruction, 0);
}

void VerticalPipelineControlUnit::advance(Word instruction) {
  if (sequencer_.is_cracking()) {
    const VerticalControlWord vw = fetch_vertical_rom(sequencer_.active_op(), sequencer_.uop_step());
    const PipelineControlBundle b = decode_compact(vw, sequencer_.saved_instruction(), sequencer_.uop_step());
    sequencer_.step_cycle(b.is_last_uop);
  } else {
    const Opcode op = opcode_of(instruction);
    if (op == Opcode::CALL || op == Opcode::RET) {
      const VerticalControlWord vw = fetch_vertical_rom(op, 0);
      const PipelineControlBundle b = decode_compact(vw, instruction, 0);
      sequencer_.dispatch(op, instruction, b.is_last_uop);
    } else {
      sequencer_.dispatch(op, instruction, true);
    }
  }
}

std::unique_ptr<IPipelineControlUnit> create_control_unit(bool vertical) {
  if (vertical) return std::make_unique<VerticalPipelineControlUnit>();
  return std::make_unique<HorizontalPipelineControlUnit>();
}

}  // namespace risc201::pipeline
