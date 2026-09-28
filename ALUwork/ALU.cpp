#include "alu.h"

using namespace std;

// ------------------------------------------
// 1. FUNDAMENTAL GATES
// ------------------------------------------
bool ALU::gate_NOT(bool a) const { return (a == true) ? false : true; }
bool ALU::gate_NAND(bool a, bool b) const { return (a == true && b == true) ? false : true; }
bool ALU::gate_NOR(bool a, bool b) const { return (a == false && b == false) ? true : false; }
bool ALU::gate_AND(bool a, bool b) const { return gate_NOT(gate_NAND(a, b)); }
bool ALU::gate_OR(bool a, bool b)  const { return gate_NOT(gate_NOR(a, b)); }
bool ALU::gate_XOR(bool a, bool b) const {
    return gate_OR(gate_AND(a, gate_NOT(b)), gate_AND(gate_NOT(a), b));
}

// ------------------------------------------
// 2. LOGIC PATHS
// ------------------------------------------
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

// ------------------------------------------
// 3. SHIFTERS
// ------------------------------------------
bitset<32> ALU::shift_LSL(const bitset<32>& a, uint32_t shift_amt) const {
    bitset<32> res;
    for (int i = 0; i < 32; ++i) res[i] = (i >= shift_amt) ? a[i - shift_amt] : false;
    return res;
}

bitset<32> ALU::shift_LSR(const bitset<32>& a, uint32_t shift_amt) const {
    bitset<32> res;
    for (int i = 0; i < 32; ++i) res[i] = (i + shift_amt < 32) ? a[i + shift_amt] : false;
    return res;
}

bitset<32> ALU::shift_ASR(const bitset<32>& a, uint32_t shift_amt) const {
    bitset<32> res;
    bool sign_bit = a[31];
    for (int i = 0; i < 32; ++i) res[i] = (i + shift_amt < 32) ? a[i + shift_amt] : sign_bit;
    return res;
}

// ------------------------------------------
// 4. BLOCK CARRY LOOKAHEAD (ADD/SUB)
// ------------------------------------------
void ALU::cla_4bit_block(const bool a[4], const bool b[4], bool cin, bool s[4], bool& cout) const {
    bool g[4], p[4];
    for(int i = 0; i < 4; i++) {
        g[i] = gate_AND(a[i], b[i]);
        p[i] = gate_XOR(a[i], b[i]);
    }

    bool c1 = gate_OR(g[0], gate_AND(p[0], cin));
    bool c2 = gate_OR(g[1], gate_OR(gate_AND(p[1], g[0]), gate_AND(p[1], gate_AND(p[0], cin))));
    bool c3 = gate_OR(g[2], gate_OR(gate_AND(p[2], g[1]), gate_OR(gate_AND(p[2], gate_AND(p[1], g[0])), gate_AND(p[2], gate_AND(p[1], gate_AND(p[0], cin))))));
    
    bool c4_t1 = gate_AND(p[3], g[2]), c4_t2 = gate_AND(p[3], gate_AND(p[2], g[1]));
    bool c4_t3 = gate_AND(p[3], gate_AND(p[2], gate_AND(p[1], g[0])));
    bool c4_t4 = gate_AND(p[3], gate_AND(p[2], gate_AND(p[1], gate_AND(p[0], cin))));
    cout = gate_OR(g[3], gate_OR(c4_t1, gate_OR(c4_t2, gate_OR(c4_t3, c4_t4))));

    s[0] = gate_XOR(p[0], cin);
    s[1] = gate_XOR(p[1], c1);
    s[2] = gate_XOR(p[2], c2);
    s[3] = gate_XOR(p[3], c3);
}

bitset<32> ALU::cla_ADD(const bitset<32>& a, const bitset<32>& b, bool cin, bool& cout, bool& overflow) const {
    bitset<32> sum;
    bool current_carry = cin;

    for (int i = 0; i < 32; i += 4) {
        bool a_block[4] = {a[i], a[i+1], a[i+2], a[i+3]};
        bool b_block[4] = {b[i], b[i+1], b[i+2], b[i+3]};
        bool s_block[4], next_carry;
        
        cla_4bit_block(a_block, b_block, current_carry, s_block, next_carry);
        
        sum[i] = s_block[0]; sum[i+1] = s_block[1]; 
        sum[i+2] = s_block[2]; sum[i+3] = s_block[3];
        
        if (i == 28) {
            bool p_28 = gate_XOR(a_block[0], b_block[0]), g_28 = gate_AND(a_block[0], b_block[0]);
            bool p_29 = gate_XOR(a_block[1], b_block[1]), g_29 = gate_AND(a_block[1], b_block[1]);
            bool p_30 = gate_XOR(a_block[2], b_block[2]), g_30 = gate_AND(a_block[2], b_block[2]);

            bool c1 = gate_OR(g_28, gate_AND(p_28, current_carry));
            bool c2 = gate_OR(g_29, gate_OR(gate_AND(p_29, g_28), gate_AND(p_29, gate_AND(p_28, current_carry))));
            bool c3 = gate_OR(g_30, gate_OR(gate_AND(p_30, g_29), gate_OR(gate_AND(p_30, gate_AND(p_29, g_28)), gate_AND(p_30, gate_AND(p_29, gate_AND(p_28, current_carry))))));
            
            overflow = gate_XOR(c3, next_carry); 
        }
        current_carry = next_carry;
    }
    cout = current_carry;
    return sum;
}

bitset<32> ALU::cla_SUB(const bitset<32>& a, const bitset<32>& b, bool& cout, bool& overflow) const {
    return cla_ADD(a, logic_NOT(b), true, cout, overflow);
}

