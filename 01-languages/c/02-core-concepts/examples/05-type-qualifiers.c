#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

/* Demonstrate type qualifiers: const, volatile, restrict, and basic atomic concepts */

/* Restrict promise: a and b do not overlap with out */
static void vector_add(size_t n, int * restrict out, const int * restrict a, const int * restrict b) {
    for (size_t i = 0; i < n; ++i) {
        out[i] = a[i] + b[i];
    }
}

/* Const qualification: pointer-to-const vs const-pointer */
static void inspect_const(const int *ptr, int val) {
    printf("Pointer-to-const points to value: %d\n", *ptr);
    /* *ptr = 10; -> compile error! */
    (void)val;
}

/* Volatile emulation: simulated hardware status register polling */
static void simulate_hardware_poll(void) {
    volatile uint32_t simulated_status_reg = 0x0000;
    printf("Simulated hardware status initial: 0x%04X\n", (unsigned int)simulated_status_reg);
    
    /* Simulate peripheral signaling ready flag */
    simulated_status_reg |= 0x0001;
    if (simulated_status_reg & 0x0001) {
        printf("Peripheral ready bit detected: 0x%04X\n", (unsigned int)simulated_status_reg);
    }
}

int main(void) {
    printf("=== C Type Qualifiers Demo ===\n");
    
    /* 1. const correctness */
    const int read_only_val = 42;
    int mutable_val = 100;
    const int *p1 = &read_only_val; /* pointer to const */
    int * const p2 = &mutable_val;   /* const pointer to mutable int */
    
    inspect_const(p1, *p1);
    *p2 = 200; /* Allowed: target is mutable */
    printf("Mutated through const pointer: %d\n", *p2);
    
    /* 2. restrict demonstration */
    int arr_a[4] = {1, 2, 3, 4};
    int arr_b[4] = {10, 20, 30, 40};
    int arr_res[4] = {0};
    vector_add(4, arr_res, arr_a, arr_b);
    printf("Restrict vector add result: [%d, %d, %d, %d]\n",
           arr_res[0], arr_res[1], arr_res[2], arr_res[3]);
           
    /* 3. volatile demonstration */
    simulate_hardware_poll();
    
    /* 4. Combining qualifiers: const volatile register */
    /* Read-only to code, but modified by external hardware/signal */
    const volatile uint32_t fake_timer = 12345;
    printf("Const volatile value read: %u\n", (unsigned int)fake_timer);

    return 0;
}
