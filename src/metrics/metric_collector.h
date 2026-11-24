#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

namespace finesse {

struct RunMetrics {
    std::string method;
    std::string dataset;
    std::string run_id;
    std::string candidate_policy = "first-fit";
    double dcr = 0;
    std::optional<double> dce_residual_fraction;
    std::optional<double> dce_savings_fraction;
    double sf_speed_mbps = 0;
    double throughput_mbps = 0;
    std::size_t chunks = 0;
    std::size_t sf_count = 0;
    std::size_t feature_count = 0;
    double elapsed_ms = 0;
    std::size_t input_bytes = 0;
    std::size_t encoded_chunks = 0;
    std::size_t chunks_with_candidates = 0;
    std::uint64_t candidate_count = 0;
    std::uint64_t delta_bytes = 0;
    std::uint64_t delta_target_bytes = 0;
    std::uint64_t candidate_target_bytes = 0;
    std::uint64_t no_candidate_raw_bytes = 0;
    std::uint64_t delta_fallback_raw_bytes = 0;
    bool roundtrip_sampled = false;
};

double compute_dce_residual_fraction(std::uint64_t delta_bytes,
                                     std::uint64_t raw_fallback_bytes,
                                     std::uint64_t matched_target_bytes);
double compute_dce_savings_fraction(std::uint64_t delta_bytes,
                                    std::uint64_t raw_fallback_bytes,
                                    std::uint64_t matched_target_bytes);

void append_metrics_csv(const std::string& path, const RunMetrics& metrics);

}  // namespace finesse
