#include "datapath/memory_unit.h"

#include <algorithm>

#include "simulator_core/core/exception.h"

namespace risc201::cu {

MemoryUnit::MemoryUnit(std::size_t words) : data_(words, Word{0}) {}

void MemoryUnit::reset() { std::fill(data_.begin(), data_.end(), Word{0}); }

bool MemoryUnit::present(Word byte_addr) const {
  if (byte_addr % 4 != 0) return false;
  return static_cast<std::size_t>(byte_addr / 4) < data_.size();
}

Word MemoryUnit::read_word(Word byte_addr) const {
  if (!present(byte_addr)) {
    throw RuntimeException{ExceptionCode::BAD_MEM_ACCESS,
                           static_cast<std::uint64_t>(byte_addr)};
  }
  return data_[static_cast<std::size_t>(byte_addr / 4)];
}

void MemoryUnit::write_word(Word byte_addr, Word value) {
  if (!present(byte_addr)) {
    throw RuntimeException{ExceptionCode::BAD_MEM_ACCESS,
                           static_cast<std::uint64_t>(byte_addr)};
  }
  data_[static_cast<std::size_t>(byte_addr / 4)] = value;
}

Word MemoryUnit::fetch(Word byte_pc) const { return read_word(byte_pc); }

void MemoryUnit::load_words(Word byte_addr, const std::vector<Word>& words) {
  for (std::size_t i = 0; i < words.size(); ++i) {
    write_word(byte_addr + static_cast<Word>(i * 4), words[i]);
  }
}

void MemoryUnit::tick(const InternalRegisters& cur, const ControlSignals& sig,
                      InternalRegisters& next) {
  if (sig.mem_op == MemOp::READ) {
    next.ld_result = read_word(cur.mar);
  } else if (sig.mem_op == MemOp::WRITE) {
    write_word(cur.mar, cur.mdr);
  }
}

}
