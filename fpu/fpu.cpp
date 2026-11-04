#include "fpu.h"

using namespace std;
using namespace risc201::cu;

// basic gates, no bitwise ops
bool FPU::gate_NOT(bool a) const { return (a == true) ? false : true; }
bool FPU::gate_NAND(bool a, bool b) const { return (a == true && b == true) ? false : true; }
bool FPU::gate_NOR(bool a, bool b) const { return (a == false && b == false) ? true : false; }
bool FPU::gate_AND(bool a, bool b) const { return gate_NOT(gate_NAND(a, b)); }
bool FPU::gate_OR(bool a, bool b)  const { return gate_NOT(gate_NOR(a, b)); }
bool FPU::gate_XOR(bool a, bool b) const { return gate_OR(gate_AND(a, gate_NOT(b)), gate_AND(gate_NOT(a), b)); }

bitset<32> FPU::logic_AND(const bitset<32>& a, const bitset<32>& b) const {
    bitset<32> res;
    for (int i = 0; i < 32; ++i) res[i] = gate_AND(a[i], b[i]);
    return res;
}
bitset<32> FPU::logic_OR(const bitset<32>& a, const bitset<32>& b) const {
    bitset<32> res;
    for (int i = 0; i < 32; ++i) res[i] = gate_OR(a[i], b[i]);
    return res;
}
bitset<32> FPU::logic_XOR(const bitset<32>& a, const bitset<32>& b) const {
    bitset<32> res;
    for (int i = 0; i < 32; ++i) res[i] = gate_XOR(a[i], b[i]);
    return res;
}
bitset<32> FPU::logic_NOT(const bitset<32>& a) const {
    bitset<32> res;
    for (int i = 0; i < 32; ++i) res[i] = gate_NOT(a[i]);
    return res;
}

bitset<32> FPU::shift_LSL(const bitset<32>& a, uint32_t shift_amt) const {
    bitset<32> s[6];
    s[0] = a;
    for (int stage = 0; stage < 5; ++stage) {
        int offset = 1 << stage;
        bool do_shift = (shift_amt >> stage) & 1;
        for (int i = 0; i < 32; ++i) {
            s[stage+1][i] = do_shift ? (i >= offset ? s[stage][i - offset] : false) : s[stage][i];
        }
    }
    return s[5];
}

bitset<32> FPU::shift_LSR(const bitset<32>& a, uint32_t shift_amt) const {
    bitset<32> s[6];
    s[0] = a;
    for (int stage = 0; stage < 5; ++stage) {
        int offset = 1 << stage;
        bool do_shift = (shift_amt >> stage) & 1;
        for (int i = 0; i < 32; ++i) {
            s[stage+1][i] = do_shift ? (i + offset < 32 ? s[stage][i + offset] : false) : s[stage][i];
        }
    }
    return s[5];
}

bitset<32> FPU::shift_ASR(const bitset<32>& a, uint32_t shift_amt) const {
    bitset<32> s[6];
    s[0] = a;
    bool sign = a[31];
    for (int stage = 0; stage < 5; ++stage) {
        int offset = 1 << stage;
        bool do_shift = (shift_amt >> stage) & 1;
        for (int i = 0; i < 32; ++i) {
            s[stage+1][i] = do_shift ? (i + offset < 32 ? s[stage][i + offset] : sign) : s[stage][i];
        }
    }
    return s[5];
}

// 5-stage barrel right-shifter driven by exponent-difference wires with Sticky-bit accumulation
bitset<32> FPU::shift_LSR_Sticky(const bitset<32>& a, const bitset<32>& shift_bits) const {
    bool ge_32 = false;
    for (int i = 5; i < 32; ++i) {
        ge_32 = gate_OR(ge_32, shift_bits[i]);
    }
    if (ge_32 == true) {
        bitset<32> out(0);
        out[0] = gate_NOT(isZero(a));
        return out;
    }

    bitset<32> s[6];
    s[0] = a;
    bool sticky = false;

    for (int stage = 0; stage < 5; ++stage) {
        int offset = 1 << stage;
        bool do_shift = shift_bits[stage];
        if (do_shift == true) {
            for (int k = 0; k < offset; ++k) {
                sticky = gate_OR(sticky, s[stage][k]);
            }
        }
        for (int i = 0; i < 32; ++i) {
            s[stage+1][i] = do_shift ? (i + offset < 32 ? s[stage][i + offset] : false) : s[stage][i];
        }
    }

    s[5][0] = gate_OR(s[5][0], sticky);
    return s[5];
}

