#ifndef ISA_H
#define ISA_H

#include <cstdint>
#include <type_traits>

#ifndef RISC201_WORD_BITS
#define RISC201_WORD_BITS 32
#endif

constexpr int kWordBits = RISC201_WORD_BITS;
static_assert(kWordBits == 32 || kWordBits == 64,
              "RISC201_WORD_BITS must be 32 or 64");

using Word  = std::conditional_t<(kWordBits <= 32), std::uint32_t, std::uint64_t>;
using Sword = std::conditional_t<(kWordBits <= 32), std::int32_t,  std::int64_t>;

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

enum class Format {
    ZERO_ADDR,
    BRANCH,
    REGISTER,
    IMMEDIATE
};

inline uint8_t  get_opcode(Word w) { return (w >> 27) & 0x1F; }
inline uint8_t  get_ibit(Word w)   { return (w >> 26) & 0x1;  }
inline uint8_t  get_rd(Word w)     { return (w >> 22) & 0xF;  }
inline uint8_t  get_rs1(Word w)    { return (w >> 18) & 0xF;  }
inline uint8_t  get_rs2(Word w)    { return (w >> 14) & 0xF;  }
inline uint8_t  get_mod(Word w)    { return (w >> 16) & 0x3;  }
inline int32_t  get_imm18(Word w) {
    int32_t v = static_cast<int32_t>(w & 0x3FFFF);
    if (v & 0x20000) v |= ~0x3FFFF;
    return v;
}
inline int32_t  get_offset27(Word w) {
    int32_t v = static_cast<int32_t>(w & 0x7FFFFFF);
    if (v & 0x4000000) v |= ~0x7FFFFFF;
    return v;
}

inline Format format_of(uint8_t opcode) {
    switch (opcode) {
        case OP_NOP: case OP_RET:
            return Format::ZERO_ADDR;
        case OP_BEQ: case OP_BGT: case OP_B: case OP_CALL:
            return Format::BRANCH;
        default:
            return Format::REGISTER;
    }
}

constexpr int NUM_REGISTERS  = 16;
constexpr int REG_WIDTH_BITS = kWordBits;

#endif
