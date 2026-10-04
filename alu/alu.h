#ifndef ALU_H
#define ALU_H

#include <cstdint>
#include <bitset>
#include <utility>

// Core architecture integrations 
#include "../simulator_core/exception.h" 
#include "../control_unit/common/control_signals.h" // Garv's horizontal/vertical control signals

#ifndef RISC201_WORD_BITS
#define RISC201_WORD_BITS 32
#endif
using Word = std::uint32_t;

// Pipeline integration struct explicitly required by Aditya's datapath loop
struct AluResult {
    Word value;
    bool zero;
    bool negative;
    bool carry;
    bool overflow;
};

class ALU {
private:
    // --- 1-Bit Gate Primitives ---
    bool gate_NOT(bool a) const;
    bool gate_NAND(bool a, bool b) const;
    bool gate_NOR(bool a, bool b) const;
    bool gate_AND(bool a, bool b) const;
    bool gate_OR(bool a, bool b) const;
    bool gate_XOR(bool a, bool b) const;

    // --- High-Speed Parallel Paths ---
    std::bitset<32> logic_AND(const std::bitset<32>& a, const std::bitset<32>& b) const;
    std::bitset<32> logic_OR(const std::bitset<32>& a, const std::bitset<32>& b) const;
    std::bitset<32> logic_XOR(const std::bitset<32>& a, const std::bitset<32>& b) const;
    std::bitset<32> logic_NOT(const std::bitset<32>& a) const;
    
    // Logarithmic Barrel Shifters O(log n)
    std::bitset<32> shift_LSL(const std::bitset<32>& a, uint32_t shift_amt) const;
    std::bitset<32> shift_LSR(const std::bitset<32>& a, uint32_t shift_amt) const;
    std::bitset<32> shift_ASR(const std::bitset<32>& a, uint32_t shift_amt) const;

    // Kogge-Stone Parallel Prefix Adder O(log n)
    std::bitset<32> ks_ADD(const std::bitset<32>& a, const std::bitset<32>& b, bool cin, bool& cout, bool& overflow) const;
    std::bitset<32> ks_SUB(const std::bitset<32>& a, const std::bitset<32>& b, bool& cout, bool& overflow) const;
    std::bitset<32> twos_complement(const std::bitset<32>& a) const;

    // Radix-4 Modified Booth & Non-Restoring Division
    std::bitset<32> algo_Radix4BoothMul(const std::bitset<32>& a, const std::bitset<32>& b) const;
    std::pair<std::bitset<32>, std::bitset<32>> algo_NonRestoringDiv(const std::bitset<32>& dividend, const std::bitset<32>& divisor) const;

    // Output formatting
    void updateStandardFlags(const std::bitset<32>& res, AluResult& out, bool cout, bool ovf_in) const;
    bool isZero(const std::bitset<32>& val) const;

public:
    ALU() = default;
    ~ALU() = default;

    // Stateless, single-cycle combinational execution mapped to Garv's control space
    AluResult execute(risc201::cu::AluOp op, Word a, Word b) const;
};

#endif // ALU_H