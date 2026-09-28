#ifndef ALU_H
#define ALU_H

#include <cstdint>
#include <bitset>
#include <utility>
#include <stdexcept>

// Note: In your actual repo, ALUOp should be included from control_signals.h[cite: 6]. 
// Defined here so this file compiles cleanly on its own.
enum class ALUOp {
    NONE, ADD, SUB, MUL, DIV, MOD, AND, OR, NOT, XOR, LSL, LSR, ASR, CMP, PASS_B
};

struct ALUFlags {
    bool zero;     // Z
    bool negative; // N
    bool carry;    // C
    bool overflow; // V
    
    bool E;        // Equal (for BEQ)[cite: 6]
    bool GT;       // Greater Than (for BGT)[cite: 6]

    void reset() { 
        zero = negative = carry = overflow = E = GT = false; 
    }
};

struct AluResult {
    uint32_t result;
    ALUFlags flags;
};

class ALU {
private:
    // Fundamental Gates
    bool gate_NOT(bool a) const;
    bool gate_NAND(bool a, bool b) const;
    bool gate_NOR(bool a, bool b) const;
    bool gate_AND(bool a, bool b) const;
    bool gate_OR(bool a, bool b) const;
    bool gate_XOR(bool a, bool b) const;

    // Logic Paths
    std::bitset<32> logic_AND(const std::bitset<32>& a, const std::bitset<32>& b) const;
    std::bitset<32> logic_OR(const std::bitset<32>& a, const std::bitset<32>& b) const;
    std::bitset<32> logic_XOR(const std::bitset<32>& a, const std::bitset<32>& b) const;
    std::bitset<32> logic_NOT(const std::bitset<32>& a) const;
    
    // Shifters
    std::bitset<32> shift_LSL(const std::bitset<32>& a, uint32_t shift_amt) const;
    std::bitset<32> shift_LSR(const std::bitset<32>& a, uint32_t shift_amt) const;
    std::bitset<32> shift_ASR(const std::bitset<32>& a, uint32_t shift_amt) const;

    // Arithmetic (Carry Lookahead)
    void cla_4bit_block(const bool a[4], const bool b[4], bool cin, bool s[4], bool& cout) const;
    std::bitset<32> cla_ADD(const std::bitset<32>& a, const std::bitset<32>& b, bool cin, bool& cout, bool& overflow) const;
    std::bitset<32> cla_SUB(const std::bitset<32>& a, const std::bitset<32>& b, bool& cout, bool& overflow) const;
    std::bitset<32> twos_complement(const std::bitset<32>& a) const;

    // Algorithms
    std::bitset<32> algo_BoothMul(const std::bitset<32>& a, const std::bitset<32>& b) const;
    std::pair<std::bitset<32>, std::bitset<32>> algo_UnsignedRestoringDiv(const std::bitset<32>& dividend, const std::bitset<32>& divisor) const;

    // Helpers
    void updateStandardFlags(const std::bitset<32>& res, ALUFlags& flags_out, bool ovf_in) const;
    bool isZero(const std::bitset<32>& val) const;

public:
    ALU() = default;
    ~ALU() = default;

    // Stateless execution entry point[cite: 6]
    AluResult execute(ALUOp op, uint32_t a, uint32_t b) const;
};

#endif // ALU_H