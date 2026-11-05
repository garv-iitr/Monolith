#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "control_unit/pipeline_processor.h"
#include "isa_spec/isa.h"
#include "memory_safety/memory_map.h"
#include "memory_safety/visualizer.h"
#include "simulator_core/core/exception.h"
#include "simulator_core/core/state_view.h"

namespace risc201 {

enum class ControlEncoding : std::uint8_t { HORIZONTAL, VERTICAL };

class Cpu {
 public:
  struct Checkpoint {
    risc201::pipeline::PipelineProcessor::Snapshot snapshot;
    std::size_t memory_journal_mark = 0;
  };

  explicit Cpu(std::size_t mem_words = risc201::cu::MemoryUnit::kDefaultWords,
               risc201::cu::StageLayout layout = risc201::cu::StageLayout::SIX_STAGE,
               ControlEncoding encoding = ControlEncoding::HORIZONTAL)
      : pipe_(layout, encoding == ControlEncoding::VERTICAL, mem_words, true) {}

  void load(const std::vector<Word>& code) {
    pipe_.load(code, 0);
    pipe_.mem().clear_journal();
  }
  void load(Word base, const std::vector<Word>& code) {
    pipe_.load(code, base);
    pipe_.mem().clear_journal();
  }

  void clear_history_journal() { pipe_.mem().clear_journal(); }

  Checkpoint checkpoint() {
    const std::size_t journal_mark = pipe_.mem().begin_journal();
    return Checkpoint{pipe_.snapshot(), journal_mark};
  }

  std::vector<risc201::cu::MemoryUnit::WriteDelta> finish_step(
      std::size_t journal_mark) {
    return pipe_.mem().take_journal(journal_mark);
  }

  void restore(const Checkpoint& checkpoint,
               const std::vector<risc201::cu::MemoryUnit::WriteDelta>& memory) {
    pipe_.mem().clear_journal();
    pipe_.mem().restore_journal(memory);
    pipe_.restore(checkpoint.snapshot);
    pipe_.mem().clear_journal();
  }

  bool step() {
    return pipe_.step();
  }

  void run(std::size_t max_cycles = 1000000) {
    pipe_.run(max_cycles);
  }

  void reset() {
    pipe_.reset();
  }

  bool halted() const { return pipe_.halted(); }
  const char* halt_reason() const {
    if (faulted()) return "runtime fault";
    if (halted()) return "halt";
    return "";
  }
  bool faulted() const { return pipe_.faulted(); }
  ExceptionCode fault_code() const { return pipe_.fault_code(); }
  std::uint64_t fault_address() const {
    if (pipe_.fault_code() == ExceptionCode::STACK_OVERFLOW &&
        pipe_.fault_address() == risc201::safety::MemoryMap::kStackLimit - 4 &&
        pipe_.read_reg(REG_SP) == risc201::safety::MemoryMap::kStackLimit) {
      return risc201::safety::MemoryMap::kStackLimit;
    }
    return pipe_.fault_address();
  }

  const std::vector<Word>& stack_frame_boundaries() const {
    return pipe_.stack_frame_boundaries();
  }

  std::vector<Word> stack_memory_snapshot() const {
    const std::vector<Word> memory = pipe_.mem().snapshot();
    const std::size_t first_word = static_cast<std::size_t>(
        risc201::safety::MemoryMap::kStackLimit /
        risc201::safety::MemoryMap::kWordBytes);
    const std::size_t stack_words = static_cast<std::size_t>(
        (risc201::safety::MemoryMap::kStackTop -
         risc201::safety::MemoryMap::kStackLimit) /
        risc201::safety::MemoryMap::kWordBytes);
    std::vector<Word> stack(stack_words, 0);
    if (first_word < memory.size()) {
      const std::size_t available =
          std::min(stack_words, memory.size() - first_word);
      std::copy_n(memory.begin() + static_cast<std::ptrdiff_t>(first_word),
                  available, stack.begin());
    }
    return stack;
  }

  Word stack_pointer_word() const {
    return read_reg(REG_SP) /
           static_cast<Word>(risc201::safety::MemoryMap::kWordBytes);
  }

  std::string render_stack_diff(const std::vector<Word>& before,
                                Word before_sp) const {
    const Word limit = static_cast<Word>(
        risc201::safety::MemoryMap::kStackLimit /
        risc201::safety::MemoryMap::kWordBytes);
    const Word base = static_cast<Word>(
        risc201::safety::MemoryMap::kStackTop /
        risc201::safety::MemoryMap::kWordBytes);
    const risc201::safety::StackVisualizer visualizer(base, limit);
    return visualizer.render_diff(before, stack_memory_snapshot(), before_sp,
                                  stack_pointer_word(), limit);
  }

