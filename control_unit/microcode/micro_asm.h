#ifndef RISC201_CU_MICRO_ASM_H
#define RISC201_CU_MICRO_ASM_H

#include <cstdint>
#include <vector>

#include "common/control_signals.h"
#include "isa_spec/isa.h"

namespace risc201::cu::micro {

constexpr int kNoLabel = -1;
constexpr int kPreamble = -2;

struct MicroStmt {
  MicroOp     op = MicroOp::MB;
  InternalReg dest = InternalReg::NONE;
  InternalReg src = InternalReg::NONE;
  std::int32_t imm = 0;
  int  label = kNoLabel;
  int  target = kNoLabel;
  std::uint16_t args = 0;
  StageClass stage = StageClass::EXECUTE;
};

using Routine = std::vector<MicroStmt>;

inline std::uint16_t a_reg(RegOp op)     { return encode_args(UnitId::REGFILE, static_cast<std::uint8_t>(op)); }
inline std::uint16_t a_alu(AluOp op)     { return encode_args(UnitId::ALU, static_cast<std::uint8_t>(op)); }
inline std::uint16_t a_mem(MemOp op)     { return encode_args(UnitId::MEMORY, static_cast<std::uint8_t>(op)); }
inline std::uint16_t a_fpu(FpuOp op)     { return encode_args(UnitId::FPU, static_cast<std::uint8_t>(op)); }
inline std::uint16_t a_chk(MemChkOp op)  { return encode_args(UnitId::MEMCHK, static_cast<std::uint8_t>(op)); }
inline std::uint16_t a_ctrl(ControlOp op) { return encode_args(UnitId::CONTROL, static_cast<std::uint8_t>(op)); }

inline MicroStmt mloadIR(StageClass stage = StageClass::FETCH) {
  MicroStmt s; s.op = MicroOp::MLOADIR; s.stage = stage; return s;
}
inline MicroStmt mdecode(StageClass stage = StageClass::DECODE) {
  MicroStmt s; s.op = MicroOp::MDECODE; s.stage = stage; return s;
}
inline MicroStmt mswitch(StageClass stage = StageClass::SEQUENCE) {
  MicroStmt s; s.op = MicroOp::MSWITCH; s.stage = stage; return s;
}
inline MicroStmt mmov(InternalReg dest, InternalReg src, std::uint16_t args = 0,
                      StageClass stage = StageClass::EXECUTE) {
  MicroStmt s; s.op = MicroOp::MMOV; s.dest = dest; s.src = src; s.args = args; s.stage = stage; return s;
}
inline MicroStmt mmovi(InternalReg dest, std::int32_t imm, std::uint16_t args = 0,
                       StageClass stage = StageClass::EXECUTE) {
  MicroStmt s; s.op = MicroOp::MMOVI; s.dest = dest; s.imm = imm; s.args = args; s.stage = stage; return s;
}
inline MicroStmt madd(InternalReg dest, std::int32_t imm, std::uint16_t args = 0,
                      StageClass stage = StageClass::EXECUTE) {
  MicroStmt s; s.op = MicroOp::MADD; s.dest = dest; s.imm = imm; s.args = args; s.stage = stage; return s;
}
inline MicroStmt mbeq(InternalReg reg, std::int32_t imm, int target,
                      StageClass stage = StageClass::SEQUENCE) {
  MicroStmt s; s.op = MicroOp::MBEQ; s.src = reg; s.imm = imm; s.target = target; s.stage = stage; return s;
}
inline MicroStmt mb(int target, StageClass stage = StageClass::SEQUENCE) {
  MicroStmt s; s.op = MicroOp::MB; s.target = target; s.stage = stage; return s;
}
inline MicroStmt mhalt(StageClass stage = StageClass::WRITEBACK) {
  MicroStmt s; s.op = MicroOp::MB; s.args = a_ctrl(ControlOp::HALT); s.target = kPreamble; s.stage = stage;
  return s;
}
inline MicroStmt mlabel(int label, MicroStmt s) { s.label = label; return s; }

// every instruction starts with this preamble
inline Routine preamble() {
  return {
      mloadIR(StageClass::FETCH),
      mdecode(StageClass::DECODE),
      madd(InternalReg::PC, 4, 0, StageClass::DECODE),
      mswitch(StageClass::SEQUENCE),
  };
}

inline void apply_unit_args(ControlSignals& sig, std::uint16_t args);

inline void micro_semantics(MicroOp op, InternalReg src, InternalReg dest, std::int32_t imm,
                            std::uint16_t target, std::uint16_t args, StageClass stage,
                            ControlSignals& sig, Word& muimm) {
  sig = ControlSignals{};
  sig.stage = stage;
  sig.args = args;
  muimm = static_cast<Word>(static_cast<Sword>(imm));

  switch (op) {
    case MicroOp::MLOADIR:
      // fetch unit drives ir straight in, no bus involved
      sig.fetch_ir = true;
      break;

    case MicroOp::MDECODE:
      break;

    case MicroOp::MSWITCH:
      sig.next_addr = NextAddr::MSWITCH;
      break;

    case MicroOp::MMOV:
      sig.write_src = src;
      sig.read(dest);
      break;

    case MicroOp::MMOVI:
      sig.write_src = InternalReg::NONE;
      sig.transfer_sel = TransferSel::MU_IMM;
      sig.read(dest);
      break;

    case MicroOp::MADD:
      sig.write_src = dest;
      sig.read(dest);
      sig.transfer_sel = TransferSel::MU_ADDER;
      break;

    case MicroOp::MBEQ:
      sig.write_src = src;
      sig.next_addr = NextAddr::BRANCH_COND;
      sig.jump_target = target;
      break;

    case MicroOp::MB:
      sig.next_addr = NextAddr::JUMP;
      sig.jump_target = target;
      break;
  }

  apply_unit_args(sig, args);
}

inline void apply_unit_args(ControlSignals& sig, std::uint16_t args) {
  const UnitId unit = args_unit(args);
  const std::uint8_t opcode = args_op(args);

  switch (unit) {
    case UnitId::ALU:
      sig.alu_op = static_cast<AluOp>(opcode);
      if (sig.alu_op == AluOp::CMP) sig.set_flags = true;
      break;
    case UnitId::REGFILE:
      sig.reg_op = static_cast<RegOp>(opcode);
      break;
    case UnitId::MEMORY:
      sig.mem_op = static_cast<MemOp>(opcode);
      break;
    case UnitId::FPU:
      sig.fpu_op = static_cast<FpuOp>(opcode);
      if (sig.fpu_op == FpuOp::FCMP) sig.fpu_set_fcc = true;
      break;
    case UnitId::MEMCHK:
      sig.memchk_op = static_cast<MemChkOp>(opcode);
      break;
    case UnitId::CONTROL:
      if (opcode == static_cast<std::uint8_t>(ControlOp::HALT)) sig.halt = true;
      break;
    case UnitId::NONE:
    default:
      break;
  }
}

}

#endif
