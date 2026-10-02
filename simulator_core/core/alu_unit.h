#pragma once

#include "common/control_signals.h"
#include "common/internal_registers.h"
#include "isa_spec/isa.h"
#include "simulator_core/core/exception.h"

namespace risc201 {

// ALU unit contract. The microcode drives this unit through the args field:
// it reads cur.A / cur.B and writes next.alu_result, and (when sig.set_flags is
// set, i.e. after CMP) the Z/N/C/V flags plus the E/GT branch mirrors.
// Atharva's real ALU (alu/) implements this same contract; ReferenceAlu is the
// temporary model that lets the engine run end to end before it lands.
class AluUnit {
 public:
  virtual ~AluUnit() = default;
  virtual void tick(const risc201::cu::InternalRegisters& cur,
                    const risc201::cu::ControlSignals& sig,
                    risc201::cu::InternalRegisters& next) = 0;
};

class ReferenceAlu : public AluUnit {
 public:
  void tick(const risc201::cu::InternalRegisters& cur,
            const risc201::cu::ControlSignals& sig,
            risc201::cu::InternalRegisters& next) override {
    const risc201::cu::AluOp op = sig.alu_op;
    if (op == risc201::cu::AluOp::NOP) return;

    const Word a = cur.A;
    const Word b = cur.B;
    const Word sign = Word{1} << (kWordBits - 1);

    Word value = 0;
    bool zero = false;
    bool negative = false;
    bool carry = false;
    bool overflow = false;

    switch (op) {
      case risc201::cu::AluOp::ADD:
        value = a + b;
        carry = value < a;
        overflow = (((a ^ b) & sign) == 0) && (((value ^ a) & sign) != 0);
        break;

      case risc201::cu::AluOp::SUB:
      case risc201::cu::AluOp::CMP:
        value = a - b;
        carry = a < b;
        overflow = (((a ^ b) & sign) != 0) && (((value ^ a) & sign) != 0);
        break;

      case risc201::cu::AluOp::MUL:
        value = a * b;
        break;

      case risc201::cu::AluOp::DIV:
        if (b == 0) throw RuntimeException{ExceptionCode::DIV_BY_ZERO, 0};
        value = static_cast<Word>(static_cast<Sword>(a) / static_cast<Sword>(b));
        break;

      case risc201::cu::AluOp::MOD:
        if (b == 0) throw RuntimeException{ExceptionCode::DIV_BY_ZERO, 0};
        value = static_cast<Word>(static_cast<Sword>(a) % static_cast<Sword>(b));
        break;

      case risc201::cu::AluOp::AND: value = a & b; break;
      case risc201::cu::AluOp::OR:  value = a | b; break;
      case risc201::cu::AluOp::NOT: value = ~b; break;
      case risc201::cu::AluOp::XOR: value = a ^ b; break;

      case risc201::cu::AluOp::LSL: value = a << (b % kWordBits); break;
      case risc201::cu::AluOp::LSR: value = a >> (b % kWordBits); break;
      case risc201::cu::AluOp::ASR:
        value = static_cast<Word>(static_cast<Sword>(a) >> (b % kWordBits));
        break;

      case risc201::cu::AluOp::PASS_A: value = a; break;
      case risc201::cu::AluOp::PASS_B: value = b; break;

      case risc201::cu::AluOp::NOP:
      default:
        value = 0;
        break;
    }

    next.alu_result = value;
    zero = (value == 0);
    negative = (value & sign) != 0;

    if (sig.set_flags) {
      next.flags.Z = zero;
      next.flags.N = negative;
      next.flags.C = carry;
      next.flags.V = overflow;
      next.flags_E = zero;
      next.flags_GT = !negative && !zero;
    }
  }
};

}  // namespace risc201
