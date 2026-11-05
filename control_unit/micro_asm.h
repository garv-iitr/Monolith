// Major use: Domain-specific micro-assembly language (DSL) for authoring micro-operations and compiling microcode routines.
#ifndef RISC201_CU_MICRO_ASM_H
#define RISC201_CU_MICRO_ASM_H

#include <cstdint>
#include <vector>

#include "control_unit/cu_types.h"
#include "isa_spec/isa.h"

namespace risc201::cu::micro {

// Sentinel constants for microcode jump targets
constexpr int kNoLabel = -1;  // Unlabeled microinstruction
constexpr int kPreamble = -2; // Special jump target designating the instruction fetch preamble (address 0)

// Intermediate representation of a micro-statement before binary layout and address resolution
struct MicroStmt {
  MicroOp     op = MicroOp::MB;              // High-level micro-op code
  InternalReg dest = InternalReg::NONE;      // Destination internal register
  InternalReg src = InternalReg::NONE;       // Source internal register
  std::int32_t imm = 0;                      // Immediate operand
  int  label = kNoLabel;                     // Local symbolic label index [0..63]
  int  target = kNoLabel;                    // Local branch target label or kPreamble
  std::uint16_t args = 0;                    // Packed subunit side-effect arguments
  StageClass stage = StageClass::EXECUTE;    // Pipeline stage attribution metadata
};

// A microcode routine is an ordered sequence of micro-statements implementing an instruction or phase
using Routine = std::vector<MicroStmt>;

// Helper functions packing subunit operation codes into the 10-bit args field:
inline std::uint16_t a_reg(RegOp op)     { return encode_args(UnitId::REGFILE, static_cast<std::uint8_t>(op)); }
inline std::uint16_t a_alu(AluOp op)     { return encode_args(UnitId::ALU, static_cast<std::uint8_t>(op)); }
inline std::uint16_t a_mem(MemOp op)     { return encode_args(UnitId::MEMORY, static_cast<std::uint8_t>(op)); }
inline std::uint16_t a_fpu(FpuOp op)     { return encode_args(UnitId::FPU, static_cast<std::uint8_t>(op)); }
inline std::uint16_t a_chk(MemChkOp op)  { return encode_args(UnitId::MEMCHK, static_cast<std::uint8_t>(op)); }
inline std::uint16_t a_ctrl(ControlOp op) { return encode_args(UnitId::CONTROL, static_cast<std::uint8_t>(op)); }

// --- Micro-Statement Builder Factories -------------------------------------

// Fetches instruction word from Memory[PC] directly into IR latch
inline MicroStmt mloadIR(StageClass stage = StageClass::FETCH) {
  MicroStmt s; s.op = MicroOp::MLOADIR; s.stage = stage; return s;
}
// Triggers combinational decoding of IR fields into internal staging registers
inline MicroStmt mdecode(StageClass stage = StageClass::DECODE) {
  MicroStmt s; s.op = MicroOp::MDECODE; s.stage = stage; return s;
}
// Dispatches micro-PC to the starting address of the routine for the opcode in IR
inline MicroStmt mswitch(StageClass stage = StageClass::SEQUENCE) {
  MicroStmt s; s.op = MicroOp::MSWITCH; s.stage = stage; return s;
}
// Moves data from source internal register to destination internal register over the shared bus
inline MicroStmt mmov(InternalReg dest, InternalReg src, std::uint16_t args = 0,
                      StageClass stage = StageClass::EXECUTE) {
  MicroStmt s; s.op = MicroOp::MMOV; s.dest = dest; s.src = src; s.args = args; s.stage = stage; return s;
}
// Drives a micro-immediate onto the shared bus and latches it into destination register
inline MicroStmt mmovi(InternalReg dest, std::int32_t imm, std::uint16_t args = 0,
                       StageClass stage = StageClass::EXECUTE) {
  MicroStmt s; s.op = MicroOp::MMOVI; s.dest = dest; s.imm = imm; s.args = args; s.stage = stage; return s;
}
// Adds an immediate value to destination register using the bus adder: dest = dest + imm
inline MicroStmt madd(InternalReg dest, std::int32_t imm, std::uint16_t args = 0,
                      StageClass stage = StageClass::EXECUTE) {
  MicroStmt s; s.op = MicroOp::MADD; s.dest = dest; s.imm = imm; s.args = args; s.stage = stage; return s;
}
// Conditional branch: jumps to target label if reg == imm (tested by bus comparator)
inline MicroStmt mbeq(InternalReg reg, std::int32_t imm, int target,
                      StageClass stage = StageClass::SEQUENCE) {
  MicroStmt s; s.op = MicroOp::MBEQ; s.src = reg; s.imm = imm; s.target = target; s.stage = stage; return s;
}
// Unconditional branch: jumps directly to target label or kPreamble
inline MicroStmt mb(int target, StageClass stage = StageClass::SEQUENCE) {
  MicroStmt s; s.op = MicroOp::MB; s.target = target; s.stage = stage; return s;
}
// Halts processor execution and branches back to preamble
inline MicroStmt mhalt(StageClass stage = StageClass::WRITEBACK) {
  MicroStmt s; s.op = MicroOp::MB; s.args = a_ctrl(ControlOp::HALT); s.target = kPreamble; s.stage = stage;
  return s;
}
// Binds a symbolic branch label to a micro-statement
inline MicroStmt mlabel(int label, MicroStmt s) { s.label = label; return s; }

// Universal instruction fetch-decode preamble:
// Fetches Memory[PC]->IR, unpacks instruction, increments PC by 4, and dispatches to opcode routine
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

// Synthesizes low-level hardware control signals from high-level micro-op definitions
inline void micro_semantics(MicroOp op, InternalReg src, InternalReg dest, std::int32_t imm,
                            std::uint16_t target, std::uint16_t args, StageClass stage,
                            ControlSignals& sig, Word& muimm) {
  sig = ControlSignals{};
  sig.stage = stage;
  sig.args = args;
  muimm = static_cast<Word>(static_cast<Sword>(imm));

  switch (op) {
    case MicroOp::MLOADIR:
      // Direct instruction memory read into IR latch without using internal bus
      // fetch unit drives ir straight in, no bus involved
      sig.fetch_ir = true;
      break;

    case MicroOp::MDECODE:
      // Combinational decode occurs passively on IR output lines
      break;

    case MicroOp::MSWITCH:
      // Trigger opcode dispatch table lookup in micro-sequencer
      sig.next_addr = NextAddr::MSWITCH;
      break;

    case MicroOp::MMOV:
      // Route src to write_bus and enable dest in read_mask
      sig.write_src = src;
      sig.read(dest);
      break;

    case MicroOp::MMOVI:
      // Drive micro-immediate onto read_bus via TransferSel::MU_IMM and latch into dest
      sig.write_src = InternalReg::NONE;
      sig.transfer_sel = TransferSel::MU_IMM;
      sig.read(dest);
      break;

    case MicroOp::MADD:
      // Route dest to bus adder input A and immediate to input B; latch sum back to dest
      sig.write_src = dest;
      sig.read(dest);
      sig.transfer_sel = TransferSel::MU_ADDER;
      break;

    case MicroOp::MBEQ:
      // Drive src to write_bus for equality comparison against muimm; select conditional branch in sequencer
      sig.write_src = src;
      sig.next_addr = NextAddr::BRANCH_COND;
      sig.jump_target = target;
      break;

    case MicroOp::MB:
      // Select unconditional jump in micro-sequencer
      sig.next_addr = NextAddr::JUMP;
      sig.jump_target = target;
      break;
  }

  // Configure functional unit control lines from packed arguments
  apply_unit_args(sig, args);
}

// Unpacks subunit arguments into discrete unit control fields (ALU, REGFILE, MEMORY, FPU, MEMCHK, CONTROL)
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
