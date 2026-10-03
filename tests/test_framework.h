#pragma once

#include <cstdio>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace risc201_test {

struct TestCase {
  std::string name;
  std::function<void()> fn;
};

inline std::vector<TestCase>& registry() {
  static std::vector<TestCase> r;
  return r;
}

inline int& failure_count() {
  static int n = 0;
  return n;
}

inline bool register_test(const std::string& name, std::function<void()> fn) {
  registry().push_back({name, std::move(fn)});
  return true;
}

}

#define RISC201_TEST(name)                                                     \
  static void name();                                                          \
  [[maybe_unused]] static const bool name##_registered =                       \
      ::risc201_test::register_test(#name, name);                              \
  static void name()

#define CHECK(cond)                                                            \
  do {                                                                         \
    if (!(cond)) {                                                             \
      std::printf("  FAIL: %s  (%s:%d)\n", #cond, __FILE__, __LINE__);         \
      ++::risc201_test::failure_count();                                       \
    }                                                                          \
  } while (0)

#define CHECK_EQ(a, b) CHECK((a) == (b))
