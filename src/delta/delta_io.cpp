#include "delta/delta_io.h"

#include <filesystem>
#include <stdexcept>
#include <utility>

namespace finesse {

DeltaIo::DeltaIo(std::unique_ptr<DeltaEncoder> encoder,
                 std::string output_directory)
    : encoder_(std::move(encoder)),
      output_directory_(std::move(output_directory)) {
    if (!encoder_) {
        throw std::invalid_argument("delta encoder must not be null");
    }
    if (output_directory_.empty()) {
        return;
    }
    const std::filesystem::path directory(output_directory_);
    if (std::filesystem::exists(directory) &&
        !std::filesystem::is_empty(directory)) {
        throw std::runtime_error(
            "delta output directory must be empty: " + output_directory_);
    }
    std::filesystem::create_directories(directory);
    payload_file_.open(directory / "blocks.bin",
                       std::ios::binary | std::ios::out | std::ios::trunc);
    index_file_.open(directory / "index.tsv",
                     std::ios::out | std::ios::trunc);
    if (!payload_file_ || !index_file_) {
        throw std::runtime_error("cannot initialize delta output files");
    }
    index_file_ << "chunk_id\tkind\tbase_id\toriginal_size\t"
                   "payload_offset\tpayload_size\n";
}

StoredRepresentation DeltaIo::store(
    std::size_t chunk_id,
    std::optional<std::size_t> base_id,
    const std::vector<std::uint8_t>& target,
    const std::vector<std::uint8_t>* base) {
    StoredRepresentation representation;
    representation.original_size = target.size();
    representation.payload = target;
    if (base_id.has_value() != (base != nullptr)) {
        throw std::invalid_argument("base ID and base bytes must be supplied together");
    }
    if (base != nullptr) {
        auto delta = encoder_->encode(*base, target);
        if (delta.size() < target.size()) {
            representation.delta_encoded = true;
            representation.base_id = base_id;
            representation.payload = std::move(delta);
        } else {
            base_id.reset();
        }
    }
    write_record(chunk_id, base_id, representation);
    return representation;
}

StoredRepresentation DeltaIo::store_best(
    std::size_t chunk_id,
    const std::vector<std::uint8_t>& target,
    const std::vector<std::size_t>& candidate_ids,
    const std::function<const std::vector<std::uint8_t>&(std::size_t)>&
        base_resolver) {
    if (!base_resolver) {
        throw std::invalid_argument("base resolver must not be empty");
    }
    StoredRepresentation representation;
    representation.original_size = target.size();
    representation.payload = target;
    std::optional<std::size_t> selected_base;
    for (const auto candidate_id : candidate_ids) {
        const auto& base = base_resolver(candidate_id);
        auto delta = encoder_->encode(base, target);
        if (delta.size() < representation.payload.size()) {
            representation.delta_encoded = true;
            representation.payload = std::move(delta);
            selected_base = candidate_id;
        }
    }
    representation.base_id = selected_base;
    write_record(chunk_id, selected_base, representation);
    return representation;
}

std::vector<std::uint8_t> DeltaIo::verify_roundtrip(
    const std::vector<std::uint8_t>& base,
    const StoredRepresentation& representation) const {
    if (!representation.delta_encoded) {
        return representation.payload;
    }
    return encoder_->decode(base, representation.payload);
}

void DeltaIo::write_record(
    std::size_t chunk_id,
    std::optional<std::size_t> base_id,
    const StoredRepresentation& representation) {
    if (!payload_file_.is_open()) {
        return;
    }
    payload_file_.write(
        reinterpret_cast<const char*>(representation.payload.data()),
        static_cast<std::streamsize>(representation.payload.size()));
    payload_file_.flush();
    if (!payload_file_) {
        throw std::runtime_error("cannot write delta payload");
    }
    index_file_ << chunk_id << '\t'
                << (representation.delta_encoded ? "delta" : "raw") << '\t';
    if (representation.delta_encoded && base_id.has_value()) {
        index_file_ << *base_id;
    } else {
        index_file_ << '-';
    }
    index_file_ << '\t' << representation.original_size << '\t'
                << payload_offset_ << '\t' << representation.payload.size()
                << '\n';
    index_file_.flush();
    if (!index_file_) {
        throw std::runtime_error("cannot write delta index");
    }
    payload_offset_ += representation.payload.size();
}

}  // namespace finesse
