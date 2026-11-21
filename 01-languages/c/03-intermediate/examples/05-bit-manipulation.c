#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

/* Basic operations: Set, Clear, Toggle, Test */
static inline uint32_t bit_set(uint32_t val, unsigned int bit) {
    return val | (1u << bit);
}

static inline uint32_t bit_clear(uint32_t val, unsigned int bit) {
    return val & ~(1u << bit);
}

static inline uint32_t bit_toggle(uint32_t val, unsigned int bit) {
    return val ^ (1u << bit);
}

static inline bool bit_test(uint32_t val, unsigned int bit) {
    return (val & (1u << bit)) != 0;
}

/* Bit twiddling hacks */
static inline bool is_power_of_two(uint32_t val) {
    return (val != 0) && ((val & (val - 1)) == 0);
}

static inline uint32_t isolate_lowest_set_bit(uint32_t val) {
    /* 2's complement negation -x is (~x + 1) */
    return val & (~val + 1u);
}

static inline unsigned int count_set_bits_kernighan(uint32_t val) {
    unsigned int count = 0;
    while (val != 0) {
        val &= (val - 1); /* Clears the lowest set bit */
        count++;
    }
    return count;
}

int main(void) {
    printf("=== Bit Manipulation Demonstrations ===\n");
    uint32_t flags = 0x00000000;

    /* Bit manipulation operations */
    flags = bit_set(flags, 3);
    flags = bit_set(flags, 7);
    printf("Flags after setting bits 3 and 7: 0x%08X (bit 3: %s, bit 4: %s)\n",
           (unsigned int)flags,
           bit_test(flags, 3) ? "set" : "clear",
           bit_test(flags, 4) ? "set" : "clear");

    flags = bit_toggle(flags, 3);
    printf("Flags after toggling bit 3: 0x%08X (bit 3: %s)\n",
           (unsigned int)flags,
           bit_test(flags, 3) ? "set" : "clear");

    flags = bit_clear(flags, 7);
    printf("Flags after clearing bit 7: 0x%08X\n", (unsigned int)flags);

    /* Bit hacks */
    uint32_t nums[] = {0, 1, 2, 14, 16, 40};
    size_t n_nums = sizeof(nums) / sizeof(nums[0]);
    for (size_t i = 0; i < n_nums; i++) {
        uint32_t x = nums[i];
        printf("x=%2u | power_of_two: %-5s | lowest_bit: 0x%02X | popcount: %u\n",
               (unsigned int)x,
               is_power_of_two(x) ? "true" : "false",
               (unsigned int)isolate_lowest_set_bit(x),
               count_set_bits_kernighan(x));
    }

    return 0;
}
