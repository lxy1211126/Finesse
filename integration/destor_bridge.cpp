#include "destor_bridge.h"

#include "chunking/rabin_chunker.h"

#include <exception>

extern "C" int finesse_destor_chunk_ranges(
    const uint8_t* data,
    size_t length,
    finesse_chunk_range* ranges,
    size_t capacity,
    size_t* range_count) {
    if (range_count == nullptr || (length != 0 && data == nullptr) ||
        (capacity != 0 && ranges == nullptr)) {
        return -1;
    }
    try {
        const auto chunks = finesse::RabinChunker{}.chunk(data, length);
        *range_count = chunks.size();
        if (capacity < chunks.size()) {
            return -1;
        }
        for (size_t i = 0; i < chunks.size(); ++i) {
            ranges[i] = {chunks[i].offset, chunks[i].length};
        }
        return 0;
    } catch (const std::exception&) {
        return -1;
    } catch (...) {
        return -1;
    }
}