// kogge stone adder
bitset<32> FPU::ks_ADD(const bitset<32>& a, const bitset<32>& b, bool cin, bool& cout, bool& overflow) const {
    bool p[6][32];
    bool g[6][32];

    for (int i = 0; i < 32; ++i) {
        p[0][i] = gate_XOR(a[i], b[i]);
        g[0][i] = gate_AND(a[i], b[i]);
    }

    for (int stage = 1; stage <= 5; ++stage) {
        int dist = 1 << (stage - 1);
        for (int i = 0; i < 32; ++i) {
            if (i >= dist) {
                g[stage][i] = gate_OR(g[stage-1][i], gate_AND(p[stage-1][i], g[stage-1][i - dist]));
                p[stage][i] = gate_AND(p[stage-1][i], p[stage-1][i - dist]);
            } else {
                g[stage][i] = g[stage-1][i];
                p[stage][i] = p[stage-1][i];
            }
        }
    }

    bitset<32> sum;
    bool c[32];
    for (int i = 0; i < 32; ++i) {
        bool prev_c = (i == 0) ? cin : c[i-1];
        c[i] = gate_OR(g[5][i], gate_AND(p[5][i], cin));
        sum[i] = gate_XOR(p[0][i], prev_c);
    }

    cout = c[31];
    overflow = gate_XOR(c[31], c[30]);
    return sum;
}

bitset<32> FPU::ks_SUB(const bitset<32>& a, const bitset<32>& b, bool& cout, bool& overflow) const {
    return ks_ADD(a, logic_NOT(b), true, cout, overflow);
}

bitset<32> FPU::twos_complement(const bitset<32>& a) const {
    bool dummy_cout, dummy_ovf;
    return ks_SUB(bitset<32>(0), a, dummy_cout, dummy_ovf);
}

// 16-iteration Radix-4 Booth Multiplier returning full 64-bit [A:Q] register pair for FMUL
pair<bitset<32>, bitset<32>> FPU::algo_Radix4BoothMul64(const bitset<32>& a, const bitset<32>& b) const {
    bitset<32> A = 0;
    bitset<32> Q = b;
    bitset<32> M = a;
    bool q_minus_1 = false, dummy_cout, dummy_ovf;

    for (int i = 0; i < 16; ++i) {
        uint8_t window = (Q[1] << 2) | (Q[0] << 1) | q_minus_1;
        bitset<32> addend = 0;

        switch (window) {
            case 1: case 2: addend = M; A = ks_ADD(A, addend, false, dummy_cout, dummy_ovf); break;
            case 3:         addend = shift_LSL(M, 1); A = ks_ADD(A, addend, false, dummy_cout, dummy_ovf); break;
            case 4:         addend = shift_LSL(M, 1); A = ks_SUB(A, addend, dummy_cout, dummy_ovf); break;
            case 5: case 6: addend = M; A = ks_SUB(A, addend, dummy_cout, dummy_ovf); break;
            case 0: case 7: break;
        }

        q_minus_1 = Q[1];
        bool a_lsb_0 = A[0], a_lsb_1 = A[1];
        Q = shift_LSR(Q, 2);
        Q[31] = a_lsb_1; Q[30] = a_lsb_0;
        A = shift_ASR(A, 2);
    }
    return {A, Q};
}

