#pragma once

#include <cstdint>
#include <map>
#include <set>
#include <vector>

#include "simulator_core/core/cpu.h"

namespace risc201 {

// interactive debugger on top of the cpu
class Debugger {
 public:
  explicit Debugger(std::size_t mem_words = 1024,
                    risc201::cu::StageLayout layout = risc201::cu::StageLayout::SIX_STAGE,
                    bool vertical = false)
      : cpu_(mem_words, layout, vertical) {}

  const Cpu& cpu() const { return cpu_; }
  const StateView& view() const { return view_; }

  void load(const std::vector<Word>& code) {
    cpu_.load(code);
    refresh();
  }

  void load(Word base, const std::vector<Word>& code) {
    cpu_.load(base, code);
    refresh();
  }

  void reset() {
    cpu_.reset();
    micro_steps_ = 0;
    micro_ops_this_instruction_ = 0;
    at_boundary_ = true;
    refresh();
  }

  // run one whole instruction (a fetch plus its microroutine)
  bool step_instruction() {
    if (cpu_.halted()) return false;
    std::size_t n = 0;
    do {
      if (!cpu_.step()) break;
      ++n;
    } while (cpu_.micro_pc() != 0);
    micro_ops_this_instruction_ = n;
    micro_steps_ = 0;
    at_boundary_ = true;
    refresh();
    return !cpu_.halted();
  }

  bool step_micro() {
    const bool alive = cpu_.step();
    ++micro_steps_;
    at_boundary_ = (cpu_.micro_pc() == 0);
    if (at_boundary_) {
      micro_ops_this_instruction_ = micro_steps_;
      micro_steps_ = 0;
    }
    refresh();
    return alive;
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
  void refresh() { view_ = cpu_.snapshot(); }

  Cpu cpu_;
  StateView view_;
  std::set<Word> breakpoints_;
  std::map<std::uint8_t, Word> watched_;
  std::size_t micro_steps_ = 0;
  std::size_t micro_ops_this_instruction_ = 0;
  bool at_boundary_ = true;
};

}  // namespace risc201
