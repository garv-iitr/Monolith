// Hardware Control Memory (ROM) emulation storing microinstructions and opcode dispatch entry tables.
#ifndef RISC201_CU_CONTROL_MEMORY_H
#define RISC201_CU_CONTROL_MEMORY_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>

namespace risc201::cu {

// Control memory sizing: 1024 micro-words maximum depth, 64-entry opcode dispatch table (6-bit opcodes)
constexpr std::size_t kControlMemoryWords = 1024;
constexpr std::size_t kOpcodeSpace        = 64;

// Emulated Control Memory (ROM) holding horizontal or vertical microinstructions and the opcode entry-point map
template <typename MicroWord>
class ControlMemory {
 public:
  static constexpr std::size_t kSize = kControlMemoryWords;
  static constexpr std::uint16_t kInvalidEntry = 0xFFFF;

  // Initializes all opcode entry points to kInvalidEntry
  ControlMemory() { entry_.fill(kInvalidEntry); }

  // Writes a micro-word into control memory at the given address and updates the count of used words
  void set(std::uint16_t addr, const MicroWord& word) {
    if (addr >= kSize) throw std::out_of_range("control memory address out of range");
    rom_[addr] = word;
    if (static_cast<std::size_t>(addr) + 1 > used_) used_ = static_cast<std::size_t>(addr) + 1;
  }

  // Fetches a micro-word from control memory by address for execution
  const MicroWord& at(std::uint16_t addr) const {
    return rom_.at(static_cast<std::size_t>(addr));
  }

  // Maps an ISA opcode (6-bit) to the starting micro-address of its execution routine
  void set_entry(std::uint8_t opcode, std::uint16_t addr) {
    entry_[opcode % kOpcodeSpace] = addr;
  }

  // Looks up the starting micro-address for an opcode; throws if no entry is configured
  std::uint16_t entry(std::uint8_t opcode) const {
    const std::uint16_t e = entry_[opcode % kOpcodeSpace];
    if (e == kInvalidEntry) throw std::out_of_range("no microcode entry point for opcode");
    return e;
  }

  // Returns true if the opcode has a configured microcode routine
  bool has_entry(std::uint8_t opcode) const {
    return entry_[opcode % kOpcodeSpace] != kInvalidEntry;
  }

  // Returns the highest allocated address + 1 in control memory
  std::size_t used() const { return used_; }

 private:
  // Internal physical ROM array and direct-mapped opcode jump table
  std::array<MicroWord, kSize> rom_{};
  std::array<std::uint16_t, kOpcodeSpace> entry_{};
  std::size_t used_ = 0;
};

}

#endif
