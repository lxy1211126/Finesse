#pragma once

#include <cstddef>
#include <cstdint>

namespace finesse {

inline constexpr std::size_t kRabinWindowSize = 48;
inline constexpr std::size_t kExpectedChunkSize = 8 * 1024;
inline constexpr std::size_t kMinimumChunkSize = 512;
inline constexpr std::size_t kMaximumChunkSize = 64 * 1024;
inline constexpr std::uint64_t kRabinPolynomial = 0xbfe6b8a5bf378d83ULL;
inline constexpr std::uint64_t kRabinBreakmark = 120;
inline constexpr std::uint32_t kSfRabinPolynomial = 0x04c11db7U;
inline constexpr unsigned int kSfRabinDegree = 32;

}  // namespace finesse
