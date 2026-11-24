#include "chunking/rabin_chunker.h"
#include "common/logger.h"
#include "common/timer.h"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

std::size_t read_size(const std::map<std::string, std::string>& values,
                      const std::string& key,
                      std::size_t fallback) {
    const auto found = values.find(key);
    if (found == values.end()) {
        return fallback;
    }
    std::size_t consumed = 0;
    const unsigned long long parsed = std::stoull(found->second, &consumed);
    if (consumed != found->second.size()) {
        throw std::runtime_error("invalid config value for " + key);
    }
    return static_cast<std::size_t>(parsed);
}

finesse::RabinChunkingConfig read_config(const std::string& path) {
    std::ifstream file(path);
    if (!file) {
        throw std::runtime_error("cannot open config: " + path);
    }
    std::map<std::string, std::string> values;
    std::string line;
    while (std::getline(file, line)) {
        const std::size_t comment = line.find('#');
        if (comment != std::string::npos) {
            line.resize(comment);
        }
        const std::size_t equals = line.find('=');
        if (equals == std::string::npos) {
            continue;
        }
        const std::string key = line.substr(0, equals);
        const std::string value = line.substr(equals + 1);
        if (key.empty() || value.empty()) {
            throw std::runtime_error("invalid config line: " + line);
        }
        values[key] = value;
    }
    if (read_size(values, "window_size", finesse::kRabinWindowSize) !=
        finesse::kRabinWindowSize) {
        throw std::runtime_error("window_size must be 48 in stage 1");
    }
    return {
        read_size(values, "expected_size", finesse::kExpectedChunkSize),
        read_size(values, "min_size", finesse::kMinimumChunkSize),
        read_size(values, "max_size", finesse::kMaximumChunkSize),
        read_size(values, "breakmark", finesse::kRabinBreakmark),
    };
}

}  // namespace

int main(int argc, char** argv) {
    try {
        std::string input_path;
        std::string config_path = "configs/rabin.params";
        for (int i = 1; i < argc; ++i) {
            const std::string option = argv[i];
            if ((option == "--input" || option == "--config") &&
                i + 1 < argc) {
                const std::string value = argv[++i];
                if (option == "--input") {
                    input_path = value;
                } else {
                    config_path = value;
                }
            } else {
                std::cerr
                    << "Usage: run_chunk_only --input FILE [--config FILE]\n";
                return 2;
            }
        }
        if (input_path.empty()) {
            std::cerr << "Usage: run_chunk_only --input FILE [--config FILE]\n";
            return 2;
        }
        std::ifstream input(input_path, std::ios::binary);
        if (!input) {
            throw std::runtime_error("cannot open input: " + input_path);
        }
        std::vector<std::uint8_t> bytes(
            std::istreambuf_iterator<char>(input), {});
        finesse::Timer timer;
        const finesse::RabinChunker chunker(read_config(config_path));
        const auto chunks = chunker.chunk(bytes.data(), bytes.size());
        const double elapsed_ms = timer.elapsed_ms();
        std::size_t total = 0;
        std::size_t minimum = chunks.empty() ? 0 : chunks.front().length;
        std::size_t maximum = 0;
        for (const auto& chunk : chunks) {
            total += chunk.length;
            minimum = std::min(minimum, chunk.length);
            maximum = std::max(maximum, chunk.length);
        }
        if (total != bytes.size()) {
            throw std::runtime_error("chunk partition does not cover input");
        }
        finesse::log(finesse::LogLevel::Info, "Rabin CDC completed");
        std::cout << "input_bytes=" << bytes.size() << '\n'
                  << "chunks=" << chunks.size() << '\n'
                  << "average_chunk_bytes="
                  << (chunks.empty() ? 0.0
                                     : static_cast<double>(total) /
                                           chunks.size())
                  << '\n'
                  << "min_chunk_bytes=" << minimum << '\n'
                  << "max_chunk_bytes=" << maximum << '\n'
                  << "elapsed_ms=" << elapsed_ms << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 1;
    }
}
