#pragma once

#include <cstddef>
#include <cstdint>
#include <list>
#include <unordered_map>
#include <vector>

namespace finesse {

class BaseChunkCache {
public:
    explicit BaseChunkCache(std::size_t capacity_bytes);
    void put(std::size_t id, std::vector<std::uint8_t> bytes);
    const std::vector<std::uint8_t>* get(std::size_t id);
    std::size_t bytes_used() const noexcept;

private:
    struct Entry {
        std::vector<std::uint8_t> bytes;
        std::list<std::size_t>::iterator position;
    };
    void evict();

    std::size_t capacity_bytes_;
    std::size_t bytes_used_ = 0;
    std::list<std::size_t> lru_;
    std::unordered_map<std::size_t, Entry> entries_;
};

}  // namespace finesse
