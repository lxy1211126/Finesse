#include "pipeline/resemblance_pipeline.h"

#include "common/timer.h"
#include "dedup/exact_chunk_index.h"
#include "delta/delta_io.h"
#include "feature_extraction/feature_extractor.h"
#include "io/mapped_input.h"
#include "similarity/sf_index.h"
#include "storage/base_chunk_store.h"
#include "super_feature/sf_generator.h"

#include <stdexcept>
#include <utility>

namespace finesse {

ResemblancePipeline::ResemblancePipeline(
    PipelineConfig config,
    std::unique_ptr<FeatureExtractor> extractor,
    std::unique_ptr<SfGenerator> sf_generator,
    std::unique_ptr<DeltaEncoder> delta_encoder)
    : config_(std::move(config)),
      extractor_(std::move(extractor)),
      sf_generator_(std::move(sf_generator)),
      delta_io_(std::move(delta_encoder), config_.store_directory) {
    if (!extractor_ || !sf_generator_) {
        throw std::invalid_argument(
            "feature extractor and SF generator must not be null");
    }
}

RunMetrics ResemblancePipeline::run(
    const std::vector<std::string>& input_paths) {
    if (input_paths.empty()) {
        throw std::invalid_argument("at least one input is required");
    }

    RunMetrics metrics;
    metrics.method = config_.method;
    metrics.dataset = config_.dataset;
    metrics.run_id = config_.run_id;
    metrics.candidate_policy = config_.candidate_policy;

    Timer whole_run;
    double sf_elapsed_ms = 0;
    std::uint64_t sf_input_bytes = 0;
    std::uint64_t unique_input_bytes = 0;
    ExactChunkIndex exact_index;
    SfIndex sf_index;
    BaseChunkStore base_store(config_.cache_bytes);
    RabinChunker chunker(config_.chunking);
    std::vector<MappedInput> inputs;
    inputs.reserve(input_paths.size());
    for (const auto& path : input_paths) {
        inputs.emplace_back(path);
        metrics.input_bytes += inputs.back().size();
    }

    for (std::size_t source_id = 0; source_id < inputs.size(); ++source_id) {
        const MappedInput& input = inputs[source_id];
        const auto chunks = chunker.chunk(input.data(), input.size());
        for (const Chunk& chunk : chunks) {
            if (exact_index.contains(chunk.data_ptr, chunk.length)) {
                continue;
            }

            const std::size_t chunk_id = base_store.chunk_count();
            std::vector<std::uint8_t> raw(
                chunk.data_ptr, chunk.data_ptr + chunk.length);
            exact_index.insert(raw.data(), raw.size(), chunk_id);
            ++metrics.chunks;
            sf_input_bytes += raw.size();
            unique_input_bytes += raw.size();

            Timer sf_timer;
            const FeatureVector features =
                extractor_->extract(raw.data(), raw.size());
            const SuperFeatures super_features =
                sf_generator_->generate(features);
            sf_elapsed_ms += sf_timer.elapsed_ms();
            metrics.feature_count += features.size();
            metrics.sf_count += super_features.values.size();

            const auto candidate_ids =
                sf_index.candidates(super_features.values);
            const bool matched = !candidate_ids.empty();
            if (matched) {
                ++metrics.chunks_with_candidates;
                metrics.candidate_count += candidate_ids.size();
                metrics.candidate_target_bytes += raw.size();
            }

            finesse::StoredRepresentation representation;
            if (config_.candidate_policy == "best-delta" && matched) {
                representation =
                    delta_io_.store_best(
                        chunk_id, raw, candidate_ids,
                        [&base_store, &inputs](std::size_t candidate_id)
                            -> const std::vector<std::uint8_t>& {
                            return base_store.read(candidate_id, inputs);
                        });
            } else {
                const std::size_t base_id =
                    matched ? candidate_ids.front() : 0;
                const std::vector<std::uint8_t>* base =
                    matched ? &base_store.read(base_id, inputs) : nullptr;
                representation = delta_io_.store(
                    chunk_id,
                    matched ? std::optional<std::size_t>(base_id)
                            : std::nullopt,
                    raw, base);
            }
            if (representation.delta_encoded) {
                ++metrics.encoded_chunks;
                metrics.delta_bytes += representation.payload.size();
                metrics.delta_target_bytes += raw.size();
                if (!metrics.roundtrip_sampled) {
                    if (!representation.base_id ||
                        delta_io_.verify_roundtrip(
                            base_store.read(*representation.base_id, inputs),
                            representation) != raw) {
                        throw std::runtime_error(
                            "delta verification failed for chunk " +
                            std::to_string(chunk_id));
                    }
                    metrics.roundtrip_sampled = true;
                }
            } else if (matched) {
                metrics.delta_fallback_raw_bytes += raw.size();
            } else {
                metrics.no_candidate_raw_bytes += raw.size();
            }

            base_store.register_chunk(chunk_id, source_id, chunk.offset,
                                      chunk.length, std::move(raw));
            sf_index.insert(super_features.values, chunk_id);
        }
    }

    const std::uint64_t stored_bytes =
        unique_input_bytes - metrics.delta_target_bytes + metrics.delta_bytes;
    metrics.dcr = stored_bytes == 0
                      ? 0.0
                      : static_cast<double>(unique_input_bytes) /
                            static_cast<double>(stored_bytes);
    if (metrics.candidate_target_bytes != 0) {
        metrics.dce_residual_fraction = compute_dce_residual_fraction(
            metrics.delta_bytes, metrics.delta_fallback_raw_bytes,
            metrics.candidate_target_bytes);
        metrics.dce_savings_fraction = compute_dce_savings_fraction(
            metrics.delta_bytes, metrics.delta_fallback_raw_bytes,
            metrics.candidate_target_bytes);
    }

    metrics.elapsed_ms = whole_run.elapsed_ms();
    const double seconds = metrics.elapsed_ms / 1000.0;
    metrics.throughput_mbps =
        seconds == 0 ? 0 : (metrics.input_bytes / 1048576.0) / seconds;
    const double sf_seconds = sf_elapsed_ms / 1000.0;
    metrics.sf_speed_mbps =
        sf_seconds == 0 ? 0 : (sf_input_bytes / 1048576.0) / sf_seconds;
    return metrics;
}

}  // namespace finesse
