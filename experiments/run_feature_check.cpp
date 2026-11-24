#include "chunking/rabin_chunker.h"
#include "feature_extraction/finesse_extractor.h"
#include "feature_extraction/ntransform_extractor.h"

#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using Values = std::map<std::string, std::string>;

Values read_values(const std::string& path) {
    std::ifstream file(path);
    if (!file) {
        throw std::runtime_error("cannot open config: " + path);
    }
    Values values;
    std::string line;
    while (std::getline(file, line)) {
        const std::size_t comment = line.find('#');
        if (comment != std::string::npos) {
            line.resize(comment);
        }
        const std::size_t equals = line.find('=');
        if (equals != std::string::npos) {
            values[line.substr(0, equals)] = line.substr(equals + 1);
        }
    }
    return values;
}

std::size_t read_count(const Values& values, const std::string& key) {
    const auto found = values.find(key);
    if (found == values.end()) {
        throw std::runtime_error("missing config key: " + key);
    }
    std::size_t consumed = 0;
    const auto parsed = std::stoull(found->second, &consumed);
    if (consumed != found->second.size() || parsed == 0) {
        throw std::runtime_error("invalid config value: " + key);
    }
    return static_cast<std::size_t>(parsed);
}

std::uint32_t parse_u32(const std::string& value) {
    std::size_t consumed = 0;
    const auto parsed = std::stoull(value, &consumed, 0);
    if (consumed != value.size() || parsed > 0xffffffffULL) {
        throw std::runtime_error("invalid uint32 value: " + value);
    }
    return static_cast<std::uint32_t>(parsed);
}

std::vector<finesse::LinearTransform> read_transforms(
    const std::string& path, std::size_t count) {
    const Values values = read_values(path);
    if (read_count(values, "feature_count") != count) {
        throw std::runtime_error("feature_count must match Finesse config");
    }
    std::vector<finesse::LinearTransform> transforms;
    transforms.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        const auto found = values.find("transform_" + std::to_string(i));
        if (found == values.end()) {
            throw std::runtime_error("missing transform_" +
                                     std::to_string(i));
        }
        const std::size_t comma = found->second.find(',');
        if (comma == std::string::npos) {
            throw std::runtime_error("invalid transform_" +
                                     std::to_string(i));
        }
        transforms.push_back({
            parse_u32(found->second.substr(0, comma)),
            parse_u32(found->second.substr(comma + 1)),
        });
    }
    return transforms;
}

void print_features(const char* label,
                    const finesse::FeatureVector& features) {
    std::cout << label << " (" << features.size() << "):";
    for (const auto value : features) {
        std::cout << " 0x" << std::hex << std::setw(8) << std::setfill('0')
                  << value;
    }
    std::cout << std::dec << '\n';
}

}  // namespace

int main(int argc, char** argv) {
    try {
        if (argc != 2) {
            std::cerr << "Usage: run_feature_check INPUT_FILE\n";
            return 2;
        }
        std::ifstream input(argv[1], std::ios::binary);
        if (!input) {
            throw std::runtime_error("cannot open input file");
        }
        const std::vector<std::uint8_t> bytes(
            std::istreambuf_iterator<char>(input), {});
        const Values finesse_config = read_values("configs/finesse.params");
        const std::size_t feature_count =
            read_count(finesse_config, "feature_count");
        const finesse::FinesseExtractor finesse_extractor(feature_count);
        const finesse::NTransformExtractor ntransform_extractor(
            read_transforms("configs/ntransform.params", feature_count));

        const auto chunks =
            finesse::RabinChunker{}.chunk(bytes.data(), bytes.size());
        if (chunks.empty()) {
            std::cout << "empty input: no chunk features to display\n";
            return 0;
        }
        const auto& chunk = chunks.front();
        std::cout << "first_chunk_bytes=" << chunk.length << '\n';
        print_features("finesse_features",
                       finesse_extractor.extract(chunk.data_ptr, chunk.length));
        print_features(
            "ntransform_features",
            ntransform_extractor.extract(chunk.data_ptr, chunk.length));
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 1;
    }
}
