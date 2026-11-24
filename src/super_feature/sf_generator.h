#pragma once

#include "feature_extraction/feature_extractor.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace finesse {

struct SuperFeatures {
    std::vector<std::uint64_t> values;
    std::vector<FeatureVector> groups;
};

class SfGenerator {
public:
    virtual ~SfGenerator() = default;
    virtual SuperFeatures generate(const FeatureVector& features) const = 0;
};

std::uint64_t hash_feature_group(const FeatureVector& features);

}  // namespace finesse
