#include "similarity/sf_index.h"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace finesse {

void SfIndex::insert(const std::vector<std::uint64_t>& super_features,
                     std::size_t chunk_id) {
    for (const auto value : super_features) {
        index_[value].push_back(chunk_id);
    }
}

bool SfIndex::first_match(
    const std::vector<std::uint64_t>& super_features,
    std::size_t* chunk_id) const {
    if (chunk_id == nullptr) {
        throw std::invalid_argument("chunk_id output must not be null");
    }
    const auto matches = candidates(super_features);
    if (matches.empty()) {
        return false;
    }
    *chunk_id = matches.front();
    return true;
}

std::vector<std::size_t> SfIndex::candidates(
    const std::vector<std::uint64_t>& super_features) const {
    std::vector<std::size_t> matches;
    for (const auto value : super_features) {
        const auto found = index_.find(value);
        if (found != index_.end() && !found->second.empty()) {
            matches.insert(matches.end(), found->second.begin(),
                           found->second.end());
        }
    }
    std::sort(matches.begin(), matches.end());
    matches.erase(std::unique(matches.begin(), matches.end()), matches.end());
    return matches;
}

}  // namespace finesse
