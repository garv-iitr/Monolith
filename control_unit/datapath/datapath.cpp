#include "datapath/datapath.h"

namespace risc201::cu {

Datapath::Datapath(std::size_t mem_words) : mem(mem_words) {}

void Datapath::reset() {
  regs.reset();
  rf.reset();
  mem.reset();
  halted = false;
  halt_reason = "";
  last_stage = StageClass::FETCH;
}

void Datapath::execute(const ControlSignals& sig, Word muimm) {
  last_stage = sig.stage;
  if (sig.halt) {
    halted = true;
    return;
  }

  InternalRegisters next = regs;

  if (sig.fetch_ir) {
    next.ir = mem.fetch(regs.pc);
  }

  decode.eval(regs, next);

  const Word write_val =
      (sig.write_src == InternalReg::NONE) ? Word{0} : regs.get(sig.write_src);

  bus.eval(write_val, muimm, sig.transfer_sel);

  for (int i = 1; i < static_cast<int>(kNumInternalRegs); ++i) {
    const InternalReg r = static_cast<InternalReg>(i);
    if (sig.reads(r)) next.set(r, bus.read_bus);
  }

  // alu/fpu/memchk live in other files, we just emit their args on the bus
  rf.tick(next, sig, next);
  if (sig.alu_op == AluOp::PASS_B) {
    next.alu_result = next.B;
  } else if (sig.alu_op == AluOp::PASS_A) {
    next.alu_result = next.A;
  }
  mem.tick(next, sig, next);

  regs = next;
}

}
