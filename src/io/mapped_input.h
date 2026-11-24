#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace finesse {

class MappedInput {
public:
    explicit MappedInput(std::string path);
    ~MappedInput();

    MappedInput(const MappedInput&) = delete;
    MappedInput& operator=(const MappedInput&) = delete;
    MappedInput(MappedInput&& other) noexcept;
    MappedInput& operator=(MappedInput&& other) noexcept;

    const std::uint8_t* data() const noexcept;
    std::size_t size() const noexcept;
    const std::string& path() const noexcept;
    std::vector<std::uint8_t> read_range(std::size_t offset,
                                         std::size_t length) const;

private:
    void close() noexcept;

    std::string path_;
    const std::uint8_t* data_ = nullptr;
    std::size_t size_ = 0;
};

}  // namespace finesse
