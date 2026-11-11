#ifndef FPU_H
#define FPU_H

#include <cstdint>
#include <bitset>
#include <utility>

#include "control_unit/cu_types.h"
#include "isa_spec/isa.h"
#include "simulator_core/core/exception.h"

struct FpuResult {
    Word value;
    bool zero;
    bool negative;
    bool carry;
    bool overflow;
    Word fcc; // Floating-point condition code for InternalReg::FCC (0 = EQ, 1 = LT, 2 = GT)
};

class FPU {
private:
    struct UnpackedFloat {
        bool sign;
        std::bitset<32> exp;
        std::bitset<32> mant;
        bool is_zero;
        bool is_inf;
        bool is_nan;
    };

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
    std::bitset<32> shift_LSR_Sticky(const std::bitset<32>& a, const std::bitset<32>& shift_bits) const;

    // kogge stone adder
    std::bitset<32> ks_ADD(const std::bitset<32>& a, const std::bitset<32>& b, bool cin, bool& cout, bool& overflow) const;
    std::bitset<32> ks_SUB(const std::bitset<32>& a, const std::bitset<32>& b, bool& cout, bool& overflow) const;
    std::bitset<32> twos_complement(const std::bitset<32>& a) const;

    // mantissa non-restoring div (A = dividend, Q = 0)
    std::pair<std::bitset<32>, std::bitset<32>> algo_MantissaNonRestoringDiv(const std::bitset<32>& dividend, const std::bitset<32>& divisor) const;

    // IEEE-754 unpack, shared normalizer/rounder, and execution lanes
    UnpackedFloat unpack(const std::bitset<32>& raw) const;
    std::bitset<32> normalizeAndPack(bool sign_res, std::bitset<32> exp_res, std::bitset<32> mant_grs, bool& cout, bool& ovf) const;

    std::bitset<32> exec_FADD(const std::bitset<32>& a, const std::bitset<32>& b, bool& cout, bool& ovf) const;
    std::bitset<32> exec_FSUB(const std::bitset<32>& a, const std::bitset<32>& b, bool& cout, bool& ovf) const;
    std::bitset<32> exec_FMUL(const std::bitset<32>& a, const std::bitset<32>& b, bool& cout, bool& ovf) const;
    std::bitset<32> exec_FDIV(const std::bitset<32>& a, const std::bitset<32>& b, bool& cout, bool& ovf) const;

    void updateStandardFlags(const std::bitset<32>& res, FpuResult& out, bool cout, bool ovf_in) const;
    bool isZero(const std::bitset<32>& val) const;

public:
    FPU() = default;
    ~FPU() = default;

    FpuResult execute(risc201::cu::FpuOp op, Word a, Word b) const;
};

#endif // FPU_H