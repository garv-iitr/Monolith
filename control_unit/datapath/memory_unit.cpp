#include "datapath/memory_unit.h"

#include <algorithm>
#include <stdexcept>

#include "simulator_core/core/exception.h"

namespace risc201::cu {
namespace {

std::size_t checked_word_count(std::size_t words) {
  if (words > risc201::safety::MemoryMap::kAddressSpaceWords) {
    throw std::invalid_argument("memory exceeds the RISC201 address map");
  }
  return words;
}

}  // namespace

MemoryUnit::MemoryUnit(std::size_t words)
    : data_(checked_word_count(words), Word{0}) {}

void MemoryUnit::reset() { std::fill(data_.begin(), data_.end(), Word{0}); }

bool MemoryUnit::present(Word byte_addr) const {
  if (byte_addr % risc201::safety::MemoryMap::kWordBytes != 0 ||
      byte_addr >= risc201::safety::MemoryMap::kAddressSpaceBytes) {
    return false;
  }
  return static_cast<std::uint64_t>(byte_addr /
                                    risc201::safety::MemoryMap::kWordBytes) <
         data_.size();
}

Word MemoryUnit::read_word(Word byte_addr) const {
  if (!present(byte_addr)) {
    throw RuntimeException{ExceptionCode::BAD_MEM_ACCESS,
                           static_cast<std::uint64_t>(byte_addr)};
  }
  return data_[static_cast<std::size_t>(
      byte_addr / risc201::safety::MemoryMap::kWordBytes)];
}

void MemoryUnit::write_word(Word byte_addr, Word value) {
  if (!present(byte_addr)) {
    throw RuntimeException{ExceptionCode::BAD_MEM_ACCESS,
                           static_cast<std::uint64_t>(byte_addr)};
  }
  data_[static_cast<std::size_t>(
      byte_addr / risc201::safety::MemoryMap::kWordBytes)] = value;
}

Word MemoryUnit::fetch(Word byte_pc) const { return read_word(byte_pc); }

void MemoryUnit::load_words(Word byte_addr, const std::vector<Word>& words) {
  if (words.empty()) return;
  if (byte_addr % risc201::safety::MemoryMap::kWordBytes != 0 ||
      byte_addr >= risc201::safety::MemoryMap::kAddressSpaceBytes ||
      byte_addr / risc201::safety::MemoryMap::kWordBytes >= data_.size() ||
      words.size() >
          data_.size() - static_cast<std::size_t>(
                             byte_addr /
                             risc201::safety::MemoryMap::kWordBytes)) {
    throw RuntimeException{ExceptionCode::BAD_MEM_ACCESS,
                           static_cast<std::uint64_t>(byte_addr)};
  }
  for (std::size_t i = 0; i < words.size(); ++i) {
    write_word(byte_addr + static_cast<Word>(
                                i * risc201::safety::MemoryMap::kWordBytes),
               words[i]);
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
