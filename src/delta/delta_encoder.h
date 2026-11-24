#pragma once

#include <cstdint>
#include <vector>

namespace finesse {

class DeltaEncoder {
public:
    virtual ~DeltaEncoder() = default;
    virtual std::vector<std::uint8_t> encode(
        const std::vector<std::uint8_t>& base,
        const std::vector<std::uint8_t>& target) const = 0;
    virtual std::vector<std::uint8_t> decode(
        const std::vector<std::uint8_t>& base,
        const std::vector<std::uint8_t>& delta) const = 0;
};

}  // namespace finesse
