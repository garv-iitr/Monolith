#include "datapath/alu_unit.h"

#include "simulator_core/core/exception.h"

namespace risc201::cu {

AluResult AluUnit::execute(AluOp op, Word a, Word b) {
  AluResult r;
  const Word sign = Word{1} << (kWordBits - 1);

  switch (op) {
    case AluOp::ADD:
    case AluOp::PASS_A:
      if (op == AluOp::ADD) {
        r.value = a + b;
        r.carry = r.value < a;
        r.overflow = (((a ^ b) & sign) == 0) && (((r.value ^ a) & sign) != 0);
      } else {
        r.value = a;
      }
      break;

    case AluOp::PASS_B:
      r.value = b;
      break;

    case AluOp::SUB:
    case AluOp::CMP:
      r.value = a - b;
      r.carry = a < b;
      r.overflow = (((a ^ b) & sign) != 0) && (((r.value ^ a) & sign) != 0);
      break;

    case AluOp::MUL:
      r.value = a * b;
      break;

    case AluOp::DIV:
      if (b == 0) throw RuntimeException{ExceptionCode::DIV_BY_ZERO, 0};
      r.value = static_cast<Word>(static_cast<Sword>(a) / static_cast<Sword>(b));
      break;

    case AluOp::MOD:
      if (b == 0) throw RuntimeException{ExceptionCode::DIV_BY_ZERO, 0};
      r.value = static_cast<Word>(static_cast<Sword>(a) % static_cast<Sword>(b));
      break;

    case AluOp::AND: r.value = a & b; break;
    case AluOp::OR:  r.value = a | b; break;
    case AluOp::NOT: r.value = ~b;   break;
    case AluOp::XOR: r.value = a ^ b; break;

    case AluOp::LSL: r.value = a << (b % kWordBits); break;
    case AluOp::LSR: r.value = a >> (b % kWordBits); break;
    case AluOp::ASR:
      r.value = static_cast<Word>(static_cast<Sword>(a) >> (b % kWordBits));
      break;

    case AluOp::NOP:
    default:
      r.value = 0;
      break;
  }

  r.zero = (r.value == 0);
  r.negative = (r.value & sign) != 0;
  return r;
}

}  // namespace risc201::cu
