#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include "control_unit/control_word.h"
#include "isa_spec/isa.h"

namespace risc201::pipeline {

// =============================================================================
// 128-WORD MICROCODE CONTROL STORE (ROM TABLES)
// =============================================================================
// The microcode control store holds precomputed control words for all 34 RISC201
// macro-opcodes along with multi-uop expansion slots (e.g. CALL, RET).
// Total capacity: 128 words.
// Single-cycle ops reside at base index: uPC = (opcode * 2).
// Multi-uop cracked sequences dispatch step 0 at uPC = (opcode * 2), step 1 at (opcode * 2 + 1).
// =============================================================================

constexpr std::size_t kControlStoreDepth = 128;

inline constexpr std::size_t rom_address(Opcode op, std::uint16_t step = 0) {
  return (static_cast<std::size_t>(static_cast<std::uint8_t>(op) & 0x3F) * 2) + (step & 0x1);
}

class ControlStore {
 public:
  static constexpr HorizontalControlWord build_horizontal_entry(Opcode op, std::uint16_t step) {
    HorizontalControlWord w{};
    w.next_micro_pc = static_cast<std::uint64_t>(rom_address(op, step));

    switch (op) {
      // Arithmetic & Logic (ALU3)
      case Opcode::ADD:
        w.alu_op = static_cast<std::uint64_t>(AluOp::ADD);
        w.op_a_src = static_cast<std::uint64_t>(OperandASrc::RS1);
        w.op_b_src = static_cast<std::uint64_t>(OperandBSrc::RS2);
        w.reg_write_en = 1;
        w.reg_write_src = static_cast<std::uint64_t>(RegWriteSrc::ALU_OUT);
        w.is_last_uop = 1;
        break;
      case Opcode::SUB:
        w.alu_op = static_cast<std::uint64_t>(AluOp::SUB);
        w.op_a_src = static_cast<std::uint64_t>(OperandASrc::RS1);
        w.op_b_src = static_cast<std::uint64_t>(OperandBSrc::RS2);
        w.reg_write_en = 1;
        w.reg_write_src = static_cast<std::uint64_t>(RegWriteSrc::ALU_OUT);
        w.is_last_uop = 1;
        break;
      case Opcode::MUL:
        w.alu_op = static_cast<std::uint64_t>(AluOp::MUL);
        w.op_a_src = static_cast<std::uint64_t>(OperandASrc::RS1);
        w.op_b_src = static_cast<std::uint64_t>(OperandBSrc::RS2);
        w.reg_write_en = 1;
        w.reg_write_src = static_cast<std::uint64_t>(RegWriteSrc::ALU_OUT);
        w.is_last_uop = 1;
        break;
      case Opcode::DIV:
        w.alu_op = static_cast<std::uint64_t>(AluOp::DIV);
        w.op_a_src = static_cast<std::uint64_t>(OperandASrc::RS1);
        w.op_b_src = static_cast<std::uint64_t>(OperandBSrc::RS2);
        w.reg_write_en = 1;
        w.reg_write_src = static_cast<std::uint64_t>(RegWriteSrc::ALU_OUT);
        w.is_last_uop = 1;
        break;
      case Opcode::MOD:
        w.alu_op = static_cast<std::uint64_t>(AluOp::MOD);
        w.op_a_src = static_cast<std::uint64_t>(OperandASrc::RS1);
        w.op_b_src = static_cast<std::uint64_t>(OperandBSrc::RS2);
        w.reg_write_en = 1;
        w.reg_write_src = static_cast<std::uint64_t>(RegWriteSrc::ALU_OUT);
        w.is_last_uop = 1;
        break;
      case Opcode::AND:
        w.alu_op = static_cast<std::uint64_t>(AluOp::AND);
        w.op_a_src = static_cast<std::uint64_t>(OperandASrc::RS1);
        w.op_b_src = static_cast<std::uint64_t>(OperandBSrc::RS2);
        w.reg_write_en = 1;
        w.reg_write_src = static_cast<std::uint64_t>(RegWriteSrc::ALU_OUT);
        w.is_last_uop = 1;
        break;
      case Opcode::OR:
        w.alu_op = static_cast<std::uint64_t>(AluOp::OR);
        w.op_a_src = static_cast<std::uint64_t>(OperandASrc::RS1);
        w.op_b_src = static_cast<std::uint64_t>(OperandBSrc::RS2);
        w.reg_write_en = 1;
        w.reg_write_src = static_cast<std::uint64_t>(RegWriteSrc::ALU_OUT);
        w.is_last_uop = 1;
        break;
      case Opcode::XOR:
        w.alu_op = static_cast<std::uint64_t>(AluOp::XOR);
        w.op_a_src = static_cast<std::uint64_t>(OperandASrc::RS1);
        w.op_b_src = static_cast<std::uint64_t>(OperandBSrc::RS2);
        w.reg_write_en = 1;
        w.reg_write_src = static_cast<std::uint64_t>(RegWriteSrc::ALU_OUT);
        w.is_last_uop = 1;
        break;
      case Opcode::LSL:
        w.alu_op = static_cast<std::uint64_t>(AluOp::LSL);
        w.op_a_src = static_cast<std::uint64_t>(OperandASrc::RS1);
        w.op_b_src = static_cast<std::uint64_t>(OperandBSrc::RS2);
        w.reg_write_en = 1;
        w.reg_write_src = static_cast<std::uint64_t>(RegWriteSrc::ALU_OUT);
        w.is_last_uop = 1;
        break;
      case Opcode::LSR:
        w.alu_op = static_cast<std::uint64_t>(AluOp::LSR);
        w.op_a_src = static_cast<std::uint64_t>(OperandASrc::RS1);
        w.op_b_src = static_cast<std::uint64_t>(OperandBSrc::RS2);
        w.reg_write_en = 1;
        w.reg_write_src = static_cast<std::uint64_t>(RegWriteSrc::ALU_OUT);
        w.is_last_uop = 1;
        break;
      case Opcode::ASR:
        w.alu_op = static_cast<std::uint64_t>(AluOp::ASR);
        w.op_a_src = static_cast<std::uint64_t>(OperandASrc::RS1);
        w.op_b_src = static_cast<std::uint64_t>(OperandBSrc::RS2);
        w.reg_write_en = 1;
        w.reg_write_src = static_cast<std::uint64_t>(RegWriteSrc::ALU_OUT);
        w.is_last_uop = 1;
        break;

      // Unary / Data Movement (ALU2)
      case Opcode::MOV:
        w.alu_op = static_cast<std::uint64_t>(AluOp::PASS_B);
        w.op_b_src = static_cast<std::uint64_t>(OperandBSrc::RS2);
        w.reg_write_en = 1;
        w.reg_write_src = static_cast<std::uint64_t>(RegWriteSrc::ALU_OUT);
        w.is_last_uop = 1;
        break;
      case Opcode::NOT:
        w.alu_op = static_cast<std::uint64_t>(AluOp::NOT);
        w.op_a_src = static_cast<std::uint64_t>(OperandASrc::ZERO);
        w.op_b_src = static_cast<std::uint64_t>(OperandBSrc::RS2);
        w.reg_write_en = 1;
        w.reg_write_src = static_cast<std::uint64_t>(RegWriteSrc::ALU_OUT);
        w.is_last_uop = 1;
        break;

      // Comparison
      case Opcode::CMP:
        w.alu_op = static_cast<std::uint64_t>(AluOp::SUB);
        w.op_a_src = static_cast<std::uint64_t>(OperandASrc::RS1);
        w.op_b_src = static_cast<std::uint64_t>(OperandBSrc::RS2);
        w.set_flags = 1;
        w.is_last_uop = 1;
        break;

      // Memory Transactions
      case Opcode::LD:
        w.alu_op = static_cast<std::uint64_t>(AluOp::ADD);
        w.op_a_src = static_cast<std::uint64_t>(OperandASrc::RS1);
        w.op_b_src = static_cast<std::uint64_t>(OperandBSrc::IMM);
        w.chk_enable = 1;
        w.chk_mode = static_cast<std::uint64_t>(ChkMode::MEMORY_ADDR);
        w.mem_read = 1;
        w.reg_write_en = 1;
        w.reg_write_src = static_cast<std::uint64_t>(RegWriteSrc::MEM_DATA);
        w.is_last_uop = 1;
        break;
      case Opcode::ST:
        w.alu_op = static_cast<std::uint64_t>(AluOp::ADD);
        w.op_a_src = static_cast<std::uint64_t>(OperandASrc::RS1);
        w.op_b_src = static_cast<std::uint64_t>(OperandBSrc::IMM);
        w.chk_enable = 1;
        w.chk_mode = static_cast<std::uint64_t>(ChkMode::MEMORY_ADDR);
        w.mem_write = 1;
        w.is_last_uop = 1;
        break;
      case Opcode::BNDCHK:
        w.alu_op = static_cast<std::uint64_t>(AluOp::ADD);
        w.op_a_src = static_cast<std::uint64_t>(OperandASrc::RS1);
        w.op_b_src = static_cast<std::uint64_t>(OperandBSrc::IMM);
        w.chk_enable = 1;
        w.chk_mode = static_cast<std::uint64_t>(ChkMode::BNDCHK_INSTR);
        w.is_last_uop = 1;
        break;

      // Control Flow Branches
      case Opcode::B:
        w.is_branch = 1;
        w.is_last_uop = 1;
        break;
      case Opcode::BEQ:
        w.is_branch = 1;
        w.branch_cond = static_cast<std::uint64_t>(Cond::EQ);
        w.is_last_uop = 1;
        break;
      case Opcode::BNE:
        w.is_branch = 1;
        w.branch_cond = static_cast<std::uint64_t>(Cond::NE);
        w.is_last_uop = 1;
        break;
      case Opcode::BGT:
        w.is_branch = 1;
        w.branch_cond = static_cast<std::uint64_t>(Cond::GT);
        w.is_last_uop = 1;
        break;
      case Opcode::BLT:
        w.is_branch = 1;
        w.branch_cond = static_cast<std::uint64_t>(Cond::LT);
        w.is_last_uop = 1;
        break;

      // Multi-uOp Complex Subroutine Management (Cracked into 2 atomic u-ops)
      case Opcode::CALL:
        if (step == 0) { // uop 0: SP = SP - 4 (stack bounds checked)
          w.alu_op = static_cast<std::uint64_t>(AluOp::SUB);
          w.op_a_src = static_cast<std::uint64_t>(OperandASrc::RS1);
          w.op_b_src = static_cast<std::uint64_t>(OperandBSrc::IMM);
          w.chk_enable = 1;
          w.chk_mode = static_cast<std::uint64_t>(ChkMode::STACK_BOUNDS);
          w.reg_write_en = 1;
          w.reg_write_src = static_cast<std::uint64_t>(RegWriteSrc::ALU_OUT);
          w.freeze_if_of = 1;
          w.is_multi_uop = 1;
          w.is_last_uop = 0;
        } else { // uop 1: Mem[SP] = PC + 4, branch to target
          w.is_branch = 1;
          w.is_call = 1;
          w.mem_write = 1;
          w.is_multi_uop = 1;
          w.is_last_uop = 1;
        }
        break;

      case Opcode::RET:
        if (step == 0) { // uop 0: RA = Mem[SP] (stack pop)
          w.alu_op = static_cast<std::uint64_t>(AluOp::PASS_A);
          w.op_a_src = static_cast<std::uint64_t>(OperandASrc::RS1);
          w.chk_enable = 1;
          w.chk_mode = static_cast<std::uint64_t>(ChkMode::STACK_BOUNDS);
          w.mem_read = 1;
          w.reg_write_en = 1;
          w.reg_write_src = static_cast<std::uint64_t>(RegWriteSrc::MEM_DATA);
          w.freeze_if_of = 1;
          w.is_multi_uop = 1;
          w.is_last_uop = 0;
        } else { // uop 1: SP = SP + 4, branch RA
          w.alu_op = static_cast<std::uint64_t>(AluOp::ADD);
          w.op_a_src = static_cast<std::uint64_t>(OperandASrc::RS1);
          w.op_b_src = static_cast<std::uint64_t>(OperandBSrc::IMM);
          w.is_branch = 1;
          w.is_ret = 1;
          w.reg_write_en = 1;
          w.reg_write_src = static_cast<std::uint64_t>(RegWriteSrc::ALU_OUT);
          w.is_multi_uop = 1;
          w.is_last_uop = 1;
        }
        break;

      // Floating-Point Unit (FPU)
      case Opcode::FADD:
        w.is_fpu = 1;
        w.fpu_op = static_cast<std::uint64_t>(FpuOp::FADD);
        w.reg_write_en = 1;
        w.wb_is_fpu = 1;
        w.reg_write_src = static_cast<std::uint64_t>(RegWriteSrc::FPU_OUT);
        w.is_last_uop = 1;
        break;
      case Opcode::FSUB:
        w.is_fpu = 1;
        w.fpu_op = static_cast<std::uint64_t>(FpuOp::FSUB);
        w.reg_write_en = 1;
        w.wb_is_fpu = 1;
        w.reg_write_src = static_cast<std::uint64_t>(RegWriteSrc::FPU_OUT);
        w.is_last_uop = 1;
        break;
      case Opcode::FMUL:
        w.is_fpu = 1;
        w.fpu_op = static_cast<std::uint64_t>(FpuOp::FMUL);
        w.reg_write_en = 1;
        w.wb_is_fpu = 1;
        w.reg_write_src = static_cast<std::uint64_t>(RegWriteSrc::FPU_OUT);
        w.is_last_uop = 1;
        break;
      case Opcode::FDIV:
        w.is_fpu = 1;
        w.fpu_op = static_cast<std::uint64_t>(FpuOp::FDIV);
        w.reg_write_en = 1;
        w.wb_is_fpu = 1;
        w.reg_write_src = static_cast<std::uint64_t>(RegWriteSrc::FPU_OUT);
        w.is_last_uop = 1;
        break;
      case Opcode::FCMP:
        w.is_fpu = 1;
        w.fpu_op = static_cast<std::uint64_t>(FpuOp::FCMP);
        w.set_flags = 1;
        w.is_last_uop = 1;
        break;
      case Opcode::FMOV:
        w.is_fpu = 1;
        w.fpu_op = static_cast<std::uint64_t>(FpuOp::FMOV);
        w.reg_write_en = 1;
        w.wb_is_fpu = 1;
        w.reg_write_src = static_cast<std::uint64_t>(RegWriteSrc::FPU_OUT);
        w.is_last_uop = 1;
        break;
      case Opcode::FLD:
        w.alu_op = static_cast<std::uint64_t>(AluOp::ADD);
        w.op_a_src = static_cast<std::uint64_t>(OperandASrc::RS1);
        w.op_b_src = static_cast<std::uint64_t>(OperandBSrc::IMM);
        w.chk_enable = 1;
        w.chk_mode = static_cast<std::uint64_t>(ChkMode::MEMORY_ADDR);
        w.mem_read = 1;
        w.ma_is_fpu = 1;
        w.reg_write_en = 1;
        w.wb_is_fpu = 1;
        w.reg_write_src = static_cast<std::uint64_t>(RegWriteSrc::MEM_DATA);
        w.is_last_uop = 1;
        break;
      case Opcode::FST:
        w.alu_op = static_cast<std::uint64_t>(AluOp::ADD);
        w.op_a_src = static_cast<std::uint64_t>(OperandASrc::RS1);
        w.op_b_src = static_cast<std::uint64_t>(OperandBSrc::IMM);
        w.chk_enable = 1;
        w.chk_mode = static_cast<std::uint64_t>(ChkMode::MEMORY_ADDR);
        w.mem_write = 1;
        w.ma_is_fpu = 1;
        w.is_last_uop = 1;
        break;

      // System
      case Opcode::HALT:
        w.halt = 1;
        w.is_last_uop = 1;
        break;
      case Opcode::NOP:
      default:
        w.is_last_uop = 1;
        break;
    }
    return w;
  }

