#ifndef RISC201_ISA_H
#define RISC201_ISA_H

#include <cstddef>
#include <cstdint>

#ifndef RISC201_WORD_BITS
#define RISC201_WORD_BITS 32
#endif

constexpr int kWordBits = RISC201_WORD_BITS;
static_assert(kWordBits == 32 || kWordBits == 64, "RISC201_WORD_BITS must be 32 or 64");

#if kWordBits == 64
using Word  = std::uint64_t;
using Sword = std::int64_t;
#else
using Word  = std::uint32_t;
using Sword = std::int32_t;
#endif

constexpr int NUM_REGISTERS        = 16;
constexpr int NUM_FLOAT_REGISTERS  = 16;
constexpr int NUM_INT_REGISTERS    = NUM_REGISTERS;
constexpr int REG_WIDTH_BITS       = kWordBits;

constexpr std::uint8_t REG_SWAP = 13;
constexpr std::uint8_t REG_SP   = 14;
constexpr std::uint8_t REG_RA   = 15;

enum class Opcode : std::uint8_t {
  NOP = 0x00, ADD = 0x01, SUB = 0x02, MUL = 0x03, DIV = 0x04, MOD = 0x05,
  AND = 0x06, OR  = 0x07, NOT = 0x08, XOR = 0x09, LSL = 0x0A, LSR = 0x0B,
  ASR = 0x0C, CMP = 0x0D,

  MOV = 0x10, LD = 0x11, ST = 0x12,

  B = 0x20, BEQ = 0x21, BNE = 0x22, BGT = 0x23, BLT = 0x24,
  CALL = 0x25, RET = 0x26, BNDCHK = 0x27, HALT = 0x28,

  FADD = 0x30, FSUB = 0x31, FMUL = 0x32, FDIV = 0x33, FCMP = 0x34,
  FLD  = 0x35, FST  = 0x36, FMOV = 0x37
};

enum class Format : std::uint8_t {
  ALU3,
  ALU2,
  COMPARE,
  MEMORY,
  BRANCH,
  NO_OPERAND,
  FPU3,
  FPU2
};

inline const char* opcode_name(Opcode op) {
  switch (op) {
    case Opcode::NOP:    return "nop";
    case Opcode::ADD:    return "add";
    case Opcode::SUB:    return "sub";
    case Opcode::MUL:    return "mul";
    case Opcode::DIV:    return "div";
    case Opcode::MOD:    return "mod";
    case Opcode::AND:    return "and";
    case Opcode::OR:     return "or";
    case Opcode::NOT:    return "not";
    case Opcode::XOR:    return "xor";
    case Opcode::LSL:    return "lsl";
    case Opcode::LSR:    return "lsr";
    case Opcode::ASR:    return "asr";
    case Opcode::CMP:    return "cmp";
    case Opcode::MOV:    return "mov";
    case Opcode::LD:     return "ld";
    case Opcode::ST:     return "st";
    case Opcode::B:      return "b";
    case Opcode::BEQ:    return "beq";
    case Opcode::BNE:    return "bne";
    case Opcode::BGT:    return "bgt";
    case Opcode::BLT:    return "blt";
    case Opcode::CALL:   return "call";
    case Opcode::RET:    return "ret";
    case Opcode::BNDCHK: return "bndchk";
    case Opcode::HALT:   return "halt";
    case Opcode::FADD:   return "fadd";
    case Opcode::FSUB:   return "fsub";
    case Opcode::FMUL:   return "fmul";
    case Opcode::FDIV:   return "fdiv";
    case Opcode::FCMP:   return "fcmp";
    case Opcode::FLD:    return "fld";
    case Opcode::FST:    return "fst";
    case Opcode::FMOV:   return "fmov";
  }
  return "???";
}

inline Format format_of(Opcode op) {
  switch (op) {
    case Opcode::ADD: case Opcode::SUB: case Opcode::MUL: case Opcode::DIV:
    case Opcode::MOD: case Opcode::AND: case Opcode::OR:  case Opcode::XOR:
    case Opcode::LSL: case Opcode::LSR: case Opcode::ASR:
      return Format::ALU3;

    case Opcode::NOT: case Opcode::MOV:
      return Format::ALU2;

    case Opcode::CMP:
      return Format::COMPARE;

    case Opcode::LD: case Opcode::ST: case Opcode::BNDCHK:
    case Opcode::FLD: case Opcode::FST:
      return Format::MEMORY;

    case Opcode::B: case Opcode::BEQ: case Opcode::BNE: case Opcode::BGT:
    case Opcode::BLT: case Opcode::CALL:
      return Format::BRANCH;

    case Opcode::NOP: case Opcode::RET: case Opcode::HALT:
      return Format::NO_OPERAND;

    case Opcode::FADD: case Opcode::FSUB: case Opcode::FMUL: case Opcode::FDIV:
      return Format::FPU3;

    case Opcode::FCMP: case Opcode::FMOV:
      return Format::FPU2;
  }
  return Format::ALU3;
}

// bits: opcode [31:26], mode [25:24], rd [23:20], rs1 [19:16], imm16 [15:0]
inline std::uint8_t  get_opcode(Word w)  { return static_cast<std::uint8_t>((w >> 26) & 0x3F); }
inline Opcode         opcode_of(Word w)  { return static_cast<Opcode>(get_opcode(w)); }
inline std::uint8_t  get_mode(Word w)    { return static_cast<std::uint8_t>((w >> 24) & 0x3); }
inline bool           get_ibit(Word w)   { return get_mode(w) != 0; }

