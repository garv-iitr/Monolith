#include "microcode/v_assembler.h"

#include "microcode/routine_layout.h"

namespace risc201::cu {

ControlMemory<VerticalMicroWord> assemble_vertical(const Program& program) {
  const MicroProgramLayout layout = layout_program(program);

  ControlMemory<VerticalMicroWord> cm;
  for (const PlacedMicroStmt& p : layout.stmts) {
    VerticalMicroWord w;
    w.op = p.stmt.op;
    w.src = p.stmt.src;
    w.dest = p.stmt.dest;
    w.imm = p.stmt.imm & 0xFFF;        // 12-bit two's-complement field
    w.target = p.target & 0x3FF;       // 10-bit microPC field
    w.args = p.stmt.args & 0x3FF;      // 10-bit unit args field
    w.stage = p.stmt.stage;
    cm.set(p.addr, w);
  }
  for (const auto& e : layout.entries) cm.set_entry(e.first, e.second);
  return cm;
}

}  // namespace risc201::cu
