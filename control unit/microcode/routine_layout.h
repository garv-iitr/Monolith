#ifndef RISC201_CU_ROUTINE_LAYOUT_H
#define RISC201_CU_ROUTINE_LAYOUT_H

// ============================================================================
// Microprogram placement.
//
// Resolves the symbolic program into absolute control-memory addresses:
//   address 0..3 : the shared preamble
//   address 4..  : the per-opcode routines, laid back to back
//
// Routine-local labels are resolved to absolute addresses here, so both the
// horizontal and vertical assemblers can consume a single resolved listing.
// ============================================================================

#include <cstdint>
#include <utility>
#include <vector>

#include "microcode/micro_asm.h"
#include "microcode/microcode_program.h"

namespace risc201::cu {

struct PlacedMicroStmt {
  micro::MicroStmt stmt;
  std::uint16_t addr = 0;
  std::uint16_t target = 0;  // resolved branch target
};

struct MicroProgramLayout {
  std::uint16_t preamble_addr = 0;
  std::vector<PlacedMicroStmt> stmts;
  std::vector<std::pair<std::uint8_t, std::uint16_t>> entries;  // opcode -> start

  std::uint16_t entry_of(Opcode op) const {
    for (const auto& e : entries) {
      if (e.first == static_cast<std::uint8_t>(op)) return e.second;
    }
    return 0;
  }
};

inline MicroProgramLayout layout_program(const Program& program) {
  MicroProgramLayout out;

  // Resolve one routine at a given start address.
  auto place = [&out](const micro::Routine& routine, std::uint16_t start) {
    std::uint16_t addr = start;
    std::vector<std::uint16_t> label_addr(64, 0);

    const std::size_t first = out.stmts.size();
    for (const micro::MicroStmt& s : routine) {
      if (s.label >= 0 && s.label < 64) label_addr[s.label] = addr;
      out.stmts.push_back(PlacedMicroStmt{s, addr, 0});
      ++addr;
    }
    // Second pass: resolve targets now that all labels are known.
    for (std::size_t i = first; i < out.stmts.size(); ++i) {
      const micro::MicroStmt& s = out.stmts[i].stmt;
      std::uint16_t t = 0;
      if (s.target == micro::kPreamble) t = out.preamble_addr;
      else if (s.target >= 0 && s.target < 64) t = label_addr[s.target];
      out.stmts[i].target = t;
    }
    return addr;
  };

  out.preamble_addr = 0;
  std::uint16_t next = place(micro::preamble(), 0);

  for (const ProgramEntry& e : program) {
    out.entries.emplace_back(static_cast<std::uint8_t>(e.opcode), next);
    next = place(e.routine, next);
  }
  return out;
}

}  // namespace risc201::cu

#endif  // RISC201_CU_ROUTINE_LAYOUT_H
