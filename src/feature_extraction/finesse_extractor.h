#pragma once

#include "feature_extraction/feature_extractor.h"

#include <cstddef>

namespace finesse {

class FinesseExtractor final : public FeatureExtractor {
public:
    explicit FinesseExtractor(std::size_t feature_count = 12);
    FeatureVector extract(const std::uint8_t* data,
                          std::size_t length) const override;

private:
    std::size_t feature_count_;
};

}  // namespace finesse
