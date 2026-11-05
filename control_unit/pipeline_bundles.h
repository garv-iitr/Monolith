#pragma once

#include <cstdint>
#include "control_unit/cu_types.h"
#include "isa_spec/isa.h"

namespace risc201::pipeline {

using cu::AluOp;
using cu::FpuOp;
using cu::ChkMode;
using cu::OperandASrc;
using cu::OperandBSrc;
using cu::RegWriteSrc;

// -----------------------------------------------------------------------------
// Stage-Decoupled Hardware Control Bundles
// -----------------------------------------------------------------------------

struct ExControl {
  cu::AluOp alu_op = cu::AluOp::NOP;
  OperandASrc operand_a_src = OperandASrc::RS1;
  OperandBSrc operand_b_src = OperandBSrc::RS2;
  bool set_flags = false;
  Cond branch_cond = Cond::EQ;
  bool is_branch = false;
  bool is_call = false;
  bool is_ret = false;
  bool is_fpu = false;
  cu::FpuOp fpu_op = cu::FpuOp::NOP;
};

struct ChkControl {
  bool memchk_enable = false;
  ChkMode chk_mode = ChkMode::STACK_BOUNDS;
};

struct MaControl {
  bool mem_read = false;
  bool mem_write = false;
  bool is_fpu = false;
};

struct WbControl {
  bool reg_write_en = false;
  RegWriteSrc reg_write_src = RegWriteSrc::ALU_OUT;
  std::uint8_t dest_reg = 0;
  bool is_fpu = false;
};

// Master Pipeline Control Bundle carrying decoded control signals through stages
struct PipelineControlBundle {
  ExControl ex;
  ChkControl chk;
  MaControl ma;
  WbControl wb;

  std::uint8_t src_reg_a = 0;
  std::uint8_t src_reg_b = 0;
  bool src_reg_a_is_fpu = false;
  bool src_reg_b_is_fpu = false;
  Word imm = 0;

  bool is_multi_uop = false;
  bool is_last_uop = true;
  bool freeze_if_of = false;
  bool halt = false;
  bool bubble = false;
  std::uint16_t micro_pc = 0;
};

// -----------------------------------------------------------------------------
// Synchronous Inter-Stage Latches
// -----------------------------------------------------------------------------

// Latch between IF (Instruction Fetch) and OF (Operand Fetch / Decode)
struct IfOfLatch {
  Word pc = 0;
  Word raw = 0;
  bool valid = false;
  bool bubble = false;

  void flush() {
    pc = 0;
    raw = 0;
    valid = false;
    bubble = true;
  }
};

// Latch between OF (Operand Fetch / Decode) and EX (Execute)
struct OfExLatch {
  Word pc = 0;
  Word raw = 0;
  Opcode opcode = Opcode::NOP;
  std::uint8_t rd = 0;
  std::uint8_t rs1 = 0;
  std::uint8_t rs2 = 0;
  bool rs1_is_fpu = false;
  bool rs2_is_fpu = false;
  bool dest_is_fpu = false;
  Word rs1_val = 0;
  Word rs2_val = 0;
  Word imm32 = 0;
  Word branch_target = 0;
  PipelineControlBundle control{};
  bool valid = false;
  bool bubble = false;

  void flush() {
    raw = 0;
    opcode = Opcode::NOP;
    control = PipelineControlBundle{};
    control.bubble = true;
    valid = false;
    bubble = true;
  }
};

// Latch after EX stage:
// In 4-Stage: serves as EX/MA_RW latch (merged memory access and writeback).
// In 6-Stage: serves as EX/CHK latch (pre-firewall latch).
struct ExStageLatch {
  Word pc = 0;
  Word alu_result = 0;
  Word store_val = 0;
  Word effective_addr = 0;
  std::uint8_t rd = 0;
  bool is_fpu_reg = false;
  PipelineControlBundle control{};
  bool valid = false;
  bool bubble = false;

  void flush() {
    valid = false;
    bubble = true;
    control = PipelineControlBundle{};
  }
};
using ExChkLatch = ExStageLatch;

// Latch between MEMCHK (Firewall Verification) and MA (Memory Access) [6-Stage only]
struct ChkMaLatch {
  Word pc = 0;
  Word verified_addr = 0;
  Word store_val = 0;
  Word alu_result = 0;
  std::uint8_t rd = 0;
  bool is_fpu_reg = false;
  PipelineControlBundle control{};
  bool valid = false;
  bool bubble = false;

  void flush() {
    valid = false;
    bubble = true;
    control = PipelineControlBundle{};
  }
};

// Latch between MA (Memory Access) and RW (Register Writeback) [6-Stage only]
struct MaRwLatch {
  Word pc = 0;
  Word wb_data = 0;
  std::uint8_t rd = 0;
  bool is_fpu_reg = false;
  PipelineControlBundle control{};
  bool valid = false;
  bool bubble = false;

  void flush() {
    valid = false;
    bubble = true;
    control = PipelineControlBundle{};
  }
};

}  // namespace risc201::pipeline

namespace risc201::cu {
using pipeline::ExControl;
using pipeline::ChkControl;
using pipeline::MaControl;
using pipeline::WbControl;
using pipeline::PipelineControlBundle;
using pipeline::IfOfLatch;
using pipeline::OfExLatch;
using pipeline::ExStageLatch;
using pipeline::ExChkLatch;
using pipeline::ChkMaLatch;
using pipeline::MaRwLatch;
}  // namespace risc201::cu
