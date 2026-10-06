#ifndef RISC201_CU_MICRO_SEQUENCER_H
#define RISC201_CU_MICRO_SEQUENCER_H

#include <cstdint>

#include "common/control_signals.h"
#include "common/mux.h"

namespace risc201::cu {

struct MicroSequencer {
  std::uint16_t micro_pc = 0;
  std::uint16_t fetch_addr = 0;

  // m1 sel: 0 = next, 1 = branch target
  Mux2<std::uint16_t> m1;
  Mux4<std::uint16_t> ufetch_mux;

  const char* last_source = "next";

  void reset(std::uint16_t fetch = 0) {
    fetch_addr = fetch;
    micro_pc = fetch;
    last_source = "reset";
  }

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

}

#endif
