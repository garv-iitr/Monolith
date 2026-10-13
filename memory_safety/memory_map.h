#pragma once

#include <cstddef>
#include <cstdint>

namespace risc201::safety {

struct MemoryMap {
  static constexpr std::uint64_t kAddressSpaceBytes = 0x20000;
  static constexpr std::uint64_t kReservedLowEnd = 0x1000;
  static constexpr std::uint64_t kDataBase = 0x10000;
  static constexpr std::uint64_t kStackLimit = 0x1F000;
  static constexpr std::uint64_t kStackTop = kAddressSpaceBytes;
  static constexpr std::size_t kWordBytes = 4;
  static constexpr std::size_t kAddressSpaceWords =
      kAddressSpaceBytes / kWordBytes;
};

}  // namespace risc201::safety
