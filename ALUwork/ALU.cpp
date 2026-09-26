#include "alu.h"
using namespace std;
ALU::ALU() : result(0) { flags.reset(); }
uint32_t ALU::getResult() const { return result; }
ALUFlags ALU::getFlags() const { return flags; }

// ==========================================
// 1. FUNDAMENTAL GATES (No C++ Bitwise Ops)
// ==========================================
bool ALU::gate_NOT(bool a) {
    if (a == true) return false;
    return true;
}

bool ALU::gate_NAND(bool a, bool b) {
    if (a == true) {
        if (b == true) return false;
        return true;
    }
    return true;
}

bool ALU::gate_NOR(bool a, bool b) {
    if (a == true) return false;
    if (b == true) return false;
    return true;
}

bool ALU::gate_AND(bool a, bool b) { return gate_NOT(gate_NAND(a, b)); }
bool ALU::gate_OR(bool a, bool b)  { return gate_NOT(gate_NOR(a, b)); }
bool ALU::gate_XOR(bool a, bool b) {
    return gate_OR(gate_AND(a, gate_NOT(b)), gate_AND(gate_NOT(a), b));
}

// ==========================================
// 2. 32-BIT LOGIC PATHS
// ==========================================
bitset<32> ALU::logic_AND(const bitset<32>& a, const bitset<32>& b) {
    bitset<32> res;
    for (int i = 0; i < 32; ++i) res[i] = gate_AND(a[i], b[i]);
    return res;
}

bitset<32> ALU::logic_OR(const bitset<32>& a, const bitset<32>& b) {
    bitset<32> res;
    for (int i = 0; i < 32; ++i) res[i] = gate_OR(a[i], b[i]);
    return res;
}

bitset<32> ALU::logic_XOR(const bitset<32>& a, const bitset<32>& b) {
    bitset<32> res;
    for (int i = 0; i < 32; ++i) res[i] = gate_XOR(a[i], b[i]);
    return res;
}

bitset<32> ALU::logic_NOT(const bitset<32>& a) {
    bitset<32> res;
    for (int i = 0; i < 32; ++i) res[i] = gate_NOT(a[i]);
    return res;
}

// ==========================================
// 3. HARDWARE SHIFTERS (Looping wire arrays)
// ==========================================
bitset<32> ALU::shift_LSL(const bitset<32>& a, uint32_t shift_amt) {
    bitset<32> res;
    for (int i = 0; i < 32; ++i) {
        if (i >= shift_amt) res[i] = a[i - shift_amt];
        else res[i] = false;
    }
    return res;
}

bitset<32> ALU::shift_LSR(const bitset<32>& a, uint32_t shift_amt) {
    bitset<32> res;
    for (int i = 0; i < 32; ++i) {
        if (i + shift_amt < 32) res[i] = a[i + shift_amt];
        else res[i] = false;
    }
    return res;
}

bitset<32> ALU::shift_ASR(const bitset<32>& a, uint32_t shift_amt) {
    bitset<32> res;
    bool sign_bit = a[31];
    for (int i = 0; i < 32; ++i) {
        if (i + shift_amt < 32) res[i] = a[i + shift_amt];
        else res[i] = sign_bit; // Propagate sign
    }
    return res;
}

