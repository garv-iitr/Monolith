// ============================================================
// disassembler.hpp — RISC201 disassembler public API
// ============================================================
#pragma once

#include "isa.hpp"
#include <cstdint>
#include <map>
#include <string>
#include <vector>

// ============================================================
// Disassembler — decode a sequence of 32-bit words to assembly
// ============================================================
class Disassembler {
public:
    // Disassemble 'words' starting at 'baseAddr'.
    //   plain:    if true, omit address/hex columns (for re-assembly)
    //   rawRegs:  if true, never use sp/ra aliases
    // Returns a multi-line string of assembly text.
    std::string disassemble(const std::vector<uint32_t>& words,
                            uint32_t baseAddr = 0,
                            bool plain    = false,
                            bool rawRegs  = false) const;
};

// ============================================================
// Input format detection and loading utilities
// ============================================================

// Format of the input file
enum class DisasmFmt { HEX, BIN, RAW };

// Auto-detect the format of an input file.
// If 'hint' is provided it overrides detection.
DisasmFmt detectFormat(const std::string& filename);

// Load words from a file in the given format.
// Big-endian byte order for RAW.
std::vector<uint32_t> loadWords(const std::string& filename, DisasmFmt fmt);
