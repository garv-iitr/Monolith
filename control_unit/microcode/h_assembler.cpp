#include "microcode/h_assembler.h"

#include "microcode/routine_layout.h"

namespace risc201::cu {

ControlMemory<HorizontalMicroWord> assemble_horizontal(const Program& program) {
  const MicroProgramLayout layout = layout_program(program);

  ControlMemory<HorizontalMicroWord> cm;
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
  for (const auto& e : layout.entries) cm.set_entry(e.first, e.second);
  return cm;
}

}
