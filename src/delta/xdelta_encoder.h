#pragma once

#include "delta/delta_encoder.h"

#include <string>

namespace finesse {

class XdeltaEncoder final : public DeltaEncoder {
public:
    explicit XdeltaEncoder(std::string executable = "xdelta3-native");
    std::vector<std::uint8_t> encode(
        const std::vector<std::uint8_t>& base,
        const std::vector<std::uint8_t>& target) const override;
    std::vector<std::uint8_t> decode(
        const std::vector<std::uint8_t>& base,
        const std::vector<std::uint8_t>& delta) const override;

private:
    std::string executable_;
};

}  // namespace finesse
