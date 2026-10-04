#include <cassert>
#include <cstdio>

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
  assert(g.push_ok(sp));  sp -= 1;
  assert(g.pop_ok(sp));   sp += 1;
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
  assert(!g.addr_ok(0x0, 4, false));
  assert(!g.addr_ok(StackGuard::kMemSize - 2, 4, false));
}

int main() {
  std::printf("Running StackGuard tests:\n");
  test_fresh_stack();        std::printf("  fresh_stack.......... PASS\n");
  test_push_pop_balance();   std::printf("  push_pop_balance..... PASS\n");
  test_overflow_at_limit();  std::printf("  overflow_at_limit.... PASS\n");
  test_underflow_on_empty(); std::printf("  underflow_on_empty... PASS\n");
  test_frame_allocation();   std::printf("  frame_allocation..... PASS\n");
  test_addr_bounds();        std::printf("  addr_bounds.......... PASS\n");
  std::printf("All tests passed.\n");
  return 0;
}