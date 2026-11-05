#include <cstdio>
#include <vector>

#include "memory_safety/visualizer.h"

using risc201::safety::StackVisualizer;

int main() {
  std::vector<Word> mem(32, 0);

  Word sp = 32;
  mem[--sp] = 0xDEADBEEF;
  mem[--sp] = 0xCAFEBABE;
  mem[--sp] = 0x00000005;

  Word fp = sp + 1;

  StackVisualizer viz(32, 16);

  std::printf("=== Stack after 3 pushes ===\n");
  std::printf("%s\n", viz.render(mem, sp, fp, 8).c_str());

  std::vector<Word> before = mem;
  mem[--sp] = 0x12345678;

  std::printf("=== After 1 more push ===\n");
  std::printf("%s\n", viz.render(mem, sp, fp, 8).c_str());

  std::printf("=== Diff ===\n");
  std::printf("%s\n", viz.render_diff(before, mem, 32, sp).c_str());

  return 0;
}