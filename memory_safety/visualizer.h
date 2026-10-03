#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "isa_spec/isa.h"

namespace risc201 {
namespace safety {

// Renders the stack region of memory as ASCII for debugging.
// Decoupled from Datapath / MemoryUnit so it can be tested standalone.
class StackVisualizer {
 public:
  StackVisualizer(Word base, Word limit);

  // memory is a vector of Words indexed by WORD INDEX (memory[0] = word 0).
  // sp and fp are WORD INDICES as well.
  std::string render(const std::vector<Word>& memory,
                     Word sp,
                     Word fp = 0,
                     int window = 8) const;

  std::string render_diff(const std::vector<Word>& before,
                          const std::vector<Word>& after,
                          Word sp) const;

  Word base()  const { return base_;  }
  Word limit() const { return limit_; }

 private:
  Word base_;
  Word limit_;
};

}  // namespace safety
}  // namespace risc201