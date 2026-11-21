#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>

static void mutate_via_pointer(int *target) {
    if (target) {
        *target += 50;
    }
}

int main(void) {
    printf("=== Pointer Operations & Provenance Demo ===\n");
    
    int val = 100;
    int *ptr = &val;
    int **dptr = &ptr;

    printf("Original value: %d\n", val);
    mutate_via_pointer(ptr);
    printf("After mutate_via_pointer(*ptr): %d\n", val);
    
    **dptr = 300;
    printf("After double-pointer dereference (**dptr = 300): %d\n", val);

    /* Pointer as integer address */
    uintptr_t addr = (uintptr_t)ptr;
    printf("Pointer virtual address via uintptr_t: 0x%" PRIxPTR "\n", addr);

    return 0;
}
