#ifndef ISA_H
#define ISA_H

#include <cstdint>


using Word = uint32_t;

// -------------------- Opcodes (5-bit) --------------------
enum Opcode : uint8_t {
    OP_ADD  = 0b00000,
    OP_SUB  = 0b00001,
    OP_MUL  = 0b00010,
    OP_DIV  = 0b00011,
    OP_MOD  = 0b00100,
    OP_CMP  = 0b00101,
    OP_AND  = 0b00110,
    OP_OR   = 0b00111,
    OP_NOT  = 0b01000,
    OP_MOV  = 0b01001,
    OP_LSL  = 0b01010,
    OP_LSR  = 0b01011,
    OP_ASR  = 0b01100,
    OP_NOP  = 0b01101,
    OP_LD   = 0b01110,
    OP_ST   = 0b01111,
    OP_BEQ  = 0b10000,
    OP_BGT  = 0b10001,
    OP_B    = 0b10010,
    OP_CALL = 0b10011,
    OP_RET  = 0b10100
};

// -------------------- Instruction formats --------------------
// 0-address:   opcode[5]                              (nop, ret)
// Branch:      opcode[5] | offset[27]                 (b, beq, bgt, call)
// Register:    opcode[5] | I=0[1] | rd[4] | rs1[4] | rs2[4]
// Immediate:   opcode[5] | I=1[1] | rd[4] | rs1[4] | mod[2] | imm[18]

enum class Format {
    ZERO_ADDR,   // nop, ret
    BRANCH,      // b, beq, bgt, call
    REGISTER,    // I-bit = 0
    IMMEDIATE    // I-bit = 1
};

// -------------------- Field extraction --------------------
inline uint8_t  get_opcode(Word w) { return (w >> 27) & 0x1F; }          // bits 31-27
inline uint8_t  get_ibit(Word w)   { return (w >> 26) & 0x1;  }          // bit 26 (R/I formats only)
inline uint8_t  get_rd(Word w)     { return (w >> 22) & 0xF;  }          // bits 25-22
inline uint8_t  get_rs1(Word w)    { return (w >> 18) & 0xF;  }          // bits 21-18
inline uint8_t  get_rs2(Word w)    { return (w >> 14) & 0xF;  }          // bits 17-14 (register format only)
inline uint8_t  get_mod(Word w)    { return (w >> 16) & 0x3;  }          // bits 17-16 (immediate format only)
inline int32_t  get_imm18(Word w)  {                                     // bits 15-0, sign-extended
    int32_t v = w & 0x3FFFF;
    if (v & 0x20000) v |= ~0x3FFFF;   // sign-extend from bit 17
    return v;
}
inline int32_t  get_offset27(Word w) {                                  // bits 26-0, sign-extended
    int32_t v = w & 0x7FFFFFF;
    if (v & 0x4000000) v |= ~0x7FFFFFF;  // sign-extend from bit 26
    return v;
}

// -------------------- Helper: which format does this opcode use --------------------
inline Format format_of(uint8_t opcode) {
    switch (opcode) {
        case OP_NOP: case OP_RET:
            return Format::ZERO_ADDR;
        case OP_BEQ: case OP_BGT: case OP_B: case OP_CALL:
            return Format::BRANCH;
        default:
            // Register vs Immediate is decided at runtime by the I-bit,
            // not by opcode alone, for all remaining instructions.
            return Format::REGISTER; // caller should still check get_ibit()
    }
}

// -------------------- Register / special register counts --------------------
constexpr int NUM_REGISTERS = 16;   // R0-R15
constexpr int REG_WIDTH_BITS = 32;

#endif 
