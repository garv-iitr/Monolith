#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "alu/alu.h"
#include "fpu/fpu.h"
#include "control_unit/control_unit.h"
#include "control_unit/cu_types.h"
#include "memory/memory_unit.h"
#include "control_unit/hazard_unit.h"
#include "control_unit/pipeline_bundles.h"
#include "isa_spec/isa.h"
#include "memory_safety/stack_guard.h"
#include "simulator_core/core/exception.h"

namespace risc201::pipeline {

// =============================================================================
// PIPELINE EXECUTION PERFORMANCE STATISTICS
// =============================================================================
struct PipelineStats {
  std::size_t total_cycles = 0;
  std::size_t instructions_retired = 0;
  std::size_t load_use_stalls = 0;
  std::size_t branch_flushes = 0;
  std::size_t uop_expansion_cycles = 0;
  std::size_t forwarding_events = 0;

  double cpi() const {
    if (instructions_retired == 0) return 0.0;
    return static_cast<double>(total_cycles) / static_cast<double>(instructions_retired);
  }

  double ipc() const {
    if (total_cycles == 0) return 0.0;
    return static_cast<double>(instructions_retired) / static_cast<double>(total_cycles);
  }
};

// =============================================================================
// CYCLE-ACCURATE PIPELINED PROCESSOR ENGINE
// =============================================================================
// Supports:
// 1. 4-Stage Pipeline: [IF] -> [OF] -> [EX] -> [MA/RW] (Merged MA/RW, maximum throughput)
// 2. 6-Stage Pipeline: [IF] -> [OF] -> [EX] -> [MEMCHK] -> [MA] -> [RW] (Isolated safety firewall)
// Method 3 Hybrid Micro-Op Cracking (48-bit Horizontal vs 24-bit Vertical ROM)
// =============================================================================
class PipelineProcessor {
 public:
  explicit PipelineProcessor(cu::StageLayout layout = cu::StageLayout::SIX_STAGE,
                             bool vertical_encoding = false,
                             std::size_t mem_words = cu::MemoryUnit::kDefaultWords,
                             bool runtime_safety = false);

  struct Snapshot {
    Word pc = 0;
    Word committed_pc = 0;
    std::array<Word, NUM_REGISTERS> regs{};
    std::array<Word, NUM_FLOAT_REGISTERS> fregs{};
    Flags flags{};
    bool halted = false;
    ExceptionCode fault_code = ExceptionCode::NONE;
    Word fault_address = 0;

    IfOfLatch if_of{};
    OfExLatch of_ex{};
    ExStageLatch ex_stage{};
    ChkMaLatch chk_ma{};
    MaRwLatch ma_rw{};

    PipelineStats stats{};
    std::vector<Word> frame_boundaries;
    std::vector<Word> shadow_return_stack;
  };

  void reset();
  void load(const std::vector<Word>& words, Word base = 0);
  void write_data(Word byte_addr, Word value);

  // Advances simulation by exactly one clock cycle
  bool step();

  // Runs processor until halt or max_cycles
  void run(std::size_t max_cycles = 2000000);

  // State inspection
  bool halted() const { return halted_; }
  bool faulted() const { return fault_code_ != ExceptionCode::NONE; }
  ExceptionCode fault_code() const { return fault_code_; }
  Word fault_address() const { return fault_address_; }

  Word read_reg(std::uint8_t r) const { return (r < NUM_REGISTERS) ? regs_[r] : 0; }
  void write_reg(std::uint8_t r, Word v) { if (r < NUM_REGISTERS && r != 0) regs_[r] = v; }

  Word read_freg(std::uint8_t f) const { return (f < NUM_FLOAT_REGISTERS) ? fregs_[f] : 0; }
  void write_freg(std::uint8_t f, Word v) { if (f < NUM_FLOAT_REGISTERS) fregs_[f] = v; }

  Word read_mem(Word addr) const { return mem_.read_word(addr); }
  void write_mem(Word addr, Word v) { mem_.write_word(addr, v); }

  const Flags& flags() const { return flags_; }
  Word pc() const { return committed_pc_; }
  Word fetch_pc() const { return pc_; }
  void set_pc(Word p) { pc_ = p; committed_pc_ = p; }
  const PipelineStats& stats() const { return stats_; }
  cu::StageLayout layout() const { return layout_; }
  const char* encoding_name() const { return cu_->encoding_name(); }

  Snapshot snapshot() const;
  void restore(const Snapshot& s);

  cu::MemoryUnit& mem() { return mem_; }
  const cu::MemoryUnit& mem() const { return mem_; }

  const std::vector<Word>& stack_frame_boundaries() const { return frame_boundaries_; }
  const std::vector<Word>& shadow_return_stack() const { return shadow_return_stack_; }

  bool runtime_safety() const { return runtime_safety_; }
  void set_runtime_safety(bool enable) { runtime_safety_ = enable; }

  // Inter-stage pipeline latches
  // 4-Stage: [IF] -> if_of_ -> [OF] -> of_ex_ -> [EX] -> ex_stage_ -> [MA/RW]
  // 6-Stage: [IF] -> if_of_ -> [OF] -> of_ex_ -> [EX] -> ex_stage_ -> [MEMCHK] -> chk_ma_ -> [MA] -> ma_rw_ -> [RW]
  const IfOfLatch& latch_if_of() const { return if_of_; }
  const OfExLatch& latch_of_ex() const { return of_ex_; }
  const ExStageLatch& latch_ex_stage() const { return ex_stage_; }
  const ExStageLatch& latch_ex_chk() const { return ex_stage_; } // Compatibility alias
  const ChkMaLatch& latch_chk_ma() const { return chk_ma_; }
  const MaRwLatch& latch_ma_rw() const { return ma_rw_; }

 private:
  cu::StageLayout layout_;
  std::unique_ptr<IPipelineControlUnit> cu_;
  cu::MemoryUnit mem_;
  ALU alu_;
  FPU fpu_;
  safety::StackGuard stack_guard_;
  bool runtime_safety_ = false;

  Word pc_ = 0;
  Word committed_pc_ = 0;
  Word regs_[NUM_REGISTERS]{};
  Word fregs_[NUM_FLOAT_REGISTERS]{};
  Flags flags_{};
  bool halted_ = false;
  ExceptionCode fault_code_ = ExceptionCode::NONE;
  Word fault_address_ = 0;

  std::vector<Word> frame_boundaries_;
  std::vector<Word> shadow_return_stack_;

  IfOfLatch if_of_{};
  OfExLatch of_ex_{};
  ExStageLatch ex_stage_{};
  ChkMaLatch chk_ma_{};
  MaRwLatch ma_rw_{};

  PipelineStats stats_{};

  void initialize_stack_pointer();
  void check_memory_safety(Word addr, const ChkControl& chk, const MaRwLatch* pending_rw = nullptr);
};

}  // namespace risc201::pipeline

namespace risc201::cu {
using PipelineStats = risc201::pipeline::PipelineStats;
using PipelineProcessor = risc201::pipeline::PipelineProcessor;
}  // namespace risc201::cu
