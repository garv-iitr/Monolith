#pragma once

#include "alu/alu.h"
#include "common/control_signals.h"
#include "common/internal_registers.h"
#include "isa_spec/isa.h"

namespace risc201 {

// what the microcode drives, reads A/B and writes out result + flags
class AluUnit {
 public:
  virtual ~AluUnit() = default;
  virtual void tick(const risc201::cu::InternalRegisters& cur,
                    const risc201::cu::ControlSignals& sig,
                    risc201::cu::InternalRegisters& next) = 0;
};

// atharvas hardware alu, wraps the gate level stuff in alu/
class HardwareAlu : public AluUnit {
 public:
  void tick(const risc201::cu::InternalRegisters& cur,
            const risc201::cu::ControlSignals& sig,
            risc201::cu::InternalRegisters& next) override {
    if (sig.alu_op == risc201::cu::AluOp::NOP) return;
    const AluResult r = alu_.execute(sig.alu_op, cur.A, cur.B);
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

 private:
  ALU alu_;
};

}  // namespace risc201
