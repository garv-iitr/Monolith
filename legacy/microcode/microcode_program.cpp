// Major use: Implements microcode routines for integer ALU, memory, branch, stack, and floating-point instructions.
#include "microcode/microcode_program.h"

namespace risc201::cu {
namespace {

using namespace micro;
using IR = InternalReg;
using SC = StageClass;

// Local symbolic jump labels within microcode routines
enum { L_IMM = 0, L_RW = 1, L_BRANCH = 2 };

// 3-operand integer ALU routine: reads RS1, evaluates I-bit, reads RS2 or IMMX, triggers ALU, and writes RD
Routine alu3(AluOp op) {
  return {
      mmov(IR::REG_SRC, IR::RS1, a_reg(RegOp::READ)),       // 1. Read rs1 from register file into reg_val
      mmov(IR::A, IR::REG_VAL),                             // 2. Latch reg_val into ALU input A
      mbeq(IR::I, 1, L_IMM),                                // 3. If immediate mode flag is set, branch to L_IMM
      mmov(IR::REG_SRC, IR::RS2, a_reg(RegOp::READ)),       // 4. Register mode: Read rs2 from register file
      mmov(IR::B, IR::REG_VAL, a_alu(op)),                  // 5. Latch rs2 into ALU input B and trigger ALU op
      mb(L_RW),                                             // 6. Jump to writeback phase
      mlabel(L_IMM, mmov(IR::B, IR::IMMX, a_alu(op))),      // 7. Immediate mode: Latch expanded immx into B and trigger ALU
      mlabel(L_RW, mmov(IR::REG_SRC, IR::RD)),              // 8. Select rd as register file write destination
      mmov(IR::REG_DATA, IR::ALU_RESULT, a_reg(RegOp::WRITE), SC::WRITEBACK), // 9. Write ALU result into rd
      mb(kPreamble),                                        // 10. Return to instruction fetch preamble
  };
}

// 2-operand bitwise NOT routine: inverts RS2 or immediate and commits result to RD
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

// Register/immediate move routine: passes RS2 or immediate through ALU pass-through B into RD
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

// Integer comparison routine: calculates (RS1 - operand2), setting CPU flags without register writeback
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

// Word load routine (LD): calculates effective address (RS1 + immx), checks safety, reads memory, writes RD
Routine ld_routine(StageLayout layout) {
  Routine r = {
      mmov(IR::REG_SRC, IR::RS1, a_reg(RegOp::READ)),
      mmov(IR::A, IR::REG_VAL),
      mmov(IR::B, IR::IMMX, a_alu(AluOp::ADD)),
  };
  // In 6-stage pipeline, insert dedicated MEMCHK hardware verification stage
  if (layout == StageLayout::SIX_STAGE) {
    r.push_back(mmov(IR::MEMCHK_ADDR, IR::ALU_RESULT, a_chk(MemChkOp::CHECK), SC::CHECK));
  }
  r.push_back(mmov(IR::MAR, IR::ALU_RESULT, a_mem(MemOp::READ), SC::MEMORY));
  r.push_back(mmov(IR::REG_DATA, IR::LD_RESULT, 0, SC::MEMORY));
  r.push_back(mmov(IR::REG_SRC, IR::RD, a_reg(RegOp::WRITE), SC::WRITEBACK));
  r.push_back(mb(kPreamble));
  return r;
}

// Word store routine (ST): calculates effective address (RS1 + immx), checks safety, writes RD to Mem[MAR]
Routine st_routine(StageLayout layout) {
  Routine r = {
      mmov(IR::REG_SRC, IR::RS1, a_reg(RegOp::READ)),
      mmov(IR::A, IR::REG_VAL),
      mmov(IR::B, IR::IMMX, a_alu(AluOp::ADD)),
  };
  // In 6-stage pipeline, insert dedicated MEMCHK hardware verification stage
  if (layout == StageLayout::SIX_STAGE) {
    r.push_back(mmov(IR::MEMCHK_ADDR, IR::ALU_RESULT, a_chk(MemChkOp::CHECK), SC::CHECK));
  }
  r.push_back(mmov(IR::MAR, IR::ALU_RESULT, 0, SC::MEMORY));
  r.push_back(mmov(IR::REG_SRC, IR::RD, a_reg(RegOp::READ), SC::MEMORY));
  r.push_back(mmov(IR::MDR, IR::REG_VAL, a_mem(MemOp::WRITE), SC::MEMORY));
  r.push_back(mb(kPreamble));
  return r;
}

// Unconditional branch routine (B): updates PC to precalculated branch target
Routine b_routine() {
  return {
      mmov(IR::PC, IR::BRANCH_TARGET),
      mb(kPreamble),
  };
}

// Conditional branch routine template: tests flag condition; if matched, updates PC to target
Routine branch_if(InternalReg flag, std::int32_t value) {
  return {
      mbeq(flag, value, L_BRANCH),
      mb(kPreamble),
      mlabel(L_BRANCH, mmov(IR::PC, IR::BRANCH_TARGET)),
      mb(kPreamble),
  };
}

// Subroutine call routine (CALL): pushes return PC onto full-descending stack at [SP-4], updates SP, and jumps
// full-descending stack, sp is r14
Routine call_routine(StageLayout layout) {
  Routine r = {
      mmovi(IR::REG_SRC, REG_SP, a_reg(RegOp::READ)),       // 1. Read stack pointer (r14)
      madd(IR::REG_VAL, -4),                                // 2. Decrement stack pointer (SP - 4)
      mmov(IR::MAR, IR::REG_VAL),                           // 3. Set memory address to decremented SP
  };
  // In 6-stage pipeline, insert dedicated MEMCHK hardware verification stage for stack bounds
  if (layout == StageLayout::SIX_STAGE) {
    r.push_back(mmov(IR::MEMCHK_ADDR, IR::MAR, a_chk(MemChkOp::CHECK), SC::CHECK));
  }
  r.push_back(mmov(IR::REG_DATA, IR::REG_VAL, a_reg(RegOp::WRITE), SC::WRITEBACK)); // 4. Commit updated SP to r14
  r.push_back(mmov(IR::MDR, IR::PC, a_mem(MemOp::WRITE), SC::MEMORY));               // 5. Store return PC at Mem[SP]
  r.push_back(mmov(IR::PC, IR::BRANCH_TARGET));                                       // 6. Redirect PC to target
  r.push_back(mb(kPreamble));
  return r;
}

// Subroutine return routine (RET): pops return address from [SP] into PC, increments SP by 4
Routine ret_routine(StageLayout layout) {
  Routine r = {
      mmovi(IR::REG_SRC, REG_SP, a_reg(RegOp::READ)),       // 1. Read stack pointer (r14)
      mmov(IR::MAR, IR::REG_VAL),                           // 2. Set memory address to current SP
  };
  // In 6-stage pipeline, insert dedicated MEMCHK hardware verification stage
  if (layout == StageLayout::SIX_STAGE) {
    r.push_back(mmov(IR::MEMCHK_ADDR, IR::MAR, a_chk(MemChkOp::CHECK), SC::CHECK));
  }
  r.push_back(mmov(IR::MAR, IR::REG_VAL, a_mem(MemOp::READ), SC::MEMORY));           // 3. Read return PC from Mem[SP]
  r.push_back(mmov(IR::PC, IR::LD_RESULT, 0, SC::MEMORY));                           // 4. Load return address into PC
  r.push_back(madd(IR::REG_VAL, 4));                                                  // 5. Increment stack pointer (SP + 4)
  r.push_back(mmov(IR::REG_DATA, IR::REG_VAL, a_reg(RegOp::WRITE), SC::WRITEBACK)); // 6. Commit updated SP to r14
  r.push_back(mb(kPreamble));
  return r;
}

// Processor halt routine (HALT): triggers CPU halt control line
Routine halt_routine() { return {mhalt()}; }

// Memory bounds check routine (BNDCHK): checks base + offset against bounds checker without memory access
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

// 3-operand floating-point operation (FADD, FSUB, FMUL, FDIV): executes FP operation and writes float reg RD
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

// Floating-point comparison (FCMP): compares two float registers and sets floating-point condition code (FCC)
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

// Floating-point move (FMOV): transfers float value from RS2 into RD
Routine fmov_routine() {
  return {
      mmov(IR::FP_SRC, IR::RS2, a_fpu(FpuOp::READ)),
      mmov(IR::FP_B, IR::FP_VAL, a_fpu(FpuOp::FMOV)),
      mmov(IR::FP_DATA, IR::FPU_RESULT),
      mmov(IR::FP_SRC, IR::RD, a_fpu(FpuOp::WRITE), SC::WRITEBACK),
      mb(kPreamble),
  };
}

// Floating-point load (FLD): reads word from memory (RS1 + immx) into float register RD
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

// Floating-point store (FST): writes float register RD to memory location (RS1 + immx)
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

}  // namespace

// Builds the full microcode image table mapping all 34 RISC201 opcodes to their microcode routines
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
