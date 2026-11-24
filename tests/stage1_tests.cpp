#include "chunking/rabin_chunker.h"
#include "feature_extraction/finesse_extractor.h"
#include "feature_extraction/ntransform_extractor.h"
#include "metrics/metric_collector.h"
#include "pipeline/resemblance_pipeline.h"
#include "rolling_hash/rabin_hash.h"
#include "cache/base_chunk_cache.h"
#include "similarity/sf_index.h"
#include "super_feature/finesse_sf_generator.h"
#include "super_feature/ntransform_sf_generator.h"
#include "delta/delta_encoder.h"

#include <algorithm>
#include <cstdint>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

class ConstantFeatureExtractor final : public finesse::FeatureExtractor {
public:
    finesse::FeatureVector extract(const std::uint8_t*,
                                   std::size_t) const override {
        return finesse::FeatureVector(12, 7);
    }
};

class ConstantSfGenerator final : public finesse::SfGenerator {
public:
    finesse::SuperFeatures generate(
        const finesse::FeatureVector&) const override {
        return {{1, 2, 3}, {}};
    }
};

class NonReducingDeltaEncoder final : public finesse::DeltaEncoder {
public:
    std::vector<std::uint8_t> encode(
        const std::vector<std::uint8_t>&,
        const std::vector<std::uint8_t>& target) const override {
        return target;
    }

    std::vector<std::uint8_t> decode(
        const std::vector<std::uint8_t>&,
        const std::vector<std::uint8_t>& delta) const override {
        return delta;
    }
};

void test_rabin_window_readiness() {
    finesse::RabinHash partial_hash;
    std::uint64_t partial_fingerprint = 0;
    require(!partial_hash.push(120, &partial_fingerprint) &&
                partial_fingerprint == 120,
            "Rabin hash must expose partial-window fingerprints");

    finesse::RabinHash hash;
    std::uint64_t fingerprint = 0;
    for (std::size_t i = 0; i + 1 < finesse::kRabinWindowSize; ++i) {
        require(!hash.push(static_cast<std::uint8_t>(i), &fingerprint),
                "Rabin hash became ready before 48 bytes");
    }
    require(hash.push(47, &fingerprint),
            "Rabin hash did not become ready at 48 bytes");
}

void test_chunker_checks_breakmark_at_minimum_size() {
    std::vector<std::uint8_t> data(2 * finesse::kMinimumChunkSize, 0);
    data[finesse::kMinimumChunkSize - 1] =
        static_cast<std::uint8_t>(finesse::kRabinBreakmark);
    const auto chunks = finesse::RabinChunker{}.chunk(data.data(), data.size());
    require(chunks.size() == 2 &&
                chunks.front().length == finesse::kMinimumChunkSize,
            "Rabin chunker must test the breakmark at minimum chunk size");
}

void test_rabin_sliding_matches_fresh_window() {
    finesse::RabinHash rolling;
    std::vector<std::uint8_t> bytes(256);
    std::uint32_t state = 0x9e3779b9U;
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        state = state * 1664525U + 1013904223U;
        bytes[i] = static_cast<std::uint8_t>(state >> 24);
    }
    for (std::size_t end = 0; end < bytes.size(); ++end) {
        std::uint64_t actual = 0;
        const bool ready = rolling.push(bytes[end], &actual);
        if (end + 1 < finesse::kRabinWindowSize) {
            require(!ready, "Rabin hash became ready before a full window");
            continue;
        }
        finesse::RabinHash fresh;
        std::uint64_t expected = 0;
        for (std::size_t i = end + 1 - finesse::kRabinWindowSize;
             i <= end;
             ++i) {
            fresh.push(bytes[i], &expected);
        }
        require(ready && actual == expected,
                "rolling Rabin hash differs from a fresh 48-byte window");
    }
}

void test_chunk_partition_and_bounds() {
    std::vector<std::uint8_t> data(1024 * 1024);
    std::uint32_t state = 0x12345678U;
    for (auto& byte : data) {
        state = state * 1664525U + 1013904223U;
        byte = static_cast<std::uint8_t>(state >> 24);
    }
    const auto chunks = finesse::RabinChunker{}.chunk(data.data(), data.size());
    std::size_t offset = 0;
    for (const auto& chunk : chunks) {
        require(chunk.offset == offset, "chunk offsets are not contiguous");
        require(chunk.length > 0, "zero-length chunk emitted");
        require(chunk.length <= finesse::kMaximumChunkSize,
                "chunk exceeded maximum size");
        require(chunk.data_ptr == data.data() + chunk.offset,
                "chunk data pointer is incorrect");
        offset += chunk.length;
    }
    require(offset == data.size(), "chunks do not cover input");
    require(chunks.size() > 100 && chunks.size() < 180,
            "8 KiB average target is outside a reasonable range");
}

void test_empty_input() {
    require(finesse::RabinChunker{}.chunk(nullptr, 0).empty(),
            "empty input should produce no chunks");
}

