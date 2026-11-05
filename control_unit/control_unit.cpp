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

PipelineControlBundle HorizontalPipelineControlUnit::crack_single_cycle(Opcode op, Word inst) {
  PipelineControlBundle b{};
  b.is_multi_uop = false;
  b.is_last_uop = true;
  b.freeze_if_of = false;
  b.micro_pc = static_cast<std::uint16_t>(rom_address(op, 0));

  const bool ibit = get_ibit(inst);
  const std::uint8_t rd = get_rd(inst);
  const std::uint8_t rs1 = get_rs1(inst);
  const std::uint8_t rs2 = get_rs2(inst);

  switch (format_of(op)) {
    case Format::ALU3:
      b.ex.alu_op = map_alu_op(op);
      b.ex.operand_a_src = OperandASrc::RS1;
      b.ex.operand_b_src = ibit ? OperandBSrc::IMM : OperandBSrc::RS2;
      b.src_reg_a = rs1;
      b.src_reg_b = ibit ? 0 : rs2;
      b.imm = expand_immediate(get_mode(inst), get_imm16(inst));
      b.wb.reg_write_en = true;
      b.wb.reg_write_src = RegWriteSrc::ALU_OUT;
      b.wb.dest_reg = rd;
      break;

    case Format::ALU2:
      b.wb.dest_reg = rd;
      b.wb.reg_write_en = true;
      b.wb.reg_write_src = RegWriteSrc::ALU_OUT;
      if (op == Opcode::MOV) {
        b.ex.alu_op = cu::AluOp::PASS_B;
        b.ex.operand_b_src = ibit ? OperandBSrc::IMM : OperandBSrc::RS2;
        b.src_reg_b = ibit ? 0 : rs2;
        b.imm = expand_immediate(get_mode(inst), get_imm16(inst));
      } else { // NOT
        b.ex.alu_op = cu::AluOp::NOT;
        b.ex.operand_a_src = OperandASrc::ZERO;
        b.ex.operand_b_src = OperandBSrc::RS2;
        b.src_reg_b = rs2;
      }
      break;

    case Format::COMPARE: // CMP
      b.ex.alu_op = cu::AluOp::SUB;
      b.ex.operand_a_src = OperandASrc::RS1;
      b.ex.operand_b_src = ibit ? OperandBSrc::IMM : OperandBSrc::RS2;
      b.src_reg_a = rs1;
      b.src_reg_b = ibit ? 0 : rs2;
      b.imm = expand_immediate(get_mode(inst), get_imm16(inst));
      b.ex.set_flags = true;
      break;

    case Format::MEMORY:
      b.src_reg_a = rs1;
      b.imm = static_cast<Word>(get_imm16_signed(inst));
      b.ex.alu_op = cu::AluOp::ADD;
      b.ex.operand_a_src = OperandASrc::RS1;
      b.ex.operand_b_src = OperandBSrc::IMM;
      if (op == Opcode::LD || op == Opcode::FLD) {
        b.chk.memchk_enable = true;
        b.chk.chk_mode = ChkMode::MEMORY_ADDR;
        b.ma.mem_read = true;
        b.ma.is_fpu = (op == Opcode::FLD);
        b.wb.reg_write_en = true;
        b.wb.is_fpu = (op == Opcode::FLD);
        b.wb.reg_write_src = RegWriteSrc::MEM_DATA;
        b.wb.dest_reg = rd;
      } else if (op == Opcode::ST || op == Opcode::FST) {
        b.src_reg_b = rd;
        b.src_reg_b_is_fpu = (op == Opcode::FST);
        b.chk.memchk_enable = true;
        b.chk.chk_mode = ChkMode::MEMORY_ADDR;
        b.ma.mem_write = true;
        b.ma.is_fpu = (op == Opcode::FST);
      } else if (op == Opcode::BNDCHK) {
        b.chk.memchk_enable = true;
        b.chk.chk_mode = ChkMode::BNDCHK_INSTR;
      }
      break;

    case Format::BRANCH:
      b.ex.is_branch = true;
      b.ex.branch_cond = map_branch_cond(op);
      b.imm = get_offset26(inst);
      break;

    case Format::FPU3:
      b.ex.is_fpu = true;
      b.ex.fpu_op = map_fpu_op(op);
      b.src_reg_a = rs1;
      b.src_reg_b = rs2;
      b.src_reg_a_is_fpu = true;
      b.src_reg_b_is_fpu = true;
      b.wb.reg_write_en = true;
      b.wb.is_fpu = true;
      b.wb.reg_write_src = RegWriteSrc::FPU_OUT;
      b.wb.dest_reg = rd;
      break;

    case Format::FPU2:
      b.ex.is_fpu = true;
      b.ex.fpu_op = map_fpu_op(op);
      b.src_reg_b = rs2;
      b.src_reg_b_is_fpu = true;
      if (op == Opcode::FCMP) {
        b.src_reg_a = rs1;
        b.src_reg_a_is_fpu = true;
        b.ex.set_flags = true;
      } else { // FMOV
        b.wb.reg_write_en = true;
        b.wb.is_fpu = true;
        b.wb.reg_write_src = RegWriteSrc::FPU_OUT;
        b.wb.dest_reg = rd;
      }
      break;

    case Format::NO_OPERAND:
      if (op == Opcode::HALT) b.halt = true;
      break;

    default:
      break;
  }
  return b;
}

