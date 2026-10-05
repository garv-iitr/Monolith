#include <cstdio>

#include "tests/test_framework.h"

RISC201_TEST(scaffolding_smoke) {
  CHECK(1 + 1 == 2);
}

int main() {
  auto& tests = risc201_test::registry();
  std::printf("Running %zu test(s)\n", tests.size());

  for (auto& t : tests) {
    const int before = risc201_test::failure_count();
    t.fn();
    const bool ok = risc201_test::failure_count() == before;
    std::printf("[%s] %s\n", ok ? "PASS" : "FAIL", t.name.c_str());
  }

  const int failures = risc201_test::failure_count();
  if (failures == 0) {
    std::printf("All tests passed.\n");
    return 0;
  }
  std::printf("%d check(s) failed.\n", failures);
  return 1;
}