  static constexpr VerticalControlWord build_vertical_entry(Opcode op, std::uint16_t step) {
    VerticalControlWord w{};
    w.next_uop_step = static_cast<std::uint32_t>(step & 0x7);

    switch (op) {
      // ALU 3-Operand
      case Opcode::ADD: w.op_class = 0; w.sub_op = 1;  w.operand_mode = 0; w.wb_target = 1; w.is_last_uop = 1; break;
      case Opcode::SUB: w.op_class = 0; w.sub_op = 2;  w.operand_mode = 0; w.wb_target = 1; w.is_last_uop = 1; break;
      case Opcode::MUL: w.op_class = 0; w.sub_op = 3;  w.operand_mode = 0; w.wb_target = 1; w.is_last_uop = 1; break;
      case Opcode::DIV: w.op_class = 0; w.sub_op = 4;  w.operand_mode = 0; w.wb_target = 1; w.is_last_uop = 1; break;
      case Opcode::MOD: w.op_class = 0; w.sub_op = 5;  w.operand_mode = 0; w.wb_target = 1; w.is_last_uop = 1; break;
      case Opcode::AND: w.op_class = 0; w.sub_op = 6;  w.operand_mode = 0; w.wb_target = 1; w.is_last_uop = 1; break;
      case Opcode::OR:  w.op_class = 0; w.sub_op = 7;  w.operand_mode = 0; w.wb_target = 1; w.is_last_uop = 1; break;
      case Opcode::NOT: w.op_class = 0; w.sub_op = 8;  w.operand_mode = 2; w.wb_target = 1; w.is_last_uop = 1; break;
      case Opcode::XOR: w.op_class = 0; w.sub_op = 9;  w.operand_mode = 0; w.wb_target = 1; w.is_last_uop = 1; break;
      case Opcode::LSL: w.op_class = 0; w.sub_op = 10; w.operand_mode = 0; w.wb_target = 1; w.is_last_uop = 1; break;
      case Opcode::LSR: w.op_class = 0; w.sub_op = 11; w.operand_mode = 0; w.wb_target = 1; w.is_last_uop = 1; break;
      case Opcode::ASR: w.op_class = 0; w.sub_op = 12; w.operand_mode = 0; w.wb_target = 1; w.is_last_uop = 1; break;
      case Opcode::CMP: w.op_class = 0; w.sub_op = 13; w.operand_mode = 0; w.wb_target = 0; w.set_flags = 1; w.is_last_uop = 1; break;
      case Opcode::MOV: w.op_class = 0; w.sub_op = 14; w.operand_mode = 2; w.wb_target = 1; w.is_last_uop = 1; break;

      // Memory
      case Opcode::LD:     w.op_class = 2; w.sub_op = 1; w.operand_mode = 1; w.mem_action = 1; w.wb_target = 2; w.is_last_uop = 1; break;
      case Opcode::ST:     w.op_class = 2; w.sub_op = 2; w.operand_mode = 1; w.mem_action = 2; w.wb_target = 0; w.is_last_uop = 1; break;
      case Opcode::FLD:    w.op_class = 2; w.sub_op = 3; w.operand_mode = 1; w.mem_action = 1; w.wb_target = 2; w.is_fpu_domain = 1; w.is_last_uop = 1; break;
      case Opcode::FST:    w.op_class = 2; w.sub_op = 4; w.operand_mode = 1; w.mem_action = 2; w.wb_target = 0; w.is_fpu_domain = 1; w.is_last_uop = 1; break;
      case Opcode::BNDCHK: w.op_class = 2; w.sub_op = 5; w.operand_mode = 1; w.mem_action = 3; w.wb_target = 0; w.is_last_uop = 1; break;

      // Branch
      case Opcode::B:   w.op_class = 3; w.sub_op = 0; w.operand_mode = 1; w.wb_target = 0; w.is_last_uop = 1; break;
      case Opcode::BEQ: w.op_class = 3; w.sub_op = 0; w.operand_mode = 1; w.wb_target = 0; w.is_last_uop = 1; break;
      case Opcode::BNE: w.op_class = 3; w.sub_op = 1; w.operand_mode = 1; w.wb_target = 0; w.is_last_uop = 1; break;
      case Opcode::BGT: w.op_class = 3; w.sub_op = 2; w.operand_mode = 1; w.wb_target = 0; w.is_last_uop = 1; break;
      case Opcode::BLT: w.op_class = 3; w.sub_op = 3; w.operand_mode = 1; w.wb_target = 0; w.is_last_uop = 1; break;

      // Multi-Uop Control Flow
      case Opcode::CALL:
        w.op_class = 5; w.sub_op = 1; w.is_multi_uop = 1;
        if (step == 0) {
          w.operand_mode = 1; w.wb_target = 1; w.freeze_if_of = 1; w.is_last_uop = 0;
        } else {
          w.operand_mode = 1; w.mem_action = 2; w.wb_target = 0; w.is_last_uop = 1;
        }
        break;
      case Opcode::RET:
        w.op_class = 5; w.sub_op = 2; w.is_multi_uop = 1;
        if (step == 0) {
          w.operand_mode = 4; w.mem_action = 1; w.wb_target = 2; w.freeze_if_of = 1; w.is_last_uop = 0;
        } else {
          w.operand_mode = 1; w.wb_target = 1; w.is_last_uop = 1;
        }
        break;

      // Floating-Point Unit
      case Opcode::FADD: w.op_class = 1; w.sub_op = 1; w.operand_mode = 0; w.wb_target = 4; w.is_fpu_domain = 1; w.is_last_uop = 1; break;
      case Opcode::FSUB: w.op_class = 1; w.sub_op = 2; w.operand_mode = 0; w.wb_target = 4; w.is_fpu_domain = 1; w.is_last_uop = 1; break;
      case Opcode::FMUL: w.op_class = 1; w.sub_op = 3; w.operand_mode = 0; w.wb_target = 4; w.is_fpu_domain = 1; w.is_last_uop = 1; break;
      case Opcode::FDIV: w.op_class = 1; w.sub_op = 4; w.operand_mode = 0; w.wb_target = 4; w.is_fpu_domain = 1; w.is_last_uop = 1; break;
      case Opcode::FCMP: w.op_class = 1; w.sub_op = 5; w.operand_mode = 0; w.wb_target = 0; w.is_fpu_domain = 1; w.set_flags = 1; w.is_last_uop = 1; break;
      case Opcode::FMOV: w.op_class = 1; w.sub_op = 6; w.operand_mode = 2; w.wb_target = 4; w.is_fpu_domain = 1; w.is_last_uop = 1; break;

      // System
      case Opcode::HALT: w.op_class = 4; w.sub_op = 1; w.halt = 1; w.is_last_uop = 1; break;
      case Opcode::NOP:
      default:
        w.op_class = 4; w.sub_op = 0; w.is_last_uop = 1; break;
    }
    return w;
  }
};

inline const HorizontalControlWord& lookup_horizontal_rom(Opcode op, std::uint16_t step = 0) {
  static const auto table = []() {
    std::array<HorizontalControlWord, kControlStoreDepth> t{};
    for (std::size_t i = 0; i < 64; ++i) {
      const Opcode o = static_cast<Opcode>(i);
      t[rom_address(o, 0)] = ControlStore::build_horizontal_entry(o, 0);
      t[rom_address(o, 1)] = ControlStore::build_horizontal_entry(o, 1);
    }
    return t;
  }();
  const std::size_t idx = rom_address(op, step);
  return table[idx < kControlStoreDepth ? idx : 0];
}

inline const VerticalControlWord& lookup_vertical_rom(Opcode op, std::uint16_t step = 0) {
  static const auto table = []() {
    std::array<VerticalControlWord, kControlStoreDepth> t{};
    for (std::size_t i = 0; i < 64; ++i) {
      const Opcode o = static_cast<Opcode>(i);
      t[rom_address(o, 0)] = ControlStore::build_vertical_entry(o, 0);
      t[rom_address(o, 1)] = ControlStore::build_vertical_entry(o, 1);
    }
    return t;
  }();
  const std::size_t idx = rom_address(op, step);
  return table[idx < kControlStoreDepth ? idx : 0];
}

}  // namespace risc201::pipeline

namespace risc201::cu {
using pipeline::rom_address;
using pipeline::lookup_horizontal_rom;
using pipeline::lookup_vertical_rom;
}  // namespace risc201::cu
