#include <cstddef>

#include "isa_spec/isa.h"
#include "simulator_core/core/exception.h"
#include "simulator_core/core/memory.h"
#include "tests/test_framework.h"

RISC201_TEST(memory_default_size) {
  Memory m;
  CHECK(m.size() == Memory::kDefaultWords);
  CHECK(Memory::kDefaultWords == 4096);
}

RISC201_TEST(memory_read_write) {
  Memory m(16);
  m.write_word(0, 0xDEADBEEFu);
  m.write_word(15, 0x1234u);
  CHECK(m.read_word(0) == 0xDEADBEEFu);
  CHECK(m.read_word(15) == 0x1234u);
  CHECK(m.read_word(1) == 0u);
}

RISC201_TEST(memory_load_words) {
  Memory m(16);
  m.load_words(4, {10u, 20u, 30u});
  CHECK(m.read_word(4) == 10u);
  CHECK(m.read_word(5) == 20u);
  CHECK(m.read_word(6) == 30u);
  CHECK(m.read_word(7) == 0u);
}

RISC201_TEST(memory_out_of_range_read_throws) {
  Memory m(16);
  bool threw = false;
  try {
    (void)m.read_word(16);
  } catch (const RuntimeException& e) {
    threw = true;
    CHECK(e.code == ExceptionCode::BAD_MEM_ACCESS);
    CHECK(e.address == 16u);
  }
  CHECK(threw);
}

RISC201_TEST(memory_out_of_range_write_throws) {
  Memory m(16);
  bool threw = false;
  try {
    m.write_word(16, 1u);
  } catch (const RuntimeException&) {
    threw = true;
  }
  CHECK(threw);
}

RISC201_TEST(memory_clear) {
  Memory m(4);
  m.write_word(2, 42u);
  m.clear();
  CHECK(m.read_word(2) == 0u);
}