void test_feature_extractors() {
    std::vector<std::uint8_t> data(1024);
    std::uint32_t state = 0xabcdef01U;
    for (auto& byte : data) {
        state = state * 1664525U + 1013904223U;
        byte = static_cast<std::uint8_t>(state >> 24);
    }
    const finesse::FinesseExtractor finesse_extractor;
    const finesse::NTransformExtractor ntransform_extractor({
        {2827249447U, 218293535U},
        {3220741841U, 3738213998U},
        {48632400U, 3470958855U},
        {2184700674U, 2043470729U},
        {1784957449U, 3678824533U},
        {3401656142U, 3926767017U},
        {1031411598U, 3150454837U},
        {2187109917U, 3330954771U},
        {4048067190U, 2356129323U},
        {1726945797U, 3783195157U},
        {1951382292U, 1151553773U},
        {1039369418U, 1679674437U},
    });
    const auto finesse_features =
        finesse_extractor.extract(data.data(), data.size());
    const auto ntransform_features =
        ntransform_extractor.extract(data.data(), data.size());
    require(finesse_features.size() == 12,
            "Finesse must return 12 features");
    require(ntransform_features.size() == 12,
            "N-transform must return 12 features");
    require(finesse_features ==
                finesse_extractor.extract(data.data(), data.size()),
            "Finesse features are not deterministic");
    require(ntransform_features ==
                ntransform_extractor.extract(data.data(), data.size()),
            "N-transform features are not deterministic");
    require(finesse_features.back() != 0,
            "last full Finesse subchunk should produce a feature");
}

void test_finesse_features_use_chunk_continuous_rabin_windows() {
    std::vector<std::uint8_t> data(257);
    std::uint32_t state = 0x31415926U;
    for (auto& byte : data) {
        state = state * 1664525U + 1013904223U;
        byte = static_cast<std::uint8_t>(state >> 24);
    }

    constexpr std::size_t feature_count = 12;
    const std::size_t subchunk_size = data.size() / feature_count;
    finesse::FeatureVector expected(feature_count, 0);
    finesse::RabinHash rabin;
    for (std::size_t position = 0;
         position < subchunk_size * feature_count;
         ++position) {
        std::uint64_t fingerprint = 0;
        if (!rabin.push(data[position], &fingerprint)) {
            continue;
        }
        const std::size_t feature = position / subchunk_size;
        expected[feature] =
            std::max(expected[feature],
                     static_cast<std::uint32_t>(fingerprint));
    }

    const auto actual =
        finesse::FinesseExtractor(feature_count).extract(data.data(),
                                                         data.size());
    require(actual == expected,
            "Finesse must use continuous Rabin windows over equal "
            "subchunks and ignore the remainder");
    auto changed_remainder = data;
    std::fill(changed_remainder.begin() + subchunk_size * feature_count,
              changed_remainder.end(), 0xff);
    require(actual == finesse::FinesseExtractor(feature_count).extract(
                          changed_remainder.data(), changed_remainder.size()),
            "Finesse features must ignore bytes beyond the equal subchunks");
}

void test_super_feature_grouping() {
    const finesse::FeatureVector features = {
        10, 30, 20, 60, 40, 50, 70, 90, 80, 120, 100, 110,
    };
    const auto finesse_sf =
        finesse::FinesseSfGenerator{}.generate(features);
    require(finesse_sf.values.size() == 3,
            "Finesse should emit three SFs");
    require(finesse_sf.groups[0] ==
                finesse::FeatureVector({30, 60, 90, 120}),
            "Finesse SF0 must take each contiguous triplet maximum");
    require(finesse_sf.groups[1] ==
                finesse::FeatureVector({20, 50, 80, 110}),
            "Finesse SF1 must take each contiguous triplet median");
    require(finesse_sf.groups[2] ==
                finesse::FeatureVector({10, 40, 70, 100}),
            "Finesse SF2 must take each contiguous triplet minimum");

    const auto ntransform_sf =
        finesse::NTransformSfGenerator{}.generate(features);
    require(ntransform_sf.values.size() == 3,
            "N-transform should emit three SFs");
    require(ntransform_sf.groups[0] ==
                finesse::FeatureVector({10, 30, 20, 60}) &&
                ntransform_sf.groups[2] ==
                    finesse::FeatureVector({80, 120, 100, 110}),
            "N-transform SFs must hash consecutive groups of four");
}

void test_figure3_sf_hash_profile() {
    const finesse::FeatureVector features = {
        0xff9ab74eU, 0xfff82845U, 0xfff1da26U,
        0xff7f156cU, 0xff12c814U, 0xffe3735cU,
        0xff32bd8eU, 0xfff9b8d2U, 0xffe87e52U,
        0xfff93729U, 0xffe2fcafU, 0xff16ecf3U,
    };
    const auto sf = finesse::FinesseSfGenerator{}.generate(features);
    require(sf.values ==
                std::vector<std::uint64_t>({
                    0x3dda360fU, 0xfbbdf144U, 0x90cdfa3fU,
                }),
            "Figure 3 SF values changed for the selected SF Rabin profile");
}

