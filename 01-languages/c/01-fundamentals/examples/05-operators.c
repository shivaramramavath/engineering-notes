#include <stdio.h>
#include <stdint.h>

int main(void) {
    printf("=== Operators & Bitwise Precedence Demo ===\n");
    
    /* Bitwise manipulation */
    uint8_t flags = 0x05; /* 0000 0101 */
    uint8_t mask = 0x02;  /* 0000 0010 */
    
    /* Set bit */
    flags |= mask;
    printf("Flags after setting bit 1: 0x%02X\n", flags);
    
    /* Short-circuit evaluation */
    int side_effect = 0;
    int a = 0;
    if (a != 0 && ++side_effect) {
        /* Not reached */
    }
    printf("Short-circuit: side_effect = %d (should be 0)\n", side_effect);

    /* Ternary operator */
    int x = 15;
    int max_val = (x > 10) ? x : 10;
    printf("Ternary result: %d\n", max_val);

    return 0;
}
