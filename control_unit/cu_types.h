#pragma once

#include <cstdint>
#include "common/control_signals.h"
#include "isa_spec/isa.h"

namespace risc201::cu {

// Memory safety firewall check modes
enum class ChkMode : std::uint8_t {
  STACK_BOUNDS = 0,
  MEMORY_ADDR  = 1,
  BNDCHK_INSTR = 2
};

// Source multiplexer for ALU/FPU Operand A
enum class OperandASrc : std::uint8_t {
  RS1 = 0, IMM = 1, PC = 2, ZERO = 3
};

// Source multiplexer for ALU/FPU Operand B
enum class OperandBSrc : std::uint8_t {
  RS2 = 0, IMM = 1, ZERO = 2
};

// Register file writeback data multiplexer
enum class RegWriteSrc : std::uint8_t {
  ALU_OUT  = 0,
  MEM_DATA = 1,
  PC_PLUS4 = 2,
  FPU_OUT  = 3,
  PASS_B   = 4
};

}  // namespace risc201::cu

namespace risc201::pipeline {
using cu::AluOp;
using cu::FpuOp;
using cu::StageLayout;
using cu::ChkMode;
using cu::OperandASrc;
using cu::OperandBSrc;
using cu::RegWriteSrc;
}  // namespace risc201::pipeline
