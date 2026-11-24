#pragma once

#include "delta/delta_encoder.h"

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace finesse {

struct StoredRepresentation {
    bool delta_encoded = false;
    std::optional<std::size_t> base_id;
    std::vector<std::uint8_t> payload;
    std::size_t original_size = 0;
};

class DeltaIo {
public:
    DeltaIo(std::unique_ptr<DeltaEncoder> encoder,
            std::string output_directory = {});

    StoredRepresentation store(std::size_t chunk_id,
                               std::optional<std::size_t> base_id,
                               const std::vector<std::uint8_t>& target,
                               const std::vector<std::uint8_t>* base);
    StoredRepresentation store_best(
        std::size_t chunk_id,
        const std::vector<std::uint8_t>& target,
        const std::vector<std::size_t>& candidate_ids,
        const std::function<const std::vector<std::uint8_t>&(std::size_t)>&
            base_resolver);
    std::vector<std::uint8_t> verify_roundtrip(
        const std::vector<std::uint8_t>& base,
        const StoredRepresentation& representation) const;

private:
    void write_record(std::size_t chunk_id,
                      std::optional<std::size_t> base_id,
                      const StoredRepresentation& representation);

    std::unique_ptr<DeltaEncoder> encoder_;
    std::string output_directory_;
    std::ofstream payload_file_;
    std::ofstream index_file_;
    std::uint64_t payload_offset_ = 0;
};

}  // namespace finesse
