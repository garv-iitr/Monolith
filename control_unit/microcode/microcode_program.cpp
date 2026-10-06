#include "microcode/microcode_program.h"

namespace risc201::cu {
namespace {

using namespace micro;
using IR = InternalReg;
using SC = StageClass;

enum { L_IMM = 0, L_RW = 1, L_BRANCH = 2 };

Routine alu3(AluOp op) {
  return {
      mmov(IR::REG_SRC, IR::RS1, a_reg(RegOp::READ)),
      mmov(IR::A, IR::REG_VAL),
      mbeq(IR::I, 1, L_IMM),
      mmov(IR::REG_SRC, IR::RS2, a_reg(RegOp::READ)),
      mmov(IR::B, IR::REG_VAL, a_alu(op)),
      mb(L_RW),
      mlabel(L_IMM, mmov(IR::B, IR::IMMX, a_alu(op))),
      mlabel(L_RW, mmov(IR::REG_SRC, IR::RD)),
      mmov(IR::REG_DATA, IR::ALU_RESULT, a_reg(RegOp::WRITE), SC::WRITEBACK),
      mb(kPreamble),
  };
}

Routine not_routine() {
  return {
      mbeq(IR::I, 1, L_IMM),
      mmov(IR::REG_SRC, IR::RS2, a_reg(RegOp::READ)),
      mmov(IR::B, IR::REG_VAL, a_alu(AluOp::NOT)),
      mb(L_RW),
      mlabel(L_IMM, mmov(IR::B, IR::IMMX, a_alu(AluOp::NOT))),
      mlabel(L_RW, mmov(IR::REG_DATA, IR::ALU_RESULT)),
      mmov(IR::REG_SRC, IR::RD, a_reg(RegOp::WRITE), SC::WRITEBACK),
      mb(kPreamble),
  };
}

Routine mov_routine() {
  return {
      mbeq(IR::I, 1, L_IMM),
      mmov(IR::REG_SRC, IR::RS2, a_reg(RegOp::READ)),
      mmov(IR::B, IR::REG_VAL, a_alu(AluOp::PASS_B)),
      mb(L_RW),
      mlabel(L_IMM, mmov(IR::B, IR::IMMX, a_alu(AluOp::PASS_B))),
      mlabel(L_RW, mmov(IR::REG_DATA, IR::ALU_RESULT)),
      mmov(IR::REG_SRC, IR::RD, a_reg(RegOp::WRITE), SC::WRITEBACK),
      mb(kPreamble),
  };
}

Routine cmp_routine() {
  return {
      mmov(IR::REG_SRC, IR::RS1, a_reg(RegOp::READ)),
      mmov(IR::A, IR::REG_VAL),
      mbeq(IR::I, 1, L_IMM),
      mmov(IR::REG_SRC, IR::RS2, a_reg(RegOp::READ)),
      mmov(IR::B, IR::REG_VAL, a_alu(AluOp::CMP)),
      mb(kPreamble),
      mlabel(L_IMM, mmov(IR::B, IR::IMMX, a_alu(AluOp::CMP))),
      mb(kPreamble),
  };
}

Routine ld_routine(StageLayout layout) {
  Routine r = {
      mmov(IR::REG_SRC, IR::RS1, a_reg(RegOp::READ)),
      mmov(IR::A, IR::REG_VAL),
      mmov(IR::B, IR::IMMX, a_alu(AluOp::ADD)),
  };
  if (layout == StageLayout::SIX_STAGE) {
    r.push_back(mmov(IR::MEMCHK_ADDR, IR::ALU_RESULT, a_chk(MemChkOp::CHECK), SC::CHECK));
  }
  r.push_back(mmov(IR::MAR, IR::ALU_RESULT, a_mem(MemOp::READ), SC::MEMORY));
  r.push_back(mmov(IR::REG_DATA, IR::LD_RESULT, 0, SC::MEMORY));
  r.push_back(mmov(IR::REG_SRC, IR::RD, a_reg(RegOp::WRITE), SC::WRITEBACK));
  r.push_back(mb(kPreamble));
  return r;
}

Routine st_routine(StageLayout layout) {
  Routine r = {
      mmov(IR::REG_SRC, IR::RS1, a_reg(RegOp::READ)),
      mmov(IR::A, IR::REG_VAL),
      mmov(IR::B, IR::IMMX, a_alu(AluOp::ADD)),
  };
  if (layout == StageLayout::SIX_STAGE) {
    r.push_back(mmov(IR::MEMCHK_ADDR, IR::ALU_RESULT, a_chk(MemChkOp::CHECK), SC::CHECK));
  }
  r.push_back(mmov(IR::MAR, IR::ALU_RESULT, 0, SC::MEMORY));
  r.push_back(mmov(IR::REG_SRC, IR::RD, a_reg(RegOp::READ), SC::MEMORY));
  r.push_back(mmov(IR::MDR, IR::REG_VAL, a_mem(MemOp::WRITE), SC::MEMORY));
  r.push_back(mb(kPreamble));
  return r;
}

Routine b_routine() {
  return {
      mmov(IR::PC, IR::BRANCH_TARGET),
      mb(kPreamble),
  };
}

Routine branch_if(InternalReg flag, std::int32_t value) {
  return {
      mbeq(flag, value, L_BRANCH),
      mb(kPreamble),
      mlabel(L_BRANCH, mmov(IR::PC, IR::BRANCH_TARGET)),
      mb(kPreamble),
  };
}

// full-descending stack, sp is r14
Routine call_routine(StageLayout layout) {
  Routine r = {
      mmovi(IR::REG_SRC, REG_SP, a_reg(RegOp::READ)),
      madd(IR::REG_VAL, -4),
      mmov(IR::MAR, IR::REG_VAL),
  };
  if (layout == StageLayout::SIX_STAGE) {
    r.push_back(mmov(IR::MEMCHK_ADDR, IR::MAR, a_chk(MemChkOp::CHECK), SC::CHECK));
  }
  r.push_back(mmov(IR::REG_DATA, IR::REG_VAL, a_reg(RegOp::WRITE), SC::WRITEBACK));
  r.push_back(mmov(IR::MDR, IR::PC, a_mem(MemOp::WRITE), SC::MEMORY));
  r.push_back(mmov(IR::PC, IR::BRANCH_TARGET));
  r.push_back(mb(kPreamble));
  return r;
}

Routine ret_routine(StageLayout layout) {
  Routine r = {
      mmovi(IR::REG_SRC, REG_SP, a_reg(RegOp::READ)),
      mmov(IR::MAR, IR::REG_VAL),
  };
  if (layout == StageLayout::SIX_STAGE) {
    r.push_back(mmov(IR::MEMCHK_ADDR, IR::MAR, a_chk(MemChkOp::CHECK), SC::CHECK));
  }
  r.push_back(mmov(IR::MAR, IR::REG_VAL, a_mem(MemOp::READ), SC::MEMORY));
  r.push_back(mmov(IR::PC, IR::LD_RESULT, 0, SC::MEMORY));
  r.push_back(madd(IR::REG_VAL, 4));
  r.push_back(mmov(IR::REG_DATA, IR::REG_VAL, a_reg(RegOp::WRITE), SC::WRITEBACK));
  r.push_back(mb(kPreamble));
  return r;
}

Routine halt_routine() { return {mhalt()}; }

// ---- Format 4: BNDCHK -------------------------------------------------------
Routine bndchk_routine() {
  return {
      mmov(IR::REG_SRC, IR::RS1, a_reg(RegOp::READ)),
      mmov(IR::A, IR::REG_VAL),
      mmov(IR::B, IR::IMMX, a_alu(AluOp::ADD)),
      mmov(IR::MEMCHK_ADDR, IR::ALU_RESULT, a_chk(MemChkOp::CHECK), SC::CHECK),
      mb(kPreamble),
  };
}

// ---- Format 7: Floating-point 3-operand (FADD, FSUB, FMUL, FDIV) ------------
Routine fpu3(FpuOp op) {
  return {
      mmov(IR::FP_SRC, IR::RS1, a_fpu(FpuOp::READ)),
      mmov(IR::FP_A, IR::FP_VAL),
      mmov(IR::FP_SRC, IR::RS2, a_fpu(FpuOp::READ)),
      mmov(IR::FP_B, IR::FP_VAL, a_fpu(op)),
      mmov(IR::FP_DATA, IR::FPU_RESULT),
      mmov(IR::FP_SRC, IR::RD, a_fpu(FpuOp::WRITE), SC::WRITEBACK),
      mb(kPreamble),
  };
}

// ---- Format 8: Floating-point 2-operand (FCMP, FMOV) ------------------------
Routine fcmp_routine() {
  return {
      mmov(IR::FP_SRC, IR::RS1, a_fpu(FpuOp::READ)),
      mmov(IR::FP_A, IR::FP_VAL),
      mmov(IR::FP_SRC, IR::RS2, a_fpu(FpuOp::READ)),
      mmov(IR::FP_B, IR::FP_VAL, a_fpu(FpuOp::FCMP)),
      mb(kPreamble),
  };
}

Routine fmov_routine() {
  return {
      mmov(IR::FP_SRC, IR::RS2, a_fpu(FpuOp::READ)),
      mmov(IR::FP_B, IR::FP_VAL, a_fpu(FpuOp::FMOV)),
      mmov(IR::FP_DATA, IR::FPU_RESULT),
      mmov(IR::FP_SRC, IR::RD, a_fpu(FpuOp::WRITE), SC::WRITEBACK),
      mb(kPreamble),
  };
}

// ---- Format 4: Floating-point memory (FLD, FST) -----------------------------
Routine fld_routine(StageLayout layout) {
  Routine r = {
      mmov(IR::REG_SRC, IR::RS1, a_reg(RegOp::READ)),
      mmov(IR::A, IR::REG_VAL),
      mmov(IR::B, IR::IMMX, a_alu(AluOp::ADD)),
  };
  if (layout == StageLayout::SIX_STAGE) {
    r.push_back(mmov(IR::MEMCHK_ADDR, IR::ALU_RESULT, a_chk(MemChkOp::CHECK), SC::CHECK));
  }
  r.push_back(mmov(IR::MAR, IR::ALU_RESULT, a_mem(MemOp::READ), SC::MEMORY));
  r.push_back(mmov(IR::FP_DATA, IR::LD_RESULT, 0, SC::MEMORY));
  r.push_back(mmov(IR::FP_SRC, IR::RD, a_fpu(FpuOp::WRITE), SC::WRITEBACK));
  r.push_back(mb(kPreamble));
  return r;
}

Routine fst_routine(StageLayout layout) {
  Routine r = {
      mmov(IR::REG_SRC, IR::RS1, a_reg(RegOp::READ)),
      mmov(IR::A, IR::REG_VAL),
      mmov(IR::B, IR::IMMX, a_alu(AluOp::ADD)),
  };
  if (layout == StageLayout::SIX_STAGE) {
    r.push_back(mmov(IR::MEMCHK_ADDR, IR::ALU_RESULT, a_chk(MemChkOp::CHECK), SC::CHECK));
  }
  r.push_back(mmov(IR::MAR, IR::ALU_RESULT, 0, SC::MEMORY));
  r.push_back(mmov(IR::FP_SRC, IR::RD, a_fpu(FpuOp::READ), SC::MEMORY));
  r.push_back(mmov(IR::MDR, IR::FP_VAL, a_mem(MemOp::WRITE), SC::MEMORY));
  r.push_back(mb(kPreamble));
  return r;
}

// ---- Format 4: BNDCHK -------------------------------------------------------
Routine bndchk_routine() {
  return {
      mmov(IR::REG_SRC, IR::RS1, a_reg(RegOp::READ)),
      mmov(IR::A, IR::REG_VAL),
      mmov(IR::B, IR::IMMX, a_alu(AluOp::ADD)),
      mmov(IR::MEMCHK_ADDR, IR::ALU_RESULT, a_chk(MemChkOp::CHECK), SC::CHECK),
      mb(kPreamble),
  };
}

// ---- Format 7: Floating-point 3-operand (FADD, FSUB, FMUL, FDIV) ------------
Routine fpu3(FpuOp op) {
  return {
      mmov(IR::FP_SRC, IR::RS1, a_fpu(FpuOp::READ)),
      mmov(IR::FP_A, IR::FP_VAL),
      mmov(IR::FP_SRC, IR::RS2, a_fpu(FpuOp::READ)),
      mmov(IR::FP_B, IR::FP_VAL, a_fpu(op)),
      mmov(IR::FP_DATA, IR::FPU_RESULT),
      mmov(IR::FP_SRC, IR::RD, a_fpu(FpuOp::WRITE), SC::WRITEBACK),
      mb(kPreamble),
  };
}

// ---- Format 8: Floating-point 2-operand (FCMP, FMOV) ------------------------
Routine fcmp_routine() {
  return {
      mmov(IR::FP_SRC, IR::RS1, a_fpu(FpuOp::READ)),
      mmov(IR::FP_A, IR::FP_VAL),
      mmov(IR::FP_SRC, IR::RS2, a_fpu(FpuOp::READ)),
      mmov(IR::FP_B, IR::FP_VAL, a_fpu(FpuOp::FCMP)),
      mb(kPreamble),
  };
}

Routine fmov_routine() {
  return {
      mmov(IR::FP_SRC, IR::RS2, a_fpu(FpuOp::READ)),
      mmov(IR::FP_B, IR::FP_VAL, a_fpu(FpuOp::FMOV)),
      mmov(IR::FP_DATA, IR::FPU_RESULT),
      mmov(IR::FP_SRC, IR::RD, a_fpu(FpuOp::WRITE), SC::WRITEBACK),
      mb(kPreamble),
  };
}

// ---- Format 4: Floating-point memory (FLD, FST) -----------------------------
Routine fld_routine(StageLayout layout) {
  Routine r = {
      mmov(IR::REG_SRC, IR::RS1, a_reg(RegOp::READ)),
      mmov(IR::A, IR::REG_VAL),
      mmov(IR::B, IR::IMMX, a_alu(AluOp::ADD)),
  };
  if (layout == StageLayout::SIX_STAGE) {
    r.push_back(mmov(IR::MEMCHK_ADDR, IR::ALU_RESULT, a_chk(MemChkOp::CHECK), SC::CHECK));
  }
  r.push_back(mmov(IR::MAR, IR::ALU_RESULT, a_mem(MemOp::READ), SC::MEMORY));
  r.push_back(mmov(IR::FP_DATA, IR::LD_RESULT, 0, SC::MEMORY));
  r.push_back(mmov(IR::FP_SRC, IR::RD, a_fpu(FpuOp::WRITE), SC::WRITEBACK));
  r.push_back(mb(kPreamble));
  return r;
}

Routine fst_routine(StageLayout layout) {
  Routine r = {
      mmov(IR::REG_SRC, IR::RS1, a_reg(RegOp::READ)),
      mmov(IR::A, IR::REG_VAL),
      mmov(IR::B, IR::IMMX, a_alu(AluOp::ADD)),
  };
  if (layout == StageLayout::SIX_STAGE) {
    r.push_back(mmov(IR::MEMCHK_ADDR, IR::ALU_RESULT, a_chk(MemChkOp::CHECK), SC::CHECK));
  }
  r.push_back(mmov(IR::MAR, IR::ALU_RESULT, 0, SC::MEMORY));
  r.push_back(mmov(IR::FP_SRC, IR::RD, a_fpu(FpuOp::READ), SC::MEMORY));
  r.push_back(mmov(IR::MDR, IR::FP_VAL, a_mem(MemOp::WRITE), SC::MEMORY));
  r.push_back(mb(kPreamble));
  return r;
}

}

Program build_program(StageLayout layout) {
  Program p;
  auto add = [&p](Opcode op, Routine r) { p.push_back(ProgramEntry{op, std::move(r)}); };

  add(Opcode::ADD, alu3(AluOp::ADD));
  add(Opcode::SUB, alu3(AluOp::SUB));
  add(Opcode::MUL, alu3(AluOp::MUL));
  add(Opcode::DIV, alu3(AluOp::DIV));
  add(Opcode::MOD, alu3(AluOp::MOD));
  add(Opcode::AND, alu3(AluOp::AND));
  add(Opcode::OR,  alu3(AluOp::OR));
  add(Opcode::XOR, alu3(AluOp::XOR));
  add(Opcode::LSL, alu3(AluOp::LSL));
  add(Opcode::LSR, alu3(AluOp::LSR));
  add(Opcode::ASR, alu3(AluOp::ASR));

  add(Opcode::NOT, not_routine());
  add(Opcode::MOV, mov_routine());
  add(Opcode::CMP, cmp_routine());

  add(Opcode::LD, ld_routine(layout));
  add(Opcode::ST, st_routine(layout));

  add(Opcode::B,    b_routine());
  add(Opcode::BEQ,  branch_if(IR::FLAGS_E, 1));
  add(Opcode::BNE,  branch_if(IR::FLAGS_E, 0));
  add(Opcode::BGT,  branch_if(IR::FLAGS_GT, 1));
  add(Opcode::BLT,  branch_if(IR::FLAGS_N, 1));
  add(Opcode::CALL, call_routine(layout));
  add(Opcode::RET,  ret_routine(layout));
  add(Opcode::HALT, halt_routine());

  add(Opcode::NOP, {mb(kPreamble)});

  // FPU, format 7/8
  add(Opcode::FADD, fpu3(FpuOp::FADD));
  add(Opcode::FSUB, fpu3(FpuOp::FSUB));
  add(Opcode::FMUL, fpu3(FpuOp::FMUL));
  add(Opcode::FDIV, fpu3(FpuOp::FDIV));
  add(Opcode::FCMP, fcmp_routine());
  add(Opcode::FMOV, fmov_routine());

  // FPU memory, format 4
  add(Opcode::FLD, fld_routine(layout));
  add(Opcode::FST, fst_routine(layout));

  // Memory bounds check
  add(Opcode::BNDCHK, bndchk_routine());

  return p;
}

}