inline std::uint8_t  get_rd(Word w)      { return static_cast<std::uint8_t>((w >> 20) & 0xF); }
inline std::uint8_t  get_rs1(Word w)     { return static_cast<std::uint8_t>((w >> 16) & 0xF); }
inline std::uint8_t  get_rs2(Word w)     { return static_cast<std::uint8_t>((w >> 12) & 0xF); }
inline std::uint8_t  get_fd(Word w)      { return get_rd(w); }
inline std::uint8_t  get_fs1(Word w)     { return get_rs1(w); }
inline std::uint8_t  get_fs2(Word w)     { return get_rs2(w); }

inline std::uint16_t get_imm16(Word w)   { return static_cast<std::uint16_t>(w & 0xFFFF); }
inline Sword         get_imm16_signed(Word w) {
  return static_cast<Sword>(static_cast<std::int16_t>(w & 0xFFFF));
}

inline Sword         get_offset26(Word w) {
  // sign extend by hand, offset is only 26 bits
  Sword v = static_cast<Sword>(w & 0x03FFFFFF);
  if (v & 0x02000000) v |= ~static_cast<Sword>(0x03FFFFFF);
  return v;
}

inline Word expand_immediate(std::uint8_t mode, std::uint16_t raw16) {
  switch (mode) {
    case 0x1: return static_cast<Word>(static_cast<Sword>(static_cast<std::int16_t>(raw16)));
    case 0x2: return static_cast<Word>(raw16);
    case 0x3: return static_cast<Word>(raw16) << 16;
    default:  return Word{0};
  }
}

struct Flags {
  bool Z = false;
  bool N = false;
  bool C = false;
  bool V = false;
};

enum class Cond : std::uint8_t { EQ = 0b00, NE = 0b01, GT = 0b10, LT = 0b11 };

inline bool eval_cond(Cond c, const Flags& f) {
  switch (c) {
    case Cond::EQ: return f.Z;
    case Cond::NE: return !f.Z;
    case Cond::GT: return !f.N && !f.Z;
    case Cond::LT: return f.N;
  }
  return false;
}

inline const char* cond_name(Cond c) {
  switch (c) {
    case Cond::EQ: return "eq";
    case Cond::NE: return "ne";
    case Cond::GT: return "gt";
    case Cond::LT: return "lt";
  }
  return "??";
}

enum class MicroOp : std::uint8_t {
  MLOADIR = 0,
  MDECODE = 1,
  MSWITCH = 2,
  MMOV    = 3,
  MMOVI   = 4,
  MADD    = 5,
  MBEQ    = 6,
  MB      = 7
};

inline const char* micro_op_name(MicroOp m) {
  switch (m) {
    case MicroOp::MLOADIR: return "mloadIR";
    case MicroOp::MDECODE: return "mdecode";
    case MicroOp::MSWITCH: return "mswitch";
    case MicroOp::MMOV:    return "mmov";
    case MicroOp::MMOVI:   return "mmovi";
    case MicroOp::MADD:    return "madd";
    case MicroOp::MBEQ:    return "mbeq";
    case MicroOp::MB:      return "mb";
  }
  return "??";
}

enum class InternalReg : std::uint8_t {
  NONE          = 0,
  PC            = 1,
  IR            = 2,
  I             = 3,
  RD            = 4,
  RS1           = 5,
  RS2           = 6,
  IMMX          = 7,
  BRANCH_TARGET = 8,
  REG_SRC       = 9,
  REG_DATA      = 10,
  REG_VAL       = 11,
  A             = 12,
  B             = 13,
  FLAGS_E       = 14,
  FLAGS_GT      = 15,
  ALU_RESULT    = 16,
  MAR           = 17,
  MDR           = 18,
  LD_RESULT     = 19,
  FP_SRC        = 20,
  FP_DATA       = 21,
  FP_VAL        = 22,
  FP_A          = 23,
  FP_B          = 24,
  FPU_RESULT    = 25,
  FCC           = 26,
  MEMCHK_STATUS = 27,
  MEMCHK_ADDR   = 28,
  FLAGS_N       = 29
};

inline const char* internal_reg_name(InternalReg r) {
  switch (r) {
    case InternalReg::NONE:          return "-";
    case InternalReg::PC:            return "pc";
    case InternalReg::IR:            return "ir";
    case InternalReg::I:             return "I";
    case InternalReg::RD:            return "rd";
    case InternalReg::RS1:           return "rs1";
    case InternalReg::RS2:           return "rs2";
    case InternalReg::IMMX:          return "immx";
    case InternalReg::BRANCH_TARGET: return "branchTarget";
    case InternalReg::REG_SRC:       return "regSrc";
    case InternalReg::REG_DATA:      return "regData";
    case InternalReg::REG_VAL:       return "regVal";
    case InternalReg::A:             return "A";
    case InternalReg::B:             return "B";
    case InternalReg::FLAGS_E:       return "flags.E";
    case InternalReg::FLAGS_GT:      return "flags.GT";
    case InternalReg::ALU_RESULT:    return "aluResult";
    case InternalReg::MAR:           return "mar";
    case InternalReg::MDR:           return "mdr";
    case InternalReg::LD_RESULT:     return "ldResult";
    case InternalReg::FP_SRC:        return "fpSrc";
    case InternalReg::FP_DATA:       return "fpData";
    case InternalReg::FP_VAL:        return "fpVal";
    case InternalReg::FP_A:          return "fA";
    case InternalReg::FP_B:          return "fB";
    case InternalReg::FPU_RESULT:    return "fpuResult";
    case InternalReg::FCC:           return "fcc";
    case InternalReg::MEMCHK_STATUS: return "memChkStatus";
    case InternalReg::MEMCHK_ADDR:   return "memChkAddr";
    case InternalReg::FLAGS_N:       return "flags.N";
  }
  return "??";
}

// keep in sync with teh enum above
constexpr std::size_t kNumInternalRegs = 30;

#endif
