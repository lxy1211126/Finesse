#include "delta/delta_io.h"
#include "delta/xdelta_encoder.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

std::vector<std::uint8_t> read_bytes(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input),
            std::istreambuf_iterator<char>()};
}

}  // namespace

int main() {
    std::random_device random;
    const auto output = std::filesystem::temp_directory_path() /
        ("finesse-delta-io-tests-" + std::to_string(random()));
    try {
        std::vector<std::uint8_t> base(48 * 1024);
        for (std::size_t i = 0; i < base.size(); ++i) {
            base[i] = static_cast<std::uint8_t>((i * 29 + i / 13) & 0xff);
        }
        auto similar = base;
        for (std::size_t i = 8000; i < 8200; ++i) {
            similar[i] ^= 0x5a;
        }
        std::vector<std::uint8_t> unrelated(base.size());
        std::mt19937 generator(0x51f1U);
        for (std::size_t i = 0; i < unrelated.size(); ++i) {
            unrelated[i] = static_cast<std::uint8_t>(generator() & 0xff);
        }

        finesse::DeltaIo io(std::make_unique<finesse::XdeltaEncoder>(),
                            output.string());
        const auto delta = io.store(11, 3, similar, &base);
        require(delta.delta_encoded, "similar target should use delta");
        require(io.verify_roundtrip(base, delta) == similar,
                "delta round-trip mismatch");

        const auto raw = io.store(12, 3, unrelated, &base);
        require(!raw.delta_encoded, "larger delta should fall back to raw");
        require(raw.payload == unrelated, "raw payload mismatch");
        require(io.verify_roundtrip(base, raw) == unrelated,
                "raw round-trip mismatch");

        const std::vector<std::size_t> candidate_ids = {7, 3};
        const auto best = io.store_best(
            13, similar, candidate_ids,
            [&unrelated, &base](std::size_t id)
                -> const std::vector<std::uint8_t>& {
                return id == 7 ? unrelated : base;
            });
        require(best.delta_encoded && best.base_id == 3,
                "best-delta must retain the base with the smallest payload");
        require(io.verify_roundtrip(base, best) == similar,
                "best-delta round-trip mismatch");

        const auto payload = read_bytes(output / "blocks.bin");
        require(payload.size() == delta.payload.size() + raw.payload.size() +
                                      best.payload.size(),
                "payload file size mismatch");

        std::ifstream index(output / "index.tsv");
        const std::string contents{std::istreambuf_iterator<char>(index),
                                   std::istreambuf_iterator<char>()};
        require(contents.find("11\tdelta\t3\t49152\t0\t") !=
                    std::string::npos,
                "delta index record missing");
        require(contents.find("12\traw\t-\t49152\t" +
                              std::to_string(delta.payload.size()) + "\t") !=
                    std::string::npos,
                "raw index record or payload offset incorrect");
        require(contents.find("13\tdelta\t3\t49152\t") !=
                    std::string::npos,
                "best-delta index did not store the selected base ID");

        std::filesystem::remove_all(output);
        std::cout << "delta_io_tests: PASS\n";
        return 0;
    } catch (const std::exception& error) {
        std::filesystem::remove_all(output);
        std::cerr << "delta_io_tests: FAIL: " << error.what() << '\n';
        return 1;
    }
}
