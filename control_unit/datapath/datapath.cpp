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

  // 1. Instruction fetch path (mloadIR).
  if (sig.fetch_ir) {
    next.ir = mem.fetch(regs.pc);
  }

  // 2. Decode unit is combinational on the (current) instruction register.
  decode.eval(regs, next);

  // 3. Drive the write bus from the selected register.
  const Word write_val =
      (sig.write_src == InternalReg::NONE) ? Word{0} : regs.get(sig.write_src);

  // 4. Micro-adder + transfer multiplexer -> read bus.
  bus.eval(write_val, muimm, sig.transfer_sel);

  // 5. Latch every register selected by the read mask.
  for (int i = 1; i < static_cast<int>(kNumInternalRegs); ++i) {
    const InternalReg r = static_cast<InternalReg>(i);
    if (sig.reads(r)) next.set(r, bus.read_bus);
  }

  // 6. Functional units owned by this module (they read the freshly-latched
  //    inputs).  ALU / FPU / memory-check args are emitted on the bus for
  //    Atharva's and Paraas's units; they latch their results from the shared
  //    internal registers at integration time.
  rf.tick(next, sig, next);
  if (sig.alu_op == AluOp::PASS_B) {
    next.alu_result = next.B;
  } else if (sig.alu_op == AluOp::PASS_A) {
    next.alu_result = next.A;
  }
  mem.tick(next, sig, next);

  regs = next;
}

}  // namespace risc201::cu
