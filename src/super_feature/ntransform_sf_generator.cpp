#include "super_feature/ntransform_sf_generator.h"

#include "super_feature/finesse_sf_generator.h"

#include <stdexcept>

namespace finesse {

NTransformSfGenerator::NTransformSfGenerator(std::size_t features_per_sf)
    : features_per_sf_(features_per_sf) {
    if (features_per_sf_ == 0) {
        throw std::invalid_argument("features_per_sf must be positive");
    }
}

SuperFeatures NTransformSfGenerator::generate(
    const FeatureVector& features) const {
    if (features.empty() || features.size() % features_per_sf_ != 0) {
        throw std::invalid_argument(
            "N-transform feature count must be divisible by features per SF");
    }
    SuperFeatures output;
    const std::size_t sf_count = features.size() / features_per_sf_;
    output.values.reserve(sf_count);
    output.groups.reserve(sf_count);
    for (std::size_t index = 0; index < sf_count; ++index) {
        FeatureVector group(
            features.begin() + index * features_per_sf_,
            features.begin() + (index + 1) * features_per_sf_);
        output.values.push_back(hash_feature_group(group));
        output.groups.push_back(std::move(group));
    }
    return output;
}

}  // namespace finesse
