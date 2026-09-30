#ifndef RISC201_CU_MEMORY_UNIT_H
#define RISC201_CU_MEMORY_UNIT_H

// ============================================================================
// Memory unit (Sarangi, sec. 9.5.5).
//
// Byte-addressed, word-granular data memory.  mar buffers the address, mdr the
// value to store, and a load deposits the data in ldResult one cycle later.
// The instruction fetch uses the same array through `fetch` (combinational),
// which lets mloadIR complete in a single micro-cycle as the book assumes.
// ============================================================================

#include <cstddef>
#include <vector>

#include "common/control_signals.h"
#include "common/internal_registers.h"
#include "isa_spec/isa.h"

namespace risc201::cu {

class MemoryUnit {
 public:
  static constexpr std::size_t kDefaultWords = 4096;

  explicit MemoryUnit(std::size_t words = kDefaultWords);

  void reset();
  std::size_t words() const { return data_.size(); }

  // Alignment + bounds: an address is "present" iff it is word-aligned and
  // inside the array.  This is exactly what the memory-check unit tests.
  bool present(Word byte_addr) const;

  Word read_word(Word byte_addr) const;
  void write_word(Word byte_addr, Word value);
  Word fetch(Word byte_pc) const;

  void load_words(Word byte_addr, const std::vector<Word>& words);

  void tick(const InternalRegisters& cur, const ControlSignals& sig, InternalRegisters& next);

 private:
  std::vector<Word> data_;
};

}  // namespace risc201::cu

#endif  // RISC201_CU_MEMORY_UNIT_H
