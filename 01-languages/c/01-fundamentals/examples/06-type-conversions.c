#include <stdio.h>
#include <stdint.h>

int main(void) {
    printf("=== Type Conversions & Integer Promotion Demo ===\n");
    
    /* Integer promotion of unsigned char to signed int */
    unsigned char a = 100;
    unsigned char b = 200;
    /* In expression (a + b), both promote to int! */
    int sum = a + b;
    printf("Unsigned char sum promoted to int: %d\n", sum);

    /* Sign conversion trap demonstration */
    int32_t negative = -1;
    uint32_t positive = 1;
    if ((uint32_t)negative > positive) {
        printf("Caution: (uint32_t)-1 (%u) > 1 is TRUE via unsigned conversion\n", (uint32_t)negative);
    }

    /* Floating point truncation */
    double pi = 3.1415926535;
    int truncated_pi = (int)pi;
    printf("Double %.4f explicitly cast to int: %d\n", pi, truncated_pi);

    return 0;
}
