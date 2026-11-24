#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int finesse_xdelta_encode_memory(const uint8_t* target,
                                 uint32_t target_size,
                                 const uint8_t* source,
                                 uint32_t source_size,
                                 uint8_t* output,
                                 uint32_t* output_size,
                                 uint32_t output_capacity);

int finesse_xdelta_decode_memory(const uint8_t* delta,
                                 uint32_t delta_size,
                                 const uint8_t* source,
                                 uint32_t source_size,
                                 uint8_t* output,
                                 uint32_t* output_size,
                                 uint32_t output_capacity);

#ifdef __cplusplus
}
#endif