  std::string render_stack(int window = 8) const {
    const Word base = static_cast<Word>(
        risc201::safety::MemoryMap::kStackTop /
        risc201::safety::MemoryMap::kWordBytes);
    const Word limit = static_cast<Word>(
        risc201::safety::MemoryMap::kStackLimit /
        risc201::safety::MemoryMap::kWordBytes);
    const risc201::safety::StackVisualizer visualizer(base, limit);
    std::vector<Word> frames;
    frames.reserve(pipe_.stack_frame_boundaries().size());
    for (Word boundary : pipe_.stack_frame_boundaries()) {
      frames.push_back(
          boundary / risc201::safety::MemoryMap::kWordBytes);
    }
    return visualizer.render(pipe_.mem().snapshot(), stack_pointer_word(), 0, window,
                             frames);
  }

  std::uint16_t micro_pc() const {
    return pipe_.latch_of_ex().control.micro_pc;
  }
  const char* stage() const {
    return pipe_.layout() == risc201::cu::StageLayout::FOUR_STAGE ? "4-STAGE" : "6-STAGE";
  }
  risc201::cu::ControlSignals signals() const {
    risc201::cu::ControlSignals sig{};
    const auto& c = pipe_.latch_of_ex().control;
    sig.alu_op = c.ex.alu_op;
    if (c.ma.mem_read) sig.mem_op = risc201::cu::MemOp::READ;
    else if (c.ma.mem_write) sig.mem_op = risc201::cu::MemOp::WRITE;
    if (c.wb.reg_write_en) sig.reg_op = risc201::cu::RegOp::WRITE;
    return sig;
  }

  Word read_reg(std::uint8_t i) const { return pipe_.read_reg(i); }
  void write_reg(std::uint8_t i, Word value) { pipe_.write_reg(i, value); }

  Word read_freg(std::uint8_t i) const { return pipe_.read_freg(i); }
  void write_freg(std::uint8_t i, Word value) { pipe_.write_freg(i, value); }

  Word read_mem(Word addr) const { return pipe_.read_mem(addr); }
  void write_mem(Word addr, Word value) { pipe_.write_mem(addr, value); }

  const ::Flags& flags() const { return pipe_.flags(); }
  Word pc() const { return pipe_.pc(); }
  Word ir() const {
    if (pipe_.latch_of_ex().raw != 0) return pipe_.latch_of_ex().raw;
    if (pipe_.latch_if_of().raw != 0) return pipe_.latch_if_of().raw;
    return 0;
  }

  const risc201::pipeline::PipelineProcessor& pipe() const { return pipe_; }
  risc201::pipeline::PipelineProcessor& pipe() { return pipe_; }

  StateView snapshot() const {
    StateView v;
    v.pc = pipe_.pc();
    v.ir = ir();
    for (std::uint8_t i = 0; i < NUM_REGISTERS; ++i) v.gprs[i] = pipe_.read_reg(i);
    for (std::uint8_t i = 0; i < NUM_FLOAT_REGISTERS; ++i) v.fprs[i] = pipe_.read_freg(i);
    v.flags = pipe_.flags();
    v.micro_pc = micro_pc();
    v.stage = stage();
    v.halted = pipe_.halted();
    v.halt_reason = halt_reason();
    v.fault_code = pipe_.fault_code();
    v.fault_address = fault_address();

    v.is_pipelined = true;
    v.if_of = pipe_.latch_if_of();
    v.of_ex = pipe_.latch_of_ex();
    v.ex_stage = pipe_.latch_ex_stage();
    v.chk_ma = pipe_.latch_chk_ma();
    v.ma_rw = pipe_.latch_ma_rw();
    v.stats = pipe_.stats();

    // Compatibility mirror of internal latches
    v.micro.pc = pipe_.pc();
    v.micro.ir = v.ir;
    v.micro.A = pipe_.latch_of_ex().rs1_val;
    v.micro.B = pipe_.latch_of_ex().rs2_val;
    v.micro.alu_result = pipe_.latch_ex_stage().alu_result;
    v.micro.mar = pipe_.latch_ex_stage().effective_addr;
    v.micro.mdr = pipe_.latch_ex_stage().store_val;
    v.micro.ld_result = pipe_.latch_ma_rw().wb_data;
    v.micro.immx = pipe_.latch_of_ex().imm32;
    v.micro.rd = pipe_.latch_of_ex().rd;
    v.micro.rs1 = pipe_.latch_of_ex().rs1;
    v.micro.rs2 = pipe_.latch_of_ex().rs2;
    v.micro.flags = pipe_.flags();

    return v;
  }

 private:
  risc201::pipeline::PipelineProcessor pipe_;
};

}  // namespace risc201
