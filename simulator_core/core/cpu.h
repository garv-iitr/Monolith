#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "control_unit.h"
#include "isa_spec/isa.h"
#include "simulator_core/core/alu_unit.h"
#include "simulator_core/core/exception.h"
#include "simulator_core/core/state_view.h"

namespace risc201 {

class Cpu {
 public:
  using ControlUnit = risc201::cu::HorizontalControlUnit;

  explicit Cpu(std::size_t mem_words = 1024,
               risc201::cu::StageLayout layout = risc201::cu::StageLayout::SIX_STAGE)
      : dp_(mem_words), cu_(dp_, layout), alu_(std::make_unique<HardwareAlu>()) {
    cu_.install(risc201::cu::assemble_horizontal(risc201::cu::build_program()));
  }

  void load(const std::vector<Word>& code) { dp_.mem.load_words(0, code); }

  bool step() {
    if (dp_.halted) return false;
    try {
      const bool alive = cu_.step();
      if (alive) alu_->tick(dp_.regs, cu_.signals(), dp_.regs);
      return alive;
    } catch (const RuntimeException& e) {
      fault_code_ = e.code;
      fault_address_ = e.address;
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
    cu_.reset();
    fault_code_ = ExceptionCode::NONE;
    fault_address_ = 0;
  }

  bool halted() const { return dp_.halted; }
  const char* halt_reason() const { return dp_.halt_reason; }
  bool faulted() const { return fault_code_ != ExceptionCode::NONE; }
  ExceptionCode fault_code() const { return fault_code_; }
  std::uint64_t fault_address() const { return fault_address_; }
  std::uint16_t micro_pc() const { return cu_.micro_pc(); }
  const char* stage() const { return cu_.stage(); }
  const risc201::cu::ControlSignals& signals() const { return cu_.signals(); }

  Word read_reg(std::uint8_t i) const { return dp_.rf.read(i); }
  void write_reg(std::uint8_t i, Word value) { dp_.rf.write(i, value); }

  Word read_mem(Word addr) const { return dp_.mem.read_word(addr); }
  void write_mem(Word addr, Word value) { dp_.mem.write_word(addr, value); }

  const ::Flags& flags() const { return dp_.regs.flags; }
  Word pc() const { return dp_.regs.pc; }
  Word ir() const { return dp_.regs.ir; }

  StateView snapshot() const {
    StateView v;
    v.pc = dp_.regs.pc;
    v.ir = dp_.regs.ir;
    for (std::uint8_t i = 0; i < NUM_REGISTERS; ++i) v.gprs[i] = dp_.rf.read(i);
    v.flags = dp_.regs.flags;
    v.micro_pc = cu_.micro_pc();
    v.stage = cu_.stage();
    v.halted = dp_.halted;
    v.halt_reason = dp_.halt_reason;
    v.fault_code = fault_code_;
    v.fault_address = fault_address_;
    return v;
  }

 private:
  risc201::cu::Datapath dp_;
  ControlUnit cu_;
  std::unique_ptr<AluUnit> alu_;
  ExceptionCode fault_code_ = ExceptionCode::NONE;
  std::uint64_t fault_address_ = 0;
};

}  // namespace risc201
