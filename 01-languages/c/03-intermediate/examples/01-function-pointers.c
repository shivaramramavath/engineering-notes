#include <stdio.h>
#include <stdlib.h>

/* Callback typedef with user context pointer */
typedef void (*ItemCallback)(int value, void *user_data);

static void array_for_each(const int *arr, size_t n, ItemCallback cb, void *ctx) {
    for (size_t i = 0; i < n; ++i) {
        cb(arr[i], ctx);
    }
}

static void sum_accumulator(int value, void *ctx) {
    long *total = (long *)ctx;
    *total += value;
}

static int compare_ints(const void *a, const void *b) {
    int arg1 = *(const int *)a;
    int arg2 = *(const int *)b;
    return (arg1 > arg2) - (arg1 < arg2); /* Safe comparator avoiding overflow */
}

int main(void) {
    printf("=== C Function Pointers & Callbacks Demo ===\n");

    int nums[5] = {40, 10, 50, 20, 30};
    
    /* 1. Callback with user context */
    long total = 0;
    array_for_each(nums, 5, sum_accumulator, &total);
    printf("Accumulated sum via callback: %ld\n", total);

    /* 2. Standard qsort */
    qsort(nums, 5, sizeof(nums[0]), compare_ints);
    printf("Sorted array: ");
    for (size_t i = 0; i < 5; ++i) {
        printf("%d ", nums[i]);
    }
    printf("\n");

    /* 3. Standard bsearch */
    int key = 30;
    int *found = bsearch(&key, nums, 5, sizeof(nums[0]), compare_ints);
    printf("bsearch for %d: %s\n", key, found ? "FOUND" : "NOT FOUND");

    return 0;
}