// 32-stage Non-Restoring Mantissa Divider (Dividend hardwired into Accumulator A, Q = 0)
pair<bitset<32>, bitset<32>> FPU::algo_MantissaNonRestoringDiv(const bitset<32>& dividend, const bitset<32>& divisor) const {
    bitset<32> A = dividend;
    bitset<32> Q = 0;
    bitset<32> M = shift_LSL(divisor, 1);
    bool dummy_cout, dummy_ovf;

    for (int i = 0; i < 32; ++i) {
        bool a_sign = A[31];

        A = shift_LSL(A, 1);
        A[0] = Q[31];
        Q = shift_LSL(Q, 1);

        if (a_sign == false) A = ks_SUB(A, M, dummy_cout, dummy_ovf);
        else                 A = ks_ADD(A, M, false, dummy_cout, dummy_ovf);

        Q[0] = gate_NOT(A[31]);
    }

    if (A[31] == true) A = ks_ADD(A, M, false, dummy_cout, dummy_ovf);
    return {Q, A};
}

bool FPU::isZero(const bitset<32>& val) const {
    for (int i = 0; i < 32; ++i) if (val[i]) return false;
    return true;
}

// Unpack 32-bit IEEE-754 word into Sign, 8-bit Exponent, and 24-bit Mantissa (hidden bit at index 23)
FPU::UnpackedFloat FPU::unpack(const bitset<32>& raw) const {
    UnpackedFloat u;
    u.sign = raw[31];
    u.exp.reset();
    u.mant.reset();

    for (int i = 0; i < 8; ++i)  u.exp[i]  = raw[23 + i];
    for (int i = 0; i < 23; ++i) u.mant[i] = raw[i];

    bool exp_zero = isZero(u.exp);
    bool mant_zero = isZero(u.mant);
    bool exp_all_ones = true;
    for (int i = 0; i < 8; ++i) {
        exp_all_ones = gate_AND(exp_all_ones, u.exp[i]);
    }

    u.is_zero = gate_AND(exp_zero, mant_zero);
    u.is_inf  = gate_AND(exp_all_ones, mant_zero);
    u.is_nan  = gate_AND(exp_all_ones, gate_NOT(mant_zero));

    bool is_normal = gate_AND(gate_NOT(exp_zero), gate_NOT(exp_all_ones));
    bool is_subnormal = gate_AND(exp_zero, gate_NOT(mant_zero));

    if (is_normal == true) {
        u.mant[23] = true;
    } else if (is_subnormal == true) {
        u.mant[23] = false;
        u.exp = bitset<32>(1);
    }
    return u;
}

