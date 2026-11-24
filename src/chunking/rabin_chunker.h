#pragma once

#include "common/constants.h"
#include "common/types.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace finesse {

struct RabinChunkingConfig {
    std::size_t expected_size = kExpectedChunkSize;
    std::size_t min_size = kMinimumChunkSize;
    std::size_t max_size = kMaximumChunkSize;
    std::uint64_t breakmark = kRabinBreakmark;
};

class RabinChunker {
public:
    explicit RabinChunker(RabinChunkingConfig config = {});
    std::vector<Chunk> chunk(const std::uint8_t* data,
                             std::size_t length) const;

private:
    RabinChunkingConfig config_;
};

}  // namespace finesse
