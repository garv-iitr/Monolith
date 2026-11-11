// State container for all microarchitectural internal registers accessible across microcode cycles.
#ifndef RISC201_CU_INTERNAL_REGISTERS_H
#define RISC201_CU_INTERNAL_REGISTERS_H

#include <cstdint>

#include "isa_spec/isa.h"

namespace risc201::cu {

// Storage for all microarchitectural registers manipulated by the micro-control unit and datapath
struct InternalRegisters {
  // --- Program Sequencing & Instruction Fetch ------------------------------
  Word pc = 0;              // Architectural Program Counter
  Word ir = 0;              // Instruction Register holding currently fetched 32-bit machine word

  // --- Instruction Decode Unit Outputs -------------------------------------
  std::uint8_t mode = 0;    // Immediate mode bits [25:24]
  bool         I = false;   // Immediate presence flag (true if mode != 0)
  std::uint8_t rd = 0;      // Destination register index [23:20]
  std::uint8_t rs1 = 0;     // First source register index [19:16]
  std::uint8_t rs2 = 0;     // Second source register index [12:15]
  Word      immx = 0;       // Sign/zero/high-extended 32-bit immediate operand
  Word      branch_target = 0; // Precalculated PC-relative branch target (pc + offset*4)

  // --- Register File Interface Latches -------------------------------------
  std::uint8_t reg_src = 0; // Register file address port (selected register index)
  Word         reg_data = 0;// Register file write port data buffer
  Word         reg_val = 0; // Register file read port output latch

  // --- Integer ALU Interface Registers -------------------------------------
  Word A = 0;               // Primary ALU operand latch (input A)
  Word B = 0;               // Secondary ALU operand latch (input B: register or immediate)
  Word alu_result = 0;      // Output buffer holding integer ALU computation result
  Flags flags{};            // Architectural CPU status flags (Z, N, C, V)
  bool  flags_E = false;    // Equality condition flag mirror (Z)
  bool  flags_GT = false;   // Greater-than condition flag mirror (!N && !Z)
  bool  flags_N = false;    // Negative condition flag mirror (N)

  // --- Memory Bus Interface Latches ----------------------------------------
  Word mar = 0;             // Memory Address Register driving memory bus address
  Word mdr = 0;             // Memory Data Register buffering data for memory write
  Word ld_result = 0;       // Load Data Buffer holding data fetched from memory

  // --- Floating Point Unit Interface Latches -------------------------------
  std::uint8_t fp_src = 0;  // FPU register address port
  Word         fp_data = 0; // FPU register write buffer
  Word         fp_val = 0;  // FPU register read buffer
  Word         fp_a = 0;    // FPU input operand A
  Word         fp_b = 0;    // FPU input operand B
  Word         fpu_result = 0; // FPU output result buffer
  bool         fcc = false; // Floating-point condition code flag

  // --- Memory Safety Firewall Registers ------------------------------------
  Word memchk_addr = 0;     // Target address submitted to bounds checker
  bool memchk_status = false;// Bounds check pass/fail status flag

  // --- Micro-Sequencer State -----------------------------------------------
  std::uint16_t micro_pc = 0;// Current execution address in Control Memory ROM
};

}  // namespace risc201::cu

#endif
