#include <cstdio>
#include <vector>

#include "memory_safety/visualizer.h"

using risc201::safety::StackVisualizer;

int main() {
  // 32 words = 128 bytes; every address below is a byte address.
  std::vector<Word> mem(32, 0);
  Word sp = 128;
  auto push = [&](Word value) {
    sp -= 4;
    mem[sp / 4] = value;
  };

  push(0xDEADBEEF);
  push(0xCAFEBABE);
  push(0x00000005);
  const Word fp = sp + 4;

  const StackVisualizer viz(128, 64);

  std::printf("=== Stack after 3 pushes ===\n");
  std::printf("%s\n", viz.render(mem, sp, fp, 8).c_str());

  const std::vector<Word> before = mem;
  push(0x12345678);

  std::printf("=== After 1 more push ===\n");
  std::printf("%s\n", viz.render(mem, sp, fp, 8).c_str());

  std::printf("=== Diff ===\n");
  std::printf("%s\n", viz.render_diff(before, mem, 116, sp).c_str());

  return 0;
}
