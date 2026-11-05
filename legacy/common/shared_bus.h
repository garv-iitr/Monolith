// Internal processor interconnect bus implementing register-to-register transfers, immediate injection, and bus arithmetic.
#ifndef RISC201_CU_SHARED_BUS_H
#define RISC201_CU_SHARED_BUS_H

#include "common/control_signals.h"
#include "common/mux.h"

namespace risc201::cu {

// Shared internal transfer bus connecting all internal registers, immediate generators, and sequencer flags
struct SharedBus {
  Word write_bus = 0;       // Data driven by the selected source register (write_src)
  Word read_bus = 0;        // Multiplexed bus line received by all destination registers (read_mask)
  Word mu_imm = 0;          // Microinstruction immediate operand value
  Word adder_out = 0;       // Output of dedicated bus adder (write_bus + mu_imm)
  bool is_m_branch = false; // Equality flag (write_bus == mu_imm) used by conditional micro-branches (mbeq)

  // 3-to-1 transfer multiplexer selecting what drives read_bus: 0=write_bus, 1=mu_imm, 2=adder_out
  Mux3<Word> transfer_mux;

  // Evaluates bus transfers, bus addition, branch condition comparison, and routes output to read_bus
  void eval(Word write_val, Word muimm, TransferSel sel) {
    write_bus = write_val;
    mu_imm = muimm;
    adder_out = write_bus + mu_imm;
    // Evaluate equality condition for microcode conditional branches (e.g. testing I flag or register values)
    // is_m_branch is just write_bus == mu_imm, thats what mbeq tests
    is_m_branch = (write_bus == mu_imm);

    // Route selected source to the read bus
    transfer_mux.in[0] = write_bus;
    transfer_mux.in[1] = mu_imm;
    transfer_mux.in[2] = adder_out;
    transfer_mux.sel = static_cast<std::size_t>(sel);
    transfer_mux.eval();
    read_bus = transfer_mux.out;
  }
};

}

#endif
