#include "chunking/rabin_chunker.h"

#include "rolling_hash/rabin_hash.h"

#include <stdexcept>

namespace finesse {

RabinChunker::RabinChunker(RabinChunkingConfig config) : config_(config) {
    if (config_.expected_size == 0 ||
        (config_.expected_size & (config_.expected_size - 1)) != 0 ||
        config_.min_size == 0 || config_.min_size > config_.expected_size ||
        config_.expected_size > config_.max_size) {
        throw std::invalid_argument("invalid Rabin chunking configuration");
    }
}

std::vector<Chunk> RabinChunker::chunk(const std::uint8_t* data,
                                       std::size_t length) const {
    if (length != 0 && data == nullptr) {
        throw std::invalid_argument("non-empty input must have data");
    }
    std::vector<Chunk> result;
    std::size_t chunk_start = 0;
    while (chunk_start < length) {
        const std::size_t remaining = length - chunk_start;
        if (remaining <= config_.min_size) {
            result.push_back(
                {chunk_start, remaining, data + chunk_start});
            break;
        }
        const std::size_t limit =
            remaining < config_.max_size ? remaining : config_.max_size;
        RabinHash hash;
        std::size_t chunk_size = limit;
        for (std::size_t candidate_size = config_.min_size;
             candidate_size < limit;
             ++candidate_size) {
            std::uint64_t fingerprint = 0;
            hash.push(data[chunk_start + candidate_size - 1], &fingerprint);
            if ((fingerprint & (config_.expected_size - 1)) ==
                    (config_.breakmark & (config_.expected_size - 1))) {
                chunk_size = candidate_size;
                break;
            }
        }
        result.push_back({chunk_start, chunk_size, data + chunk_start});
        chunk_start += chunk_size;
    }
    return result;
}

}  // namespace finesse
