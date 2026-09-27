#ifndef ISA_H
#define ISA_H

#include <cstdint>

// ============================================================
// RISC201 Instruction Set Architecture — v2 (post-pivot)
// 32-bit fixed-width instructions
// Two register files: 16 integer (R0-R15) + 16 float (F0-F15)
// ============================================================

using Word = uint32_t;

// -------------------- Opcodes (6-bit) --------------------
// Values match the team's opcode table exactly (0x00-0x3F).
enum Opcode : uint8_t {
    // ---- ALU / Logic ----
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

    // ---- Data Movement ----
    OP_MOV  = 0x10,
    OP_LD   = 0x11,
    OP_ST   = 0x12,
    OP_PUSH = 0x13,
    OP_POP  = 0x14,
    OP_SWAP = 0x15,
    OP_CSEL = 0x16,

    // ---- Memory Safety & Stack ----
    OP_EXIT    = 0x17,
    OP_SPUSH   = 0x18,
    OP_SPOP    = 0x19,
    OP_ENTER   = 0x1A,
    OP_LEAVE   = 0x1B,
    OP_CALLCHK = 0x27,
    OP_BNDCHK  = 0x3E,

    // ---- Control Flow ----
    OP_B    = 0x20,
    OP_BEQ  = 0x21,
    OP_BNE  = 0x22,
    OP_BGT  = 0x23,
    OP_BLT  = 0x24,
    OP_CALL = 0x25,
    OP_RET  = 0x26,

    // ---- Extended Control Flow ----
    OP_BIF  = 0x28,
    OP_SKIP = 0x29,
    OP_BC1T = 0x2A,
    OP_BC1F = 0x2B,

    // ---- FPU Conversions & Movement (comparisons live here per team's table) ----
    OP_C_EQ_S   = 0x2C,
    OP_C_LE_S   = 0x2D,
    OP_C_LT_S   = 0x2E,
    OP_ABS_S    = 0x35,
    OP_NEG_S    = 0x36,
    OP_LS       = 0x37,   // L.S — load word into FPU register
    OP_SS       = 0x38,   // S.S — store word from FPU register
    OP_MOV_S    = 0x39,
    OP_MFC1     = 0x3A,
    OP_MTC1     = 0x3B,
    OP_CVT_W_S  = 0x3C,
    OP_CVT_S_W  = 0x3D,

    // ---- FPU Math ----
    OP_FADD = 0x30,
    OP_FSUB = 0x31,
    OP_FMUL = 0x32,
    OP_FDIV = 0x33,
    OP_FCMP = 0x34,

    // ---- System ----
    OP_HALT = 0x3F
};

// -------------------- Instruction formats --------------------
// R-type:  opcode(6) | mode(2)      | rd(4)     | rs(4) | rt(4) | cond(3) | unused(9)
// I-type:  opcode(6) | mode(2)      | rd/rt(4)  | rs(4) | imm(16)
// SH-type: opcode(6) | mode(2)=00   | rd(4)     | rs(4) | shamt(5) | unused(11)
// BR-type: opcode(6) | offset(26, signed, PC-relative)
//          -- also covers zero-operand instructions (nop, ret, halt,
//             exit, leave), which simply leave offset unused/zero,
//             per the official PRD. (No separate zero-address format.)
// FP-type: 3 sub-shapes (reg-only, load/store, GPR-transfer) -- see get_fp_* helpers
//
// mode(2) doubles as the R/I format selector for shared-opcode
// instructions (ADD/SUB/MUL/XOR/CMP/MOV):
//   mode = 00 -> R-type (register form; mode otherwise unused)
//   mode = 01 -> I-type, unsigned immediate
//   mode = 10 -> I-type, "high" immediate
//   mode = 11 -> I-type, signed immediate

enum class Format {
    R,       // register-operand ALU/data/system instructions
    I,       // immediate-operand instructions
    SH,      // shift instructions
    BR,      // branches / call / zero-operand instructions (unused offset)
    FP       // floating-point family (3 sub-shapes)
};

// -------------------- Field extraction (generic) --------------------
inline uint8_t get_opcode(Word w) { return (w >> 26) & 0x3F; }   // bits 31-26, all formats
inline uint8_t get_mode(Word w)   { return (w >> 24) & 0x3;  }   // bits 25-24, R/I/SH-type only

// -------------------- R-type fields --------------------
inline uint8_t get_r_rd(Word w)   { return (w >> 20) & 0xF; }    // bits 23-20
inline uint8_t get_r_rs(Word w)   { return (w >> 16) & 0xF; }    // bits 19-16
inline uint8_t get_r_rt(Word w)   { return (w >> 12) & 0xF; }    // bits 15-12
inline uint8_t get_r_cond(Word w) { return (w >> 9)  & 0x7; }    // bits 11-9
// Interpreted via the Cond enum + eval_cond() below (default mapping,
// reusing branch condition semantics — see note there).

// -------------------- I-type fields --------------------
inline uint8_t get_i_rd(Word w)    { return (w >> 20) & 0xF; }   // bits 23-20 (rd/rt, shared slot)
inline uint8_t get_i_rs(Word w)    { return (w >> 16) & 0xF; }   // bits 19-16
inline int32_t get_i_imm(Word w) {                                // bits 15-0, sign-extended
    int32_t v = w & 0xFFFF;
    if (v & 0x8000) v |= ~0xFFFF;   // sign-extend from bit 15
    return v;
}
inline uint32_t get_i_imm_unsigned(Word w) { return w & 0xFFFF; } // for mode=01 (unsigned) reads

