#include "microcode/microcode_program.h"

namespace risc201::cu {
namespace {

using namespace micro;
using IR = InternalReg;  // shorthand
using SC = StageClass;

// Routine-local labels.
enum { L_IMM = 0, L_RW = 1, L_BRANCH = 2 };

// ---- Format 1: three-operand ALU (reg or imm) ------------------------------
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

// ---- Format 2: NOT (two-operand, ALU) --------------------------------------
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

// ---- Format 2: MOV ----------------------------------------------------------
Routine mov_routine() {
  return {
      mbeq(IR::I, 1, L_IMM),
      mmov(IR::REG_SRC, IR::RS2, a_reg(RegOp::READ)),
      mmov(IR::REG_DATA, IR::REG_VAL),
      mb(L_RW),
      mlabel(L_IMM, mmov(IR::REG_DATA, IR::IMMX)),
      mlabel(L_RW, mmov(IR::REG_SRC, IR::RD, a_reg(RegOp::WRITE), SC::WRITEBACK)),
      mb(kPreamble),
  };
}

// ---- Format 3: CMP ----------------------------------------------------------
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

// ---- Data movement: LD / ST -------------------------------------------------
Routine ld_routine() {
  return {
      mmov(IR::REG_SRC, IR::RS1, a_reg(RegOp::READ)),
      mmov(IR::A, IR::REG_VAL),
      mmov(IR::B, IR::IMMX, a_alu(AluOp::ADD)),          // effective address
      mmov(IR::MEMCHK_ADDR, IR::ALU_RESULT, a_chk(MemChkOp::CHECK), SC::CHECK),
      mmov(IR::MAR, IR::ALU_RESULT, a_mem(MemOp::READ), SC::MEMORY),
      mmov(IR::REG_DATA, IR::LD_RESULT, 0, SC::MEMORY),
      mmov(IR::REG_SRC, IR::RD, a_reg(RegOp::WRITE), SC::WRITEBACK),
      mb(kPreamble),
  };
}

Routine st_routine() {
  return {
      mmov(IR::REG_SRC, IR::RS1, a_reg(RegOp::READ)),
      mmov(IR::A, IR::REG_VAL),
      mmov(IR::B, IR::IMMX, a_alu(AluOp::ADD)),
      mmov(IR::MEMCHK_ADDR, IR::ALU_RESULT, a_chk(MemChkOp::CHECK), SC::CHECK),
      mmov(IR::MAR, IR::ALU_RESULT, 0, SC::MEMORY),
      mmov(IR::REG_SRC, IR::RD, a_reg(RegOp::READ), SC::MEMORY),
      mmov(IR::MDR, IR::REG_VAL, a_mem(MemOp::WRITE), SC::MEMORY),
      mb(kPreamble),
  };
}

// ---- Control flow -----------------------------------------------------------
Routine b_routine() {
  return {
      mmov(IR::PC, IR::BRANCH_TARGET),
      mb(kPreamble),
  };
}

// Conditional branch: `flag` == `value` -> jump.
Routine branch_if(InternalReg flag, std::int32_t value) {
  return {
      mbeq(flag, value, L_BRANCH),
      mb(kPreamble),
      mlabel(L_BRANCH, mmov(IR::PC, IR::BRANCH_TARGET)),
      mb(kPreamble),
  };
}

// Full-descending stack call / ret (SP = r14).
Routine call_routine() {
  return {
      mmovi(IR::REG_SRC, REG_SP, a_reg(RegOp::READ)),  // regSrc <- sp
      madd(IR::REG_VAL, -4),                           // sp - 4
      mmov(IR::MAR, IR::REG_VAL),                      // mar <- new sp
      mmov(IR::REG_DATA, IR::REG_VAL, a_reg(RegOp::WRITE), SC::WRITEBACK),
      mmov(IR::MDR, IR::PC, a_mem(MemOp::WRITE), SC::MEMORY),
      mmov(IR::PC, IR::BRANCH_TARGET),
      mb(kPreamble),
  };
}

Routine ret_routine() {
  return {
      mmovi(IR::REG_SRC, REG_SP, a_reg(RegOp::READ)),
      mmov(IR::MAR, IR::REG_VAL, a_mem(MemOp::READ), SC::MEMORY),
      mmov(IR::PC, IR::LD_RESULT, 0, SC::MEMORY),
      madd(IR::REG_VAL, 4),
      mmov(IR::REG_DATA, IR::REG_VAL, a_reg(RegOp::WRITE), SC::WRITEBACK),
      mb(kPreamble),
  };
}

// BNDCHK rs1, #imm -- bounds/presence check of the effective address.
Routine bndchk_routine() {
  return {
      mmov(IR::REG_SRC, IR::RS1, a_reg(RegOp::READ)),
      mmov(IR::A, IR::REG_VAL),
      mmov(IR::B, IR::IMMX, a_alu(AluOp::ADD)),
      mmov(IR::MEMCHK_ADDR, IR::ALU_RESULT, a_chk(MemChkOp::CHECK), SC::CHECK),
      mb(kPreamble),
  };
}

Routine halt_routine() { return {mhalt()}; }

// ---- FPU --------------------------------------------------------------------
Routine fpu3(FpuOp op) {
  return {
      mmov(IR::FP_SRC, IR::RS1, a_fpu(FpuOp::READ)),   // fs1
      mmov(IR::FP_A, IR::FP_VAL),
      mmov(IR::FP_SRC, IR::RS2, a_fpu(FpuOp::READ)),   // fs2
      mmov(IR::FP_B, IR::FP_VAL, a_fpu(op)),
      mmov(IR::FP_SRC, IR::RD),                        // fd
      mmov(IR::FP_DATA, IR::FPU_RESULT, a_fpu(FpuOp::WRITE), SC::WRITEBACK),
      mb(kPreamble),
  };
}

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
      mmov(IR::FP_SRC, IR::RD),
      mmov(IR::FP_DATA, IR::FPU_RESULT, a_fpu(FpuOp::WRITE), SC::WRITEBACK),
      mb(kPreamble),
  };
}

