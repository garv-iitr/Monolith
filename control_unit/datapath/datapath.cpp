// Implements datapath clock cycle execution, bus routing, register latching, and unit clocking.
#include "datapath/datapath.h"

namespace risc201::cu {

// Allocates datapath resources and initial memory array
Datapath::Datapath(std::size_t mem_words) : mem(mem_words) {}

// Resets internal registers, general register file, memory array, and status flags
void Datapath::reset() {
  regs.reset();
  rf.reset();
  mem.reset();
  halted = false;
  halt_reason = "";
  last_stage = StageClass::FETCH;
}

// Executes one micro-cycle across the datapath according to control signals and micro-immediate
void Datapath::execute(const ControlSignals& sig, Word muimm) {
  last_stage = sig.stage;

  // 1. Process CPU halt control strobe
  if (sig.halt) {
    halted = true;
    return;
  }

  // Work on next-state register container to model synchronous clock latching
  InternalRegisters next = regs;

  // 2. Instruction fetch strobe: Load machine instruction directly from Memory[PC] into IR
  if (sig.fetch_ir) {
    next.ir = mem.fetch(regs.pc);
  }

  // 3. Instruction decode: Extract register fields (rd, rs1, rs2) and expand immediates
  decode.eval(regs, next);

  // 4. Source register drive: Read value from selected source register onto the write bus
  const Word write_val =
      (sig.write_src == InternalReg::NONE) ? Word{0} : regs.get(sig.write_src);

  // 5. Shared bus evaluation: Compute adder sum, equality branch flag, and multiplex to read_bus
  bus.eval(write_val, muimm, sig.transfer_sel);

  // 6. Register write-enable latching: Write read_bus data into all registers selected by read_mask
  for (int i = 1; i < static_cast<int>(kNumInternalRegs); ++i) {
    const InternalReg r = static_cast<InternalReg>(i);
    if (sig.reads(r)) next.set(r, bus.read_bus);
  }

  // 7. Subunit execution: Clock register file read/write operations
  // alu/fpu/memchk live in other files, we just emit their args on the bus
  rf.tick(next, sig, next);

  // 8. Internal ALU pass-through modes (optimizations for MOV and NOT operations)
  if (sig.alu_op == AluOp::PASS_B) {
    next.alu_result = next.B;
  } else if (sig.alu_op == AluOp::PASS_A) {
    next.alu_result = next.A;
  }

  // 9. Memory subsystem execution: Clock load/store data operations via MAR/MDR
  mem.tick(next, sig, next);

  // 10. Synchronous state commit: Latch next-cycle registers
  regs = next;
}

}
