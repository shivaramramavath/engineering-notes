#include <stdio.h>

/* File-scope static variable with static storage duration */
static int g_counter = 0;

void increment_static_counter(void) {
    /* Block-scope static variable retains value across calls */
    static int call_count = 0;
    call_count++;
    g_counter += 10;
    printf("Call #%d: call_count = %d, g_counter = %d\n", call_count, call_count, g_counter);
}

int main(void) {
    printf("=== Storage Duration & Constant Demonstration ===\n");
    
    /* Automatic storage duration */
    int auto_val = 5;
    printf("Automatic variable 'auto_val' = %d\n", auto_val);

    increment_static_counter();
    increment_static_counter();
    increment_static_counter();

    /* Compile-time constant via enum */
    enum Config { MAX_ITEMS = 128 };
    printf("Compile-time enum constant MAX_ITEMS = %d\n", MAX_ITEMS);

    return 0;
}
