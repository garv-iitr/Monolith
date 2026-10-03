#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "control_unit.h"
#include "isa_spec/isa.h"

namespace risc201 {

class Cpu {
 public:
  using ControlUnit = risc201::cu::HorizontalControlUnit;

  explicit Cpu(std::size_t mem_words = 1024,
               risc201::cu::StageLayout layout = risc201::cu::StageLayout::SIX_STAGE)
      : dp_(mem_words), cu_(dp_, layout) {
    cu_.install(risc201::cu::assemble_horizontal(risc201::cu::build_program()));
  }

  void load(const std::vector<Word>& code) { dp_.mem.load_words(0, code); }

  bool step() { return cu_.step(); }

  void run(std::size_t max_cycles = 1000000) {
    std::size_t cycles = 0;
    while (!dp_.halted && cycles < max_cycles) {
      cu_.step();
      ++cycles;
    }
  }

  void reset() {
    dp_.reset();
    cu_.reset();
  }

  bool halted() const { return dp_.halted; }
  const char* halt_reason() const { return dp_.halt_reason; }
  std::uint16_t micro_pc() const { return cu_.micro_pc(); }
  const char* stage() const { return cu_.stage(); }
  const risc201::cu::ControlSignals& signals() const { return cu_.signals(); }

  Word read_reg(std::uint8_t i) const { return dp_.rf.read(i); }
  Word read_mem(Word addr) const { return dp_.mem.read_word(addr); }

  const ::Flags& flags() const { return dp_.regs.flags; }

 private:
  risc201::cu::Datapath dp_;
  ControlUnit cu_;
};

}  // namespace risc201
