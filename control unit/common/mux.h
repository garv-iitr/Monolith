#ifndef RISC201_CU_MUX_H
#define RISC201_CU_MUX_H

// ============================================================================
// Combinational hardware primitives used throughout the control unit and the
// microprogrammed datapath: a parameterised multiplexer and an N-to-2^N
// one-hot decoder.  These mirror the physical components described in
// Sarangi, ch. 9 (the transfer multiplexer, the micro-fetch multiplexer,
// the opcode dispatch decoder).
// ============================================================================

#include <array>
#include <cstddef>
#include <cstdint>

namespace risc201::cu {

// N-to-1 multiplexer.  `sel` picks `in[sel]`; out-of-range selects yield the
// zero value, which matches the "unconnected input drives 0" behaviour of a
// real multiplexer with a floating select.
template <typename T, std::size_t N>
struct Mux {
  std::array<T, N> in{};
  std::size_t sel = 0;
  T out{};

  constexpr void eval() { out = (sel < N) ? in[sel] : T{}; }
};

// Convenience aliases for the fixed-width muxes in the datapath.
template <typename T> using Mux2 = Mux<T, 2>;
template <typename T> using Mux3 = Mux<T, 3>;
template <typename T> using Mux4 = Mux<T, 4>;

// One-hot decoder: `sel` (0..N-1) asserts exactly one output line.
template <std::size_t N>
struct Decoder {
  std::size_t sel = 0;
  std::array<bool, N> out{};

  constexpr void eval() {
    out.fill(false);
    if (sel < N) out[sel] = true;
  }
};

}  // namespace risc201::cu

#endif  // RISC201_CU_MUX_H
