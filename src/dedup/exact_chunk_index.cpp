#include "dedup/exact_chunk_index.h"

#include <openssl/evp.h>

#include <stdexcept>

namespace finesse {

bool ExactChunkIndex::contains(const std::uint8_t* data,
                               std::size_t length) const {
    return chunks_.find(fingerprint(data, length)) != chunks_.end();
}

void ExactChunkIndex::insert(const std::uint8_t* data,
                             std::size_t length,
                             std::size_t chunk_id) {
    const auto result = chunks_.emplace(fingerprint(data, length), chunk_id);
    if (!result.second) {
        throw std::logic_error("duplicate content inserted into exact index");
    }
}

std::size_t ExactChunkIndex::size() const noexcept {
    return chunks_.size();
}

std::string ExactChunkIndex::fingerprint(const std::uint8_t* data,
                                         std::size_t length) {
    if (length != 0 && data == nullptr) {
        throw std::invalid_argument("non-empty chunk must have data");
    }
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int digest_length = 0;
    EVP_MD_CTX* context = EVP_MD_CTX_new();
    if (context == nullptr) {
        throw std::runtime_error("EVP_MD_CTX_new failed");
    }
    const bool ok = EVP_DigestInit_ex(context, EVP_sha256(), nullptr) == 1 &&
                    EVP_DigestUpdate(context, data, length) == 1 &&
                    EVP_DigestFinal_ex(context, digest, &digest_length) == 1;
    EVP_MD_CTX_free(context);
    if (!ok) {
        throw std::runtime_error("SHA-256 calculation failed");
    }
    return {reinterpret_cast<const char*>(digest), digest_length};
}

}  // namespace finesse
