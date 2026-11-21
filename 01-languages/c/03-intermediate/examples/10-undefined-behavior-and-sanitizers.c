#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <limits.h>

/* Safe checked arithmetic preventing undefined behavior */
static bool safe_add_signed(int a, int b, int *result) {
    if ((b > 0 && a > INT_MAX - b) ||
        (b < 0 && a < INT_MIN - b)) {
        return false; /* Signed overflow would be Undefined Behavior */
    }
    *result = a + b;
    return true;
}

static bool safe_multiply_signed(int a, int b, int *result) {
    if (a > 0) {
        if (b > 0 && a > INT_MAX / b) return false;
        if (b < 0 && b < INT_MIN / a) return false;
    } else if (a < 0) {
        if (b > 0 && a < INT_MIN / b) return false;
        if (b < 0 && b < INT_MAX / a) return false;
    }
    *result = a * b;
    return true;
}

int main(void) {
    printf("=== Undefined Behavior Prevention and Detection ===\n");

    /* 1. Signed arithmetic overflow prevention */
    int res = 0;
    int a1 = 2000000000, b1 = 1500000000;
    printf("Attempting signed addition: %d + %d...\n", a1, b1);
    if (safe_add_signed(a1, b1, &res)) {
        printf("  Result: %d\n", res);
    } else {
        printf("  DETECTED OVERFLOW: Operation safely prevented before UB occurred.\n");
    }

    int a2 = 100000, b2 = 50000;
    printf("Attempting signed multiplication: %d * %d...\n", a2, b2);
    if (safe_multiply_signed(a2, b2, &res)) {
        printf("  Result: %d\n", res);
    } else {
        printf("  DETECTED OVERFLOW: Multiplication exceeds INT_MAX (%d).\n", INT_MAX);
    }

    /* 2. Bounds checking demonstration */
    int arr[4] = {10, 20, 30, 40};
    int requested_index = 5;
    printf("Accessing array of size 4 at index %d...\n", requested_index);
    if (requested_index >= 0 && (size_t)requested_index < (sizeof(arr)/sizeof(arr[0]))) {
        printf("  Value: %d\n", arr[requested_index]);
    } else {
        printf("  OUT-OF-BOUNDS PREVENTED: Index %d rejected (valid range 0..3).\n", requested_index);
    }

    return 0;
}
