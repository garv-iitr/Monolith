// Implements assembly translation from microcode routines into populated horizontal ControlMemory ROM.
#include "microcode/h_assembler.h"

#include "microcode/routine_layout.h"

namespace risc201::cu {

// Assembles a high-level microprogram into horizontal control memory with pre-decoded hardware control signals
ControlMemory<HorizontalMicroWord> assemble_horizontal(const Program& program) {
  // 1. Link program, allocate sequential physical ROM addresses, and resolve symbolic jump labels
  const MicroProgramLayout layout = layout_program(program);

  ControlMemory<HorizontalMicroWord> cm;
  // 2. Synthesize pre-decoded control signals for each micro-statement and write into ROM
  for (const PlacedMicroStmt& p : layout.stmts) {
    HorizontalMicroWord w;
    Word muimm = 0;
    micro::micro_semantics(p.stmt.op, p.stmt.src, p.stmt.dest, p.stmt.imm, p.target,
                           p.stmt.args, p.stmt.stage, w.sig, muimm);
    w.imm = p.stmt.imm;
    w.target = p.target;
    w.op = p.stmt.op;
    cm.set(p.addr, w);
  }
  // 3. Populate opcode dispatch entry table
  for (const auto& e : layout.entries) cm.set_entry(e.first, e.second);
  return cm;
}

}