Routine fld_routine() {
  return {
      mmov(IR::REG_SRC, IR::RS1, a_reg(RegOp::READ)),
      mmov(IR::A, IR::REG_VAL),
      mmov(IR::B, IR::IMMX, a_alu(AluOp::ADD)),
      mmov(IR::MEMCHK_ADDR, IR::ALU_RESULT, a_chk(MemChkOp::CHECK), SC::CHECK),
      mmov(IR::MAR, IR::ALU_RESULT, a_mem(MemOp::READ), SC::MEMORY),
      mmov(IR::FP_DATA, IR::LD_RESULT, 0, SC::MEMORY),
      mmov(IR::FP_SRC, IR::RD, a_fpu(FpuOp::WRITE), SC::WRITEBACK),
      mb(kPreamble),
  };
}

Routine fst_routine() {
  return {
      mmov(IR::REG_SRC, IR::RS1, a_reg(RegOp::READ)),
      mmov(IR::A, IR::REG_VAL),
      mmov(IR::B, IR::IMMX, a_alu(AluOp::ADD)),
      mmov(IR::MEMCHK_ADDR, IR::ALU_RESULT, a_chk(MemChkOp::CHECK), SC::CHECK),
      mmov(IR::MAR, IR::ALU_RESULT, 0, SC::MEMORY),
      mmov(IR::FP_SRC, IR::RD, a_fpu(FpuOp::READ), SC::MEMORY),
      mmov(IR::MDR, IR::FP_VAL, a_mem(MemOp::WRITE), SC::MEMORY),
      mb(kPreamble),
  };
}

}  // namespace

Program build_program() {
  Program p;
  auto add = [&p](Opcode op, Routine r) { p.push_back(ProgramEntry{op, std::move(r)}); };

  // ALU, format 1
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

  // ALU, format 2/3
  add(Opcode::NOT, not_routine());
  add(Opcode::MOV, mov_routine());
  add(Opcode::CMP, cmp_routine());

  // Data movement
  add(Opcode::LD, ld_routine());
  add(Opcode::ST, st_routine());

  // Control flow
  add(Opcode::B,    b_routine());
  add(Opcode::BEQ,  branch_if(IR::FLAGS_E, 1));
  add(Opcode::BNE,  branch_if(IR::FLAGS_E, 0));
  add(Opcode::BGT,  branch_if(IR::FLAGS_GT, 1));
  add(Opcode::BLT,  branch_if(IR::FLAGS_N, 1));
  add(Opcode::CALL, call_routine());
  add(Opcode::RET,  ret_routine());
  add(Opcode::BNDCHK, bndchk_routine());
  add(Opcode::HALT, halt_routine());

  // FPU
  add(Opcode::FADD, fpu3(FpuOp::FADD));
  add(Opcode::FSUB, fpu3(FpuOp::FSUB));
  add(Opcode::FMUL, fpu3(FpuOp::FMUL));
  add(Opcode::FDIV, fpu3(FpuOp::FDIV));
  add(Opcode::FCMP, fcmp_routine());
  add(Opcode::FMOV, fmov_routine());
  add(Opcode::FLD,  fld_routine());
  add(Opcode::FST,  fst_routine());

  add(Opcode::NOP, {mb(kPreamble)});

  return p;
}

}  // namespace risc201::cu
