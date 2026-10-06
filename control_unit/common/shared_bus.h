#ifndef RISC201_CU_SHARED_BUS_H
#define RISC201_CU_SHARED_BUS_H

#include "common/control_signals.h"
#include "common/mux.h"

namespace risc201::cu {

struct SharedBus {
  Word write_bus = 0;
  Word read_bus = 0;
  Word mu_imm = 0;
  Word adder_out = 0;
  bool is_m_branch = false;

  Mux3<Word> transfer_mux;

  void eval(Word write_val, Word muimm, TransferSel sel) {
    write_bus = write_val;
    mu_imm = muimm;
    adder_out = write_bus + mu_imm;
    // is_m_branch is just write_bus == mu_imm, thats what mbeq tests
    is_m_branch = (write_bus == mu_imm);

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
