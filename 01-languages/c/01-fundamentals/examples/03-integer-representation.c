#include <stdio.h>
#include <stdint.h>
#include <limits.h>

static void inspect_bytes(const void *ptr, size_t n) {
    const uint8_t *b = (const uint8_t *)ptr;
    for (size_t i = 0; i < n; ++i) {
        printf("%02X ", b[i]);
    }
    printf("\n");
}

int main(void) {
    printf("=== Integer Representation & Two's Complement ===\n");
    int32_t pos = 1;
    int32_t neg = -1;
    int32_t min_int = INT32_MIN;

    printf("Value  1 in memory (hex bytes): ");
    inspect_bytes(&pos, sizeof(pos));

    printf("Value -1 in memory (hex bytes): ");
    inspect_bytes(&neg, sizeof(neg));

    printf("INT32_MIN in memory (hex bytes): ");
    inspect_bytes(&min_int, sizeof(min_int));

    /* Unsigned modular arithmetic wrap-around */
    uint32_t u_max = UINT32_MAX;
    uint32_t u_wrapped = u_max + 1U;
    printf("UINT32_MAX (%u) + 1 = %u (Defined modular wrap)\n", u_max, u_wrapped);

    /* Endianness check */
    uint16_t endian_test = 0x0102;
    uint8_t *first_byte = (uint8_t *)&endian_test;
    if (*first_byte == 0x02) {
        printf("Host Byte Ordering: Little-Endian\n");
    } else {
        printf("Host Byte Ordering: Big-Endian\n");
    }

    return 0;
}
