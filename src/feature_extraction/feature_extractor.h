#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace finesse {

using FeatureVector = std::vector<std::uint32_t>;

class FeatureExtractor {
public:
    virtual ~FeatureExtractor() = default;
    virtual FeatureVector extract(const std::uint8_t* data,
                                 std::size_t length) const = 0;
};

}  // namespace finesse
