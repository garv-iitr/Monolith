// Hardware memory interface modeling word-aligned data loads, stores, and instruction fetches.
#ifndef RISC201_CU_MEMORY_UNIT_H
#define RISC201_CU_MEMORY_UNIT_H

#include <cstddef>
#include <vector>

#include "common/control_signals.h"
#include "common/internal_registers.h"
#include "isa_spec/isa.h"

namespace risc201::cu {

// Emulated unified memory controller providing instruction fetch and data read/write ports
class MemoryUnit {
 public:
  static constexpr std::size_t kDefaultWords = 4096; // Default memory capacity (16 KB)

  // Constructs memory with the given word capacity
  explicit MemoryUnit(std::size_t words = kDefaultWords);

  // Clears all memory contents to zero
  void reset();

  // Returns total memory capacity in words
  std::size_t words() const { return data_.size(); }

  // Verifies that a byte address is word-aligned (multiple of 4) and within bounds
  // present means word-aligned and inside the array
  bool present(Word byte_addr) const;

  // Reads a 32-bit word at the specified byte address; throws RuntimeException if invalid
  Word read_word(Word byte_addr) const;

  // Writes a 32-bit word to the specified byte address; throws RuntimeException if invalid
  void write_word(Word byte_addr, Word value);

  // Instruction fetch port reading word at byte_pc
  Word fetch(Word byte_pc) const;

  // Bulk-initializes memory with an array of words starting at byte_addr (used by program loaders)
  void load_words(Word byte_addr, const std::vector<Word>& words);

  // Hardware clock step: processes MemOp::READ (MAR -> ld_result) and MemOp::WRITE (MDR -> Mem[MAR])
  void tick(const InternalRegisters& cur, const ControlSignals& sig, InternalRegisters& next);

 private:
  std::vector<Word> data_; // Underlying word-indexed storage array
};

}

#endif
