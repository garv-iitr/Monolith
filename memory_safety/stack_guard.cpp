#include "memory_safety/stack_guard.h"

namespace risc201 {
namespace safety {

bool StackGuard::push_ok(Word sp) const {
  return sp > limit_;
}

bool StackGuard::pop_ok(Word sp) const {
  return sp < base_;
}

bool StackGuard::frame_ok(Word sp, Word size) const {
  return sp >= limit_ + size;
}

bool StackGuard::addr_ok(Word addr, Word size, bool is_write) const {
  (void)is_write;
  if (addr < 0x1000) return false;
  if (addr + size > kMemSize) return false;
  return true;
}

::RuntimeException StackGuard::check_push(Word sp, Word pc) const {
  ::RuntimeException e;
  e.address = sp;
  if (!push_ok(sp)) e.code = ::ExceptionCode::STACK_OVERFLOW;
  (void)pc;
  return e;
}

::RuntimeException StackGuard::check_pop(Word sp, Word pc) const {
  ::RuntimeException e;
  e.address = sp;
  if (!pop_ok(sp)) e.code = ::ExceptionCode::STACK_UNDERFLOW;
  (void)pc;
  return e;
}

::RuntimeException StackGuard::check_mem(Word addr, Word size,
                                         bool is_write, Word pc) const {
  ::RuntimeException e;
  e.address = addr;
  if (!addr_ok(addr, size, is_write)) e.code = ::ExceptionCode::BAD_MEM_ACCESS;
  (void)pc;
  return e;
}

}  // namespace safety
}  // namespace risc201
