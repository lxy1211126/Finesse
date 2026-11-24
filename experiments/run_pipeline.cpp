#include "delta/xdelta_encoder.h"
#include "feature_extraction/finesse_extractor.h"
#include "feature_extraction/ntransform_extractor.h"
#include "metrics/metric_collector.h"
#include "pipeline/resemblance_pipeline.h"
#include "super_feature/finesse_sf_generator.h"
#include "super_feature/ntransform_sf_generator.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

struct Options {
    std::string method;
    std::string dataset = "dataset";
    std::string run_id = "1";
    std::string manifest;
    std::string csv = "results/processed/metrics.csv";
    std::string log;
    std::string xdelta;
    std::string store_directory;
    std::string candidate_policy = "first-fit";
    std::size_t cache_mb = 0;
    std::optional<std::uint32_t> ntransform_seed;
    std::vector<std::string> inputs;
};

using ConfigValues = std::map<std::string, std::string>;

ConfigValues read_config(const std::string& path) {
    std::ifstream config(path);
    if (!config) {
        throw std::runtime_error("cannot open config: " + path);
    }
    ConfigValues values;
    std::string line;
    while (std::getline(config, line)) {
        const std::size_t comment = line.find('#');
        if (comment != std::string::npos) line.resize(comment);
        const std::size_t equals = line.find('=');
        if (equals != std::string::npos) {
            values[line.substr(0, equals)] = line.substr(equals + 1);
        }
    }
    return values;
}

std::size_t config_count(const ConfigValues& values, const std::string& key) {
    const auto found = values.find(key);
    if (found == values.end()) {
        throw std::runtime_error("missing config key: " + key);
    }
    const auto parsed = std::stoull(found->second);
    if (parsed == 0) {
        throw std::runtime_error("config value must be positive: " + key);
    }
    return static_cast<std::size_t>(parsed);
}

void parse_args(int argc, char** argv, Options* options) {
    for (int i = 1; i < argc; ++i) {
        const std::string key = argv[i];
        if (key == "--help") {
            std::cout << "Usage: run_{finesse,ntransform} "
                         "--input FILE [--input FILE ...] "
                         "[--manifest FILE] [--dataset NAME] [--run-id ID] "
                         "[--csv FILE] [--log FILE] [--xdelta PATH] "
                         "[--store-dir DIR] "
                         "[--candidate-policy first-fit|best-delta] "
                         "[--ntransform-seed UINT32]\n";
            std::exit(0);
        }
        if (i + 1 >= argc) {
            throw std::invalid_argument("missing value for " + key);
        }
        const std::string value = argv[++i];
        if (key == "--input") {
            options->inputs.push_back(value);
        } else if (key == "--manifest") {
            options->manifest = value;
        } else if (key == "--dataset") {
            options->dataset = value;
        } else if (key == "--run-id") {
            options->run_id = value;
        } else if (key == "--csv") {
            options->csv = value;
        } else if (key == "--log") {
            options->log = value;
        } else if (key == "--xdelta") {
            options->xdelta = value;
        } else if (key == "--store-dir") {
            options->store_directory = value;
        } else if (key == "--candidate-policy") {
            options->candidate_policy = value;
        } else if (key == "--cache-mb") {
            options->cache_mb = std::stoull(value);
        } else if (key == "--ntransform-seed") {
            const auto seed = std::stoull(value);
            if (seed > std::numeric_limits<std::uint32_t>::max()) {
                throw std::invalid_argument(
                    "--ntransform-seed must fit in uint32");
            }
            options->ntransform_seed = static_cast<std::uint32_t>(seed);
        } else {
            throw std::invalid_argument("unknown option: " + key);
        }
    }
}

void append_manifest(const std::string& path,
                     std::vector<std::string>* inputs) {
    std::ifstream manifest(path);
    if (!manifest) {
        throw std::runtime_error("cannot open manifest: " + path);
    }
    const auto base = std::filesystem::absolute(path).parent_path();
    std::string line;
    while (std::getline(manifest, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }
        std::filesystem::path input(line);
        if (input.is_relative()) {
            input = base / input;
        }
        inputs->push_back(input.lexically_normal().string());
    }
}

