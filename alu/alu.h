#pragma once

#include "control_signals.h"
#include "isa_spec/isa.h"

struct AluResult {
  Word value = 0;
  bool zero = false;
  bool negative = false;
  bool carry = false;
  bool overflow = false;
};

class Alu {
 public:
  virtual ~Alu() = default;
  virtual AluResult execute(ALUOp op, Word a, Word b) const = 0;
};

class SimpleAlu : public Alu {
 public:
  AluResult execute(ALUOp op, Word a, Word b) const override;
};
