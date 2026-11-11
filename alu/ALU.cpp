#include "alu.h"

using namespace std;
using namespace risc201::cu;

// basic gates, no bitwise ops
bool ALU::gate_NOT(bool a) const { return (a == true) ? false : true; }
bool ALU::gate_NAND(bool a, bool b) const { return (a == true && b == true) ? false : true; }
bool ALU::gate_NOR(bool a, bool b) const { return (a == false && b == false) ? true : false; }
bool ALU::gate_AND(bool a, bool b) const { return gate_NOT(gate_NAND(a, b)); }
bool ALU::gate_OR(bool a, bool b)  const { return gate_NOT(gate_NOR(a, b)); }
bool ALU::gate_XOR(bool a, bool b) const { return gate_OR(gate_AND(a, gate_NOT(b)), gate_AND(gate_NOT(a), b)); }

bitset<32> ALU::logic_AND(const bitset<32>& a, const bitset<32>& b) const {
    bitset<32> res;
    for (int i = 0; i < 32; ++i) res[i] = gate_AND(a[i], b[i]);
    return res;
}
bitset<32> ALU::logic_OR(const bitset<32>& a, const bitset<32>& b) const {
    bitset<32> res;
    for (int i = 0; i < 32; ++i) res[i] = gate_OR(a[i], b[i]);
    return res;
}
bitset<32> ALU::logic_XOR(const bitset<32>& a, const bitset<32>& b) const {
    bitset<32> res;
    for (int i = 0; i < 32; ++i) res[i] = gate_XOR(a[i], b[i]);
    return res;
}
bitset<32> ALU::logic_NOT(const bitset<32>& a) const {
    bitset<32> res;
    for (int i = 0; i < 32; ++i) res[i] = gate_NOT(a[i]);
    return res;
}

bitset<32> ALU::shift_LSL(const bitset<32>& a, uint32_t shift_amt) const {
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

bitset<32> ALU::shift_LSR(const bitset<32>& a, uint32_t shift_amt) const {
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

bitset<32> ALU::shift_ASR(const bitset<32>& a, uint32_t shift_amt) const {
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

// kogge stone adder
bitset<32> ALU::ks_ADD(const bitset<32>& a, const bitset<32>& b, bool cin, bool& cout, bool& overflow) const {
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

bitset<32> ALU::ks_SUB(const bitset<32>& a, const bitset<32>& b, bool& cout, bool& overflow) const {
    return ks_ADD(a, logic_NOT(b), true, cout, overflow);
}

bitset<32> ALU::twos_complement(const bitset<32>& a) const {
    bool dummy_cout, dummy_ovf;
    return ks_SUB(bitset<32>(0), a, dummy_cout, dummy_ovf);
}

// booth and non restoring div
pair<bitset<32>, bitset<32>> ALU::algo_Radix4BoothMul64(const bitset<32>& a, const bitset<32>& b) const {
    bitset<32> A = 0;
    bitset<32> Q = b;
    bitset<32> M = a;
    bool q_minus_1 = false, dummy_cout, dummy_ovf;

    for (int i = 0; i < 16; ++i) {
        const uint8_t window = static_cast<uint8_t>(
            (static_cast<uint8_t>(Q[1]) << 2) |
            (static_cast<uint8_t>(Q[0]) << 1) |
            static_cast<uint8_t>(q_minus_1));
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

pair<bitset<32>, bitset<32>> ALU::radix4BoothMul64(const bitset<32>& a, const bitset<32>& b) {
    return ALU().algo_Radix4BoothMul64(a, b);
}

bitset<32> ALU::algo_Radix4BoothMul(const bitset<32>& a, const bitset<32>& b) const {
    return algo_Radix4BoothMul64(a, b).second;
}

pair<bitset<32>, bitset<32>> ALU::algo_NonRestoringDiv(const bitset<32>& dividend, const bitset<32>& divisor) const {
    bitset<32> A = 0, Q = dividend, M = divisor;
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

bool ALU::isZero(const bitset<32>& val) const {
    for (int i = 0; i < 32; ++i) if (val[i]) return false;
    return true;
}

void ALU::updateStandardFlags(const bitset<32>& res, AluResult& out, bool cout, bool ovf_in) const {
    out.zero = isZero(res);
    out.negative = res[31];
    out.carry = cout;
    out.overflow = ovf_in;
}

AluResult ALU::execute(AluOp op, Word a, Word b) const {
    AluResult out{0, false, false, false, false};
    
    bitset<32> wires_a(a), wires_b(b), wires_out(0);
    bool cout = false, ovf = false;

    switch (op) {
        case AluOp::ADD:
            wires_out = ks_ADD(wires_a, wires_b, false, cout, ovf);
            updateStandardFlags(wires_out, out, cout, ovf);
            break;

        case AluOp::SUB:
            wires_out = ks_SUB(wires_a, wires_b, cout, ovf);
            updateStandardFlags(wires_out, out, gate_NOT(cout), ovf); 
            break;

        case AluOp::MUL: 
            wires_out = algo_Radix4BoothMul(wires_a, wires_b);
            updateStandardFlags(wires_out, out, false, false);
            break;

        case AluOp::DIV:
        case AluOp::MOD: {
            if (isZero(wires_b)) {
                // div by zero
                throw RuntimeException{ExceptionCode::DIV_BY_ZERO, 0};
            }
            
            bool sign_a = wires_a[31], sign_b = wires_b[31];
            bitset<32> abs_a = sign_a ? twos_complement(wires_a) : wires_a;
            bitset<32> abs_b = sign_b ? twos_complement(wires_b) : wires_b;

            auto res_pair = algo_NonRestoringDiv(abs_a, abs_b);
            
            if (op == AluOp::DIV) {
                wires_out = (sign_a != sign_b) ? twos_complement(res_pair.first) : res_pair.first;
            } else {
                wires_out = sign_a ? twos_complement(res_pair.second) : res_pair.second;
            }
            updateStandardFlags(wires_out, out, false, false);
            break;
        }

        case AluOp::AND: wires_out = logic_AND(wires_a, wires_b); updateStandardFlags(wires_out, out, false, false); break;
        case AluOp::OR:  wires_out = logic_OR(wires_a,  wires_b); updateStandardFlags(wires_out, out, false, false); break;
        case AluOp::XOR: wires_out = logic_XOR(wires_a, wires_b); updateStandardFlags(wires_out, out, false, false); break;
        
        case AluOp::LSL: wires_out = shift_LSL(wires_a, b & 0x1F); updateStandardFlags(wires_out, out, false, false); break;
        case AluOp::LSR: wires_out = shift_LSR(wires_a, b & 0x1F); updateStandardFlags(wires_out, out, false, false); break;
        case AluOp::ASR: wires_out = shift_ASR(wires_a, b & 0x1F); updateStandardFlags(wires_out, out, false, false); break;

        case AluOp::CMP:
            wires_out = ks_SUB(wires_a, wires_b, cout, ovf);
            updateStandardFlags(wires_out, out, gate_NOT(cout), ovf);
            wires_out.reset(); 
            break;

        case AluOp::NOT: 
            wires_out = logic_NOT(wires_b); 
            updateStandardFlags(wires_out, out, false, false); 
            break;

        case AluOp::PASS_A: 
            wires_out = wires_a; 
            updateStandardFlags(wires_out, out, false, false); 
            break;

        case AluOp::PASS_B: 
            wires_out = wires_b; 
            updateStandardFlags(wires_out, out, false, false); 
            break;

        case AluOp::NOP:
        default:
            wires_out.reset();
            break;
    }

    out.value = static_cast<Word>(wires_out.to_ulong());
    return out;
}