void write_log(const std::string& path,
               const finesse::RunMetrics& metrics) {
    if (path.empty()) {
        return;
    }
    const auto parent = std::filesystem::path(path).parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent);
    }
    std::ofstream output(path, std::ios::trunc);
    if (!output) {
        throw std::runtime_error("cannot create log: " + path);
    }
    output << "method=" << metrics.method << '\n'
           << "dataset=" << metrics.dataset << '\n'
           << "run_id=" << metrics.run_id << '\n'
           << "candidate_policy=" << metrics.candidate_policy << '\n'
           << "input_bytes=" << metrics.input_bytes << '\n'
           << "unique_chunks=" << metrics.chunks << '\n'
           << "encoded_chunks=" << metrics.encoded_chunks << '\n'
           << "chunks_with_candidates=" << metrics.chunks_with_candidates
           << '\n'
           << "candidate_count=" << metrics.candidate_count << '\n'
           << "candidate_target_bytes=" << metrics.candidate_target_bytes
           << '\n'
           << "no_candidate_raw_bytes=" << metrics.no_candidate_raw_bytes
           << '\n'
           << "delta_fallback_raw_bytes="
           << metrics.delta_fallback_raw_bytes << '\n'
           << "roundtrip_sampled="
           << (metrics.roundtrip_sampled ? "true" : "false") << '\n'
           << "delta_bytes=" << metrics.delta_bytes << '\n'
           << "delta_target_bytes=" << metrics.delta_target_bytes << '\n'
           << "dcr=" << metrics.dcr << '\n'
           << "dce_residual_fraction=";
    if (metrics.dce_residual_fraction.has_value()) {
        output << *metrics.dce_residual_fraction;
    } else {
        output << "NA";
    }
    output << '\n' << "dce_savings_fraction=";
    if (metrics.dce_savings_fraction.has_value()) {
        output << *metrics.dce_savings_fraction;
    } else {
        output << "NA";
    }
    output << '\n'
           << "sf_speed_mbps=" << metrics.sf_speed_mbps << '\n'
           << "throughput_mbps=" << metrics.throughput_mbps << '\n'
           << "chunks=" << metrics.chunks << '\n'
           << "sf_count=" << metrics.sf_count << '\n'
           << "feature_count=" << metrics.feature_count << '\n'
           << "elapsed_ms=" << metrics.elapsed_ms << '\n';
}

}  // namespace

