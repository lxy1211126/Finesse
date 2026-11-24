#include "super_feature/finesse_sf_generator.h"

#include "common/constants.h"

#include <stdexcept>
#include <utility>

namespace finesse {
namespace {

void append_sf_byte(std::uint32_t* fingerprint, std::uint8_t byte) {
    for (int bit = 7; bit >= 0; --bit) {
        const bool feedback =
            ((*fingerprint >> (kSfRabinDegree - 1)) & 1U) ^
            ((byte >> bit) & 1U);
        *fingerprint <<= 1;
        if (feedback) {
            *fingerprint ^= kSfRabinPolynomial;
        }
    }
}

}  // namespace

std::uint64_t hash_feature_group(const FeatureVector& features) {
    if (features.empty()) {
        throw std::invalid_argument("cannot hash an empty feature group");
    }
    std::uint32_t fingerprint = 0;
    for (const std::uint32_t feature : features) {
        append_sf_byte(&fingerprint, static_cast<std::uint8_t>(feature >> 24));
        append_sf_byte(&fingerprint, static_cast<std::uint8_t>(feature >> 16));
        append_sf_byte(&fingerprint, static_cast<std::uint8_t>(feature >> 8));
        append_sf_byte(&fingerprint, static_cast<std::uint8_t>(feature));
    }
    return fingerprint;
}

FinesseSfGenerator::FinesseSfGenerator(std::size_t features_per_group)
    : features_per_group_(features_per_group) {
    if (features_per_group_ == 0) {
        throw std::invalid_argument("features_per_group must be positive");
    }
}

SuperFeatures FinesseSfGenerator::generate(
    const FeatureVector& features) const {
    if (features.empty() || features.size() % features_per_group_ != 0) {
        throw std::invalid_argument(
            "Finesse feature count must be divisible by group size");
    }
    const std::size_t group_count = features.size() / features_per_group_;
    SuperFeatures output;
    output.values.resize(features_per_group_);
    output.groups.resize(features_per_group_);
    for (auto& group : output.groups) {
        group.reserve(group_count);
    }

    if (features_per_group_ == 3) {
        for (std::size_t group = 0; group < group_count; ++group) {
            const std::size_t offset = group * 3;
            const std::uint32_t a = features[offset];
            const std::uint32_t b = features[offset + 1];
            const std::uint32_t c = features[offset + 2];
            if (a >= b && a >= c) {
                output.groups[0].push_back(a);
                if (b >= c) {
                    output.groups[1].push_back(b);
                    output.groups[2].push_back(c);
                } else {
                    output.groups[1].push_back(c);
                    output.groups[2].push_back(b);
                }
            } else if (b >= c) {
                output.groups[0].push_back(b);
                if (a >= c) {
                    output.groups[1].push_back(a);
                    output.groups[2].push_back(c);
                } else {
                    output.groups[1].push_back(c);
                    output.groups[2].push_back(a);
                }
            } else {
                output.groups[0].push_back(c);
                if (a >= b) {
                    output.groups[1].push_back(a);
                    output.groups[2].push_back(b);
                } else {
                    output.groups[1].push_back(b);
                    output.groups[2].push_back(a);
                }
            }
        }
    } else {
        for (std::size_t group_index = 0; group_index < group_count;
             ++group_index) {
            FeatureVector ranked(
                features.begin() + group_index * features_per_group_,
                features.begin() + (group_index + 1) * features_per_group_);
            for (std::size_t i = 0; i < features_per_group_; ++i) {
                for (std::size_t j = i + 1; j < features_per_group_; ++j) {
                    if (ranked[j] > ranked[i]) {
                        std::swap(ranked[i], ranked[j]);
                    }
                }
                output.groups[i].push_back(ranked[i]);
            }
        }
    }
    for (std::size_t rank = 0; rank < features_per_group_; ++rank) {
        output.values[rank] = hash_feature_group(output.groups[rank]);
    }
    return output;
}

}  // namespace finesse
