#include "memory_safety/visualizer.h"

#include <algorithm>
#include <iomanip>
#include <sstream>

namespace risc201 {
namespace safety {

StackVisualizer::StackVisualizer(Word base, Word limit)
    : base_(base), limit_(limit) {}

std::string StackVisualizer::render(const std::vector<Word>& memory,
                                    Word sp,
                                    Word fp,
                                    int window) const {
  std::ostringstream os;
  os << "        +----------- STACK -----------+\n";

  int top = static_cast<int>(sp) + (window / 2);
  int bot = static_cast<int>(sp) - (window / 2);

  if (top > static_cast<int>(base_))  top = static_cast<int>(base_);
  if (bot < static_cast<int>(limit_)) bot = static_cast<int>(limit_);

  for (int a = top; a >= bot; --a) {
    Word value = (a >= 0 && a < static_cast<int>(memory.size()))
                     ? memory[a]
                     : 0;

    os << "  0x" << std::hex << std::setw(4) << std::setfill('0') << a
       << " | 0x" << std::setw(8) << value << " |";

    if (static_cast<Word>(a) == sp)    os << "  <- SP";
    if (static_cast<Word>(a) == fp && fp != 0) os << "  <- FP";
    if (static_cast<Word>(a) == limit_) os << "  <- LIMIT";
    if (static_cast<Word>(a) == base_)  os << "  <- BASE";
    os << "\n";
  }

  os << "        +-----------------------------+\n";
  os << std::dec;
  return os.str();
}

std::string StackVisualizer::render_diff(const std::vector<Word>& before,
                                         const std::vector<Word>& after,
                                         Word sp) const {
  std::ostringstream os;
  os << "[STACK DIFF]\n";

  std::size_t n = std::min(before.size(), after.size());
  bool any = false;
  for (std::size_t i = 0; i < n; ++i) {
    if (before[i] != after[i]) {
      any = true;
      os << "  0x" << std::hex << std::setw(4) << std::setfill('0') << i
         << " : 0x" << std::setw(8) << before[i]
         << " -> 0x" << std::setw(8) << after[i] << "\n";
    }
  }
  if (!any) os << "  (no changes)\n";

  os << "  SP = 0x" << std::hex << sp << "\n";
  os << std::dec;
  return os.str();
}

}
}
