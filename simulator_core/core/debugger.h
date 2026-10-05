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
  explicit Debugger(std::size_t mem_words = 1024) : cpu_(mem_words) {}

  const Cpu& cpu() const { return cpu_; }
  const StateView& view() const { return view_; }

  void load(const std::vector<Word>& code) {
    cpu_.load(code);
    refresh();
  }

  void reset() {
    cpu_.reset();
    refresh();
  }

  // Advance one ISA instruction (a full fetch + routine). Returns false once
  // the machine has halted or faulted.
  bool step_instruction() {
    if (cpu_.halted()) return false;
    do {
      if (!cpu_.step()) break;
    } while (cpu_.micro_pc() != 0);
    refresh();
    return !cpu_.halted();
  }

  // Advance a single micro-cycle.
  bool step_micro() {
    const bool alive = cpu_.step();
    refresh();
    return alive;
  }

  // Run to completion (halt or fault), ignoring breakpoints.
  void run() {
    while (step_instruction()) {}
  }

  // Run until a breakpoint is reached or the machine halts/faults.
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

  // Registers whose value changed since they were last watched / reported.
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
};

}  // namespace risc201
