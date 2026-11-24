#include "delta/xdelta_encoder.h"

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>

int main() {
    try {
        std::vector<std::uint8_t> base(48 * 1024);
        for (std::size_t i = 0; i < base.size(); ++i) {
            base[i] = static_cast<std::uint8_t>((i * 29 + i / 13) & 0xff);
        }
        std::vector<std::uint8_t> target = base;
        for (std::size_t i = 8000; i < 8200; ++i) {
            target[i] ^= 0x5a;
        }

        finesse::XdeltaEncoder encoder;
        const auto delta = encoder.encode(base, target);
        const auto restored = encoder.decode(base, delta);
        if (restored != target) {
            throw std::runtime_error("memory API round-trip mismatch");
        }
        if (delta.size() >= target.size()) {
            throw std::runtime_error("similar target did not delta-compress");
        }
        std::cout << "xdelta3_memory_tests: PASS ("
                  << delta.size() << " delta bytes)\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "xdelta3_memory_tests: FAIL: " << error.what() << '\n';
        return 1;
    }
}
