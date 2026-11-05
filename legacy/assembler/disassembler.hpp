#pragma once

#include "isa.hpp"
#include <cstdint>
#include <map>
#include <string>
#include <vector>

class Disassembler {
public:
    std::string disassemble(const std::vector<uint32_t>& words,
                            uint32_t baseAddr = 0,
                            bool plain    = false,
                            bool rawRegs  = false) const;
};


enum class DisasmFmt { HEX, BIN, RAW };

DisasmFmt detectFormat(const std::string& filename);

std::vector<uint32_t> loadWords(const std::string& filename, DisasmFmt fmt);
