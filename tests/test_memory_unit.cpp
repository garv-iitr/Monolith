#include <cassert>
#include <cstdio>
#include <stdexcept>

#include "datapath/memory_unit.h"
#include "memory_safety/stack_guard.h"
#include "simulator_core/core/exception.h"

using risc201::cu::MemoryUnit;
using risc201::safety::StackGuard;

static void test_default_map_boundary() {
  MemoryUnit memory;
  assert(memory.words() == 32768);
  assert(memory.present(0x1FFFC));
  assert(memory.present(0x10000));
  assert(!memory.present(0x20000));
  assert(!memory.present(0x20001));
  memory.write_word(0x1FFFC, 0x1234);
  assert(memory.read_word(0x1FFFC) == 0x1234);
  memory.write_word(0x10000, 0x5678);
  assert(memory.read_word(0x10000) == 0x5678);
  StackGuard guard;
  assert(guard.push_ok(guard.base()));
  assert(memory.present(guard.base() - 4));
  assert(!memory.present(guard.base()));

  bool rejected = false;
  try {
    memory.read_word(0x20000);
  } catch (const RuntimeException& e) {
    rejected = e.code == ExceptionCode::BAD_MEM_ACCESS;
  }
  assert(rejected);

  rejected = false;
  try {
    memory.write_word(0x20000, 0xA5A5);
  } catch (const RuntimeException& e) {
    rejected = e.code == ExceptionCode::BAD_MEM_ACCESS;
  }
  assert(rejected);

  rejected = false;
  try {
    memory.write_word(0x1FFFF, 0xA5A5);
  } catch (const RuntimeException& e) {
    rejected = e.code == ExceptionCode::BAD_MEM_ACCESS;
  }
  assert(rejected);
}

static void test_configured_small_memory() {
  MemoryUnit memory(4);
  assert(memory.words() == 4);
  assert(memory.present(0x0C));
  assert(!memory.present(0x10));
  assert(!memory.present(0x1000));
  memory.write_word(0x0C, 0x5678);
  assert(memory.read_word(0x0C) == 0x5678);
}

static void test_memory_size_cannot_exceed_map() {
  bool rejected = false;
  try {
    MemoryUnit too_large(MemoryUnit::kDefaultWords + 1);
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);
}

int main() {
  test_default_map_boundary();
  test_configured_small_memory();
  test_memory_size_cannot_exceed_map();
  std::puts("MemoryUnit map tests passed.");
}
