#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "isa_spec/isa.h"

namespace risc201 {
namespace safety {

// One stack slot plus the markers a caller may want to style.
struct StackRow {
  Word address = 0;  // byte address
  bool in_memory = false;
  Word value = 0;
  bool is_sp = false;
  bool is_fp = false;
  bool is_frame = false;
  bool is_base = false;
  bool is_limit = false;
};

// The window of stack rows plus the bounds, shared by the ASCII and TUI views.
// Every address here is a byte address; `memory` stays word indexed.
struct StackTable {
  Word sp = 0;
  Word base = 0;
  Word limit = 0;
  bool valid_sp = false;
  bool empty = false;
  std::vector<StackRow> rows;
};

class StackVisualizer {
 public:
  StackVisualizer(Word base, Word limit);

  // Structured view: render() formats it, the TUI colors the same rows.
  StackTable table(const std::vector<Word>& memory,
                   Word sp,
                   Word fp = 0,
                   int window = 8,
                   const std::vector<Word>& frame_boundaries = {}) const;

  // All addresses in and out are byte addresses.
  std::string render(const std::vector<Word>& memory,
                     Word sp,
                     Word fp = 0,
                     int window = 8,
                     const std::vector<Word>& frame_boundaries = {}) const;

  std::string render_diff(const std::vector<Word>& before,
                          const std::vector<Word>& after,
                          Word before_sp,
                          Word after_sp,
                          Word first_address = 0) const;

  Word base()  const { return base_;  }
  Word limit() const { return limit_; }

 private:
  Word base_;
  Word limit_;
};

}
}