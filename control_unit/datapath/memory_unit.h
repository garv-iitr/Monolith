#ifndef RISC201_CU_MEMORY_UNIT_H
#define RISC201_CU_MEMORY_UNIT_H

#include <cstddef>
#include <vector>

#include "common/control_signals.h"
#include "common/internal_registers.h"
#include "isa_spec/isa.h"
#include "memory_safety/memory_map.h"

namespace risc201::cu {

class MemoryUnit {
 public:
  struct WriteDelta {
    std::size_t index;
    Word previous;
  };

  static constexpr std::size_t kDefaultWords =
      risc201::safety::MemoryMap::kAddressSpaceWords;

  explicit MemoryUnit(std::size_t words = kDefaultWords);

  void reset();
  std::size_t words() const { return data_.size(); }
  std::vector<Word> snapshot() const { return data_; }
  std::size_t begin_journal();
  std::vector<WriteDelta> take_journal(std::size_t mark);
  void restore_journal(const std::vector<WriteDelta>& deltas);
  void clear_journal() { journal_.clear(); journal_active_ = false; }

  // present means word-aligned and inside the array
  bool present(Word byte_addr) const;

  Word read_word(Word byte_addr) const;
  void write_word(Word byte_addr, Word value);
  Word fetch(Word byte_pc) const;

  void load_words(Word byte_addr, const std::vector<Word>& words);

  void tick(const InternalRegisters& cur, const ControlSignals& sig, InternalRegisters& next);

 private:
  std::vector<Word> data_;
  std::vector<WriteDelta> journal_;
  bool journal_active_ = false;
};

}

#endif
