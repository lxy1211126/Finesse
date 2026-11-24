#include "destor_bridge.h"

#include <stdio.h>
#include <stdlib.h>

int main(void) {
    const size_t input_size = 128 * 1024;
    uint8_t* input = (uint8_t*)malloc(input_size);
    if (input == NULL) {
        return 1;
    }
    for (size_t i = 0; i < input_size; ++i) {
        input[i] = (uint8_t)((i * 37 + i / 11) & 0xff);
    }
    size_t count = 0;
    if (finesse_destor_chunk_ranges(input, input_size, NULL, 0, &count) == 0 ||
        count == 0) {
        free(input);
        return 2;
    }
    finesse_chunk_range* ranges =
        (finesse_chunk_range*)calloc(count, sizeof(*ranges));
    if (ranges == NULL) {
        free(input);
        return 3;
    }
    if (finesse_destor_chunk_ranges(input, input_size, ranges, count, &count) !=
        0) {
        free(ranges);
        free(input);
        return 4;
    }
    size_t covered = 0;
    for (size_t i = 0; i < count; ++i) {
        if (ranges[i].offset != covered || ranges[i].length == 0) {
            free(ranges);
            free(input);
            return 5;
        }
        covered += ranges[i].length;
    }
    free(ranges);
    free(input);
    if (covered != input_size) {
        return 6;
    }
    printf("destor_bridge_smoke: PASS (%zu chunks, %zu bytes)\n",
           count, covered);
    return 0;
}
