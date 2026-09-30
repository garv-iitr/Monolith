#include "datapath/fpu_unit.h"

#include <cstring>

namespace risc201::cu {

Word FpuUnit::pack(float f) {
  std::uint32_t bits = 0;
  std::memcpy(&bits, &f, sizeof(bits));
  return static_cast<Word>(bits);
}

float FpuUnit::unpack(Word w) {
  const std::uint32_t bits = static_cast<std::uint32_t>(w);
  float f = 0.0f;
  std::memcpy(&f, &bits, sizeof(f));
  return f;
}

void FpuUnit::tick(const InternalRegisters& cur, const ControlSignals& sig, InternalRegisters& next) {
  if (sig.fpu_op == FpuOp::NOP) return;

  const float a = unpack(cur.fp_a);
  const float b = unpack(cur.fp_b);

  switch (sig.fpu_op) {
    case FpuOp::READ:
      next.fp_val = fprs[cur.fp_src % NUM_FLOAT_REGISTERS];
      break;

    case FpuOp::WRITE:
      fprs[cur.fp_src % NUM_FLOAT_REGISTERS] = cur.fp_data;
      break;

    case FpuOp::FADD: next.fpu_result = pack(a + b); break;
    case FpuOp::FSUB: next.fpu_result = pack(a - b); break;
    case FpuOp::FMUL: next.fpu_result = pack(a * b); break;
    case FpuOp::FDIV: next.fpu_result = pack(a / b); break;
    case FpuOp::FMOV: next.fpu_result = cur.fp_b; break;

    case FpuOp::FCMP:
      next.fcc = (a < b) ? true : (a > b) ? false : false;
      if (sig.fpu_set_fcc) {
        next.flags.Z = (a == b);
        next.flags.N = (a < b);
        next.flags.C = false;
        next.flags.V = false;
        next.flags_E = (a == b);
        next.flags_GT = (a > b);
      }
      break;

    case FpuOp::NOP:
    default:
      break;
  }
}

}  // namespace risc201::cu
