// Major use: Microcode linker resolving symbolic labels, assigning ROM physical addresses, and generating opcode entry tables.
#ifndef RISC201_CU_ROUTINE_LAYOUT_H
#define RISC201_CU_ROUTINE_LAYOUT_H

#include <cstdint>
#include <utility>
#include <vector>

#include "microcode/micro_asm.h"
#include "microcode/microcode_program.h"

namespace risc201::cu {

// Micro-statement positioned at a concrete physical address with resolved branch targets
struct PlacedMicroStmt {
  micro::MicroStmt stmt;    // Underlying intermediate representation statement
  std::uint16_t addr = 0;   // Absolute physical ROM address allocated to this statement
  std::uint16_t target = 0; // Resolved absolute physical branch/jump target address
};

// Complete binary layout of control memory containing statements and opcode entry points
struct MicroProgramLayout {
  std::uint16_t preamble_addr = 0; // Starting address of the common fetch preamble (address 0)
  std::vector<PlacedMicroStmt> stmts; // Ordered list of all placed micro-statements in ROM
  std::vector<std::pair<std::uint8_t, std::uint16_t>> entries; // Opcode-to-address entry point map

  // Looks up the starting ROM address for an instruction opcode
  std::uint16_t entry_of(Opcode op) const {
    for (const auto& e : entries) {
      if (e.first == static_cast<std::uint8_t>(op)) return e.second;
    }
    return 0;
  }
};

// Two-pass linker laying out microcode routines in ROM and resolving symbolic jump labels
inline MicroProgramLayout layout_program(const Program& program) {
  MicroProgramLayout out;

  // Helper lambda allocating sequential ROM addresses and resolving intra-routine labels
  auto place = [&out](const micro::Routine& routine, std::uint16_t start) {
    std::uint16_t addr = start;
    std::vector<std::uint16_t> label_addr(64, 0);

    const std::size_t first = out.stmts.size();
    // Pass 1: Assign addresses and record definitions for numeric labels [0..63]
    for (const micro::MicroStmt& s : routine) {
      if (s.label >= 0 && s.label < 64) label_addr[s.label] = addr;
      out.stmts.push_back(PlacedMicroStmt{s, addr, 0});
      ++addr;
    }
    // Pass 2: Patch jump and branch targets to absolute physical addresses
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
  // Place universal fetch/decode preamble at ROM address 0
  // preamble sits at 0, the opcode routines follow right after it
  std::uint16_t next = place(micro::preamble(), 0);

  // Sequentially link each opcode subroutine directly following the preamble
  for (const ProgramEntry& e : program) {
    out.entries.emplace_back(static_cast<std::uint8_t>(e.opcode), next);
    next = place(e.routine, next);
  }
  return out;
}

}

#endif
