#pragma once

#include <cstdint>

#include "isa_spec/isa.h"
#include "simulator_core/core/exception.h"

namespace risc201 {
namespace safety {

// Stack grows DOWN from STACK_BASE toward STACK_LIMIT.
// SP (R14) points at the top-most valid pushed word.
//
//   valid SP range : [STACK_LIMIT, STACK_BASE]
//   push OK        : sp > STACK_LIMIT
//   pop  OK        : sp < STACK_BASE
//
// Uses ::ExceptionCode from simulator_core/core/exception.h.
// Does NOT define its own error codes.

class StackGuard {
 public:
  static constexpr Word kMemSize    = 64 * 1024;         // 64 KB
  static constexpr Word kStackBase  = kMemSize;          // top of memory
  static constexpr Word kStackLimit = kMemSize - 4096;   // 4 KB max stack

  StackGuard() = default;
  StackGuard(Word base, Word limit) : base_(base), limit_(limit) {}

  bool push_ok(Word sp) const;
  bool pop_ok(Word sp) const;
  bool frame_ok(Word sp, Word size) const;
  bool addr_ok(Word addr, Word size, bool is_write) const;

  ::RuntimeException check_push(Word sp, Word pc) const;
  ::RuntimeException check_pop(Word sp, Word pc) const;
  ::RuntimeException check_mem(Word addr, Word size, bool is_write, Word pc) const;

  Word base()  const { return base_;  }
  Word limit() const { return limit_; }

 private:
  Word base_  = kStackBase;
  Word limit_ = kStackLimit;
};

}  // namespace safety
}  // namespace risc201