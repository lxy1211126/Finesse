#include "rolling_hash/rabin_hash.h"

#include <stdexcept>

namespace finesse {
namespace {

std::uint64_t polynomial_mod(std::uint64_t high,
                             std::uint64_t low,
                             std::uint64_t divisor) {
    const int degree = 63 - __builtin_clzll(divisor);
    divisor <<= 63 - degree;
    if (high != 0) {
        if ((high & (std::uint64_t{1} << 63)) != 0) {
            high ^= divisor;
        }
        for (int bit = 62; bit >= 0; --bit) {
            if ((high & (std::uint64_t{1} << bit)) == 0) {
                continue;
            }
            high ^= divisor >> (63 - bit);
            low ^= divisor << (bit + 1);
        }
    }
    for (int bit = 63; bit >= degree; --bit) {
        if ((low & (std::uint64_t{1} << bit)) != 0) {
            low ^= divisor >> (63 - bit);
        }
    }
    return low;
}

void polynomial_multiply(std::uint64_t x,
                         std::uint64_t y,
                         std::uint64_t* high,
                         std::uint64_t* low) {
    std::uint64_t result_high = 0;
    std::uint64_t result_low = (x & 1U) != 0 ? y : 0;
    for (int bit = 1; bit < 64; ++bit) {
        if ((x & (std::uint64_t{1} << bit)) == 0) {
            continue;
        }
        result_high ^= y >> (64 - bit);
        result_low ^= y << bit;
    }
    *high = result_high;
    *low = result_low;
}

std::uint64_t multiply_mod(std::uint64_t x,
                           std::uint64_t y,
                           std::uint64_t divisor) {
    std::uint64_t high = 0;
    std::uint64_t low = 0;
    polynomial_multiply(x, y, &high, &low);
    return polynomial_mod(high, low, divisor);
}

struct Tables {
    Tables() {
        const int degree = 63 - __builtin_clzll(kRabinPolynomial);
        shift = degree - 8;
        const std::uint64_t top = polynomial_mod(
            0, std::uint64_t{1} << degree, kRabinPolynomial);
        for (std::size_t i = 0; i < 256; ++i) {
            mod[i] = multiply_mod(i, top, kRabinPolynomial) |
                     (static_cast<std::uint64_t>(i) << degree);
        }
        std::uint64_t size_shift = 1;
        for (std::size_t i = 1; i < kRabinWindowSize; ++i) {
            size_shift = append(size_shift, 0);
        }
        for (std::size_t i = 0; i < 256; ++i) {
            out[i] = multiply_mod(i, size_shift, kRabinPolynomial);
        }
    }

    std::uint64_t append(std::uint64_t fp, std::uint8_t byte) const {
        return ((fp << 8) | byte) ^ mod[fp >> shift];
    }

    int shift = 0;
    std::array<std::uint64_t, 256> mod{};
    std::array<std::uint64_t, 256> out{};
};

const Tables& tables() {
    static const Tables value;
    return value;
}

}  // namespace

RabinHash::RabinHash() = default;

bool RabinHash::push(std::uint8_t byte, std::uint64_t* fingerprint) {
    const Tables& lookup = tables();
    if (filled_ < window_size) {
        window_[position_] = byte;
        if (++position_ == window_size) {
            position_ = 0;
        }
        fingerprint_ = lookup.append(fingerprint_, byte);
        ++filled_;
    } else {
        const std::uint8_t outgoing = window_[position_];
        window_[position_] = byte;
        if (++position_ == window_size) {
            position_ = 0;
        }
        fingerprint_ = lookup.append(fingerprint_ ^ lookup.out[outgoing],
                                      byte);
    }
    if (fingerprint != nullptr) {
        *fingerprint = fingerprint_;
    }
    return filled_ == window_size;
}

void RabinHash::reset() {
    window_.fill(0);
    position_ = 0;
    filled_ = 0;
    fingerprint_ = 0;
}

std::uint64_t RabinHash::fingerprint_bytes(const std::uint8_t* data,
                                           std::size_t length) {
    if (length != 0 && data == nullptr) {
        throw std::invalid_argument("non-empty input must have data");
    }
    const Tables& lookup = tables();
    std::uint64_t fingerprint = 0;
    for (std::size_t i = 0; i < length; ++i) {
        fingerprint = lookup.append(fingerprint, data[i]);
    }
    return fingerprint;
}

std::uint64_t RabinHash::fingerprint_u32_be(const std::uint32_t* values,
                                            std::size_t count) {
    if (count != 0 && values == nullptr) {
        throw std::invalid_argument("non-empty values must not be null");
    }
    const Tables& lookup = tables();
    std::uint64_t fingerprint = 0;
    for (std::size_t i = 0; i < count; ++i) {
        const std::uint32_t value = values[i];
        fingerprint = lookup.append(
            fingerprint, static_cast<std::uint8_t>(value >> 24));
        fingerprint = lookup.append(
            fingerprint, static_cast<std::uint8_t>(value >> 16));
        fingerprint = lookup.append(
            fingerprint, static_cast<std::uint8_t>(value >> 8));
        fingerprint = lookup.append(
            fingerprint, static_cast<std::uint8_t>(value));
    }
    return fingerprint;
}

}  // namespace finesse
