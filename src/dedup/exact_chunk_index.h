#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>

namespace finesse {

class ExactChunkIndex {
public:
    bool contains(const std::uint8_t* data, std::size_t length) const;
    void insert(const std::uint8_t* data, std::size_t length,
                std::size_t chunk_id);
    std::size_t size() const noexcept;

private:
    static std::string fingerprint(const std::uint8_t* data,
                                   std::size_t length);

    std::unordered_map<std::string, std::size_t> chunks_;
};

}  // namespace finesse
