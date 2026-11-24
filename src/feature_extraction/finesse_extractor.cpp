#include "feature_extraction/finesse_extractor.h"

#include "rolling_hash/rabin_hash.h"

#include <algorithm>
#include <stdexcept>

namespace finesse {

FinesseExtractor::FinesseExtractor(std::size_t feature_count)
    : feature_count_(feature_count) {
    if (feature_count_ == 0) {
        throw std::invalid_argument("Finesse feature count must be positive");
    }
}

FeatureVector FinesseExtractor::extract(const std::uint8_t* data,
                                        std::size_t length) const {
    if (length != 0 && data == nullptr) {
        throw std::invalid_argument("non-empty chunk must have data");
    }
    FeatureVector features(feature_count_, 0);
    if (length == 0) {
        return features;
    }

    const std::size_t subchunk_size = length / feature_count_;
    RabinHash rabin;
    std::size_t feature = 0;
    std::size_t next_boundary = subchunk_size;
    for (std::size_t position = 0; position < length; ++position) {
        if (subchunk_size == 0 ||
            position >= subchunk_size * feature_count_) {
            break;
        }
        std::uint64_t fingerprint = 0;
        const bool ready = rabin.push(data[position], &fingerprint);
        const auto value = static_cast<std::uint32_t>(fingerprint);
        if (ready && value > features[feature]) {
            features[feature] = value;
        }
        if (position + 1 == next_boundary &&
            feature + 1 < feature_count_) {
            ++feature;
            next_boundary += subchunk_size;
        }
    }
    return features;
}

}  // namespace finesse
