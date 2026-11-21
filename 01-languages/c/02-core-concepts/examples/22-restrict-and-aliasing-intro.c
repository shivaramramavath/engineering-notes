#include <stdio.h>

/* Restrict indicates disjoint memory blocks */
static void vector_accumulate(int * restrict sum, const int * restrict vals, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        *sum += vals[i];
    }
}

int main(void) {
    printf("=== C Restrict and Aliasing Demo ===\n");

    int numbers[4] = {10, 20, 30, 40};
    int total = 0;

    vector_accumulate(&total, numbers, 4);
    printf("Vector accumulate result: total=%d\n", total);

    /* Strict aliasing char* exception: inspect bytes of integer */
    unsigned int word = 0x12345678;
    unsigned char *byte_ptr = (unsigned char *)&word;
    printf("Low byte of 0x12345678 in memory: 0x%02X (Endianness check)\n", byte_ptr[0]);

    return 0;
}
