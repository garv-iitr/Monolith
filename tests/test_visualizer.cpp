#include <cassert>
#include <sstream>
#include <string>
#include <vector>

#include "memory_safety/visualizer.h"

using risc201::safety::StackVisualizer;

int main() {
  std::vector<Word> memory(16, 0);
  memory[14] = 0xDEADBEEF;
  const StackVisualizer visualizer(16, 8);

  const std::string active = visualizer.render(memory, 14, 0, 5, {14});
  assert(active.find("0xdeadbeef") != std::string::npos);
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

  const std::string empty = visualizer.render(memory, 16, 0, 5);
  assert(empty.find("exclusive top; empty stack") != std::string::npos);
  assert(empty.find("<top>") != std::string::npos);

  std::vector<Word> after = memory;
  after[13] = 0x12345678;
  const std::string push = visualizer.render_diff(memory, after, 16, 13);
  assert(push.find("word 0x000d") != std::string::npos);
  assert(push.find("(push)") != std::string::npos);

  const std::string pop = visualizer.render_diff(after, memory, 13, 16);
  assert(pop.find("(pop)") != std::string::npos);
}
