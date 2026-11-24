#pragma once

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace finesse {

class SfIndex {
public:
    void insert(const std::vector<std::uint64_t>& super_features,
                std::size_t chunk_id);
    std::vector<std::size_t> candidates(
        const std::vector<std::uint64_t>& super_features) const;
    bool first_match(const std::vector<std::uint64_t>& super_features,
                     std::size_t* chunk_id) const;

private:
    std::unordered_map<std::uint64_t, std::vector<std::size_t>> index_;
};

}  // namespace finesse
