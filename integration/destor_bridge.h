#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct finesse_chunk_range {
    size_t offset;
    size_t length;
} finesse_chunk_range;

/* Returns 0 on success, -1 on invalid arguments or an internal exception. */
int finesse_destor_chunk_ranges(const uint8_t* data,
                                size_t length,
                                finesse_chunk_range* ranges,
                                size_t capacity,
                                size_t* range_count);

#ifdef __cplusplus
}
#endif
