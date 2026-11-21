#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <limits.h>

int main(void) {
    printf("=== C Data Types & Widths Verification ===\n");
    printf("sizeof(char):        %zu byte  (Bits: %d)\n", sizeof(char), CHAR_BIT);
    printf("sizeof(short):       %zu bytes\n", sizeof(short));
    printf("sizeof(int):         %zu bytes\n", sizeof(int));
    printf("sizeof(long):        %zu bytes\n", sizeof(long));
    printf("sizeof(long long):   %zu bytes\n", sizeof(long long));
    printf("sizeof(size_t):      %zu bytes\n", sizeof(size_t));
    printf("sizeof(ptrdiff_t):   %zu bytes\n", sizeof(ptrdiff_t));
    printf("sizeof(intptr_t):    %zu bytes\n", sizeof(intptr_t));
    printf("sizeof(double):      %zu bytes\n", sizeof(double));

    /* Fixed-width integers */
    uint32_t val32 = 0xDEADBEEFU;
    int64_t val64 = 9223372036854775807LL;
    printf("uint32_t value:      0x%X\n", val32);
    printf("int64_t max value:   %lld\n", (long long)val64);
    return 0;
}
