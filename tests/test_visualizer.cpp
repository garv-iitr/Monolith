#include <cassert>
#include <sstream>
#include <string>
#include <vector>

#include "memory_safety/visualizer.h"

using risc201::safety::StackVisualizer;

int main() {
  // 16 words = 64 bytes; base is the exclusive top, limit the bottom.
  std::vector<Word> memory(16, 0);
  memory[14] = 0xDEADBEEF;  // byte address 0x38
  const StackVisualizer visualizer(64, 32);

  const std::string active = visualizer.render(memory, 56, 0, 5, {56});
  assert(active.find("0xdeadbeef") != std::string::npos);
  assert(active.find("byte addresses") != std::string::npos);
  assert(active.find("SP address: 0x38") != std::string::npos);
  assert(active.find("LIMIT=0x20") != std::string::npos);
  assert(active.find("BASE=0x40") != std::string::npos);
  std::istringstream rendered(active);
  std::string line;
  std::size_t border_width = 0;
  int border_count = 0;
  while (std::getline(rendered, line)) {
    if (line.find('+') == std::string::npos) continue;
    if (border_width == 0) border_width = line.size();
    assert(line.size() == border_width);
    ++border_count;
  }
  assert(border_count == 3);
  assert(active.find("SP / FRAME") != std::string::npos);
  assert(active.find("CALL FRAME") != std::string::npos);
  assert(active.find("LIMIT") != std::string::npos);
  assert(active.find("BASE") != std::string::npos);

  const std::string empty = visualizer.render(memory, 64, 0, 5);
  assert(empty.find("exclusive top; empty stack") != std::string::npos);
  assert(empty.find("<top>") != std::string::npos);

  std::vector<Word> after = memory;
  after[13] = 0x12345678;  // byte address 0x34
  const std::string push = visualizer.render_diff(memory, after, 64, 52);
  assert(push.find("addr 0x00034") != std::string::npos);
  assert(push.find("(push)") != std::string::npos);

  const std::string pop = visualizer.render_diff(after, memory, 52, 64);
  assert(pop.find("addr 0x00034") != std::string::npos);
  assert(pop.find("(pop)") != std::string::npos);
}
