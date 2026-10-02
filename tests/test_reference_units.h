#ifndef RISC201_TEST_REFERENCE_UNITS_H
#define RISC201_TEST_REFERENCE_UNITS_H

// ============================================================================
// Testbench-only reference models of the ALU, FPU and memory-check units.
//
// These let the control-unit test programs execute end to end before Atharva's
// (alu/) and Paraas's (memory_safety/) modules are integrated.  They implement
// exactly the args contract in `control unit/common/control_signals.h`, so
// swapping them for the real units is a drop-in change.
//
// This is the *oracle*, not the deliverable: any mismatch between these models
// and the real units is caught the moment the real units are wired in, because
// the programs below check exact architectural results.
// ============================================================================

#include <array>
#include <cstddef>
#include <cstring>

#include "common/control_signals.h"
#include "common/internal_registers.h"
#include "isa_spec/isa.h"
#include "simulator_core/core/exception.h"

namespace risc201::testbench {

using risc201::cu::AluOp;
using risc201::cu::ControlSignals;
using risc201::cu::FpuOp;
using risc201::cu::InternalRegisters;
using risc201::cu::MemChkOp;

// ----------------------------------------------------------------------------
// ALU reference (Atharva's contract: A, B -> aluResult + Z/N/C/V)
// ----------------------------------------------------------------------------
struct RefAluResult {
  Word value = 0;
  bool zero = false;
  bool negative = false;
  bool carry = false;
  bool overflow = false;
};

struct RefAlu {
  static RefAluResult execute(AluOp op, Word a, Word b) {
    RefAluResult r;
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

  void tick(const InternalRegisters& cur, const ControlSignals& sig,
            InternalRegisters& next) const {
    if (sig.alu_op == AluOp::NOP) return;

    const RefAluResult r = execute(sig.alu_op, cur.A, cur.B);
    next.alu_result = r.value;

    if (sig.set_flags) {
      next.flags.Z = r.zero;
      next.flags.N = r.negative;
      next.flags.C = r.carry;
      next.flags.V = r.overflow;
      next.flags_E = r.zero;
      next.flags_GT = !r.negative && !r.zero;
    }
  }
};

// ----------------------------------------------------------------------------
// FPU reference (Atharva's contract: fpSrc/fpData/fpVal + fpA/fpB -> fpuResult)
// ----------------------------------------------------------------------------
struct RefFpu {
  std::array<Word, NUM_FLOAT_REGISTERS> fprs{};

  void reset() { fprs.fill(Word{0}); }

  static Word pack(float f) {
    std::uint32_t bits = 0;
    std::memcpy(&bits, &f, sizeof(bits));
    return static_cast<Word>(bits);
  }

  static float unpack(Word w) {
    const std::uint32_t bits = static_cast<std::uint32_t>(w);
    float f = 0.0f;
    std::memcpy(&f, &bits, sizeof(f));
    return f;
  }

  void tick(const InternalRegisters& cur, const ControlSignals& sig,
            InternalRegisters& next) {
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
        next.fcc = (a < b);
        if (sig.fpu_set_fcc) {
          // Mirror the float compare into the architectural flags so the
          // branch microcode can reuse flags.E / flags.GT / flags.N.
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
};

// ----------------------------------------------------------------------------
// Memory-check reference (Paraas's contract: memChkAddr -> memChkStatus)
// ----------------------------------------------------------------------------
struct RefMemChk {
  bool trap_on_fail = true;

  static bool present(Word byte_addr, std::size_t words) {
    if (byte_addr % 4 != 0) return false;
    return static_cast<std::size_t>(byte_addr / 4) < words;
  }

  void tick(const InternalRegisters& cur, const ControlSignals& sig,
            InternalRegisters& next, std::size_t words) const {
    (void)cur;
    if (sig.memchk_op != MemChkOp::CHECK) return;
    next.memchk_status = present(next.memchk_addr, words);
    if (!next.memchk_status && trap_on_fail) {
      throw RuntimeException{ExceptionCode::BAD_MEM_ACCESS,
                             static_cast<std::uint64_t>(next.memchk_addr)};
    }
  }
};

}  // namespace risc201::testbench

#endif  // RISC201_TEST_REFERENCE_UNITS_H
