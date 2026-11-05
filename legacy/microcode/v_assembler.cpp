// Major use: Implements packing and assembly translation of microcode routines into vertical ControlMemory ROM.
#include "microcode/v_assembler.h"

#include "microcode/routine_layout.h"

namespace risc201::cu {

// Compiles high-level microcode into compact 45-bit vertical micro-words and populates control memory
ControlMemory<VerticalMicroWord> assemble_vertical(const Program& program) {
  // 1. Link routines, assign sequential addresses, and resolve branch targets
  const MicroProgramLayout layout = layout_program(program);

  ControlMemory<VerticalMicroWord> cm;
  // 2. Pack each micro-statement into compressed bitfields (12-bit imm, 10-bit target, 10-bit args)
  for (const PlacedMicroStmt& p : layout.stmts) {
    VerticalMicroWord w;
    w.op = p.stmt.op;
    w.src = p.stmt.src;
    w.dest = p.stmt.dest;
    w.imm = p.stmt.imm & 0xFFF;
    w.target = p.target & 0x3FF;
    w.args = p.stmt.args & 0x3FF;
    w.stage = p.stmt.stage;
    cm.set(p.addr, w);
  }
  // 3. Populate opcode dispatch entry table
  for (const auto& e : layout.entries) cm.set_entry(e.first, e.second);
  return cm;
}

}