// -------------------- SH-type fields --------------------
inline uint8_t get_sh_rd(Word w)     { return (w >> 20) & 0xF; } // bits 23-20
inline uint8_t get_sh_rs(Word w)     { return (w >> 16) & 0xF; } // bits 19-16
inline uint8_t get_sh_shamt(Word w)  { return (w >> 11) & 0x1F;} // bits 15-11 (5 bits, 0-31)

// -------------------- BR-type fields --------------------
inline int32_t get_br_offset(Word w) {                            // bits 25-0, sign-extended
    int32_t v = w & 0x3FFFFFF;
    if (v & 0x2000000) v |= ~0x3FFFFFF;  // sign-extend from bit 25
    return v;
}

// -------------------- FP-type fields (3 sub-shapes) --------------------
// FP reg-only:      opcode(6) | fd(4) | fs(4) | ft(4) | unused(14)
inline uint8_t get_fp_fd(Word w) { return (w >> 22) & 0xF; }     // bits 25-22
inline uint8_t get_fp_fs(Word w) { return (w >> 18) & 0xF; }     // bits 21-18
inline uint8_t get_fp_ft(Word w) { return (w >> 14) & 0xF; }     // bits 17-14

// FP load/store:     opcode(6) | mode(2) | fd/fs(4) | rs(4, GPR base) | imm(16)
inline uint8_t get_fpls_freg(Word w) { return (w >> 20) & 0xF; } // bits 23-20
inline uint8_t get_fpls_rs(Word w)   { return (w >> 16) & 0xF; } // bits 19-16
inline int32_t get_fpls_imm(Word w)  { return get_i_imm(w); }    // same layout as I-type imm

// FP<->GPR transfer:  opcode(6) | rd/rs(4, GPR) | fs/fd(4, FP) | unused(18)
inline uint8_t get_fpxfer_gpr(Word w) { return (w >> 22) & 0xF; } // bits 25-22
inline uint8_t get_fpxfer_fpr(Word w) { return (w >> 18) & 0xF; } // bits 21-18

// -------------------- Which format does this opcode use --------------------
// NOTE: for shared-opcode instructions (ADD/SUB/MUL/XOR/CMP/MOV),
// the caller must also check get_mode(): mode==0 means R, else I.
// This function returns the format family assuming that check is
// done separately; it does not read mode itself.
inline Format format_of(uint8_t opcode) {
    switch (opcode) {
        case OP_LSL: case OP_LSR: case OP_ASR:
            return Format::SH;

        case OP_B: case OP_BEQ: case OP_BNE: case OP_BGT: case OP_BLT:
        case OP_CALL: case OP_CALLCHK: case OP_BIF: case OP_SKIP:
        case OP_BC1T: case OP_BC1F:
        case OP_NOP: case OP_RET: case OP_HALT: case OP_EXIT: case OP_LEAVE:
            // zero-operand instructions are BR-type with offset unused/zero
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
            return Format::R;  // caller must check get_mode() to distinguish I-type

        default:
            return Format::R;  // AND, OR, NOT, PUSH, POP, SWAP, CSEL,
                                // SPUSH, SPOP, BNDCHK, ENTER — all pure R-type
    }
}

// -------------------- Register file layout --------------------
constexpr int NUM_INT_REGISTERS   = 16;   // R0-R15
constexpr int NUM_FLOAT_REGISTERS = 16;   // F0-F15
constexpr int REG_WIDTH_BITS      = 32;

constexpr int REG_SWAP = 13;  // R13 — Swap
constexpr int REG_SP   = 14;  // R14 — Stack Pointer
constexpr int REG_RA   = 15;  // R15 — Return Address

// -------------------- Condition codes (for branches, and CSEL) --------------------
// BEQ: Z==1 | BNE: Z==0 | BGT: N==0 && Z==0 | BLT: N==1
struct Flags {
    bool Z = false;  // zero
    bool N = false;  // negative
    bool C = false;  // carry
    bool V = false;  // overflow
};

// CSEL's cond(3) field: DEFAULT semantics adopted (reuses the branch
// condition scheme above), NOT yet explicitly confirmed by the team —
// flag this in Milestone 0 discussion before the shared isa.h freezes.
// Values 4-7 are reserved/undefined under this default.
enum class Cond : uint8_t {
    EQ = 0b000,  // Z==1            (matches BEQ)
    NE = 0b001,  // Z==0            (matches BNE)
    GT = 0b010,  // N==0 && Z==0    (matches BGT)
    LT = 0b011   // N==1            (matches BLT)
    // 0b100-0b111 reserved
};

inline bool eval_cond(Cond c, const Flags& f) {
    switch (c) {
        case Cond::EQ: return f.Z;
        case Cond::NE: return !f.Z;
        case Cond::GT: return !f.N && !f.Z;
        case Cond::LT: return f.N;
        default:       return false;  // reserved values: default to not-taken
    }
}

// FCC — 1-bit float condition code. Write-set RESOLVED from the PRD's
// own instruction operation descriptions: only FCMP, C.EQ.S, C.LE.S,
// and C.LT.S are described as comparison/flag-producing FP ops. No
// other FP instruction (FADD/FSUB/FMUL/FDIV/ABS.S/NEG.S/MOV.S/
// CVT.W.S/CVT.S.W/L.S/S.S/MFC1/MTC1) writes it. Read by BC1T/BC1F.
using FCC = bool;

#endif // ISA_H