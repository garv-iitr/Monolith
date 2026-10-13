#include "memory_safety/stack_guard.h"

#include <stdexcept>

namespace risc201 {
namespace safety {

StackGuard::StackGuard(Word base, Word limit) : base_(base), limit_(limit) {
  if (base_ > kMemSize || limit_ < kDataBase || limit_ >= base_ ||
      base_ % MemoryMap::kWordBytes != 0 ||
      limit_ % MemoryMap::kWordBytes != 0) {
    throw std::invalid_argument("invalid stack bounds for RISC201 memory map");
  }
}

bool StackGuard::push_ok(Word sp) const {
  return sp % MemoryMap::kWordBytes == 0 && sp > limit_ && sp <= base_;
}

bool StackGuard::pop_ok(Word sp) const {
  return sp % MemoryMap::kWordBytes == 0 && sp >= limit_ && sp < base_;
}

bool StackGuard::frame_ok(Word sp, Word size) const {
  return sp % MemoryMap::kWordBytes == 0 &&
         size % MemoryMap::kWordBytes == 0 && sp >= limit_ && sp <= base_ &&
         size <= sp - limit_;
}

bool StackGuard::addr_ok(Word addr, Word size, bool is_write) const {
  (void)is_write;
  if (addr % MemoryMap::kWordBytes != 0 ||
      size % MemoryMap::kWordBytes != 0 || size == 0 || addr < kDataBase ||
      addr > limit_) {
    return false;
  }
  return size <= limit_ - addr;
}

::RuntimeException StackGuard::check_push(Word sp, Word pc) const {
  ::RuntimeException e;
  e.address = sp;
  if (sp % MemoryMap::kWordBytes != 0) {
    e.code = ::ExceptionCode::MISALIGNED_ACCESS;
  } else if (!push_ok(sp)) {
    e.code = ::ExceptionCode::STACK_OVERFLOW;
  }
  (void)pc;
  return e;
}

::RuntimeException StackGuard::check_pop(Word sp, Word pc) const {
  ::RuntimeException e;
  e.address = sp;
  if (sp % MemoryMap::kWordBytes != 0) {
    e.code = ::ExceptionCode::MISALIGNED_ACCESS;
  } else if (!pop_ok(sp)) {
    e.code = ::ExceptionCode::STACK_UNDERFLOW;
  }
  (void)pc;
  return e;
}

::RuntimeException StackGuard::check_mem(Word addr, Word size,
                                         bool is_write, Word pc) const {
  ::RuntimeException e;
  e.address = addr;
  if (addr % MemoryMap::kWordBytes != 0 ||
      size % MemoryMap::kWordBytes != 0) {
    e.code = ::ExceptionCode::MISALIGNED_ACCESS;
  } else if (!addr_ok(addr, size, is_write)) {
    e.code = ::ExceptionCode::BAD_MEM_ACCESS;
  }
  (void)pc;
  return e;
}

}
}
