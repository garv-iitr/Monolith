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

#include "control_unit.h"
#include "isa_spec/isa.h"
#include "memory_safety/memory_map.h"
#include "memory_safety/visualizer.h"
#include "microcode/routine_layout.h"
#include "simulator_core/core/alu_unit.h"
#include "simulator_core/core/exception.h"
#include "simulator_core/core/state_view.h"

namespace risc201 {

enum class ControlEncoding : std::uint8_t { HORIZONTAL, VERTICAL };

class CuUnit {
 public:
  struct State {
    virtual ~State() = default;
  };

  virtual ~CuUnit() = default;
  virtual bool step() = 0;
  virtual void reset() = 0;
  virtual std::uint16_t micro_pc() const = 0;
  virtual const char* stage() const = 0;
  virtual const risc201::cu::ControlSignals& signals() const = 0;
  virtual std::unique_ptr<State> snapshot() const = 0;
  virtual void restore(const State& state) = 0;
};

template <typename ControlUnit, typename MicroWord>
class CuHolder final : public CuUnit {
 public:
  CuHolder(risc201::cu::Datapath& datapath,
           risc201::cu::StageLayout layout,
           risc201::cu::ControlMemory<MicroWord> memory)
      : control_unit_(datapath, layout) {
    control_unit_.install(std::move(memory));
  }

  bool step() override { return control_unit_.step(); }
  void reset() override { control_unit_.reset(); }
  std::uint16_t micro_pc() const override { return control_unit_.micro_pc(); }
  const char* stage() const override { return control_unit_.stage(); }
  const risc201::cu::ControlSignals& signals() const override {
    return control_unit_.signals();
  }

  std::unique_ptr<typename CuUnit::State> snapshot() const override {
    return std::make_unique<ControlState>(control_unit_.snapshot());
  }

  void restore(const typename CuUnit::State& state) override {
    const auto* control_state = dynamic_cast<const ControlState*>(&state);
    if (!control_state) throw std::invalid_argument("control-unit history mismatch");
    control_unit_.restore(control_state->snapshot);
  }

 private:
  struct ControlState final : CuUnit::State {
    explicit ControlState(typename ControlUnit::Snapshot value)
        : snapshot(std::move(value)) {}
    typename ControlUnit::Snapshot snapshot;
  };

  ControlUnit control_unit_;
};

class Cpu {
 public:
  struct Checkpoint {
    risc201::cu::Datapath::Snapshot datapath;
    std::unique_ptr<CuUnit::State> control;
    ExceptionCode fault_code;
    std::uint64_t fault_address;
    std::vector<Word> frame_boundaries;
    std::vector<Word> shadow_return_stack;
    std::size_t memory_journal_mark;
  };

  explicit Cpu(std::size_t mem_words = risc201::cu::MemoryUnit::kDefaultWords,
               risc201::cu::StageLayout layout = risc201::cu::StageLayout::SIX_STAGE,
               ControlEncoding encoding = ControlEncoding::HORIZONTAL)
      : dp_(mem_words, true), alu_(std::make_unique<HardwareAlu>()) {
    const risc201::cu::Program program = risc201::cu::build_program(layout);
    micro_layout_ = risc201::cu::layout_program(program);

    if (encoding == ControlEncoding::VERTICAL) {
      cu_ = std::make_unique<CuHolder<risc201::cu::VerticalControlUnit,
                                      risc201::cu::VerticalMicroWord>>(
          dp_, layout, risc201::cu::assemble_vertical(program));
    } else {
      cu_ = std::make_unique<CuHolder<risc201::cu::HorizontalControlUnit,
                                      risc201::cu::HorizontalMicroWord>>(
          dp_, layout, risc201::cu::assemble_horizontal(program));
    }
    initialize_stack_pointer();
  }

  void load(const std::vector<Word>& code) {
    dp_.mem.load_words(0, code);
    dp_.mem.clear_journal();
  }
  void load(Word base, const std::vector<Word>& code) {
    dp_.mem.load_words(base, code);
    dp_.mem.clear_journal();
  }

  void clear_history_journal() { dp_.mem.clear_journal(); }

  Checkpoint checkpoint() {
    const std::size_t journal_mark = dp_.mem.begin_journal();
    return Checkpoint{dp_.snapshot(), cu_->snapshot(), fault_code_,
                      fault_address_, frame_boundaries_, shadow_return_stack_,
                      journal_mark};
  }

  std::vector<risc201::cu::MemoryUnit::WriteDelta> finish_step(
      std::size_t journal_mark) {
    return dp_.mem.take_journal(journal_mark);
  }

  void restore(const Checkpoint& checkpoint,
               const std::vector<risc201::cu::MemoryUnit::WriteDelta>& memory) {
    dp_.mem.clear_journal();
    dp_.mem.restore_journal(memory);
    dp_.restore(checkpoint.datapath);
    cu_->restore(*checkpoint.control);
    fault_code_ = checkpoint.fault_code;
    fault_address_ = checkpoint.fault_address;
    frame_boundaries_ = checkpoint.frame_boundaries;
    shadow_return_stack_ = checkpoint.shadow_return_stack;
    dp_.mem.clear_journal();
  }

