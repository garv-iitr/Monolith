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

  if (window < 1) window = 1;
  const bool valid_sp = sp >= limit_ && sp <= base_;
  const std::int64_t center = valid_sp
                                  ? static_cast<std::int64_t>(sp)
                                  : static_cast<std::int64_t>(base_);
  int top = static_cast<int>(std::min<std::int64_t>(
      static_cast<std::int64_t>(base_), center + (window / 2)));
  int bot = static_cast<int>(std::max<std::int64_t>(
      static_cast<std::int64_t>(limit_), center - (window / 2)));

  std::vector<std::string> rows;
  std::ostringstream sp_line;
  sp_line << "  SP word index: 0x" << std::hex << sp;
  if (sp == base_) sp_line << " (exclusive top; empty stack)";
  if (!valid_sp) sp_line << " (outside configured stack bounds)";
  rows.push_back(sp_line.str());

  std::ostringstream bounds_line;
  bounds_line << "  bounds: LIMIT=0x" << std::hex << limit_ << " BASE=0x" << base_;
  rows.push_back(bounds_line.str());

  for (int a = top; a >= bot; --a) {
    const bool is_memory_word =
        a >= 0 && static_cast<std::size_t>(a) < memory.size();
    const Word value =
        is_memory_word ? memory[static_cast<std::size_t>(a)] : Word{0};

    std::ostringstream row;
    row << "  0x" << std::hex << std::setw(4) << std::setfill('0') << a
        << " | ";
    if (is_memory_word) {
      row << "0x" << std::setw(8) << value;
    } else {
      row << "   <top>";
    }
    row << " |";

    if (static_cast<Word>(a) == sp) row << "  <- SP / FRAME";
    if (static_cast<Word>(a) == fp && fp != 0) row << "  <- FP";
    if (std::find(frame_boundaries.begin(), frame_boundaries.end(),
                  static_cast<Word>(a)) != frame_boundaries.end()) {
      row << "  <- CALL FRAME";
    }
    if (static_cast<Word>(a) == limit_) row << "  <- LIMIT";
    if (static_cast<Word>(a) == base_) row << "  <- BASE";
    rows.push_back(row.str());
  }

  const std::string title = "STACK (word addresses)";
  std::size_t width = title.size();
  for (const std::string& row : rows) width = std::max(width, row.size());
  const std::string border = "  +" + std::string(width + 2, '-') + "+\n";
  os << border << "  | " << title << std::string(width - title.size(), ' ') << " |\n";
  os << border;
  for (const std::string& row : rows)
    os << "  | " << row << std::string(width - row.size(), ' ') << " |\n";
  os << border;
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