PipelineControlBundle HorizontalPipelineControlUnit::crack_multi_uop(Opcode op, Word inst, std::uint16_t step) {
  PipelineControlBundle b{};
  b.is_multi_uop = true;
  b.micro_pc = static_cast<std::uint16_t>(rom_address(op, step));

  if (op == Opcode::CALL) {
    if (step == 0) { // uop 0: SP = SP - 4
      b.ex.alu_op = cu::AluOp::SUB;
      b.ex.operand_a_src = OperandASrc::RS1;
      b.ex.operand_b_src = OperandBSrc::IMM;
      b.src_reg_a = REG_SP;
      b.imm = 4;
      b.chk.memchk_enable = true;
      b.chk.chk_mode = ChkMode::STACK_BOUNDS;
      b.wb.reg_write_en = true;
      b.wb.dest_reg = REG_SP;
      b.freeze_if_of = true;
      b.is_last_uop = false;
    } else { // uop 1: Mem[SP] = PC + 4, branch target
      b.ex.is_branch = true;
      b.ex.is_call = true;
      b.src_reg_a = REG_SP;
      b.imm = get_offset26(inst);
      b.ma.mem_write = true;
      b.is_last_uop = true;
    }
  } else if (op == Opcode::RET) {
    if (step == 0) { // uop 0: RA = Mem[SP]
      b.ex.alu_op = cu::AluOp::PASS_A;
      b.ex.operand_a_src = OperandASrc::RS1;
      b.src_reg_a = REG_SP;
      b.chk.memchk_enable = true;
      b.chk.chk_mode = ChkMode::STACK_BOUNDS;
      b.ma.mem_read = true;
      b.wb.reg_write_en = true;
      b.wb.reg_write_src = RegWriteSrc::MEM_DATA;
      b.wb.dest_reg = REG_RA;
      b.freeze_if_of = true;
      b.is_last_uop = false;
    } else { // uop 1: SP = SP + 4, branch RA
      b.ex.alu_op = cu::AluOp::ADD;
      b.ex.operand_a_src = OperandASrc::RS1;
      b.ex.operand_b_src = OperandBSrc::IMM;
      b.src_reg_a = REG_SP;
      b.src_reg_b = REG_RA;
      b.imm = 4;
      b.ex.is_ret = true;
      b.ex.is_branch = true;
      b.wb.reg_write_en = true;
      b.wb.dest_reg = REG_SP;
      b.is_last_uop = true;
    }
  }
  return b;
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
  const Opcode op = static_cast<Opcode>((vw.op_class << 4) | (vw.sub_op & 0xF));
  if (op == Opcode::CALL || op == Opcode::RET) {
    return HorizontalPipelineControlUnit::crack_multi_uop(op, inst, step);
  }
  return HorizontalPipelineControlUnit::crack_single_cycle(op, inst);
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