void test_sf_index_first_fit() {
    finesse::SfIndex index;
    index.insert({100, 200, 300}, 9);
    index.insert({400, 200, 500}, 3);
    std::size_t matched = 0;
    require(index.first_match({500, 200}, &matched) && matched == 3,
            "FirstFit must select the earliest inserted matching chunk");
    require(index.candidates({500, 200}) ==
                std::vector<std::size_t>({3, 9}),
            "candidate lookup must return unique chunk IDs in FirstFit order");
    require(index.candidates({700}).empty(),
            "candidate lookup reported a nonexistent match");
    require(!index.first_match({700}, &matched),
            "SF index reported a nonexistent match");
}

void test_base_cache_lru() {
    finesse::BaseChunkCache cache(6);
    cache.put(1, {1, 2, 3});
    cache.put(2, {4, 5});
    require(cache.get(1) != nullptr, "cache missed a resident base");
    cache.put(3, {6, 7, 8});
    require(cache.get(1) != nullptr,
            "LRU cache evicted recently used base");
    require(cache.get(2) == nullptr, "LRU cache kept the oldest base");
    require(cache.bytes_used() <= 6, "LRU cache exceeded its capacity");
}

void test_dce_efficiency_definition() {
    require(std::abs(finesse::compute_dce_residual_fraction(25, 0, 100) -
                     0.25) < 1e-12,
            "DCE residual fraction should include stored candidate bytes");
    require(std::abs(finesse::compute_dce_savings_fraction(25, 0, 100) -
                     0.75) < 1e-12,
            "DCE savings fraction should be 1 - residual fraction");
    require(std::abs(
                finesse::compute_dce_residual_fraction(25, 50, 100) - 0.75) <
                1e-12,
            "DCE residual fraction must include matched raw fallbacks");
    require(std::abs(
                finesse::compute_dce_savings_fraction(25, 50, 100) - 0.25) <
                1e-12,
            "DCE savings must penalize matched raw fallbacks");
}

void test_pipeline_dce_counts_raw_fallbacks() {
    std::string directory_template =
        (std::filesystem::temp_directory_path() /
         "finesse-dce-fallback-XXXXXX").string();
    std::vector<char> writable_path(directory_template.begin(),
                                    directory_template.end());
    writable_path.push_back('\0');
    char* directory_name = mkdtemp(writable_path.data());
    require(directory_name != nullptr,
            "could not create DCE pipeline test directory");

    const std::filesystem::path directory(directory_name);
    const auto first_path = directory / "first.bin";
    const auto second_path = directory / "second.bin";
    try {
        const std::vector<std::uint8_t> first(1024, 0x11);
        const std::vector<std::uint8_t> second(1024, 0x22);
        for (const auto& item :
             {std::pair<std::filesystem::path,
                        const std::vector<std::uint8_t>&>(first_path, first),
              std::pair<std::filesystem::path,
                        const std::vector<std::uint8_t>&>(second_path,
                                                          second)}) {
            std::ofstream output(item.first, std::ios::binary);
            output.write(reinterpret_cast<const char*>(item.second.data()),
                         static_cast<std::streamsize>(item.second.size()));
            require(static_cast<bool>(output),
                    "could not write DCE pipeline test input");
        }

        finesse::PipelineConfig config;
        config.method = "test";
        config.dataset = "dce-fallback";
        config.run_id = "1";
        finesse::ResemblancePipeline pipeline(
            config, std::make_unique<ConstantFeatureExtractor>(),
            std::make_unique<ConstantSfGenerator>(),
            std::make_unique<NonReducingDeltaEncoder>());
        const auto metrics =
            pipeline.run({first_path.string(), second_path.string()});
        require(metrics.candidate_target_bytes > 0 &&
                    metrics.delta_fallback_raw_bytes ==
                        metrics.candidate_target_bytes &&
                    metrics.delta_bytes == 0,
                "test candidates did not all fall back to raw");
        require(metrics.dce_residual_fraction.has_value() &&
                    std::abs(*metrics.dce_residual_fraction - 1.0) < 1e-12 &&
                    metrics.dce_savings_fraction.has_value() &&
                    std::abs(*metrics.dce_savings_fraction) < 1e-12,
                "DCE must count candidate bytes stored as raw");
    } catch (...) {
        std::filesystem::remove_all(directory);
        throw;
    }
    std::filesystem::remove_all(directory);
}

}  // namespace

int main() {
    try {
        test_rabin_window_readiness();
        test_rabin_sliding_matches_fresh_window();
        test_chunker_checks_breakmark_at_minimum_size();
        test_chunk_partition_and_bounds();
        test_empty_input();
        test_feature_extractors();
        test_finesse_features_use_chunk_continuous_rabin_windows();
        test_super_feature_grouping();
        test_figure3_sf_hash_profile();
        test_sf_index_first_fit();
        test_base_cache_lru();
        test_dce_efficiency_definition();
        test_pipeline_dce_counts_raw_fallbacks();
        std::cout << "stage1_tests: PASS\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "stage1_tests: FAIL: " << error.what() << '\n';
        return 1;
    }
}
