// ============================================================
// isa.hpp — RISC201 ISA: register tables, format IDs,
//           InstructionDef, and ISA registry singleton.
//
// HOW TO ADD AN INSTRUCTION:
//   Add ONE new row to kISATable in isa.cpp.
//   A new Format subclass (formats.hpp/cpp) is only needed
//   when the encoding layout is genuinely different from the
//   existing eight formats.
// ============================================================
#pragma once

#include <cstdint>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

// ============================================================
// FmtId — identifies which Format subclass handles encoding
// ============================================================
enum class FmtId : uint8_t {
    F1 = 1, // ALU 3-operand  (ADD SUB MUL DIV MOD AND OR XOR LSL LSR ASR)
    F2,     // ALU 2-operand  (NOT MOV)
    F3,     // Compare        (CMP)
    F4,     // Memory         (LD ST FLD FST BNDCHK)
    F5,     // Branch/call    (B BEQ BNE BGT BLT CALL)
    F6,     // No operands    (NOP RET HALT)
    F7,     // FPU 3-register (FADD FSUB FMUL FDIV)
    F8,     // FPU cmp/move   (FCMP FMOV)
};

// ============================================================
// Register kind — GPR (r0..r15) or FPR (f0..f15)
// ============================================================
enum class RegKind { GPR, FPR };

// ============================================================
// InstructionDef — one row in the ISA table
// ============================================================
struct InstructionDef {
    const char* mnemonic; // uppercase canonical mnemonic
    uint8_t     opcode;   // 6-bit value stored in bits [31:26]
    FmtId       fmt;      // format class that handles this instruction
};

// ============================================================
// ISA — singleton registry; lookup by mnemonic or opcode
// ============================================================
class ISA {
public:
    // Access the singleton (constructed once on first call)
    static const ISA& instance();

    // Lookup by mnemonic (case-insensitive).
    // Throws std::runtime_error with a descriptive message if not found.
    const InstructionDef& byMnemonic(const std::string& mnemonic) const;

    // Lookup by 6-bit opcode.
    // Returns nullptr if the opcode is not in the table.
    const InstructionDef* byOpcode(uint8_t opcode) const;

    // Iterate over all definitions (e.g., for documentation tools)
    const std::vector<InstructionDef>& all() const { return defs_; }

    // Check whether a mnemonic exists (case-insensitive), without throwing
    bool hasMnemonic(const std::string& mnemonic) const;

    // --------------------------------------------------------
    // Register utilities
    // --------------------------------------------------------

    struct RegInfo {
        RegKind kind;
        int     index; // 0..15, or -1 if the name is not a valid register
    };

    // Parse a register name (case-insensitive).
    // Returns {kind, -1} on failure; the caller is responsible for
    // reporting the error.
    static RegInfo parseReg(const std::string& name);

    // Canonical GPR display name.
    // With rawRegs=false (default): r14 -> "sp", r15 -> "ra".
    static std::string gprName(int index, bool rawRegs = false);

    // Canonical FPR display name: "f0".."f15"
    static std::string fprName(int index);

private:
    ISA(); // private; builds the tables from kISATable

    std::vector<InstructionDef>      defs_;
    std::map<std::string, uint32_t>  byMnem_; // upper-case mnemonic -> index
    std::map<uint8_t,     uint32_t>  byOp_;   // opcode -> index
};
