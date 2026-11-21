#include <stdio.h>

/* Parameter points to read-only memory; cannot be modified by callee */
static void inspect_data(const int *data, size_t count) {
    printf("Inspecting data: [");
    for (size_t i = 0; i < count; ++i) {
        printf("%d%s", data[i], (i + 1 < count) ? ", " : "");
    }
    printf("]\n");
}

int main(void) {
    printf("=== Const-Correctness & Pointer Semantics ===\n");
    int values[] = { 10, 20, 30, 40 };
    inspect_data(values, 4);

    /* Pointer to const vs Const pointer */
    const int target = 99;
    const int *ptr_to_const = &target; /* Value cannot change via ptr */
    printf("Value via ptr_to_const: %d\n", *ptr_to_const);

    int mutable_val = 100;
    int *const const_ptr = &mutable_val; /* Pointer address cannot change */
    *const_ptr = 200;                    /* Value CAN change! */
    printf("Mutated value via const_ptr: %d\n", *const_ptr);

    return 0;
}
