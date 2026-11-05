// Generic microprogrammed control unit orchestrating ROM fetching, decoding, datapath execution, and micro-sequencing.
#ifndef RISC201_CU_MICRO_CONTROL_UNIT_H
#define RISC201_CU_MICRO_CONTROL_UNIT_H

#include <cstdint>
#include <utility>

#include "common/control_memory.h"
#include "common/micro_sequencer.h"
#include "common/microinstruction.h"
#include "datapath/datapath.h"
#include "isa_spec/isa.h"

namespace risc201::cu {

// Templated microcode controller parameterizable with Horizontal or Vertical micro-word types and codecs
template <typename MicroWord, typename Codec>
class MicroControlUnit {
 public:
  struct Snapshot {
    MicroSequencer sequencer;
    MicroWord word{};
    DecodedMicro decoded{};
  };

  ControlMemory<MicroWord> cm; // Control memory storing microcode routines
  MicroSequencer seq;          // Next-micro-address generator
  StageLayout layout = StageLayout::SIX_STAGE; // Active pipeline stage layout mapping
  Datapath& dp;                // Reference to the controlled processor datapath

  // Attaches controller to the target datapath and sets pipeline layout
  MicroControlUnit(Datapath& datapath, StageLayout stage_layout = StageLayout::SIX_STAGE)
      : layout(stage_layout), dp(datapath) {
    dp.layout = layout;
  }

  // Flashes a newly assembled microprogram into control memory and resets the sequencer
  void install(ControlMemory<MicroWord> mem) {
    cm = std::move(mem);
    seq.reset(0);
  }

  // Resets sequencer and datapath micro-PC to the preamble start address (0)
  void reset() {
    seq.reset(0);
    dp.regs.micro_pc = 0;
  }

  // Advances execution by one micro-cycle: fetches ROM, decodes signals, executes datapath, and steps sequencer
  bool step() {
    if (dp.halted) return false;

    // 1. Fetch microinstruction from Control Memory at current micro_pc
    const MicroWord& word = cm.at(seq.micro_pc);
    last_word_ = word;
    // 2. Decode micro-word into discrete hardware control signals and immediate
    last_ = Codec::decode(word);

    // 3. Handle dynamic opcode dispatch (mswitch) by indexing into the opcode jump table
    std::uint16_t switch_target = 0;
    if (last_.sig.next_addr == NextAddr::MSWITCH) {
      const std::uint8_t opcode = get_opcode(dp.regs.ir);
      if (!cm.has_entry(opcode)) {
        dp.halted = true;
        dp.halt_reason = "illegal instruction";
        return false;
      }
      switch_target = cm.entry(opcode);
    }

    // 4. Drive control signals and immediate across datapath buses and trigger functional units
    try {
      dp.execute(last_.sig, last_.muimm);
    } catch (...) {
      dp.halted = true;
      dp.halt_reason = "exception";
      throw;
    }

    // 5. Compute next micro-address based on sequencer mode and branch comparator
    seq.advance(last_.sig, dp.bus.is_m_branch, switch_target);
    // mirror sequencer pc into regs, the tracer reads it
    dp.regs.micro_pc = seq.micro_pc;
    return !dp.halted;
  }

  Snapshot snapshot() const { return Snapshot{seq, last_word_, last_}; }

  void restore(const Snapshot& snapshot) {
    seq = snapshot.sequencer;
    last_word_ = snapshot.word;
    last_ = snapshot.decoded;
    dp.regs.micro_pc = seq.micro_pc;
  }

  // Inspection getters for debugger, trace logging, and verification testbenches
  const DecodedMicro& decoded() const { return last_; }
  const ControlSignals& signals() const { return last_.sig; }
  const MicroWord& word() const { return last_word_; }
  std::uint16_t micro_pc() const { return seq.micro_pc; }
  const char* stage() const { return stage_name(layout, last_.sig.stage); }
  const char* last_sequence_source() const { return seq.last_source; }

 private:
  MicroWord last_word_{};
  DecodedMicro last_{};
};

}

#endif
