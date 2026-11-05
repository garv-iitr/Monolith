// Hardware micro-sequencer logic calculating the next micro-PC via multiplexers and branch comparator flags.
#ifndef RISC201_CU_MICRO_SEQUENCER_H
#define RISC201_CU_MICRO_SEQUENCER_H

#include <cstdint>

#include "common/control_signals.h"
#include "common/mux.h"

namespace risc201::cu {

// Next-micro-address generation hardware consisting of micro-PC latch, branch mux, and source mux
struct MicroSequencer {
  std::uint16_t micro_pc = 0;   // Current Control Memory address pointer
  std::uint16_t fetch_addr = 0; // Preamble reset address (typically 0)

  // Hardware multiplexers:
  // m1: 2-to-1 mux selecting between sequential address (next) and branch target based on condition flag
  // m1 sel: 0 = next, 1 = branch target
  Mux2<std::uint16_t> m1;
  // ufetch_mux: 4-to-1 main sequencing multiplexer
  Mux4<std::uint16_t> ufetch_mux;

  const char* last_source = "next"; // Debug string tracking the source of the last micro-PC transition

  // Resets the sequencer to the designated fetch address
  void reset(std::uint16_t fetch = 0) {
    fetch_addr = fetch;
    micro_pc = fetch;
    last_source = "reset";
  }

  // Computes the next micro-PC value by evaluating the active control signals, micro-branch flag, and opcode dispatch target
  void advance(const ControlSignals& sig, bool is_m_branch, std::uint16_t switch_target) {
    const std::uint16_t next = static_cast<std::uint16_t>(micro_pc + 1);

    // Evaluate conditional branch mux (m1):
    // If is_m_branch is true (write_bus == mu_imm), select jump_target; else sequential next
    m1.in[0] = next;
    m1.in[1] = sig.jump_target;
    m1.sel = is_m_branch ? 1 : 0;
    m1.eval();

    // Fast-path return to instruction fetch preamble
    if (sig.next_addr == NextAddr::FETCH) {
      micro_pc = fetch_addr;
      last_source = "fetch";
      return;
    }

    // Configure main next-address multiplexer based on sequencer command
    std::size_t sel = 0;
    switch (sig.next_addr) {
      case NextAddr::INCREMENT:   sel = 0; last_source = "next";   break;
      case NextAddr::JUMP:        sel = 1; last_source = "target"; break;
      case NextAddr::MSWITCH:     sel = 2; last_source = "switch"; break;
      case NextAddr::BRANCH_COND: sel = 3; last_source = "branch"; break;
      case NextAddr::FETCH:       sel = 0; break;
    }

    // ufetch_mux inputs: 0=next, 1=unconditional jump, 2=opcode dispatch, 3=conditional branch output
    ufetch_mux.in[0] = next;
    ufetch_mux.in[1] = sig.jump_target;
    ufetch_mux.in[2] = switch_target;
    ufetch_mux.in[3] = m1.out;
    ufetch_mux.sel = sel;
    ufetch_mux.eval();
    micro_pc = ufetch_mux.out;
  }
};

}

#endif
