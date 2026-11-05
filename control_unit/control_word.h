#pragma once

#include <cstddef>
#include <cstdint>
#include "control_unit/cu_types.h"
#include "isa_spec/isa.h"

namespace risc201::pipeline {

// =============================================================================
// FIRST-PRINCIPLES CONTROL MEMORY SPECIFICATIONS
// =============================================================================
// Horizontal ROM: Uncompressed 48-bit control word driving datapath muxes
// directly with zero combinational decode delay (T_dec = 0 ns).
//
// Vertical ROM: Field-encoded 24-bit compact control word expanded via a
// combinational micro-decoder, achieving exactly 50.0% ROM silicon area reduction.
// =============================================================================

constexpr std::size_t kHorizontalWordBits = 48;
constexpr std::size_t kVerticalWordBits   = 24;
constexpr double kControlAreaReductionPercent = 50.0;

// -----------------------------------------------------------------------------
// 1. Horizontal Control Word (48 Bits)
// -----------------------------------------------------------------------------
#pragma pack(push, 1)
struct HorizontalControlWord {
  std::uint64_t alu_op         : 4;  // [3:0]   16 integer ALU operations
  std::uint64_t op_a_src       : 2;  // [5:4]   Operand A mux: RS1, IMM, PC, ZERO
  std::uint64_t op_b_src       : 2;  // [7:6]   Operand B mux: RS2, IMM, ZERO
  std::uint64_t set_flags      : 1;  // [8]     Condition code register update strobe
  std::uint64_t branch_cond    : 3;  // [11:9]  Branch condition (EQ, NE, GT, LT)
  std::uint64_t is_branch      : 1;  // [12]    Branch / jump evaluation strobe
  std::uint64_t is_call        : 1;  // [13]    Subroutine call push
  std::uint64_t is_ret         : 1;  // [14]    Subroutine return pop
  std::uint64_t is_fpu         : 1;  // [15]    Execution routed to floating-point unit
  std::uint64_t fpu_op         : 3;  // [18:16] FPU op: FADD, FSUB, FMUL, FDIV, FCMP, FMOV
  std::uint64_t chk_enable     : 1;  // [19]    Memory safety firewall enable strobe
  std::uint64_t chk_mode       : 2;  // [21:20] Check mode: STACK_BOUNDS, MEMORY_ADDR, BNDCHK
  std::uint64_t mem_read       : 1;  // [22]    Data memory read strobe
  std::uint64_t mem_write      : 1;  // [23]    Data memory write strobe
  std::uint64_t ma_is_fpu      : 1;  // [24]    Memory access targets floating-point regfile
  std::uint64_t reg_write_en   : 1;  // [25]    Register file write enable strobe
  std::uint64_t reg_write_src  : 3;  // [28:26] Writeback mux: ALU_OUT, MEM, PC+4, FPU, PASS_B
  std::uint64_t wb_is_fpu      : 1;  // [29]    Writeback destination is FPR
  std::uint64_t freeze_if_of   : 1;  // [30]    Freeze IF/OF latches during u-op cracking
  std::uint64_t is_multi_uop   : 1;  // [31]    Flag indicating multi-uop cracked sequence
  std::uint64_t is_last_uop    : 1;  // [32]    Final u-op indicator (instruction retirement)
  std::uint64_t halt           : 1;  // [33]    Processor execution halt strobe
  std::uint64_t next_micro_pc  : 14; // [47:34] Micro-sequencer dispatch address
};
#pragma pack(pop)

static_assert(sizeof(HorizontalControlWord) <= 8, "HorizontalControlWord fits in 64 bits (48b active)");

// -----------------------------------------------------------------------------
// 2. Vertical Control Word (24 Bits)
// -----------------------------------------------------------------------------
#pragma pack(push, 1)
struct VerticalControlWord {
  std::uint32_t op_class       : 3;  // [2:0]   Operation class: 0=ALU, 1=FPU, 2=MEM, 3=BRANCH, 4=SYSTEM, 5=FLOW
  std::uint32_t sub_op         : 4;  // [6:3]   Sub-operation within class
  std::uint32_t operand_mode   : 3;  // [9:7]   Joint encoding of operand source muxes
  std::uint32_t mem_action     : 2;  // [11:10] Memory action: 0=None, 1=Read, 2=Write, 3=ChkOnly
  std::uint32_t wb_target      : 3;  // [14:12] Writeback target: 0=None, 1=ALU, 2=MEM, 3=PC+4, 4=FPU
  std::uint32_t is_fpu_domain  : 1;  // [15]    Domain selector: 0=GPR, 1=FPR
  std::uint32_t set_flags      : 1;  // [16]    Condition code register update strobe
  std::uint32_t freeze_if_of   : 1;  // [17]    Pipeline freeze strobe
  std::uint32_t is_multi_uop   : 1;  // [18]    Multi u-op active indicator
  std::uint32_t is_last_uop    : 1;  // [19]    Final u-op indicator
  std::uint32_t halt           : 1;  // [20]    Halt strobe
  std::uint32_t next_uop_step  : 3;  // [23:21] Micro-step index (0-7)
};
#pragma pack(pop)

static_assert(sizeof(VerticalControlWord) <= 4, "VerticalControlWord fits in 32 bits (24b active)");

}  // namespace risc201::pipeline

namespace risc201::cu {
using pipeline::HorizontalControlWord;
using pipeline::VerticalControlWord;
using pipeline::kControlAreaReductionPercent;
constexpr std::size_t kPipelineHorizontalWordBits = pipeline::kHorizontalWordBits;
constexpr std::size_t kPipelineVerticalWordBits   = pipeline::kVerticalWordBits;
}  // namespace risc201::cu
