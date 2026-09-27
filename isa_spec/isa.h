#ifndef ISA_H
#define ISA_H

#include <cstdint>

/*
 * RISC201 ISA v2 specifications:
 * 32-bit fixed-width instructions; 6-bit opcodes (0x00-0x3F).
 * Register files: 16 32-bit integer registers (R0-R15) and 16 32-bit
 * floating-point registers (F0-F15). Instruction formats: R, I, SH,
 * BR, and FP. Shared opcodes use mode 00 for R-type and modes 01-11
 * for immediate forms. BR offsets are signed and PC-relative.
 */

using Word = uint32_t;

enum Opcode : uint8_t {
    OP_NOP  = 0x00,
    OP_ADD  = 0x01,
    OP_SUB  = 0x02,
    OP_MUL  = 0x03,
    OP_DIV  = 0x04,
    OP_MOD  = 0x05,
    OP_AND  = 0x06,
    OP_OR   = 0x07,
    OP_NOT  = 0x08,
    OP_XOR  = 0x09,
    OP_LSL  = 0x0A,
    OP_LSR  = 0x0B,
    OP_ASR  = 0x0C,
    OP_CMP  = 0x0D,

    OP_MOV  = 0x10,
    OP_LD   = 0x11,
    OP_ST   = 0x12,
    OP_PUSH = 0x13,
    OP_POP  = 0x14,
    OP_SWAP = 0x15,
    OP_CSEL = 0x16,

    OP_EXIT    = 0x17,
    OP_SPUSH   = 0x18,
    OP_SPOP    = 0x19,
    OP_ENTER   = 0x1A,
    OP_LEAVE   = 0x1B,
    OP_CALLCHK = 0x27,
    OP_BNDCHK  = 0x3E,

    OP_B    = 0x20,
    OP_BEQ  = 0x21,
    OP_BNE  = 0x22,
    OP_BGT  = 0x23,
    OP_BLT  = 0x24,
    OP_CALL = 0x25,
    OP_RET  = 0x26,

    OP_BIF  = 0x28,
    OP_SKIP = 0x29,
    OP_BC1T = 0x2A,
    OP_BC1F = 0x2B,

    OP_C_EQ_S   = 0x2C,
    OP_C_LE_S   = 0x2D,
    OP_C_LT_S   = 0x2E,
    OP_ABS_S    = 0x35,
    OP_NEG_S    = 0x36,
    OP_LS       = 0x37,
    OP_SS       = 0x38,
    OP_MOV_S    = 0x39,
    OP_MFC1     = 0x3A,
    OP_MTC1     = 0x3B,
    OP_CVT_W_S  = 0x3C,
    OP_CVT_S_W  = 0x3D,

    OP_FADD = 0x30,
    OP_FSUB = 0x31,
    OP_FMUL = 0x32,
    OP_FDIV = 0x33,
    OP_FCMP = 0x34,

    OP_HALT = 0x3F
};

enum class Format {
    R,
    I,
    SH,
    BR,
    FP
};

inline uint8_t get_opcode(Word w) { return (w >> 26) & 0x3F; }
inline uint8_t get_mode(Word w)   { return (w >> 24) & 0x3;  }

inline uint8_t get_r_rd(Word w)   { return (w >> 20) & 0xF; }
inline uint8_t get_r_rs(Word w)   { return (w >> 16) & 0xF; }
inline uint8_t get_r_rt(Word w)   { return (w >> 12) & 0xF; }
inline uint8_t get_r_cond(Word w) { return (w >> 9)  & 0x7; }

inline uint8_t get_i_rd(Word w)    { return (w >> 20) & 0xF; }
inline uint8_t get_i_rs(Word w)    { return (w >> 16) & 0xF; }
inline int32_t get_i_imm(Word w) {
    int32_t v = w & 0xFFFF;
    if (v & 0x8000) v |= ~0xFFFF;
    return v;
}
inline uint32_t get_i_imm_unsigned(Word w) { return w & 0xFFFF; }

inline uint8_t get_sh_rd(Word w)     { return (w >> 20) & 0xF; }
inline uint8_t get_sh_rs(Word w)     { return (w >> 16) & 0xF; }
inline uint8_t get_sh_shamt(Word w)  { return (w >> 11) & 0x1F;}

inline int32_t get_br_offset(Word w) {
    int32_t v = w & 0x3FFFFFF;
    if (v & 0x2000000) v |= ~0x3FFFFFF;
    return v;
}

inline uint8_t get_fp_fd(Word w) { return (w >> 22) & 0xF; }
inline uint8_t get_fp_fs(Word w) { return (w >> 18) & 0xF; }
inline uint8_t get_fp_ft(Word w) { return (w >> 14) & 0xF; }

inline uint8_t get_fpls_freg(Word w) { return (w >> 20) & 0xF; }
inline uint8_t get_fpls_rs(Word w)   { return (w >> 16) & 0xF; }
inline int32_t get_fpls_imm(Word w)  { return get_i_imm(w); }

inline uint8_t get_fpxfer_gpr(Word w) { return (w >> 22) & 0xF; }
inline uint8_t get_fpxfer_fpr(Word w) { return (w >> 18) & 0xF; }

inline Format format_of(uint8_t opcode) {
    switch (opcode) {
        case OP_LSL: case OP_LSR: case OP_ASR:
            return Format::SH;

        case OP_B: case OP_BEQ: case OP_BNE: case OP_BGT: case OP_BLT:
        case OP_CALL: case OP_CALLCHK: case OP_BIF: case OP_SKIP:
        case OP_BC1T: case OP_BC1F:
        case OP_NOP: case OP_RET: case OP_HALT: case OP_EXIT: case OP_LEAVE:
            return Format::BR;

        case OP_FADD: case OP_FSUB: case OP_FMUL: case OP_FDIV: case OP_FCMP:
        case OP_C_EQ_S: case OP_C_LE_S: case OP_C_LT_S:
        case OP_ABS_S: case OP_NEG_S: case OP_MOV_S:
        case OP_CVT_W_S: case OP_CVT_S_W:
        case OP_LS: case OP_SS:
        case OP_MFC1: case OP_MTC1:
            return Format::FP;

        case OP_ADD: case OP_SUB: case OP_MUL: case OP_XOR:
        case OP_CMP: case OP_MOV:
            return Format::R;

        default:
            return Format::R;
    }
}

constexpr int NUM_INT_REGISTERS   = 16;
constexpr int NUM_FLOAT_REGISTERS = 16;
constexpr int REG_WIDTH_BITS      = 32;

constexpr int REG_SWAP = 13;
constexpr int REG_SP   = 14;
constexpr int REG_RA   = 15;

struct Flags {
    bool Z = false;
    bool N = false;
    bool C = false;
    bool V = false;
};

enum class Cond : uint8_t {
    EQ = 0b000,
    NE = 0b001,
    GT = 0b010,
    LT = 0b011
};

inline bool eval_cond(Cond c, const Flags& f) {
    switch (c) {
        case Cond::EQ: return f.Z;
        case Cond::NE: return !f.Z;
        case Cond::GT: return !f.N && !f.Z;
        case Cond::LT: return f.N;
        default:       return false;
    }
}

using FCC = bool;

    #endif