#include "simulator_core/core/memory.h"

#include <algorithm>
#include <cstdint>

#include "simulator_core/core/exception.h"

Memory::Memory(std::size_t words) : data_(words, Word{0}) {}

Word Memory::read_word(std::size_t addr) const {
  if (addr >= data_.size()) {
    throw RuntimeException{ExceptionCode::BAD_MEM_ACCESS,
                           static_cast<std::uint64_t>(addr)};
  }
  return data_[addr];
}

void Memory::write_word(std::size_t addr, Word value) {
  if (addr >= data_.size()) {
    throw RuntimeException{ExceptionCode::BAD_MEM_ACCESS,
                           static_cast<std::uint64_t>(addr)};
  }
  data_[addr] = value;
}

void Memory::load_words(std::size_t addr, const std::vector<Word>& words) {
  for (std::size_t i = 0; i < words.size(); ++i) {
    write_word(addr + i, words[i]);
  }
}

void Memory::clear() {
  std::fill(data_.begin(), data_.end(), Word{0});
}
