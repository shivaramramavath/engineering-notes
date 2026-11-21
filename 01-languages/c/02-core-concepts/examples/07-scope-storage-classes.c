#include <stdio.h>

/* File-scope external linkage variable */
int global_counter = 10;

/* File-scope internal linkage variable (private to this translation unit) */
static int file_private_counter = 100;

/* Static local variable retaining state across function invocations */
static int get_next_sequence_id(void) {
    static int sequence = 1000;
    return ++sequence;
}

/* Static inline function */
static inline int square(int x) {
    return x * x;
}

int main(void) {
    printf("=== C Scope and Storage Classes Demo ===\n");

    /* Block scope and shadowing */
    int x = 50;
    printf("Outer block x: %d\n", x);
    {
        int x = 999; /* Shadows outer x */
        printf("Inner nested block x (shadowed): %d\n", x);
    }
    printf("Outer block x restored: %d\n", x);

    /* Storage duration: static local persistence */
    printf("Sequence ID 1: %d\n", get_next_sequence_id());
    printf("Sequence ID 2: %d\n", get_next_sequence_id());
    printf("Sequence ID 3: %d\n", get_next_sequence_id());

    /* Linkage inspection */
    printf("Global counter (external linkage): %d\n", global_counter);
    printf("File private counter (internal linkage): %d\n", file_private_counter);
    printf("Inline square(9): %d\n", square(9));

    return 0;
}