bitset<32> ALU::twos_complement(const bitset<32>& a) const {
    bool dummy_cout, dummy_ovf;
    bitset<32> zero_bits;
    return cla_SUB(zero_bits, a, dummy_cout, dummy_ovf);
}

// ------------------------------------------
// 5. ALGORITHMS (Booth & Restoring)
// ------------------------------------------
bitset<32> ALU::algo_BoothMul(const bitset<32>& a, const bitset<32>& b) const {
    bitset<32> A, Q = b, M = a; 
    bool q_minus_1 = false, dummy_cout, dummy_ovf;

    for (int i = 0; i < 32; ++i) {
        if (Q[0] == true && q_minus_1 == false) A = cla_SUB(A, M, dummy_cout, dummy_ovf);
        else if (Q[0] == false && q_minus_1 == true) A = cla_ADD(A, M, false, dummy_cout, dummy_ovf);

        q_minus_1 = Q[0];
        bool a_lsb = A[0];
        Q = shift_LSR(Q, 1);
        Q[31] = a_lsb; 
        A = shift_ASR(A, 1); 
    }
    return Q; 
}

pair<bitset<32>, bitset<32>> ALU::algo_UnsignedRestoringDiv(const bitset<32>& dividend, const bitset<32>& divisor) const {
    bitset<32> A, Q = dividend, M = divisor;
    bool dummy_cout, dummy_ovf;

    for (int i = 0; i < 32; ++i) {
        bool q_msb = Q[31];
        A = shift_LSL(A, 1);
        A[0] = q_msb;
        Q = shift_LSL(Q, 1);
        
        A = cla_SUB(A, M, dummy_cout, dummy_ovf);
        
        if (A[31] == true) { 
            A = cla_ADD(A, M, false, dummy_cout, dummy_ovf);
            Q[0] = false;
        } else {
            Q[0] = true;
        }
    }
    return {Q, A};
}

// ------------------------------------------
// 6. PIPELINE ENGINE 
// ------------------------------------------
bool ALU::isZero(const bitset<32>& val) const {
    for (int i = 0; i < 32; ++i) {
        if (val[i] == true) return false;
    }
    return true;
}

void ALU::updateStandardFlags(const bitset<32>& res, ALUFlags& f, bool ovf_in) const {
    f.zero = isZero(res);
    f.negative = res[31];
    f.overflow = ovf_in;
    
    f.E = f.zero; 
    f.GT = (f.negative == f.overflow) && !f.zero; 
}

AluResult ALU::execute(ALUOp op, uint32_t a, uint32_t b) const {
    AluResult output;
    output.flags.reset();
    
    bitset<32> wires_a(a);
    bitset<32> wires_b(b);
    bitset<32> wires_out;

    bool cout = false, ovf = false;

    switch (op) {
        case ALUOp::ADD:
            wires_out = cla_ADD(wires_a, wires_b, false, cout, ovf);
            output.flags.carry = cout;
            updateStandardFlags(wires_out, output.flags, ovf);
            break;

        case ALUOp::SUB:
        case ALUOp::CMP:
            wires_out = cla_SUB(wires_a, wires_b, cout, ovf);
            output.flags.carry = gate_NOT(cout); 
            updateStandardFlags(wires_out, output.flags, ovf);
            break;

        case ALUOp::MUL: 
            wires_out = algo_BoothMul(wires_a, wires_b);
            updateStandardFlags(wires_out, output.flags, false);
            break;

        case ALUOp::DIV:
        case ALUOp::MOD: {
            if (isZero(wires_b)) throw runtime_error("ALU Exception: Div by Zero");
            
            bool sign_a = wires_a[31], sign_b = wires_b[31];
            bitset<32> abs_a = sign_a ? twos_complement(wires_a) : wires_a;
            bitset<32> abs_b = sign_b ? twos_complement(wires_b) : wires_b;

            auto result_pair = algo_UnsignedRestoringDiv(abs_a, abs_b);
            
            if (op == ALUOp::DIV) {
                wires_out = (sign_a != sign_b) ? twos_complement(result_pair.first) : result_pair.first;
            } else {
                wires_out = sign_a ? twos_complement(result_pair.second) : result_pair.second;
            }
            updateStandardFlags(wires_out, output.flags, false);
            break;
        }

        case ALUOp::AND: wires_out = logic_AND(wires_a, wires_b); updateStandardFlags(wires_out, output.flags, false); break;
        case ALUOp::OR:  wires_out = logic_OR(wires_a, wires_b);  updateStandardFlags(wires_out, output.flags, false); break;
        case ALUOp::XOR: wires_out = logic_XOR(wires_a, wires_b); updateStandardFlags(wires_out, output.flags, false); break;
        
        case ALUOp::NOT: wires_out = logic_NOT(wires_b); updateStandardFlags(wires_out, output.flags, false); break;

        case ALUOp::LSL: wires_out = shift_LSL(wires_a, b & 0x1F); updateStandardFlags(wires_out, output.flags, false); break;
        case ALUOp::LSR: wires_out = shift_LSR(wires_a, b & 0x1F); updateStandardFlags(wires_out, output.flags, false); break;
        case ALUOp::ASR: wires_out = shift_ASR(wires_a, b & 0x1F); updateStandardFlags(wires_out, output.flags, false); break;

        case ALUOp::PASS_B: wires_out = wires_b; updateStandardFlags(wires_out, output.flags, false); break;

        case ALUOp::NONE:
        default:
            wires_out.reset();
            break;
    }

    output.result = static_cast<uint32_t>(wires_out.to_ulong());
    return output;
}