// ==========================================
// 4. ADDER & SUBTRACTOR (Carry Lookahead)
// ==========================================
void ALU::cla_4bit_block(bool a[4], bool b[4], bool cin, bool s[4], bool& cout) {
    bool g[4], p[4];
    
    // Generate (G) and Propagate (P)
    for(int i = 0; i < 4; i++) {
        g[i] = gate_AND(a[i], b[i]);
        p[i] = gate_XOR(a[i], b[i]);
    }

    // Lookahead Carry Logic
    bool c1 = gate_OR(g[0], gate_AND(p[0], cin));
    
    bool c2_term1 = gate_AND(p[1], g[0]);
    bool c2_term2 = gate_AND(p[1], gate_AND(p[0], cin));
    bool c2 = gate_OR(g[1], gate_OR(c2_term1, c2_term2));

    bool c3_term1 = gate_AND(p[2], g[1]);
    bool c3_term2 = gate_AND(p[2], gate_AND(p[1], g[0]));
    bool c3_term3 = gate_AND(p[2], gate_AND(p[1], gate_AND(p[0], cin)));
    bool c3 = gate_OR(g[2], gate_OR(c3_term1, gate_OR(c3_term2, c3_term3)));

    bool c4_term1 = gate_AND(p[3], g[2]);
    bool c4_term2 = gate_AND(p[3], gate_AND(p[2], g[1]));
    bool c4_term3 = gate_AND(p[3], gate_AND(p[2], gate_AND(p[1], g[0])));
    bool c4_term4 = gate_AND(p[3], gate_AND(p[2], gate_AND(p[1], gate_AND(p[0], cin))));
    cout = gate_OR(g[3], gate_OR(c4_term1, gate_OR(c4_term2, gate_OR(c4_term3, c4_term4))));

    // Sum Logic
    s[0] = gate_XOR(p[0], cin);
    s[1] = gate_XOR(p[1], c1);
    s[2] = gate_XOR(p[2], c2);
    s[3] = gate_XOR(p[3], c3);
}

