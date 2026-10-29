#include "memory_safety/visualizer.h"

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <cstdint>

namespace risc201 {
namespace safety {

StackVisualizer::StackVisualizer(Word base, Word limit)
    : base_(base), limit_(limit) {}

std::string StackVisualizer::render(const std::vector<Word>& memory,
                                    Word sp,
                                    Word fp,
                                    int window,
                                    const std::vector<Word>& frame_boundaries) const {
  std::ostringstream os;
  os << "        +------ STACK (word addresses) ------+\n";

  if (window < 1) window = 1;
  const bool valid_sp = sp >= limit_ && sp <= base_;
  const std::int64_t center = valid_sp
                                  ? static_cast<std::int64_t>(sp)
                                  : static_cast<std::int64_t>(base_);
  int top = static_cast<int>(std::min<std::int64_t>(
      static_cast<std::int64_t>(base_), center + (window / 2)));
  int bot = static_cast<int>(std::max<std::int64_t>(
      static_cast<std::int64_t>(limit_), center - (window / 2)));

  os << "  SP word index: 0x" << std::hex << sp;
  if (sp == base_) os << " (exclusive top; empty stack)";
  if (!valid_sp) os << " (outside configured stack bounds)";
  os << "\n";
  os << "  bounds: LIMIT=0x" << limit_ << " BASE=0x" << base_ << "\n";

  for (int a = top; a >= bot; --a) {
    const bool is_memory_word =
        a >= 0 && static_cast<std::size_t>(a) < memory.size();
    const Word value =
        is_memory_word ? memory[static_cast<std::size_t>(a)] : Word{0};

    os << "  0x" << std::hex << std::setw(4) << std::setfill('0') << a
       << " | ";
    if (is_memory_word) {
      os << "0x" << std::setw(8) << value;
    } else {
      os << "   <top>";
    }
    os << " |";

    if (static_cast<Word>(a) == sp) os << "  <- SP / FRAME";
    if (static_cast<Word>(a) == fp && fp != 0) os << "  <- FP";
    if (std::find(frame_boundaries.begin(), frame_boundaries.end(),
                  static_cast<Word>(a)) != frame_boundaries.end()) {
      os << "  <- CALL FRAME";
    }
    if (static_cast<Word>(a) == limit_) os << "  <- LIMIT";
    if (static_cast<Word>(a) == base_) os << "  <- BASE";
    os << "\n";
  }

  os << "        +-----------------------------+\n";
  os << std::dec;
  return os.str();
}

std::string StackVisualizer::render_diff(const std::vector<Word>& before,
                                         const std::vector<Word>& after,
                                         Word before_sp,
                                         Word after_sp,
                                         Word first_word) const {
  std::ostringstream os;
  os << "[STACK DIFF]\n";

  std::size_t n = std::min(before.size(), after.size());
  bool any = false;
  for (std::size_t i = 0; i < n; ++i) {
    if (before[i] != after[i]) {
      any = true;
      os << "  word 0x" << std::hex << std::setw(4) << std::setfill('0')
         << (first_word + static_cast<Word>(i))
         << " : 0x" << std::setw(8) << before[i]
         << " -> 0x" << std::setw(8) << after[i] << "\n";
    }
  }
  if (!any) os << "  (no changes)\n";

  os << "  SP: 0x" << std::hex << before_sp << " -> 0x" << after_sp;
  if (after_sp < before_sp) os << " (push)";
  if (after_sp > before_sp) os << " (pop)";
  os << "\n";
  os << std::dec;
  return os.str();
}

}
}