// Shared Normalization, Round-to-Nearest-Even (GRS), and Packing Block
bitset<32> FPU::normalizeAndPack(bool sign_res, bitset<32> exp_res, bitset<32> mant_grs, bool& cout, bool& ovf) const {
    cout = false;
    ovf = false;
    bool dummy_cout, dummy_ovf;
    const bitset<32> ONE(1);
    const bitset<32> EXP_MAX(255);

    if (isZero(mant_grs) == true) {
        bitset<32> zero_out(0);
        zero_out[31] = sign_res;
        return zero_out;
    }

    // 1. Right-Shift Normalization if Carry-Out into Bit 27 occurred (Value in [2.0, 4.0))
    if (mant_grs[27] == true) {
        bool dropped_lsb = mant_grs[0];
        mant_grs = shift_LSR(mant_grs, 1);
        mant_grs[0] = gate_OR(mant_grs[0], dropped_lsb);
        exp_res = ks_ADD(exp_res, ONE, false, dummy_cout, dummy_ovf);
        cout = true;
    } else {
        // 2. Left-Shift Normalization via Leading-Zero Counter if Bit 26 is 0 (Value < 1.0)
        uint32_t lzc = 0;
        for (int i = 26; i >= 0; --i) {
            if (mant_grs[i] == true) break;
            ++lzc;
        }
        if (lzc > 0) {
            mant_grs = shift_LSL(mant_grs, lzc);
            exp_res = ks_SUB(exp_res, bitset<32>(lzc), dummy_cout, dummy_ovf);
        }
    }

    // 3. IEEE-754 Round-to-Nearest-Even using Guard (Bit 2), Round (Bit 1), Sticky (Bit 0)
    bool guard  = mant_grs[2];
    bool round  = mant_grs[1];
    bool sticky = mant_grs[0];
    bool lsb    = mant_grs[3];

    bitset<32> mant_24 = shift_LSR(mant_grs, 3);
    bool round_up = gate_AND(guard, gate_OR(round, gate_OR(sticky, lsb)));

    if (round_up == true) {
        mant_24 = ks_ADD(mant_24, ONE, false, dummy_cout, dummy_ovf);
        if (mant_24[24] == true) {
            mant_24 = shift_LSR(mant_24, 1);
            exp_res = ks_ADD(exp_res, ONE, false, dummy_cout, dummy_ovf);
            cout = true;
        }
    }

    // 4. Exponent Underflow (exp_res <= 0) or Overflow (exp_res >= 255) checks
    bool exp_le_zero = gate_OR(exp_res[31], isZero(exp_res));
    if (exp_le_zero == true) {
        bitset<32> udf_zero(0);
        udf_zero[31] = sign_res;
        return udf_zero;
    }

    bitset<32> ovf_diff = ks_SUB(exp_res, EXP_MAX, dummy_cout, dummy_ovf);
    if (ovf_diff[31] == false) {
        ovf = true;
        bitset<32> inf_out(0x7F800000);
        inf_out[31] = sign_res;
        return inf_out;
    }

    // 5. Pack {Sign, Exponent[7:0], Mantissa[22:0]} (Implicit Bit 23 stripped)
    bitset<32> out(0);
    out[31] = sign_res;
    for (int i = 0; i < 8; ++i)  out[23 + i] = exp_res[i];
    for (int i = 0; i < 23; ++i) out[i]      = mant_24[i];
    return out;
}

// Lane 1: FADD / FSUB
bitset<32> FPU::exec_FADD(const bitset<32>& a, const bitset<32>& b, bool& cout, bool& ovf) const {
    cout = false;
    ovf = false;
    UnpackedFloat ua = unpack(a);
    UnpackedFloat ub = unpack(b);

    if (gate_OR(ua.is_nan, ub.is_nan) == true) return bitset<32>(0x7FC00000);
    if (gate_AND(ua.is_inf, ub.is_inf) == true) {
        if (gate_XOR(ua.sign, ub.sign) == true) return bitset<32>(0x7FC00000);
        return a;
    }
    if (ua.is_inf == true) return a;
    if (ub.is_inf == true) return b;
    if (gate_AND(ua.is_zero, ub.is_zero) == true) {
        bitset<32> z(0);
        z[31] = gate_AND(ua.sign, ub.sign);
        return z;
    }
    if (ua.is_zero == true) return b;
    if (ub.is_zero == true) return a;

    // Step 1: Compare Exponents via Kogge-Stone Subtractor
    bool dummy_cout, dummy_ovf;
    bitset<32> exp_diff = ks_SUB(ua.exp, ub.exp, dummy_cout, dummy_ovf);

    bool swap_operands = false;
    if (exp_diff[31] == true) {
        swap_operands = true;
    } else if (isZero(exp_diff) == true) {
        bitset<32> mant_diff = ks_SUB(ua.mant, ub.mant, dummy_cout, dummy_ovf);
        if (mant_diff[31] == true) swap_operands = true;
    }

    if (swap_operands == true) {
        UnpackedFloat tmp = ua;
        ua = ub;
        ub = tmp;
        exp_diff = ks_SUB(ua.exp, ub.exp, dummy_cout, dummy_ovf);
    }

    // Step 2: Shift both 24-bit mantissas left by 3 for Guard, Round, Sticky slots, then align ub.mant
    bitset<32> mant_a_grs = shift_LSL(ua.mant, 3);
    bitset<32> mant_b_grs = shift_LSL(ub.mant, 3);
    mant_b_grs = shift_LSR_Sticky(mant_b_grs, exp_diff);

    // Step 3: Add or Subtract Aligned Mantissas via Kogge-Stone
    bitset<32> mant_res;
    bool diff_signs = gate_XOR(ua.sign, ub.sign);
    if (diff_signs == false) {
        mant_res = ks_ADD(mant_a_grs, mant_b_grs, false, dummy_cout, dummy_ovf);
    } else {
        mant_res = ks_SUB(mant_a_grs, mant_b_grs, dummy_cout, dummy_ovf);
        if (isZero(mant_res) == true) return bitset<32>(0);
    }

    return normalizeAndPack(ua.sign, ua.exp, mant_res, cout, ovf);
}

