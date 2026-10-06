#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "isa_spec/isa.h"

namespace risc201 {
namespace safety {

class StackVisualizer {
 public:
  StackVisualizer(Word base, Word limit);

  // memory, sp and fp are word indices, not byte addresses
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

}
}