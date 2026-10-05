#ifndef RISC201_CU_MICRO_CONTROL_UNIT_H
#define RISC201_CU_MICRO_CONTROL_UNIT_H

// ============================================================================
// The micro-control unit (Sarangi, sec. 9.8), shared by both encodings.
//
//   MicroWord -- the control-memory word (horizontal or vertical)
//   Codec     -- how to materialise a control word into signals:
//                  horizontal: wire-through
//                  vertical  : run the micro-decoder
//
// Each step: fetch the word at microPC, materialise its signals, drive the
// datapath for one micro-cycle, then advance the microPC.  The opcode for
// mswitch is read from the ir register, exactly as in fig. 9.22.
// ============================================================================

#include <cstdint>
#include <utility>

#include "common/control_memory.h"
#include "common/micro_sequencer.h"
#include "common/microinstruction.h"
#include "datapath/datapath.h"
#include "isa_spec/isa.h"

namespace risc201::cu {

template <typename MicroWord, typename Codec>
class MicroControlUnit {
 public:
  ControlMemory<MicroWord> cm;
  MicroSequencer seq;
  StageLayout layout = StageLayout::SIX_STAGE;
  Datapath& dp;

  MicroControlUnit(Datapath& datapath, StageLayout stage_layout = StageLayout::SIX_STAGE)
      : layout(stage_layout), dp(datapath) {
    dp.layout = layout;
  }

  void install(ControlMemory<MicroWord> mem) {
    cm = std::move(mem);
    seq.reset(0);
  }

  void reset() {
    seq.reset(0);
    dp.regs.micro_pc = 0;
  }

  // Execute a single micro-cycle.  Returns false once the machine has halted.
  bool step() {
    if (dp.halted) return false;

    const MicroWord& word = cm.at(seq.micro_pc);
    last_word_ = word;
    last_ = Codec::decode(word);

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

    try {
      dp.execute(last_.sig, last_.muimm);
    } catch (...) {
      dp.halted = true;
      dp.halt_reason = "exception";
      throw;
    }

    seq.advance(last_.sig, dp.bus.is_m_branch, switch_target);
    dp.regs.micro_pc = seq.micro_pc;
    return !dp.halted;
  }

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

}  // namespace risc201::cu

#endif  // RISC201_CU_MICRO_CONTROL_UNIT_H
