#pragma once

#include <cstdint>

#include "isa_spec/isa.h"
#include "memory_safety/memory_map.h"
#include "simulator_core/core/exception.h"

namespace risc201 {
namespace safety {

// Stack grows DOWN from the exclusive STACK_BASE toward STACK_LIMIT.
// SP (R14) is the next free byte address; a push stores at SP - 4.
//
//   valid empty SP : STACK_BASE (one-past-end; never dereferenced)
//   push OK        : aligned STACK_LIMIT < sp <= STACK_BASE
//   pop  OK        : aligned STACK_LIMIT <= sp < STACK_BASE
//
// Uses ::ExceptionCode from simulator_core/core/exception.h.
// Does NOT define its own error codes.

class StackGuard {
 public:
  static constexpr Word kMemSize =
      static_cast<Word>(MemoryMap::kAddressSpaceBytes);
  static constexpr Word kDataBase =
      static_cast<Word>(MemoryMap::kReservedLowEnd);
  static constexpr Word kStackBase =
      static_cast<Word>(MemoryMap::kStackTop);
  static constexpr Word kStackLimit =
      static_cast<Word>(MemoryMap::kStackLimit);

  StackGuard() = default;
  StackGuard(Word base, Word limit);

  bool push_ok(Word sp) const;
  bool pop_ok(Word sp) const;
  bool frame_ok(Word sp, Word size) const;
  bool addr_ok(Word addr, Word size, bool is_write) const;

  ::RuntimeException check_access(Word addr, Word sp, Word size,
                                  bool is_write, Word pc) const;
  ::RuntimeException check_push(Word sp, Word pc) const;
  ::RuntimeException check_pop(Word sp, Word pc) const;
  ::RuntimeException check_mem(Word addr, Word size, bool is_write, Word pc) const;

  Word base()  const { return base_;  }
  Word limit() const { return limit_; }

 private:
  Word base_  = kStackBase;
  Word limit_ = kStackLimit;
};

}
}