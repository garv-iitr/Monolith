#ifndef RISC201_CU_SHARED_BUS_H
#define RISC201_CU_SHARED_BUS_H

// ============================================================================
// The shared bus (Sarangi, sec. 9.7).
//
//   registers --(write bus)--> [ transfer mux ] --(read bus)--> registers
//                                   ^      ^
//                                muimm   micro-adder (write bus + muimm)
//
// Exactly one register drives the write bus.  The transfer multiplexer picks
// between the write bus, the sign-extended micro-immediate (muimm) and the
// micro-adder output; the result is the read bus, which any number of
// registers may latch.  isMBranch is the equality of the write bus and muimm
// and is what the mbeq microinstruction branches on.
// ============================================================================

#include "common/control_signals.h"
#include "common/mux.h"

namespace risc201::cu {

struct SharedBus {
  Word write_bus = 0;
  Word read_bus = 0;
  Word mu_imm = 0;      // sign-extended micro-immediate
  Word adder_out = 0;   // micro-adder output
  bool is_m_branch = false;

  Mux3<Word> transfer_mux;  // { write_bus, mu_imm, adder_out }

  // Combinational evaluation for one micro-cycle.  `write_val` is the value on
  // the write bus (driven by the selected register), `muimm` is already
  // sign-extended.
  void eval(Word write_val, Word muimm, TransferSel sel) {
    write_bus = write_val;
    mu_imm = muimm;
    adder_out = write_bus + mu_imm;
    is_m_branch = (write_bus == mu_imm);

    transfer_mux.in[0] = write_bus;
    transfer_mux.in[1] = mu_imm;
    transfer_mux.in[2] = adder_out;
    transfer_mux.sel = static_cast<std::size_t>(sel);
    transfer_mux.eval();
    read_bus = transfer_mux.out;
  }
};

}  // namespace risc201::cu

#endif  // RISC201_CU_SHARED_BUS_H
