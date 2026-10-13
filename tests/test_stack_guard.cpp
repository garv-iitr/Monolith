#include <cassert>
#include <cstdio>
#include <stdexcept>

#include "memory_safety/stack_guard.h"

using risc201::safety::StackGuard;

static void test_fresh_stack() {
  StackGuard g;
  assert(g.push_ok(g.base()));
  assert(!g.pop_ok(g.base()));
}

static void test_push_pop_balance() {
  StackGuard g;
  Word sp = g.base();
  assert(g.push_ok(sp));  sp -= 4;
  assert(g.pop_ok(sp));   sp += 4;
  assert(sp == g.base());
}

static void test_overflow_at_limit() {
  StackGuard g;
  Word sp = g.limit();
  assert(!g.push_ok(sp));
  auto e = g.check_push(sp, 0);
  assert(e.code == ::ExceptionCode::STACK_OVERFLOW);
}

static void test_underflow_on_empty() {
  StackGuard g;
  auto e = g.check_pop(g.base(), 0);
  assert(e.code == ::ExceptionCode::STACK_UNDERFLOW);
}

static void test_frame_allocation() {
  StackGuard g;
  assert(g.frame_ok(g.base(), 100));
  assert(!g.frame_ok(g.base(), 99999));
}

static void test_addr_bounds() {
  StackGuard g;
  assert(g.addr_ok(0x1000, 4, false));
  assert(g.addr_ok(0x1EFFC, 4, false));
  assert(g.addr_ok(0x10000, 4, false));
  assert(!g.addr_ok(0x0, 4, false));
  assert(!g.addr_ok(0x1F000, 4, false));
  assert(!g.addr_ok(0x1FFFC, 4, false));
  assert(!g.addr_ok(0x1EFFC, 8, false));
  assert(!g.addr_ok(0x1001, 4, false));
  assert(!g.addr_ok(0x1000, 0, false));
  assert(!g.addr_ok(static_cast<Word>(-4), 8, false));
}

static void test_data_region_edges_and_overflow() {
  StackGuard g;
  const Word first_data_word = StackGuard::kDataBase;
  const Word last_data_word = StackGuard::kStackLimit - 4;
  const Word max_word = static_cast<Word>(-1);

  assert(g.addr_ok(first_data_word, 4, false));
  assert(g.addr_ok(last_data_word, 4, true));
  assert(g.addr_ok(last_data_word, 4, false));
  assert(!g.addr_ok(last_data_word, 8, false));
  assert(!g.addr_ok(first_data_word - 4, 4, false));
  assert(!g.addr_ok(first_data_word, 0, false));
  assert(!g.addr_ok(first_data_word, 2, false));
  assert(!g.addr_ok(first_data_word + 1, 4, false));
  assert(!g.addr_ok(max_word, 4, false));
  assert(!g.addr_ok(max_word - 3, 8, true));

  assert(g.check_mem(last_data_word, 4, true, 0).code ==
         ::ExceptionCode::NONE);
  assert(g.check_mem(last_data_word, 8, true, 0).code ==
         ::ExceptionCode::BAD_MEM_ACCESS);
}

static void test_stack_boundaries_and_alignment() {
  StackGuard g;
  assert(g.base() == 0x20000);
  assert(g.limit() == 0x1F000);
  assert(g.push_ok(0x20000));
  assert(g.push_ok(0x1F004));
  assert(!g.push_ok(0x1F000));
  assert(!g.push_ok(0x20004));
  assert(!g.push_ok(0xFFFF));
  assert(g.pop_ok(0x1F000));
  assert(!g.pop_ok(0x20000));
  assert(!g.pop_ok(0x1EFFC));
  assert(g.frame_ok(0x20000, 0x1000));
  assert(!g.frame_ok(0x20000, 0x1004));
  assert(!g.frame_ok(0x20000, 2));
  assert(g.check_push(0xFFFF, 0).code ==
         ::ExceptionCode::MISALIGNED_ACCESS);
  assert(g.check_pop(0xFFFF, 0).code ==
         ::ExceptionCode::MISALIGNED_ACCESS);
  assert(g.check_push(g.limit() + 4, 0).code == ::ExceptionCode::NONE);
  assert(g.check_pop(g.limit(), 0).code == ::ExceptionCode::NONE);
  assert(g.check_push(g.limit(), 0).code ==
         ::ExceptionCode::STACK_OVERFLOW);
  assert(g.check_pop(g.base(), 0).code ==
         ::ExceptionCode::STACK_UNDERFLOW);
  assert(g.check_mem(0x1001, 4, false, 0).code ==
         ::ExceptionCode::MISALIGNED_ACCESS);
}

static void test_custom_stack_bounds() {
  StackGuard small(0x2000, 0x1800);
  assert(small.push_ok(0x2000));
  assert(small.pop_ok(0x1800));
  assert(small.addr_ok(0x17FC, 4, false));
  assert(!small.addr_ok(0x1800, 4, false));

  bool rejected = false;
  try {
    StackGuard invalid(0x1000, 0x1000);
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);
}

int main() {
  std::printf("Running StackGuard tests:\n");
  test_fresh_stack();        std::printf("  fresh_stack.......... PASS\n");
  test_push_pop_balance();   std::printf("  push_pop_balance..... PASS\n");
  test_overflow_at_limit();  std::printf("  overflow_at_limit.... PASS\n");
  test_underflow_on_empty(); std::printf("  underflow_on_empty... PASS\n");
  test_frame_allocation();   std::printf("  frame_allocation..... PASS\n");
  test_addr_bounds();        std::printf("  addr_bounds.......... PASS\n");
  test_data_region_edges_and_overflow();
  std::printf("  data_region_edges.... PASS\n");
  test_stack_boundaries_and_alignment();
  std::printf("  stack_boundaries..... PASS\n");
  test_custom_stack_bounds();
  std::printf("  custom_stack_bounds.. PASS\n");
  std::printf("All tests passed.\n");
  return 0;
}