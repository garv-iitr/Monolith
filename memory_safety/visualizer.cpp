#include "memory_safety/visualizer.h"

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <cstdint>

#include "memory_safety/memory_map.h"

namespace risc201 {
namespace safety {

StackVisualizer::StackVisualizer(Word base, Word limit)
    : base_(base), limit_(limit) {}

StackTable StackVisualizer::table(const std::vector<Word>& memory,
                                  Word sp,
                                  Word fp,
                                  int window,
                                  const std::vector<Word>& frame_boundaries) const {
  StackTable out;
  out.sp = sp;
  out.base = base_;
  out.limit = limit_;

  if (window < 1) window = 1;
  out.valid_sp = sp >= limit_ && sp <= base_;
  out.empty = (sp == base_);
  const std::int64_t center = out.valid_sp
                                  ? static_cast<std::int64_t>(sp)
                                  : static_cast<std::int64_t>(base_);
  const std::int64_t step = static_cast<std::int64_t>(MemoryMap::kWordBytes);
  const std::int64_t top = std::min<std::int64_t>(
      static_cast<std::int64_t>(base_), center + (window / 2) * step);
  const std::int64_t bot = std::max<std::int64_t>(
      static_cast<std::int64_t>(limit_), center - (window / 2) * step);

  for (std::int64_t a = top; a >= bot; a -= step) {
    StackRow row;
    row.address = static_cast<Word>(a);
    const std::size_t index = static_cast<std::size_t>(a / step);
    row.in_memory = index < memory.size();
    row.value = row.in_memory ? memory[index] : Word{0};
    row.is_sp = row.address == sp;
    row.is_fp = row.address == fp && fp != 0;
    row.is_frame = std::find(frame_boundaries.begin(), frame_boundaries.end(),
                             row.address) != frame_boundaries.end();
    row.is_limit = row.address == limit_;
    row.is_base = row.address == base_;
    out.rows.push_back(row);
  }
  return out;
}

std::string StackVisualizer::render(const std::vector<Word>& memory,
                                    Word sp,
                                    Word fp,
                                    int window,
                                    const std::vector<Word>& frame_boundaries) const {
  std::ostringstream os;
  const StackTable view = table(memory, sp, fp, window, frame_boundaries);

  std::vector<std::string> rows;
  std::ostringstream sp_line;
  sp_line << "  SP address: 0x" << std::hex << view.sp;
  if (view.empty) sp_line << " (exclusive top; empty stack)";
  if (!view.valid_sp) sp_line << " (outside configured stack bounds)";
  rows.push_back(sp_line.str());

  std::ostringstream bounds_line;
  bounds_line << "  bounds: LIMIT=0x" << std::hex << view.limit
              << " BASE=0x" << view.base;
  rows.push_back(bounds_line.str());

  for (const StackRow& r : view.rows) {
    std::ostringstream row;
    row << " 0x" << std::hex << std::setw(5) << std::setfill('0') << r.address
        << " | ";
    if (r.in_memory) {
      row << "0x" << std::setw(8) << r.value;
    } else {
      row << "   <top>";
    }
    row << " |";

    if (r.is_sp) row << "  <- SP / FRAME";
    if (r.is_fp) row << "  <- FP";
    if (r.is_frame) row << "  <- CALL FRAME";
    if (r.is_limit) row << "  <- LIMIT";
    if (r.is_base) row << "  <- BASE";
    rows.push_back(row.str());
  }

  const std::string title = "STACK (byte addresses)";
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
                                         Word first_address) const {
  std::ostringstream os;
  os << "[STACK DIFF]\n";

  const Word step = static_cast<Word>(MemoryMap::kWordBytes);
  std::size_t n = std::min(before.size(), after.size());
  bool any = false;
  for (std::size_t i = 0; i < n; ++i) {
    if (before[i] != after[i]) {
      any = true;
      os << "  addr 0x" << std::hex << std::setw(5) << std::setfill('0')
         << (first_address + static_cast<Word>(i) * step)
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