bitset<32> FPU::exec_FSUB(const bitset<32>& a, const bitset<32>& b, bool& cout, bool& ovf) const {
    bitset<32> b_neg = b;
    b_neg[31] = gate_NOT(b[31]);
    return exec_FADD(a, b_neg, cout, ovf);
}

// Lane 2: FMUL
bitset<32> FPU::exec_FMUL(const bitset<32>& a, const bitset<32>& b, bool& cout, bool& ovf) const {
    cout = false;
    ovf = false;
    UnpackedFloat ua = unpack(a);
    UnpackedFloat ub = unpack(b);
    bool sign_res = gate_XOR(ua.sign, ub.sign);

    if (gate_OR(ua.is_nan, ub.is_nan) == true) return bitset<32>(0x7FC00000);
    if (gate_OR(gate_AND(ua.is_inf, ub.is_zero), gate_AND(ua.is_zero, ub.is_inf)) == true) {
        return bitset<32>(0x7FC00000);
    }
    if (gate_OR(ua.is_inf, ub.is_inf) == true) {
        bitset<32> inf_out(0x7F800000);
        inf_out[31] = sign_res;
        return inf_out;
    }
    if (gate_OR(ua.is_zero, ub.is_zero) == true) {
        bitset<32> zero_out(0);
        zero_out[31] = sign_res;
        return zero_out;
    }

    // Step 1: Exponent Addition (E_A + E_B - 127) via Kogge-Stone
    bool dummy_cout, dummy_ovf;
    bitset<32> exp_sum = ks_ADD(ua.exp, ub.exp, false, dummy_cout, dummy_ovf);
    bitset<32> exp_res = ks_SUB(exp_sum, bitset<32>(127), dummy_cout, dummy_ovf);

    // Step 2: 24x24 Mantissa Multiplication via 16-stage Radix-4 Booth Multiplier
    auto prod_pair = algo_Radix4BoothMul64(ua.mant, ub.mant);
    bitset<32> prod_hi = prod_pair.first;
    bitset<32> prod_lo = prod_pair.second;

    // Extract bits [47:20] into mant_grs[27:0] and fold bits [20:0] into Sticky bit
    bitset<32> mant_grs(0);
    for (int i = 21; i < 32; ++i) mant_grs[i - 20] = prod_lo[i];
    for (int i = 0; i < 16; ++i)  mant_grs[i + 12] = prod_hi[i];

    bool sticky = false;
    for (int i = 0; i <= 20; ++i) {
        sticky = gate_OR(sticky, prod_lo[i]);
    }
    mant_grs[0] = sticky;

    return normalizeAndPack(sign_res, exp_res, mant_grs, cout, ovf);
}