bitset<32> ALU::cla_ADD(const bitset<32>& a, const bitset<32>& b, bool cin, bool& cout, bool& overflow) {
    bitset<32> sum;
    bool current_carry = cin;

    // Chain 8 blocks of 4-bit CLAs to form the 32-bit adder
    for (int i = 0; i < 32; i += 4) {
        bool a_block[4] = {a[i], a[i+1], a[i+2], a[i+3]};
        bool b_block[4] = {b[i], b[i+1], b[i+2], b[i+3]};
        bool s_block[4];
        bool next_carry;
        
        cla_4bit_block(a_block, b_block, current_carry, s_block, next_carry);
        
        sum[i]   = s_block[0];
        sum[i+1] = s_block[1];
        sum[i+2] = s_block[2];
        sum[i+3] = s_block[3];
        
        // Overflow flag calculation at the MSB block (bits 28-31)
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

bitset<32> ALU::cla_SUB(const bitset<32>& a, const bitset<32>& b, bool& cout, bool& overflow) {
    // A - B = A + (~B) + 1
    bitset<32> not_b = logic_NOT(b);
    return cla_ADD(a, not_b, true, cout, overflow);
}

// ==========================================
// 5. HARDWARE ALGORITHMS (Using CLA Engine)
// ==========================================

// Radix-2 Booth's Algorithm
bitset<32> ALU::algo_BoothMul(const bitset<32>& a, const bitset<32>& b) {
    bitset<32> A; 
    bitset<32> Q = b; 
    bitset<32> M = a; 
    bool q_minus_1 = false;
    
    bool dummy_cout, dummy_ovf;

    for (int i = 0; i < 32; ++i) {
        bool q0 = Q[0];
        if (q0 == true && q_minus_1 == false) {
            A = cla_SUB(A, M, dummy_cout, dummy_ovf);
        } else if (q0 == false && q_minus_1 == true) {
            A = cla_ADD(A, M, false, dummy_cout, dummy_ovf);
        }

        q_minus_1 = Q[0];
        bool a_lsb = A[0];
        Q = shift_LSR(Q, 1);
        Q[31] = a_lsb; 
        
        A = shift_ASR(A, 1); 
    }
    return Q; 
}

// Unsigned Restoring Division 
pair<bitset<32>, bitset<32>> ALU::algo_RestoringDiv(const bitset<32>& dividend, const bitset<32>& divisor) {
    bitset<32> A; 
    bitset<32> Q = dividend; 
    bitset<32> M = divisor;
    
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

// ==========================================
// 6. PIPELINE EXECUTION ENGINE
// ==========================================
bool ALU::isZero(const bitset<32>& val) {
    for (int i = 0; i < 32; ++i) {
        if (val[i] == true) return false;
    }
    return true;
}

void ALU::updateStandardFlags(const bitset<32>& res) {
    flags.zero = isZero(res);
    flags.negative = res[31];
}

uint32_t ALU::execute(Opcode op, uint32_t a, uint32_t b) {
    flags.reset();
    
    bitset<32> wires_a(a);
    bitset<32> wires_b(b);
    bitset<32> wires_out;

    bool cout = false;
    bool ovf = false;

    switch (op) {
        case OP_ADD:
        case OP_LD:
        case OP_ST:
        case OP_CALL:
        case OP_B:
        case OP_BEQ:
        case OP_BGT:
            wires_out = cla_ADD(wires_a, wires_b, false, cout, ovf);
            flags.carry = cout;
            flags.overflow = ovf;
            updateStandardFlags(wires_out);
            break;

        case OP_SUB:
        case OP_CMP:
            wires_out = cla_SUB(wires_a, wires_b, cout, ovf);
            flags.carry = gate_NOT(cout); 
            flags.overflow = ovf;
            updateStandardFlags(wires_out);
            break;

        case OP_MUL:
            wires_out = algo_BoothMul(wires_a, wires_b);
            updateStandardFlags(wires_out);
            break;

        case OP_DIV: 
        case OP_MOD: {
            if (isZero(wires_b)) {
                flags.overflow = true;
                throw runtime_error("ALU Exception: Division by Zero");
            }
            
            // 1. Hardware Absolute Value (Two's complement if negative)
            bitset<32> zero_val; 
            bitset<32> abs_a = wires_a[31] ? cla_SUB(zero_val, wires_a, cout, ovf) : wires_a;
            bitset<32> abs_b = wires_b[31] ? cla_SUB(zero_val, wires_b, cout, ovf) : wires_b;

            // 2. Run Unsigned Restoring Division
            auto div_res = algo_RestoringDiv(abs_a, abs_b);
            bitset<32> quotient = div_res.first;
            bitset<32> remainder = div_res.second;

            // 3. Apply signs via hardware negation
            bool q_sign = gate_XOR(wires_a[31], wires_b[31]);
            if (q_sign) quotient = cla_SUB(zero_val, quotient, cout, ovf);
            
            bool r_sign = wires_a[31];
            if (r_sign) remainder = cla_SUB(zero_val, remainder, cout, ovf);

            // 4. Output routing
            if (op == OP_DIV) {
                wires_out = quotient;
            } else {
                wires_out = remainder;
            }
            
            updateStandardFlags(wires_out);
            break;
        }

        case OP_AND:
            wires_out = logic_AND(wires_a, wires_b);
            updateStandardFlags(wires_out);
            break;
        case OP_OR:
            wires_out = logic_OR(wires_a, wires_b);
            updateStandardFlags(wires_out);
            break;
        case OP_XOR:
            wires_out = logic_XOR(wires_a, wires_b);
            updateStandardFlags(wires_out);
            break;
        case OP_NOT:
            wires_out = logic_NOT(wires_a);
            updateStandardFlags(wires_out);
            break;

        case OP_LSL: {
            uint32_t shift_amt = b & 0x1F;
            wires_out = shift_LSL(wires_a, shift_amt);
            updateStandardFlags(wires_out);
            if (shift_amt > 0 && shift_amt <= 32) flags.carry = wires_a[32 - shift_amt];
            break;
        }
        case OP_LSR: {
            uint32_t shift_amt = b & 0x1F;
            wires_out = shift_LSR(wires_a, shift_amt);
            updateStandardFlags(wires_out);
            if (shift_amt > 0 && shift_amt <= 32) flags.carry = wires_a[shift_amt - 1];
            break;
        }
        case OP_ASR: {
            uint32_t shift_amt = b & 0x1F;
            wires_out = shift_ASR(wires_a, shift_amt);
            updateStandardFlags(wires_out);
            if (shift_amt > 0 && shift_amt <= 32) flags.carry = wires_a[shift_amt - 1];
            break;
        }

        case OP_MOV:
            wires_out = wires_a;
            updateStandardFlags(wires_out);
            break;

        case OP_NOP:
        case OP_RET:
        default:
            wires_out.reset();
            break;
    }

    result = static_cast<uint32_t>(wires_out.to_ulong());
    return result;
}