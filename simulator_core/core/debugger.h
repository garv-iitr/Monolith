#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <set>
#include <vector>

#include "simulator_core/core/cpu.h"

namespace risc201 {

// interactive debugger on top of the cpu
class Debugger {
 public:
  static constexpr std::size_t kHistoryLimit = 4096;

  explicit Debugger(
      std::size_t mem_words = risc201::cu::MemoryUnit::kDefaultWords,
      risc201::cu::StageLayout layout = risc201::cu::StageLayout::SIX_STAGE,
      ControlEncoding encoding = ControlEncoding::HORIZONTAL)
      : cpu_(mem_words, layout, encoding) {
    refresh();
  }

  const Cpu& cpu() const { return cpu_; }
  Word active_instruction_address() const { return active_instruction_address_; }

  void write_reg(std::uint8_t index, Word value) {
    clear_history();
    cpu_.write_reg(index, value);
    refresh();
  }

  void write_freg(std::uint8_t index, Word value) {
    clear_history();
    cpu_.write_freg(index, value);
    refresh();
  }

  void write_mem(Word address, Word value) {
    clear_history();
    cpu_.write_mem(address, value);
    refresh();
  }

  std::string render_stack(int window = 8) const {
    return cpu_.render_stack(window);
  }
  const StateView& view() const { return view_; }

  void load(const std::vector<Word>& code) {
    clear_history();
    cpu_.load(code);
    refresh();
  }

  void load(Word base, const std::vector<Word>& code) {
    clear_history();
    cpu_.load(base, code);
    refresh();
  }

  void load_program(Word text_base, const std::vector<Word>& text,
                    Word data_base, const std::vector<Word>& data) {
    reset();
    cpu_.load(text_base, text);
    cpu_.load(data_base, data);
    refresh();
  }

  void reset() {
    clear_history();
    cpu_.reset();
    micro_steps_ = 0;
    micro_ops_this_instruction_ = 0;
    at_boundary_ = true;
    active_instruction_address_ = cpu_.pc();
    refresh();
  }

  // run one whole instruction (advances cycles until one instruction retires)
  bool step_instruction() {
    if (cpu_.halted()) return false;
    const std::size_t start_retired = cpu_.pipe().stats().instructions_retired;
    do {
      if (!record_micro_step()) break;
    } while (cpu_.pipe().stats().instructions_retired == start_retired && !cpu_.halted());
    at_boundary_ = true;
    refresh();
    return !cpu_.halted();
  }

  bool step_micro() {
    return record_micro_step();
  }

  bool step_back_micro() {
    if (history_.empty()) return false;
    restore_history_entry(history_.size() - 1);
    history_.pop_back();
    refresh();
    return true;
  }

  bool step_back_instruction() {
    if (history_.empty()) return false;
    std::size_t target = history_.size() - 1;
    while (target > 0 && !history_[target].instruction_start) --target;
    if (!history_[target].instruction_start) return false;
    restore_history_range(target);
    history_.erase(history_.begin() + static_cast<std::ptrdiff_t>(target), history_.end());
    refresh();
    return true;
  }

  void run() {
    while (step_instruction()) {}
  }

  void run_to_breakpoint() {
    while (!cpu_.halted() && !at_breakpoint()) step_instruction();
  }

  void add_breakpoint(Word addr) { breakpoints_.insert(addr); }
  void remove_breakpoint(Word addr) { breakpoints_.erase(addr); }
  void clear_breakpoints() { breakpoints_.clear(); }
  bool at_breakpoint() const { return breakpoints_.count(cpu_.pc()) != 0; }
  const std::set<Word>& breakpoints() const { return breakpoints_; }

  void watch(std::uint8_t reg) { watched_[reg] = cpu_.read_reg(reg); }
  void unwatch(std::uint8_t reg) { watched_.erase(reg); }
  const std::map<std::uint8_t, Word>& watched() const { return watched_; }

  // micro step counters for the mi command
  std::size_t micro_steps() const { return micro_steps_; }
  std::size_t micro_ops_last_instruction() const { return micro_ops_this_instruction_; }
  bool at_instruction_boundary() const { return at_boundary_; }

  std::vector<std::uint8_t> changed_registers() {
    std::vector<std::uint8_t> changed;
    for (auto& [reg, baseline] : watched_) {
      const Word cur = cpu_.read_reg(reg);
      if (cur != baseline) {
        changed.push_back(reg);
        baseline = cur;
      }
    }
    return changed;
  }

 private:
  struct HistoryEntry {
    Cpu::Checkpoint cpu;
    std::vector<risc201::cu::MemoryUnit::WriteDelta> memory;
    std::size_t micro_steps;
    std::size_t micro_ops_this_instruction;
    bool at_boundary;
    bool instruction_start;
    Word active_instruction_address;
  };

  bool record_micro_step() {
    if (cpu_.halted()) return false;
    const std::size_t retired_before = cpu_.pipe().stats().instructions_retired;
    const bool instruction_start = at_boundary_;
    HistoryEntry entry{cpu_.checkpoint(), {}, micro_steps_,
                       micro_ops_this_instruction_, at_boundary_,
                       instruction_start, active_instruction_address_};
    if (instruction_start) active_instruction_address_ = cpu_.pc();
    const std::size_t journal_mark = entry.cpu.memory_journal_mark;
    bool alive = false;
    try {
      alive = cpu_.step();
    } catch (...) {
      entry.memory = cpu_.finish_step(journal_mark);
      push_history(std::move(entry));
      throw;
    }
    entry.memory = cpu_.finish_step(journal_mark);
    push_history(std::move(entry));
    ++micro_steps_;
    const bool retired_now = cpu_.pipe().stats().instructions_retired > retired_before;
    at_boundary_ = retired_now || cpu_.halted();
    if (at_boundary_) {
      micro_ops_this_instruction_ = micro_steps_;
      micro_steps_ = 0;
    }
    refresh();
    return alive;
  }

  void push_history(HistoryEntry entry) {
    if (history_.size() == kHistoryLimit) history_.pop_front();
    history_.push_back(std::move(entry));
  }

  void restore_history_entry(std::size_t index) {
    HistoryEntry& entry = history_[index];
    cpu_.restore(entry.cpu, entry.memory);
    micro_steps_ = entry.micro_steps;
    micro_ops_this_instruction_ = entry.micro_ops_this_instruction;
    at_boundary_ = entry.at_boundary;
    active_instruction_address_ = entry.active_instruction_address;
  }

  void restore_history_range(std::size_t start) {
    std::vector<risc201::cu::MemoryUnit::WriteDelta> memory;
    for (std::size_t i = start; i < history_.size(); ++i)
      memory.insert(memory.end(), history_[i].memory.begin(), history_[i].memory.end());
    HistoryEntry& entry = history_[start];
    cpu_.restore(entry.cpu, memory);
    micro_steps_ = entry.micro_steps;
    micro_ops_this_instruction_ = entry.micro_ops_this_instruction;
    at_boundary_ = entry.at_boundary;
    active_instruction_address_ = entry.active_instruction_address;
  }

  void clear_history() {
    history_.clear();
    cpu_.clear_history_journal();
  }

  void refresh() { view_ = cpu_.snapshot(); }

  Cpu cpu_;
  std::deque<HistoryEntry> history_;
  StateView view_;
  std::set<Word> breakpoints_;
  std::map<std::uint8_t, Word> watched_;
  std::size_t micro_steps_ = 0;
  std::size_t micro_ops_this_instruction_ = 0;
  bool at_boundary_ = true;
  Word active_instruction_address_ = 0;
};

}  // namespace risc201
