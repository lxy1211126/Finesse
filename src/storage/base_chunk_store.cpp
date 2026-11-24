#include "storage/base_chunk_store.h"

#include <stdexcept>
#include <utility>

namespace finesse {

BaseChunkStore::BaseChunkStore(std::size_t cache_bytes)
    : cache_(cache_bytes) {}

void BaseChunkStore::register_chunk(
    std::size_t chunk_id,
    std::size_t source_id,
    std::size_t source_offset,
    std::size_t chunk_size,
    std::vector<std::uint8_t> bytes) {
    if (chunk_id != locations_.size() || bytes.size() != chunk_size) {
        throw std::logic_error("invalid base chunk registration");
    }
    locations_.push_back({source_id, source_offset, chunk_size});
    cache_.put(chunk_id, std::move(bytes));
}

const std::vector<std::uint8_t>& BaseChunkStore::read(
    std::size_t chunk_id,
    const std::vector<MappedInput>& inputs) {
    if (chunk_id >= locations_.size()) {
        throw std::out_of_range("base chunk ID does not exist");
    }
    if (const auto* resident = cache_.get(chunk_id)) {
        return *resident;
    }
    const Location& location = locations_[chunk_id];
    if (location.source_id >= inputs.size()) {
        throw std::logic_error("base chunk source does not exist");
    }
    loaded_ = inputs[location.source_id].read_range(
        location.source_offset, location.chunk_size);
    cache_.put(chunk_id, loaded_);
    return loaded_;
}

std::size_t BaseChunkStore::chunk_count() const noexcept {
    return locations_.size();
}

}  // namespace finesse
