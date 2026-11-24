#pragma once

#include "delta/delta_encoder.h"
#include "delta/delta_io.h"
#include "chunking/rabin_chunker.h"
#include "feature_extraction/feature_extractor.h"
#include "metrics/metric_collector.h"
#include "similarity/sf_index.h"
#include "super_feature/sf_generator.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace finesse {

struct PipelineConfig {
    std::size_t cache_bytes = 400ULL * 1024 * 1024;
    RabinChunkingConfig chunking;
    std::string store_directory;
    std::string method;
    std::string dataset;
    std::string run_id;
    std::string candidate_policy = "first-fit";
};

class ResemblancePipeline {
public:
    ResemblancePipeline(PipelineConfig config,
                        std::unique_ptr<FeatureExtractor> extractor,
                        std::unique_ptr<SfGenerator> sf_generator,
                        std::unique_ptr<DeltaEncoder> delta_encoder);
    RunMetrics run(const std::vector<std::string>& input_paths);

private:
    PipelineConfig config_;
    std::unique_ptr<FeatureExtractor> extractor_;
    std::unique_ptr<SfGenerator> sf_generator_;
    DeltaIo delta_io_;
};

}  // namespace finesse
