#include "delta/xdelta_encoder.h"

#include "delta/xdelta3_memory_adapter.h"

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <spawn.h>
#include <stdexcept>
#include <string>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

extern char** environ;

namespace finesse {
namespace {

constexpr const char* kNativeXdelta = "xdelta3-native";
constexpr std::size_t kMaximumChunkBytes = 64 * 1024;

class TemporaryDirectory {
public:
    TemporaryDirectory() {
        const char* configured_tmp = std::getenv("TMPDIR");
        const std::filesystem::path temp_root =
            configured_tmp == nullptr || configured_tmp[0] == '\0'
                ? std::filesystem::path("/tmp")
                : std::filesystem::path(configured_tmp);
        std::string pattern =
            (temp_root / "finesse-xdelta-XXXXXX").string();
        std::vector<char> writable(pattern.begin(), pattern.end());
        writable.push_back('\0');
        char* created = mkdtemp(writable.data());
        if (created == nullptr) {
            throw std::runtime_error("mkdtemp failed: " +
                                     std::string(std::strerror(errno)));
        }
        path_ = created;
    }
    ~TemporaryDirectory() {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }
    const std::filesystem::path& path() const { return path_; }

private:
    std::filesystem::path path_;
};

void write_bytes(const std::filesystem::path& path,
                 const std::vector<std::uint8_t>& bytes) {
    std::ofstream output(path, std::ios::binary);
    if (!output) {
        throw std::runtime_error("cannot create temporary input");
    }
    output.write(reinterpret_cast<const char*>(bytes.data()),
                 static_cast<std::streamsize>(bytes.size()));
    if (!output) {
        throw std::runtime_error("failed writing temporary input");
    }
}

std::vector<std::uint8_t> read_bytes(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("xdelta3 did not produce output");
    }
    return {std::istreambuf_iterator<char>(input), {}};
}

void run_xdelta(const std::string& executable,
                const std::vector<std::string>& arguments) {
    std::vector<char*> argv;
    argv.reserve(arguments.size() + 2);
    argv.push_back(const_cast<char*>(executable.c_str()));
    for (const auto& argument : arguments) {
        argv.push_back(const_cast<char*>(argument.c_str()));
    }
    argv.push_back(nullptr);
    pid_t child = 0;
    const int result = posix_spawnp(&child, executable.c_str(), nullptr,
                                    nullptr, argv.data(), environ);
    if (result != 0) {
        throw std::runtime_error("cannot launch " + executable + ": " +
                                 std::strerror(result));
    }
    int status = 0;
    while (waitpid(child, &status, 0) < 0) {
        if (errno != EINTR) {
            throw std::runtime_error("waitpid failed: " +
                                     std::string(std::strerror(errno)));
        }
    }
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        throw std::runtime_error(executable +
                                 " failed with non-zero exit status");
    }
}

std::vector<std::uint8_t> transform(
    const std::string& executable,
    const std::vector<std::uint8_t>& source,
    const std::vector<std::uint8_t>& payload,
    bool decode) {
    TemporaryDirectory temporary;
    const auto source_path = temporary.path() / "source.bin";
    const auto payload_path =
        temporary.path() / (decode ? "delta.vcdiff" : "target.bin");
    const auto output_path = temporary.path() / "output.bin";
    write_bytes(source_path, source);
    write_bytes(payload_path, payload);
    const std::vector<std::string> arguments = decode
        ? std::vector<std::string>{
              "-d", "-s", source_path.string(), payload_path.string(),
              output_path.string()}
        : std::vector<std::string>{
              "-e", "-s", source_path.string(), payload_path.string(),
              output_path.string()};
    run_xdelta(executable, arguments);
    return read_bytes(output_path);
}

std::vector<std::uint8_t> native_transform(
    const std::vector<std::uint8_t>& source,
    const std::vector<std::uint8_t>& payload,
    bool decode) {
    if (source.size() > kMaximumChunkBytes ||
        payload.size() > kMaximumChunkBytes * 2) {
        throw std::invalid_argument("xdelta input exceeds configured chunk limit");
    }
    const std::size_t initial_capacity =
        std::max<std::size_t>(4096, payload.size() * 2 + 4096);
    std::vector<std::uint8_t> output(
        decode ? kMaximumChunkBytes : initial_capacity);
    std::uint32_t output_size = 0;
    int result = 0;
    for (;;) {
        if (output.size() > std::numeric_limits<std::uint32_t>::max()) {
            throw std::runtime_error("xdelta output exceeds API size limit");
        }
        result = decode
                     ? finesse_xdelta_decode_memory(
                           payload.data(),
                           static_cast<std::uint32_t>(payload.size()),
                           source.data(),
                           static_cast<std::uint32_t>(source.size()),
                           output.data(), &output_size,
                           static_cast<std::uint32_t>(output.size()))
                     : finesse_xdelta_encode_memory(
                           payload.data(),
                           static_cast<std::uint32_t>(payload.size()),
                           source.data(),
                           static_cast<std::uint32_t>(source.size()),
                           output.data(), &output_size,
                           static_cast<std::uint32_t>(output.size()));
        if (result == 0) {
            output.resize(output_size);
            return output;
        }
        if (result != ENOSPC || output.size() >= kMaximumChunkBytes * 8) {
            throw std::runtime_error(
                "xdelta3 memory API failed with code " +
                std::to_string(result));
        }
        output.resize(output.size() * 2);
    }
}

}  // namespace

XdeltaEncoder::XdeltaEncoder(std::string executable)
    : executable_(std::move(executable)) {}

std::vector<std::uint8_t> XdeltaEncoder::encode(
    const std::vector<std::uint8_t>& base,
    const std::vector<std::uint8_t>& target) const {
    if (executable_ == kNativeXdelta) {
        return native_transform(base, target, false);
    }
    return transform(executable_, base, target, false);
}

std::vector<std::uint8_t> XdeltaEncoder::decode(
    const std::vector<std::uint8_t>& base,
    const std::vector<std::uint8_t>& delta) const {
    if (executable_ == kNativeXdelta) {
        return native_transform(base, delta, true);
    }
    return transform(executable_, base, delta, true);
}

}  // namespace finesse
