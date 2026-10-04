#ifndef RISC201_CU_MICRO_SEQUENCER_H
#define RISC201_CU_MICRO_SEQUENCER_H

// ============================================================================
// Micro-sequencer (Sarangi, sec. 9.8).
//
// Holds the microPC and the micro-fetch multiplexer.  Each cycle the next
// microPC is one of:
//   * microPC + 1              (mloadIR, mdecode, mmov, mmovi, madd)
//   * the microcode entry point of the current opcode (mswitch)
//   * the micro-branch target  (mb)
//   * microPC + 1 or the target, selected by isMBranch (mbeq)
//   * the preamble address     (end of instruction -> next fetch)
//
// The multiplexer tree is exactly the one drawn in figs. 9.25 / 9.27: a 2:1
// M1 picks between "next" and "target" using isMBranch, and the 4:1 micro-fetch
// multiplexer chooses the final value.
// ============================================================================

#include <cstdint>

#include "common/control_signals.h"
#include "common/mux.h"

namespace risc201::cu {

struct MicroSequencer {
  std::uint16_t micro_pc = 0;
  std::uint16_t fetch_addr = 0;

  // Combinational hardware of the micro-fetch path.
  Mux2<std::uint16_t> m1;          // { next, branch_target }      sel = isMBranch
  Mux4<std::uint16_t> ufetch_mux;  // { next, target, switch, m1 }

  const char* last_source = "next";

  void reset(std::uint16_t fetch = 0) {
    fetch_addr = fetch;
    micro_pc = fetch;
    last_source = "reset";
  }

  // Compute the next microPC.  `switch_target` is entry_point(opcode), only
  // meaningful when sig.next_addr == MSWITCH.
  void advance(const ControlSignals& sig, bool is_m_branch, std::uint16_t switch_target) {
    const std::uint16_t next = static_cast<std::uint16_t>(micro_pc + 1);

    m1.in[0] = next;
    m1.in[1] = sig.jump_target;
    m1.sel = is_m_branch ? 1 : 0;
    m1.eval();

    if (sig.next_addr == NextAddr::FETCH) {
      micro_pc = fetch_addr;
      last_source = "fetch";
      return;
    }

    std::size_t sel = 0;
    switch (sig.next_addr) {
      case NextAddr::INCREMENT:   sel = 0; last_source = "next";   break;
      case NextAddr::JUMP:        sel = 1; last_source = "target"; break;
      case NextAddr::MSWITCH:     sel = 2; last_source = "switch"; break;
      case NextAddr::BRANCH_COND: sel = 3; last_source = "branch"; break;
      case NextAddr::FETCH:       sel = 0; break;
    }

    ufetch_mux.in[0] = next;
    ufetch_mux.in[1] = sig.jump_target;
    ufetch_mux.in[2] = switch_target;
    ufetch_mux.in[3] = m1.out;
    ufetch_mux.sel = sel;
    ufetch_mux.eval();
    micro_pc = ufetch_mux.out;
  }
};

// The "switch" unit: maps an opcode to the microcode entry point.
struct SwitchUnit {
  std::uint16_t operator()(std::uint8_t opcode, const std::uint16_t* entry_table,
                           std::size_t entries) const {
    if (opcode >= entries) return 0;
    return entry_table[opcode];
  }
};

}  // namespace risc201::cu

#endif  // RISC201_CU_MICRO_SEQUENCER_H
