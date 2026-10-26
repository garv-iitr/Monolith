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

void MemoryUnit::reset() {
  std::fill(data_.begin(), data_.end(), Word{0});
  journal_.clear();
}

std::size_t MemoryUnit::begin_journal() {
  journal_.clear();
  journal_active_ = true;
  return 0;
}

std::vector<MemoryUnit::WriteDelta> MemoryUnit::take_journal(std::size_t mark) {
  std::vector<WriteDelta> deltas(journal_.begin() + static_cast<std::ptrdiff_t>(mark),
                                 journal_.end());
  journal_.resize(mark);
  journal_active_ = false;
  return deltas;
}

void MemoryUnit::restore_journal(const std::vector<WriteDelta>& deltas) {
  for (auto it = deltas.rbegin(); it != deltas.rend(); ++it)
    data_[it->index] = it->previous;
}

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
  const std::size_t index = static_cast<std::size_t>(
      byte_addr / risc201::safety::MemoryMap::kWordBytes);
  if (journal_active_) journal_.push_back({index, data_[index]});
  data_[index] = value;
}

Word MemoryUnit::fetch(Word byte_pc) const { return read_word(byte_pc); }

void MemoryUnit::load_words(Word byte_addr, const std::vector<Word>& words) {
  if (words.empty()) return;
  journal_.clear();
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
  journal_.clear();
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
