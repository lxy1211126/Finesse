#pragma once

#include "cache/base_chunk_cache.h"
#include "io/mapped_input.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace finesse {

class BaseChunkStore {
public:
    explicit BaseChunkStore(std::size_t cache_bytes);

    void register_chunk(std::size_t chunk_id,
                        std::size_t source_id,
                        std::size_t source_offset,
                        std::size_t chunk_size,
                        std::vector<std::uint8_t> bytes);
    const std::vector<std::uint8_t>& read(
        std::size_t chunk_id,
        const std::vector<MappedInput>& inputs);
    std::size_t chunk_count() const noexcept;

private:
    struct Location {
        std::size_t source_id;
        std::size_t source_offset;
        std::size_t chunk_size;
    };

    BaseChunkCache cache_;
    std::vector<Location> locations_;
    std::vector<std::uint8_t> loaded_;
};

}  // namespace finesse