int main(int argc, char** argv) {
    try {
        Options options;
        const std::string executable =
            std::filesystem::path(argv[0]).filename().string();
        options.method = executable.find("ntransform") != std::string::npos
                             ? "ntransform"
                             : "finesse";
        parse_args(argc, argv, &options);
        if (options.ntransform_seed.has_value() &&
            options.method != "ntransform") {
            throw std::invalid_argument(
                "--ntransform-seed is only valid for run_ntransform");
        }
        if (!options.manifest.empty()) {
            append_manifest(options.manifest, &options.inputs);
        }
        if (options.inputs.empty()) {
            throw std::invalid_argument("no input files provided");
        }
        if (options.candidate_policy != "first-fit" &&
            options.candidate_policy != "best-delta") {
            throw std::invalid_argument(
                "--candidate-policy must be first-fit or best-delta");
        }
        std::unique_ptr<finesse::FeatureExtractor> extractor;
        std::unique_ptr<finesse::SfGenerator> generator;
        const ConfigValues pipeline_values =
            read_config("configs/pipeline.params");
        const ConfigValues finesse_values =
            read_config("configs/finesse.params");
        const ConfigValues ntransform_values =
            read_config("configs/ntransform.params");
        const ConfigValues rabin_values = read_config("configs/rabin.params");
        if (options.cache_mb == 0) {
            options.cache_mb = config_count(pipeline_values, "cache_mb");
        }
        if (options.xdelta.empty()) {
            const auto found = pipeline_values.find("delta");
            options.xdelta =
                found == pipeline_values.end() ? "xdelta3" : found->second;
        }
        if (options.method == "finesse") {
            const std::size_t feature_count =
                config_count(finesse_values, "feature_count");
            const std::size_t group_size =
                config_count(finesse_values, "features_per_group");
            if (feature_count % group_size != 0 ||
                config_count(finesse_values, "sf_count") != group_size) {
                throw std::runtime_error("inconsistent Finesse SF config");
            }
            extractor =
                std::make_unique<finesse::FinesseExtractor>(feature_count);
            generator =
                std::make_unique<finesse::FinesseSfGenerator>(group_size);
        } else {
            const std::size_t feature_count =
                config_count(ntransform_values, "feature_count");
            const std::size_t features_per_sf =
                config_count(ntransform_values, "features_per_sf");
            if (feature_count % features_per_sf != 0 ||
                config_count(ntransform_values, "sf_count") !=
                    feature_count / features_per_sf) {
                throw std::runtime_error("inconsistent N-transform SF config");
            }
            std::vector<finesse::LinearTransform> transforms;
            if (options.ntransform_seed.has_value()) {
                std::mt19937 generator(*options.ntransform_seed);
                transforms.reserve(feature_count);
                for (std::size_t i = 0; i < feature_count; ++i) {
                    const std::uint32_t multiplier = generator();
                    const std::uint32_t addend = generator();
                    transforms.push_back({multiplier, addend});
                }
            } else {
                for (std::size_t i = 0; i < feature_count; ++i) {
                    const std::string key =
                        "transform_" + std::to_string(i);
                    const auto found = ntransform_values.find(key);
                    if (found == ntransform_values.end()) {
                        throw std::runtime_error("missing " + key);
                    }
                    const std::size_t comma = found->second.find(',');
                    if (comma == std::string::npos) {
                        throw std::runtime_error("invalid " + key);
                    }
                    transforms.push_back({
                        static_cast<std::uint32_t>(
                            std::stoull(found->second.substr(0, comma))),
                        static_cast<std::uint32_t>(
                            std::stoull(found->second.substr(comma + 1))),
                    });
                }
            }
            extractor =
                std::make_unique<finesse::NTransformExtractor>(transforms);
            generator =
                std::make_unique<finesse::NTransformSfGenerator>(
                    features_per_sf);
        }
        finesse::PipelineConfig config;
        config.method = options.method;
        config.dataset = options.dataset;
        config.run_id = options.run_id;
        config.candidate_policy = options.candidate_policy;
        config.store_directory = options.store_directory;
        config.cache_bytes = options.cache_mb * 1024ULL * 1024ULL;
        config.chunking.expected_size =
            config_count(rabin_values, "expected_size");
        config.chunking.min_size = config_count(rabin_values, "min_size");
        config.chunking.max_size = config_count(rabin_values, "max_size");
        config.chunking.breakmark =
            config_count(rabin_values, "breakmark");
        if (config_count(rabin_values, "window_size") !=
            finesse::kRabinWindowSize) {
            throw std::runtime_error("Rabin window_size must be 48");
        }
        finesse::ResemblancePipeline pipeline(
            config, std::move(extractor), std::move(generator),
            std::make_unique<finesse::XdeltaEncoder>(options.xdelta));
        const auto metrics = pipeline.run(options.inputs);
        finesse::append_metrics_csv(options.csv, metrics);
        write_log(options.log, metrics);
        std::cout << "method=" << metrics.method << '\n'
                  << "dataset=" << metrics.dataset << '\n'
                  << "run_id=" << metrics.run_id << '\n'
                  << "candidate_policy=" << metrics.candidate_policy << '\n'
                  << "dcr=" << metrics.dcr << '\n'
                  << "dce_residual_fraction=";
        if (metrics.dce_residual_fraction.has_value())
            std::cout << *metrics.dce_residual_fraction;
        else
            std::cout << "NA";
        std::cout << '\n' << "dce_savings_fraction=";
        if (metrics.dce_savings_fraction.has_value())
            std::cout << *metrics.dce_savings_fraction;
        else
            std::cout << "NA";
        std::cout << '\n'
                  << "sf_speed_mbps=" << metrics.sf_speed_mbps << '\n'
                  << "throughput_mbps=" << metrics.throughput_mbps << '\n'
                  << "chunks=" << metrics.chunks << '\n'
                  << "chunks_with_candidates="
                  << metrics.chunks_with_candidates << '\n'
                  << "candidate_count=" << metrics.candidate_count << '\n'
                  << "candidate_target_bytes="
                  << metrics.candidate_target_bytes << '\n'
                  << "no_candidate_raw_bytes="
                  << metrics.no_candidate_raw_bytes << '\n'
                  << "delta_fallback_raw_bytes="
                  << metrics.delta_fallback_raw_bytes << '\n'
                  << "sf_count=" << metrics.sf_count << '\n'
                  << "feature_count=" << metrics.feature_count << '\n'
                  << "roundtrip_sampled="
                  << (metrics.roundtrip_sampled ? "true" : "false") << '\n'
                  << "elapsed_ms=" << metrics.elapsed_ms << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 1;
    }
}
