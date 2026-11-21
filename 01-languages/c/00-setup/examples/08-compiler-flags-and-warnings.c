#include <stdio.h>
#include <stdint.h>

/* Clean function with zero compiler warnings */
static int32_t calculate_checksum(const uint8_t *data, size_t length) {
    if (data == NULL) {
        return -1;
    }
    int32_t sum = 0;
    for (size_t i = 0; i < length; ++i) {
        sum += (int32_t)data[i];
    }
    return sum;
}

int main(void) {
    printf("=== Compiler Flags & Warning Hygiene Demo ===\n");
    uint8_t payload[] = {0x10, 0x20, 0x30, 0x40};
    int32_t cs = calculate_checksum(payload, sizeof(payload));
    printf("Calculated checksum: %d (0x%X)\n", cs, (unsigned int)cs);
    return 0;
}
