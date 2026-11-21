#include <stdio.h>
#include <stdint.h>

int main(void) {
    printf("=== Integer Promotions & Conversions Tree ===\n");
    
    uint8_t u8_a = 250;
    uint8_t u8_b = 10;
    /* uint8_t promotes to signed int; (250 - 10) produces signed int 240 */
    printf("uint8_t expression (u8_a - u8_b) = %d\n", u8_a - u8_b);

    /* Usual arithmetic conversions: unsigned vs signed of same rank */
    int32_t s32 = -5;
    uint32_t u32 = 10;
    if ((uint32_t)s32 > u32) {
        printf("Usual arithmetic conversion: -5 becomes %u (> 10)\n", (uint32_t)s32);
    }
    return 0;
}
