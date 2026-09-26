#ifndef ALU_H
#define ALU_H

#include <cstdint>
#include <bitset>
#include <utility>
#include <stdexcept>


// -------------------- Opcodes (6-bit) --------------------
// Synced with Garv's extended ISA spec
enum Opcode : uint8_t {
    OP_NOP  = 0x00,
    OP_ADD  = 0x01, OP_SUB  = 0x02, OP_MUL  = 0x03, OP_DIV  = 0x04, OP_MOD  = 0x05,
    OP_AND  = 0x06, OP_OR   = 0x07, OP_NOT  = 0x08, OP_XOR  = 0x09,
    OP_LSL  = 0x0A, OP_LSR  = 0x0B, OP_ASR  = 0x0C, OP_CMP  = 0x0D,
    OP_MOV  = 0x10, OP_LD   = 0x11, OP_ST   = 0x12, OP_PUSH = 0x13, OP_POP  = 0x14,
    OP_SWAP = 0x15, OP_CSEL = 0x16,
    OP_B    = 0x20, OP_BEQ  = 0x21, OP_BNE  = 0x22, OP_BGT  = 0x23, OP_BLT  = 0x24,
    OP_CALL = 0x25, OP_RET  = 0x26,
    OP_FADD = 0x30, OP_FSUB = 0x31, OP_FMUL = 0x32, OP_FDIV = 0x33,
    OP_HALT = 0x3F
};

// -------------------- Flags --------------------
struct ALUFlags {
    bool zero;     // Z
    bool negative; // N
    bool carry;    // C
    bool overflow; // V

    void reset() { zero = false; negative = false; carry = false; overflow = false; }
};

class ALU {
private:
    uint32_t result;
    ALUFlags flags;

    // --- Fundamental 1-Bit Logic Gates (Truth Tables) ---
    bool gate_NOT(bool a);
    bool gate_NAND(bool a, bool b);
    bool gate_NOR(bool a, bool b);
    bool gate_AND(bool a, bool b);
    bool gate_OR(bool a, bool b);
    bool gate_XOR(bool a, bool b);

    // --- 32-Bit Structural Data Paths ---
    bitset<32> logic_AND(const bitset<32>& a, const bitset<32>& b);
    bitset<32> logic_OR(const bitset<32>& a, const bitset<32>& b);
    bitset<32> logic_XOR(const bitset<32>& a, const bitset<32>& b);
    bitset<32> logic_NOT(const bitset<32>& a);
    
    // Shifts (implemented via wire rerouting)
    bitset<32> shift_LSL(const bitset<32>& a, uint32_t shift_amt);
    bitset<32> shift_LSR(const bitset<32>& a, uint32_t shift_amt);
    bitset<32> shift_ASR(const bitset<32>& a, uint32_t shift_amt);

    // Arithmetic via Block Carry Lookahead (CLA)
    void cla_4bit_block(bool a[4], bool b[4], bool cin, bool s[4], bool& cout);
    bitset<32> cla_ADD(const bitset<32>& a, const bitset<32>& b, bool cin, bool& cout, bool& overflow);
    bitset<32> cla_SUB(const bitset<32>& a, const bitset<32>& b, bool& cout, bool& overflow);

    // Algorithms required by PRD
    bitset<32> algo_BoothMul(const bitset<32>& a, const bitset<32>& b);
    pair<bitset<32>, bitset<32>> algo_RestoringDiv(const bitset<32>& dividend, const bitset<32>& divisor);

    // Helpers
    void updateStandardFlags(const bitset<32>& res);
    bool isZero(const bitset<32>& val);

public:
    ALU();
    ~ALU() = default;

    // Pipeline entry point
    uint32_t execute(Opcode op, uint32_t a, uint32_t b);

    uint32_t getResult() const;
    ALUFlags getFlags() const;
};

#endif // ALU_H