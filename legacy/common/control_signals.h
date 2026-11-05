// Defines architectural control signal structures, subunit opcodes, bus routing selectors, and sequencer modes.
#ifndef RISC201_CU_CONTROL_SIGNALS_H
#define RISC201_CU_CONTROL_SIGNALS_H

#include <cstdint>
#include <initializer_list>
#include <string>

#include "isa_spec/isa.h"

namespace risc201::cu {

// Hardware subunit identifiers used to pack side-effect arguments into microinstructions
enum class UnitId : std::uint8_t {
  NONE     = 0,
  ALU      = 1,
  REGFILE  = 2,
  MEMORY   = 3,
  FPU      = 4,
  MEMCHK   = 5,
  CONTROL  = 6
};

constexpr std::uint8_t kArgsUnitBits = 3;
constexpr std::uint8_t kArgsOpBits   = 7;

// Packs a target subunit ID (3 bits) and its sub-operation code (7 bits) into a 10-bit argument field
inline std::uint16_t encode_args(UnitId unit, std::uint8_t opcode) {
  return static_cast<std::uint16_t>((static_cast<std::uint16_t>(unit) << kArgsOpBits) |
                                    (opcode & 0x7F));
}
// Extracts the target subunit from packed argument bits
inline UnitId       args_unit(std::uint16_t args) { return static_cast<UnitId>(args >> kArgsOpBits); }
// Extracts the subunit operation code from packed argument bits
inline std::uint8_t args_op(std::uint16_t args)   { return static_cast<std::uint8_t>(args & 0x7F); }

// Stringifier for subunit identifiers
inline const char* unit_name(UnitId u) {
  switch (u) {
    case UnitId::NONE:    return "-";
    case UnitId::ALU:     return "alu";
    case UnitId::REGFILE: return "regfile";
    case UnitId::MEMORY:  return "mem";
    case UnitId::FPU:     return "fpu";
    case UnitId::MEMCHK:  return "memchk";
    case UnitId::CONTROL: return "ctrl";
  }
  return "??";
}

// ALU operation codes driven into the integer arithmetic and logic unit
enum class AluOp : std::uint8_t {
  NOP = 0, ADD, SUB, MUL, DIV, MOD, AND, OR, NOT, XOR,
  LSL, LSR, ASR, CMP, PASS_A, PASS_B
};

// Register file access strobes (read/write ports)
enum class RegOp : std::uint8_t { NOP = 0, READ, WRITE };

// Data memory bus transaction commands
enum class MemOp : std::uint8_t { NOP = 0, READ, WRITE };

// Floating-point execution and register transfer commands
enum class FpuOp : std::uint8_t {
  NOP = 0, FADD, FSUB, FMUL, FDIV, FCMP, FMOV, READ, WRITE
};

// Memory safety firewall verification commands
enum class MemChkOp : std::uint8_t { NOP = 0, CHECK };

// Top-level processor control operations (e.g. processor halt)
enum class ControlOp : std::uint8_t { NOP = 0, HALT };

inline const char* alu_op_name(AluOp op) {
  switch (op) {
    case AluOp::NOP:    return "nop";
    case AluOp::ADD:    return "add";
    case AluOp::SUB:    return "sub";
    case AluOp::MUL:    return "mul";
    case AluOp::DIV:    return "div";
    case AluOp::MOD:    return "mod";
    case AluOp::AND:    return "and";
    case AluOp::OR:     return "or";
    case AluOp::NOT:    return "not";
    case AluOp::XOR:    return "xor";
    case AluOp::LSL:    return "lsl";
    case AluOp::LSR:    return "lsr";
    case AluOp::ASR:    return "asr";
    case AluOp::CMP:    return "cmp";
    case AluOp::PASS_A: return "passA";
    case AluOp::PASS_B: return "passB";
  }
  return "??";
}

inline const char* reg_op_name(RegOp op) {
  switch (op) {
    case RegOp::NOP:   return "-";
    case RegOp::READ:  return "read";
    case RegOp::WRITE: return "write";
  }
  return "??";
}

inline const char* mem_op_name(MemOp op) {
  switch (op) {
    case MemOp::NOP:   return "-";
    case MemOp::READ:  return "load";
    case MemOp::WRITE: return "store";
  }
  return "??";
}

inline const char* fpu_op_name(FpuOp op) {
  switch (op) {
    case FpuOp::NOP:   return "nop";
    case FpuOp::FADD:  return "fadd";
    case FpuOp::FSUB:  return "fsub";
    case FpuOp::FMUL:  return "fmul";
    case FpuOp::FDIV:  return "fdiv";
    case FpuOp::FCMP:  return "fcmp";
    case FpuOp::FMOV:  return "fmov";
    case FpuOp::READ:  return "bread";
    case FpuOp::WRITE: return "bwrite";
  }
  return "??";
}

// Transfer Mux select line determining what source drives the internal ReadBus:
// - WRITE_BUS: Unmodified output of the register selected by write_src
// - MU_IMM: Direct 32-bit sign-extended immediate from the microinstruction
// - MU_ADDER: Output of the internal bus adder (write_bus + mu_imm)
enum class TransferSel : std::uint8_t {
  WRITE_BUS = 0,
  MU_IMM    = 1,
  MU_ADDER  = 2
};

// Micro-sequencer next address selection modes:
// - INCREMENT: Step sequentially to micro_pc + 1
// - MSWITCH: Dispatch dynamically based on IR opcode table lookup
// - JUMP: Unconditional branch to jump_target
// - BRANCH_COND: Conditional micro-branch (takes branch if write_bus == mu_imm)
// - FETCH: Reset micro_pc back to the preamble entry point (address 0)
enum class NextAddr : std::uint8_t {
  INCREMENT   = 0,
  MSWITCH     = 1,
  JUMP        = 2,
  BRANCH_COND = 3,
  FETCH       = 4
};

// Micro-operation pipeline stage classification (used for trace logging and multi-stage mapping)
enum class StageClass : std::uint8_t {
  FETCH = 0, DECODE, EXECUTE, CHECK, MEMORY, WRITEBACK, SEQUENCE
};

// Hardware pipeline configuration modes: 4-stage (classic) vs 6-stage (with dedicated MEMCHK stage)
enum class StageLayout : std::uint8_t { FOUR_STAGE = 4, SIX_STAGE = 6 };

// Maps stage classification to a human-readable stage name based on the active pipeline layout
inline const char* stage_name(StageLayout layout, StageClass sc) {
  if (layout == StageLayout::FOUR_STAGE) {
    switch (sc) {
      case StageClass::FETCH:     return "IF/OF";
      case StageClass::DECODE:    return "IF/OF";
      case StageClass::EXECUTE:   return "EX";
      case StageClass::CHECK:     return "MA";
      case StageClass::MEMORY:    return "MA";
      case StageClass::WRITEBACK: return "RW";
      case StageClass::SEQUENCE:  return "IF/OF";
    }
  }
  switch (sc) {
    case StageClass::FETCH:     return "IF";
    case StageClass::DECODE:    return "OF";
    case StageClass::EXECUTE:   return "EX";
    case StageClass::CHECK:     return "MEMCHK";
    case StageClass::MEMORY:    return "MA";
    case StageClass::WRITEBACK: return "RW";
    case StageClass::SEQUENCE:  return "OF";
  }
  return "??";
}

// Generates a 1-hot bitmask for testing or enabling an internal register in read_mask
inline std::uint32_t reg_bit(InternalReg r) {
  return std::uint32_t{1} << static_cast<std::uint8_t>(r);
}

// Master control signal bundle emitted per micro-cycle to drive all internal buses and functional units
struct ControlSignals {
  // Source register currently driving the shared write_bus
  InternalReg write_src = InternalReg::NONE;

