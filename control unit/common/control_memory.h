#ifndef RISC201_CU_CONTROL_MEMORY_H
#define RISC201_CU_CONTROL_MEMORY_H

// ============================================================================
// Control memory: the microprogram ROM plus the opcode -> microcode entry
// point dispatch table.  Sarangi sizes this at 1024 microinstructions with a
// 10-bit microPC; the RISC201 ISA uses a 6-bit opcode, so the dispatch table
// covers 64 entries.
//
// The template is parameterised on the microinstruction word so the same ROM
// type backs both the horizontal and the vertical control unit.
// ============================================================================

#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>

namespace risc201::cu {

constexpr std::size_t kControlMemoryWords = 1024;
constexpr std::size_t kOpcodeSpace        = 64;

template <typename MicroWord>
class ControlMemory {
 public:
  static constexpr std::size_t kSize = kControlMemoryWords;
  static constexpr std::uint16_t kInvalidEntry = 0xFFFF;

  ControlMemory() { entry_.fill(kInvalidEntry); }

  void set(std::uint16_t addr, const MicroWord& word) {
    if (addr >= kSize) throw std::out_of_range("control memory address out of range");
    rom_[addr] = word;
    if (static_cast<std::size_t>(addr) + 1 > used_) used_ = static_cast<std::size_t>(addr) + 1;
  }

  const MicroWord& at(std::uint16_t addr) const {
    return rom_.at(static_cast<std::size_t>(addr));
  }

  void set_entry(std::uint8_t opcode, std::uint16_t addr) {
    entry_[opcode % kOpcodeSpace] = addr;
  }

  std::uint16_t entry(std::uint8_t opcode) const {
    const std::uint16_t e = entry_[opcode % kOpcodeSpace];
    if (e == kInvalidEntry) throw std::out_of_range("no microcode entry point for opcode");
    return e;
  }

  bool has_entry(std::uint8_t opcode) const {
    return entry_[opcode % kOpcodeSpace] != kInvalidEntry;
  }

  std::size_t used() const { return used_; }

 private:
  std::array<MicroWord, kSize> rom_{};
  std::array<std::uint16_t, kOpcodeSpace> entry_{};
  std::size_t used_ = 0;
};

}  // namespace risc201::cu

#endif  // RISC201_CU_CONTROL_MEMORY_H
