#include "feature_extraction/ntransform_extractor.h"

#include "rolling_hash/rabin_hash.h"

#include <stdexcept>

namespace finesse {

NTransformExtractor::NTransformExtractor(
    std::vector<LinearTransform> transforms)
    : transforms_(std::move(transforms)) {
    if (transforms_.empty()) {
        throw std::invalid_argument("N-transform requires at least one seed");
    }
}

FeatureVector NTransformExtractor::extract(const std::uint8_t* data,
                                           std::size_t length) const {
    if (length != 0 && data == nullptr) {
        throw std::invalid_argument("non-empty chunk must have data");
    }
    FeatureVector features(transforms_.size(), 0);
    RabinHash rabin;
    for (std::size_t i = 0; i < length; ++i) {
        std::uint64_t fingerprint = 0;
        if (!rabin.push(data[i], &fingerprint)) {
            continue;
        }
        const std::uint32_t fp = static_cast<std::uint32_t>(fingerprint);
        for (std::size_t dimension = 0; dimension < transforms_.size();
             ++dimension) {
            const auto& transform = transforms_[dimension];
            const std::uint32_t value = static_cast<std::uint32_t>(
                static_cast<std::uint64_t>(transform.multiplier) * fp +
                transform.addend);
            if (value > features[dimension]) {
                features[dimension] = value;
            }
        }
    }
    return features;
}

}  // namespace finesse
