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

  return p;
}

}
