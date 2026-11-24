#pragma once

#include "super_feature/sf_generator.h"

#include <cstddef>

namespace finesse {

class FinesseSfGenerator final : public SfGenerator {
public:
    explicit FinesseSfGenerator(std::size_t features_per_group = 3);
    SuperFeatures generate(const FeatureVector& features) const override;

private:
    std::size_t features_per_group_;
};

}  // namespace finesse
