#include "io/mapped_input.h"

#include <fcntl.h>
#include <stdexcept>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <utility>

namespace finesse {

MappedInput::MappedInput(std::string path) : path_(std::move(path)) {
    const int fd = open(path_.c_str(), O_RDONLY);
    if (fd < 0) {
        throw std::runtime_error("cannot open input: " + path_);
    }
    struct stat info {};
    if (fstat(fd, &info) != 0 || info.st_size < 0) {
        ::close(fd);
        throw std::runtime_error("cannot stat input: " + path_);
    }
    size_ = static_cast<std::size_t>(info.st_size);
    if (size_ != 0) {
        void* mapping = mmap(nullptr, size_, PROT_READ, MAP_PRIVATE, fd, 0);
        if (mapping == MAP_FAILED) {
            ::close(fd);
            throw std::runtime_error("cannot mmap input: " + path_);
        }
        data_ = static_cast<const std::uint8_t*>(mapping);
    }
    ::close(fd);
}

MappedInput::~MappedInput() {
    close();
}

MappedInput::MappedInput(MappedInput&& other) noexcept
    : path_(std::move(other.path_)),
      data_(other.data_),
      size_(other.size_) {
    other.data_ = nullptr;
    other.size_ = 0;
}

MappedInput& MappedInput::operator=(MappedInput&& other) noexcept {
    if (this != &other) {
        close();
        path_ = std::move(other.path_);
        data_ = other.data_;
        size_ = other.size_;
        other.data_ = nullptr;
        other.size_ = 0;
    }
    return *this;
}

const std::uint8_t* MappedInput::data() const noexcept {
    return data_;
}

std::size_t MappedInput::size() const noexcept {
    return size_;
}

const std::string& MappedInput::path() const noexcept {
    return path_;
}

std::vector<std::uint8_t> MappedInput::read_range(
    std::size_t offset, std::size_t length) const {
    if (offset > size_ || length > size_ - offset) {
        throw std::out_of_range("input range extends beyond mapped file");
    }
    return {data_ + offset, data_ + offset + length};
}

void MappedInput::close() noexcept {
    if (data_ != nullptr) {
        munmap(const_cast<std::uint8_t*>(data_), size_);
        data_ = nullptr;
        size_ = 0;
    }
}

}  // namespace finesse
