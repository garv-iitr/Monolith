#pragma once

#include <cstddef>
#include <vector>

#include "isa_spec/isa.h"

class Memory {
 public:
  static constexpr std::size_t kDefaultWords = 4096;

  explicit Memory(std::size_t words = kDefaultWords);

  std::size_t size() const { return data_.size(); }

  Word read_word(std::size_t addr) const;
  void write_word(std::size_t addr, Word value);

  void load_words(std::size_t addr, const std::vector<Word>& words);
  void clear();

 private:
  std::vector<Word> data_;
};
