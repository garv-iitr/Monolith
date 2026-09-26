#include "alu/alu.h"

#include "simulator_core/core/exception.h"

AluResult SimpleAlu::execute(ALUOp op, Word a, Word b) const {
  AluResult r;
  const Word sign = Word{1} << (kWordBits - 1);

  switch (op) {
    case ALUOp::ADD: {
      r.value = a + b;
      r.carry = r.value < a;
      const bool same_sign = ((a ^ b) & sign) == 0;
      r.overflow = same_sign && ((r.value ^ a) & sign) != 0;
      break;
    }
    case ALUOp::SUB: {
      r.value = a - b;
      r.carry = a < b;
      const bool diff_sign = ((a ^ b) & sign) != 0;
      r.overflow = diff_sign && ((r.value ^ a) & sign) != 0;
      break;
    }
    case ALUOp::MUL:
      r.value = a * b;
      break;
    case ALUOp::DIV:
      if (b == 0) throw RuntimeException{ExceptionCode::DIV_BY_ZERO, 0};
      r.value = static_cast<Word>(static_cast<Sword>(a) / static_cast<Sword>(b));
      break;
    case ALUOp::MOD:
      if (b == 0) throw RuntimeException{ExceptionCode::DIV_BY_ZERO, 0};
      r.value = static_cast<Word>(static_cast<Sword>(a) % static_cast<Sword>(b));
      break;
    case ALUOp::AND:
      r.value = a & b;
      break;
    case ALUOp::OR:
      r.value = a | b;
      break;
    case ALUOp::NOT:
      r.value = ~b;
      break;
    case ALUOp::LSL:
      r.value = a << (b % kWordBits);
      break;
    case ALUOp::LSR:
      r.value = a >> (b % kWordBits);
      break;
    case ALUOp::ASR:
      r.value = static_cast<Word>(static_cast<Sword>(a) >> (b % kWordBits));
      break;
    case ALUOp::CMP:
      r.value = a - b;
      r.carry = a < b;
      r.overflow = (((a ^ b) & sign) != 0) && (((r.value ^ a) & sign) != 0);
      break;
    case ALUOp::PASS_A:
      r.value = a;
      break;
    case ALUOp::NONE:
      r.value = 0;
      break;
  }

  r.zero = (r.value == 0);
  r.negative = (r.value & sign) != 0;
  return r;
}
