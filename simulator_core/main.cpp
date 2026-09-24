#include <cstdio>

#ifndef RISC201_WORD_BITS
#define RISC201_WORD_BITS 64
#endif

int main(int argc, char** argv) {
  (void)argc;
  (void)argv;
  std::printf("risc201-sim - 4-stage SimpleRISC core engine (scaffolding)\n");
  std::printf("  data word width : %d bits\n", RISC201_WORD_BITS);
  std::printf("  status          : not yet implemented\n");
  return 0;
}
