#ifndef ALU_H
#define ALU_H

#include <cstdint>
#include <bitset>
#include <utility>

#include "control_unit/cu_types.h"
#include "isa_spec/isa.h"
#include "simulator_core/core/exception.h"

struct AluResult {
    Word value;
    bool zero;
    bool negative;
    bool carry;
    bool overflow;
};

class ALU {
private:
    // basic gates, no bitwise operators
    bool gate_NOT(bool a) const;
    bool gate_NAND(bool a, bool b) const;
    bool gate_NOR(bool a, bool b) const;
    bool gate_AND(bool a, bool b) const;
    bool gate_OR(bool a, bool b) const;
    bool gate_XOR(bool a, bool b) const;

    std::bitset<32> logic_AND(const std::bitset<32>& a, const std::bitset<32>& b) const;
    std::bitset<32> logic_OR(const std::bitset<32>& a, const std::bitset<32>& b) const;
    std::bitset<32> logic_XOR(const std::bitset<32>& a, const std::bitset<32>& b) const;
    std::bitset<32> logic_NOT(const std::bitset<32>& a) const;

    // barrel shifter, log n stages
    std::bitset<32> shift_LSL(const std::bitset<32>& a, uint32_t shift_amt) const;
    std::bitset<32> shift_LSR(const std::bitset<32>& a, uint32_t shift_amt) const;
    std::bitset<32> shift_ASR(const std::bitset<32>& a, uint32_t shift_amt) const;

    // kogge stone adder
    std::bitset<32> ks_ADD(const std::bitset<32>& a, const std::bitset<32>& b, bool cin, bool& cout, bool& overflow) const;
    std::bitset<32> ks_SUB(const std::bitset<32>& a, const std::bitset<32>& b, bool& cout, bool& overflow) const;
    std::bitset<32> twos_complement(const std::bitset<32>& a) const;

    // booth mul and non restoring div
    std::bitset<32> algo_Radix4BoothMul(const std::bitset<32>& a, const std::bitset<32>& b) const;
    std::pair<std::bitset<32>, std::bitset<32>> algo_NonRestoringDiv(const std::bitset<32>& dividend, const std::bitset<32>& divisor) const;

    void updateStandardFlags(const std::bitset<32>& res, AluResult& out, bool cout, bool ovf_in) const;
    bool isZero(const std::bitset<32>& val) const;

public:
    ALU() = default;
    ~ALU() = default;

    AluResult execute(risc201::cu::AluOp op, Word a, Word b) const;
};

#endif // ALU_H
