#include "metrics/metric_collector.h"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <stdexcept>

namespace finesse {

double compute_dce_residual_fraction(std::uint64_t delta_bytes,
                                     std::uint64_t raw_fallback_bytes,
                                     std::uint64_t matched_target_bytes) {
    if (matched_target_bytes == 0) {
        throw std::invalid_argument("DCE requires matched target bytes");
    }
    return (static_cast<double>(delta_bytes) +
            static_cast<double>(raw_fallback_bytes)) /
           static_cast<double>(matched_target_bytes);
}

double compute_dce_savings_fraction(std::uint64_t delta_bytes,
                                    std::uint64_t raw_fallback_bytes,
                                    std::uint64_t matched_target_bytes) {
    return 1.0 - compute_dce_residual_fraction(
                     delta_bytes, raw_fallback_bytes, matched_target_bytes);
}

void append_metrics_csv(const std::string& path, const RunMetrics& metrics) {
    const std::string header =
        "method,dataset,run_id,candidate_policy,dcr,dce_residual_fraction,"
        "dce_savings_fraction,sf_speed_mbps,throughput_mbps,chunks,"
        "sf_count,feature_count,elapsed_ms,input_bytes,encoded_chunks,"
        "chunks_with_candidates,candidate_count,candidate_target_bytes,"
        "no_candidate_raw_bytes,delta_fallback_raw_bytes,delta_bytes,"
        "delta_target_bytes";
    const auto parent = std::filesystem::path(path).parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent);
    }
    const bool exists = std::filesystem::exists(path);
    if (exists && std::filesystem::file_size(path) != 0) {
        std::ifstream existing(path);
        std::string existing_header;
        std::getline(existing, existing_header);
        if (existing_header != header) {
            throw std::runtime_error(
                "metrics CSV header mismatch; use a new output file: " +
                path);
        }
    }
    std::ofstream output(path, std::ios::app);
    if (!output) {
        throw std::runtime_error("cannot open metrics CSV: " + path);
    }
    if (!exists || std::filesystem::file_size(path) == 0) {
        output << header << '\n';
    }
    output << std::setprecision(10) << metrics.method << ','
           << metrics.dataset << ',' << metrics.run_id << ','
           << metrics.candidate_policy << ',' << metrics.dcr << ',';
    if (metrics.dce_residual_fraction.has_value()) {
        output << *metrics.dce_residual_fraction;
    }
    output << ',';
    if (metrics.dce_savings_fraction.has_value()) {
        output << *metrics.dce_savings_fraction;
    }
    output << ',' << metrics.sf_speed_mbps << ','
           << metrics.throughput_mbps << ',' << metrics.chunks << ','
           << metrics.sf_count << ',' << metrics.feature_count << ','
           << metrics.elapsed_ms << ',' << metrics.input_bytes << ','
           << metrics.encoded_chunks << ',' << metrics.chunks_with_candidates
           << ',' << metrics.candidate_count << ','
           << metrics.candidate_target_bytes << ','
           << metrics.no_candidate_raw_bytes << ','
           << metrics.delta_fallback_raw_bytes << ',' << metrics.delta_bytes
           << ',' << metrics.delta_target_bytes << '\n';
    if (!output) {
        throw std::runtime_error("failed writing metrics CSV: " + path);
    }
}

}  // namespace finesse
