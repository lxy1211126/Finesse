#include "cache/base_chunk_cache.h"

#include <utility>

namespace finesse {

BaseChunkCache::BaseChunkCache(std::size_t capacity_bytes)
    : capacity_bytes_(capacity_bytes) {}

void BaseChunkCache::put(std::size_t id,
                         std::vector<std::uint8_t> bytes) {
    const auto old = entries_.find(id);
    if (old != entries_.end()) {
        bytes_used_ -= old->second.bytes.size();
        lru_.erase(old->second.position);
        entries_.erase(old);
    }
    if (bytes.size() > capacity_bytes_) {
        return;
    }
    lru_.push_front(id);
    const std::size_t byte_count = bytes.size();
    entries_.emplace(id, Entry{std::move(bytes), lru_.begin()});
    bytes_used_ += byte_count;
    evict();
}

const std::vector<std::uint8_t>* BaseChunkCache::get(std::size_t id) {
    const auto found = entries_.find(id);
    if (found == entries_.end()) {
        return nullptr;
    }
    lru_.splice(lru_.begin(), lru_, found->second.position);
    found->second.position = lru_.begin();
    return &found->second.bytes;
}

std::size_t BaseChunkCache::bytes_used() const noexcept {
    return bytes_used_;
}

void BaseChunkCache::evict() {
    while (bytes_used_ > capacity_bytes_ && !lru_.empty()) {
        const std::size_t id = lru_.back();
        const auto found = entries_.find(id);
        bytes_used_ -= found->second.bytes.size();
        entries_.erase(found);
        lru_.pop_back();
    }
}

}  // namespace finesse
