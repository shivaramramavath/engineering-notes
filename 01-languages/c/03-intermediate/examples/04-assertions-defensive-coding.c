#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stddef.h>

#ifndef static_assert
#define static_assert _Static_assert
#endif

/* Compile-time static assertions */
static_assert(sizeof(int) >= 4, "System requires at least 32-bit int");
static_assert(sizeof(void *) == sizeof(char *), "All data pointers must match pointer size");

static double calculate_mean(const int *arr, size_t count) {
    /* Defensive runtime invariant assertion */
    assert(arr != NULL && "Input array must not be NULL");
    assert(count > 0 && "Count must be greater than zero");

    long sum = 0;
    for (size_t i = 0; i < count; ++i) {
        sum += arr[i];
    }
    return (double)sum / (double)count;
}

int main(void) {
    printf("=== C Assertions and Defensive Coding Demo ===\n");

    int values[4] = {10, 20, 30, 40};
    double mean = calculate_mean(values, 4);
    printf("Calculated mean: %.2f\n", mean);

    printf("Static assertions verified successfully at compile time.\n");
    return 0;
}
