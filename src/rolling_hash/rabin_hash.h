#pragma once

#include "common/constants.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace finesse {

class RabinHash {
public:
    static constexpr std::size_t window_size = kRabinWindowSize;

    RabinHash();
    bool push(std::uint8_t byte, std::uint64_t* fingerprint);
    void reset();
    static std::uint64_t fingerprint_bytes(const std::uint8_t* data,
                                           std::size_t length);
    static std::uint64_t fingerprint_u32_be(const std::uint32_t* values,
                                            std::size_t count);

private:
    std::array<std::uint8_t, window_size> window_{};
    std::size_t position_ = 0;
    std::size_t filled_ = 0;
    std::uint64_t fingerprint_ = 0;
};

}  // namespace finesse
