#include "xdelta3.h"

#include "delta/xdelta3_memory_adapter.h"

int finesse_xdelta_encode_memory(const uint8_t* target,
                                 uint32_t target_size,
                                 const uint8_t* source,
                                 uint32_t source_size,
                                 uint8_t* output,
                                 uint32_t* output_size,
                                 uint32_t output_capacity) {
    usize_t encoded_size = 0;
    const int result = xd3_encode_memory(
        target, target_size, source, source_size, output, &encoded_size,
        output_capacity, XD3_SEC_LZMA);
    *output_size = encoded_size;
    return result;
}

int finesse_xdelta_decode_memory(const uint8_t* delta,
                                 uint32_t delta_size,
                                 const uint8_t* source,
                                 uint32_t source_size,
                                 uint8_t* output,
                                 uint32_t* output_size,
                                 uint32_t output_capacity) {
    usize_t decoded_size = 0;
    const int result = xd3_decode_memory(
        delta, delta_size, source, source_size, output, &decoded_size,
        output_capacity, 0);
    *output_size = decoded_size;
    return result;
}
