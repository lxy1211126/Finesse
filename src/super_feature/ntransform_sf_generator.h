#pragma once

#include "super_feature/sf_generator.h"

#include <cstddef>

namespace finesse {

class NTransformSfGenerator final : public SfGenerator {
public:
    explicit NTransformSfGenerator(std::size_t features_per_sf = 4);
    SuperFeatures generate(const FeatureVector& features) const override;

private:
    std::size_t features_per_sf_;
};

}  // namespace finesse
