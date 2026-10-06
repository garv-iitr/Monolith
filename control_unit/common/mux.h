#ifndef RISC201_CU_MUX_H
#define RISC201_CU_MUX_H

#include <array>
#include <cstddef>
#include <cstdint>

namespace risc201::cu {

// out of range sel gives 0, like a real mux with nothing hooked up
template <typename T, std::size_t N>
struct Mux {
  std::array<T, N> in{};
  std::size_t sel = 0;
  T out{};

  constexpr void eval() { out = (sel < N) ? in[sel] : T{}; }
};

template <typename T> using Mux2 = Mux<T, 2>;
template <typename T> using Mux3 = Mux<T, 3>;
template <typename T> using Mux4 = Mux<T, 4>;

template <std::size_t N>
struct Decoder {
  std::size_t sel = 0;
  std::array<bool, N> out{};

  constexpr void eval() {
    out.fill(false);
    if (sel < N) out[sel] = true;
  }
};

}

#endif