// Lane 3: FDIV
bitset<32> FPU::exec_FDIV(const bitset<32>& a, const bitset<32>& b, bool& cout, bool& ovf) const {
    cout = false;
    ovf = false;
    UnpackedFloat ua = unpack(a);
    UnpackedFloat ub = unpack(b);

    // Hardware Exception Trap on Divide-by-Zero (matches ALU::execute and exception.h)
    if (ub.is_zero == true) {
        throw RuntimeException{ExceptionCode::DIV_BY_ZERO, 0};
    }

    bool sign_res = gate_XOR(ua.sign, ub.sign);

    if (gate_OR(ua.is_nan, ub.is_nan) == true) return bitset<32>(0x7FC00000);
    if (gate_AND(ua.is_inf, ub.is_inf) == true) return bitset<32>(0x7FC00000);
    if (ua.is_zero == true || ub.is_inf == true) {
        bitset<32> zero_out(0);
        zero_out[31] = sign_res;
        return zero_out;
    }
    if (ua.is_inf == true) {
        bitset<32> inf_out(0x7F800000);
        inf_out[31] = sign_res;
        return inf_out;
    }

    // Step 1: Exponent Subtraction (E_A - E_B + 127) via Kogge-Stone
    bool dummy_cout, dummy_ovf;
    bitset<32> exp_diff = ks_SUB(ua.exp, ub.exp, dummy_cout, dummy_ovf);
    bitset<32> exp_res  = ks_ADD(exp_diff, bitset<32>(127), false, dummy_cout, dummy_ovf);

    // Step 2: 32-Stage Non-Restoring Mantissa Division (Dividend in A, Q = 0)
    auto div_pair = algo_MantissaNonRestoringDiv(ua.mant, ub.mant);
    bitset<32> quot = div_pair.first;
    bitset<32> rem  = div_pair.second;

    // Align quotient (1.31 at Bit 31) down to mant_grs (1.26 at Bit 26) while preserving Sticky bit
    bitset<32> mant_grs = shift_LSR_Sticky(quot, bitset<32>(5));
    bool rem_nonzero = gate_NOT(isZero(rem));
    mant_grs[0] = gate_OR(mant_grs[0], rem_nonzero);

    return normalizeAndPack(sign_res, exp_res, mant_grs, cout, ovf);
}

void FPU::updateStandardFlags(const bitset<32>& res, FpuResult& out, bool cout, bool ovf_in) const {
    bitset<32> mag = res;
    mag[31] = false; // Ignore sign bit so -0.0 still sets Zero flag
    out.zero = isZero(mag);
    out.negative = gate_AND(res[31], gate_NOT(out.zero));
    out.carry = cout;
    out.overflow = ovf_in;

    // Populate FCC condition code register value (0 = EQ, 1 = LT, 2 = GT)
    if (out.zero == true) {
        out.fcc = 0;
    } else if (out.negative == true) {
        out.fcc = 1;
    } else {
        out.fcc = 2;
    }
}

FpuResult FPU::execute(FpuOp op, Word a, Word b) const {
    FpuResult out{0, false, false, false, false, 0};

    bitset<32> wires_a(static_cast<uint32_t>(a));
    bitset<32> wires_b(static_cast<uint32_t>(b));
    bitset<32> wires_out(0);
    bool cout = false, ovf = false;

    switch (op) {
        case FpuOp::FADD:
            wires_out = exec_FADD(wires_a, wires_b, cout, ovf);
            updateStandardFlags(wires_out, out, cout, ovf);
            break;

        case FpuOp::FSUB:
            wires_out = exec_FSUB(wires_a, wires_b, cout, ovf);
            updateStandardFlags(wires_out, out, cout, ovf);
            break;

        case FpuOp::FMUL:
            wires_out = exec_FMUL(wires_a, wires_b, cout, ovf);
            updateStandardFlags(wires_out, out, cout, ovf);
            break;

        case FpuOp::FDIV:
            wires_out = exec_FDIV(wires_a, wires_b, cout, ovf);
            updateStandardFlags(wires_out, out, cout, ovf);
            break;

        case FpuOp::FCMP:
            wires_out = exec_FSUB(wires_a, wires_b, cout, ovf);
            updateStandardFlags(wires_out, out, cout, ovf);
            wires_out.reset();
            break;

        case FpuOp::FMOV:
            wires_out = wires_b;
            updateStandardFlags(wires_out, out, false, false);
            break;

        case FpuOp::READ:
            wires_out = wires_a;
            updateStandardFlags(wires_out, out, false, false);
            break;

        case FpuOp::WRITE:
            wires_out = wires_b;
            updateStandardFlags(wires_out, out, false, false);
            break;

        case FpuOp::NOP:
        default:
            wires_out.reset();
            break;
    }

    out.value = static_cast<Word>(wires_out.to_ulong());
    return out;
}