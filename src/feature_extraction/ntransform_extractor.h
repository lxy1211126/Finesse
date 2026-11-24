#pragma once

#include "feature_extraction/feature_extractor.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace finesse {

struct LinearTransform {
    std::uint32_t multiplier = 0;
    std::uint32_t addend = 0;
};

class NTransformExtractor final : public FeatureExtractor {
public:
    explicit NTransformExtractor(std::vector<LinearTransform> transforms);
    FeatureVector extract(const std::uint8_t* data,
                          std::size_t length) const override;

private:
    std::vector<LinearTransform> transforms_;
};

}  // namespace finesse