  bool step() {
    if (dp_.halted) return false;
    try {
      const bool alive = cu_->step();
      if (alive) {
        alu_->tick(dp_.regs, cu_->signals(), dp_.regs);
        const Opcode opcode = static_cast<Opcode>(get_opcode(dp_.regs.ir));
        if (opcode == Opcode::CALL &&
            cu_->signals().mem_op == risc201::cu::MemOp::WRITE) {
          shadow_return_stack_.push_back(read_mem(read_reg(REG_SP)));
          frame_boundaries_.push_back(read_reg(REG_SP));
        } else if (opcode == Opcode::RET &&
                   cu_->signals().mem_op == risc201::cu::MemOp::READ &&
                   (shadow_return_stack_.empty() ||
                    dp_.regs.ld_result != shadow_return_stack_.back())) {
          throw RuntimeException{ExceptionCode::SHADOW_STACK_MISMATCH,
                                 read_reg(REG_SP)};
        } else if (opcode == Opcode::RET &&
                   cu_->signals().mem_op == risc201::cu::MemOp::READ) {
          shadow_return_stack_.pop_back();
          if (!frame_boundaries_.empty()) frame_boundaries_.pop_back();
        }
      }
      return alive;
    } catch (const RuntimeException& e) {
      fault_code_ = e.code;
      fault_address_ = e.address;
      dp_.halted = true;
      dp_.halt_reason = "runtime fault";
      return false;
    }
  }

  void run(std::size_t max_cycles = 1000000) {
    std::size_t cycles = 0;
    while (!dp_.halted && cycles < max_cycles) {
      step();
      ++cycles;
    }
  }

  void reset() {
    dp_.reset();
    cu_->reset();
    initialize_stack_pointer();
    fault_code_ = ExceptionCode::NONE;
    fault_address_ = 0;
    frame_boundaries_.clear();
    shadow_return_stack_.clear();
  }

  bool halted() const { return dp_.halted; }
  const char* halt_reason() const { return dp_.halt_reason; }
  bool faulted() const { return fault_code_ != ExceptionCode::NONE; }
  ExceptionCode fault_code() const { return fault_code_; }
  std::uint64_t fault_address() const { return fault_address_; }

  const std::vector<Word>& stack_frame_boundaries() const {
    return frame_boundaries_;
  }

  std::vector<Word> stack_memory_snapshot() const {
    const std::vector<Word> memory = dp_.mem.snapshot();
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
    frames.reserve(frame_boundaries_.size());
    for (Word boundary : frame_boundaries_) {
      frames.push_back(
          boundary / risc201::safety::MemoryMap::kWordBytes);
    }
    return visualizer.render(dp_.mem.snapshot(), stack_pointer_word(), 0, window,
                             frames);
  }

  std::uint16_t micro_pc() const { return cu_->micro_pc(); }
  const char* stage() const { return cu_->stage(); }
  const risc201::cu::ControlSignals& signals() const { return cu_->signals(); }

  Word read_reg(std::uint8_t i) const { return dp_.rf.read(i); }
  void write_reg(std::uint8_t i, Word value) { dp_.rf.write(i, value); }

  Word read_mem(Word addr) const { return dp_.mem.read_word(addr); }
  void write_mem(Word addr, Word value) { dp_.mem.write_word(addr, value); }

  const ::Flags& flags() const { return dp_.regs.flags; }
  Word pc() const { return dp_.regs.pc; }
  Word ir() const { return dp_.regs.ir; }

  std::uint16_t micro_routine_start(std::uint8_t opcode) const {
    return micro_layout_.entry_of(static_cast<Opcode>(opcode));
  }

  std::uint16_t micro_routine_end(std::uint8_t opcode) const {
    const std::uint16_t start = micro_routine_start(opcode);
    std::uint16_t end = static_cast<std::uint16_t>(micro_layout_.stmts.size());
    for (const auto& e : micro_layout_.entries)
      if (e.second > start && e.second < end) end = e.second;
    return end;
  }

  std::uint16_t micro_preamble_len() const {
    std::uint16_t first = static_cast<std::uint16_t>(micro_layout_.stmts.size());
    for (const auto& e : micro_layout_.entries)
      if (e.second < first) first = e.second;
    return first;
  }

  const risc201::cu::MicroProgramLayout& micro_layout() const {
    return micro_layout_;
  }

  StateView snapshot() const {
    StateView v;
    v.pc = dp_.regs.pc;
    v.ir = dp_.regs.ir;
    for (std::uint8_t i = 0; i < NUM_REGISTERS; ++i) v.gprs[i] = dp_.rf.read(i);
    v.flags = dp_.regs.flags;
    v.micro_pc = cu_->micro_pc();
    v.stage = cu_->stage();
    v.halted = dp_.halted;
    v.halt_reason = dp_.halt_reason;
    v.fault_code = fault_code_;
    v.fault_address = fault_address_;
    v.micro = dp_.regs;
    return v;
  }

 private:
  void initialize_stack_pointer() {
    dp_.rf.write(REG_SP, static_cast<Word>(risc201::safety::MemoryMap::kStackTop));
  }

  risc201::cu::Datapath dp_;
  std::unique_ptr<CuUnit> cu_;
  std::unique_ptr<AluUnit> alu_;
  risc201::cu::MicroProgramLayout micro_layout_;
  ExceptionCode fault_code_ = ExceptionCode::NONE;
  std::uint64_t fault_address_ = 0;
  std::vector<Word> frame_boundaries_;
  std::vector<Word> shadow_return_stack_;
};

}  // namespace risc201
