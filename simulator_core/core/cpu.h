#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "control_unit.h"
#include "isa_spec/isa.h"
#include "microcode/routine_layout.h"
#include "simulator_core/core/alu_unit.h"
#include "simulator_core/core/exception.h"
#include "simulator_core/core/state_view.h"
#include "memory_safety/visualizer.h"

namespace risc201 {

// type eraser so the cpu can hold the horizontal or the vertical unit
class CuUnit {
 public:
  virtual ~CuUnit() = default;
  virtual bool step() = 0;
  virtual void reset() = 0;
  virtual std::uint16_t micro_pc() const = 0;
  virtual const char* stage() const = 0;
  virtual const risc201::cu::ControlSignals& signals() const = 0;
};

template <typename CU, typename Rom>
class CuHolder : public CuUnit {
 public:
  CuHolder(risc201::cu::Datapath& dp, risc201::cu::StageLayout layout, Rom rom)
      : cu_(dp, layout) {
    cu_.install(std::move(rom));
  }
  bool step() override { return cu_.step(); }
  void reset() override { cu_.reset(); }
  std::uint16_t micro_pc() const override { return cu_.micro_pc(); }
  const char* stage() const override { return cu_.stage(); }
  const risc201::cu::ControlSignals& signals() const override { return cu_.signals(); }

 private:
  CU cu_;
};

class Cpu {
 public:
  explicit Cpu(std::size_t mem_words = risc201::cu::MemoryUnit::kDefaultWords,
               risc201::cu::StageLayout layout = risc201::cu::StageLayout::SIX_STAGE,
               bool vertical = false)
      : dp_(mem_words, true),
        alu_(std::make_unique<HardwareAlu>()),
        vertical_(vertical) {
    const risc201::cu::Program program = risc201::cu::build_program(layout);
    if (vertical) {
      cu_ = std::make_unique<
          CuHolder<risc201::cu::VerticalControlUnit,
                   risc201::cu::ControlMemory<risc201::cu::VerticalMicroWord>>>(
          dp_, layout, risc201::cu::assemble_vertical(program));
    } else {
      cu_ = std::make_unique<
          CuHolder<risc201::cu::HorizontalControlUnit,
                   risc201::cu::ControlMemory<risc201::cu::HorizontalMicroWord>>>(
          dp_, layout, risc201::cu::assemble_horizontal(program));
    }
    micro_layout_ = risc201::cu::layout_program(program);
    dp_.rf.write(REG_SP, static_cast<Word>(
                             risc201::safety::MemoryMap::kStackTop));
  }

  void load(const std::vector<Word>& code) { dp_.mem.load_words(0, code); }
  void load(Word base, const std::vector<Word>& code) { dp_.mem.load_words(base, code); }

  bool step() {
    if (dp_.halted) return false;
    try {
      const bool alive = cu_->step();
      if (alive) {
        alu_->tick(dp_.regs, cu_->signals(), dp_.regs);
        const Opcode opcode = static_cast<Opcode>(get_opcode(dp_.regs.ir));
        if (opcode == Opcode::CALL &&
            cu_->signals().mem_op == risc201::cu::MemOp::WRITE) {
          frame_boundaries_.push_back(read_reg(REG_SP));
        } else if (opcode == Opcode::RET &&
                   cu_->signals().mem_op == risc201::cu::MemOp::READ &&
                   !frame_boundaries_.empty()) {
          frame_boundaries_.pop_back();
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
    dp_.rf.write(REG_SP, static_cast<Word>(
                             risc201::safety::MemoryMap::kStackTop));
    fault_code_ = ExceptionCode::NONE;
    fault_address_ = 0;
    frame_boundaries_.clear();
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
    return visualizer.render(
        dp_.mem.snapshot(),
        stack_pointer_word(), 0, window,
        frames);
  }
  std::uint16_t micro_pc() const { return cu_->micro_pc(); }
  const char* stage() const { return cu_->stage(); }
  const risc201::cu::ControlSignals& signals() const { return cu_->signals(); }
  bool vertical() const { return vertical_; }

  Word read_reg(std::uint8_t i) const { return dp_.rf.read(i); }
  void write_reg(std::uint8_t i, Word value) { dp_.rf.write(i, value); }

  Word read_mem(Word addr) const { return dp_.mem.read_word(addr); }
  void write_mem(Word addr, Word value) { dp_.mem.write_word(addr, value); }

  const ::Flags& flags() const { return dp_.regs.flags; }
  Word pc() const { return dp_.regs.pc; }
  Word ir() const { return dp_.regs.ir; }

  // where each opcodes microroutine sits in control memory
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
  risc201::cu::Datapath dp_;
  std::unique_ptr<CuUnit> cu_;
  std::unique_ptr<AluUnit> alu_;
  risc201::cu::MicroProgramLayout micro_layout_;
  bool vertical_ = false;
  ExceptionCode fault_code_ = ExceptionCode::NONE;
  std::uint64_t fault_address_ = 0;
  std::vector<Word> frame_boundaries_;
};

}  // namespace risc201
