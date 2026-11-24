#pragma once

#include <cstddef>
#include <cstdint>

namespace finesse {

struct Chunk {
    std::size_t offset = 0;
    std::size_t length = 0;
    const std::uint8_t* data_ptr = nullptr;
};

}  // namespace finesse
