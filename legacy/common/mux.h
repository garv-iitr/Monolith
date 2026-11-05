// Generic hardware simulation primitives for parameterized N-to-1 multiplexers and 1-of-N decoders.
#ifndef RISC201_CU_MUX_H
#define RISC201_CU_MUX_H

#include <array>
#include <cstddef>
#include <cstdint>

namespace risc201::cu {

// Parameterized N-to-1 multiplexer modeling hardware selection logic
// out of range sel gives 0, like a real mux with nothing hooked up
template <typename T, std::size_t N>
struct Mux {
  std::array<T, N> in{}; // Input channels [0 .. N-1]
  std::size_t sel = 0;   // Select line input
  T out{};               // Output channel

  // Evaluates multiplexer selection; defaults to zero/empty if select line is out of bounds
  constexpr void eval() { out = (sel < N) ? in[sel] : T{}; }
};

// Common hardware multiplexer sizes
template <typename T> using Mux2 = Mux<T, 2>;
template <typename T> using Mux3 = Mux<T, 3>;
template <typename T> using Mux4 = Mux<T, 4>;

// Hardware 1-of-N decoder producing a 1-hot output array from a binary select index
template <std::size_t N>
struct Decoder {
  std::size_t sel = 0;      // Binary select line
  std::array<bool, N> out{};// 1-hot decoded output lines

  // Evaluates decoder outputs, asserting only out[sel] if within range
  constexpr void eval() {
    out.fill(false);
    if (sel < N) out[sel] = true;
  }
};

}

#endif