  // Bitmask of destination registers latching data from the shared read_bus
  // one bit per internal reg, bits line up with the InternalReg values
  std::uint32_t read_mask = 0;

  // Multiplexer selector determining the source for read_bus
  TransferSel transfer_sel = TransferSel::WRITE_BUS;

  // Subunit operation commands
  AluOp      alu_op    = AluOp::NOP;
  RegOp      reg_op    = RegOp::NOP;
  MemOp      mem_op    = MemOp::NOP;
  FpuOp      fpu_op    = FpuOp::NOP;
  MemChkOp   memchk_op = MemChkOp::NOP;
  bool       set_flags = false;
  bool       fpu_set_fcc = false;
  bool       fetch_ir = false;

  // Micro-sequencer control lines
  NextAddr next_addr   = NextAddr::INCREMENT;
  std::uint16_t jump_target = 0;
  bool     halt        = false;

  // Pipeline tracking metadata and packed subunit arguments
  StageClass stage = StageClass::EXECUTE;
  std::uint16_t args = 0;

  // Query whether an internal register is configured to latch from the bus this cycle
  bool reads(InternalReg r) const { return (read_mask & reg_bit(r)) != 0; }
  // Fluent builder method to add a destination register to read_mask
  ControlSignals& read(InternalReg r) { read_mask |= reg_bit(r); return *this; }
  // Fluent builder method to set the source register driving write_bus
  ControlSignals& write(InternalReg r) { write_src = r; return *this; }
  // Fluent builder method to encode a subunit argument into args
  ControlSignals& arg(UnitId u, std::uint8_t op) { args = encode_args(u, op); return *this; }
};

// Convenience helper to set multiple destination registers in read_mask simultaneously
inline void read_all(ControlSignals& s, std::initializer_list<InternalReg> regs) {
  for (InternalReg r : regs) s.read(r);
}

}

#endif
