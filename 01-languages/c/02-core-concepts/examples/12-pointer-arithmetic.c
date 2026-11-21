#include <stdio.h>
#include <stddef.h>
#include <stdint.h>

int main(void) {
    printf("=== C Pointer Arithmetic Demo ===\n");

    int arr[5] = {10, 20, 30, 40, 50};
    int *start = arr;
    int *end = arr + 5; /* One-past-the-end pointer: valid to compute, not to dereference */

    /* 1. Traversal using pointer increment */
    printf("Forward traversal: ");
    for (int *p = start; p < end; ++p) {
        printf("%d ", *p);
    }
    printf("\n");

    /* 2. Pointer difference (ptrdiff_t) */
    int *p2 = &arr[4];
    int *p1 = &arr[1];
    ptrdiff_t diff = p2 - p1;
    printf("p2 - p1 difference: %td elements\n", diff);

    /* 3. Pointer scale factor by sizeof(T) */
    uintptr_t byte_diff = (uintptr_t)p2 - (uintptr_t)p1;
    printf("Byte distance: %zu bytes (diff * sizeof(int) = %td * %zu = %zu)\n",
           (size_t)byte_diff, diff, sizeof(int), (size_t)(diff * sizeof(int)));

    /* 4. Negative indexing and symmetry */
    int *mid = &arr[2]; /* Points to 30 */
    printf("mid[0]=%d, mid[-1]=%d, mid[1]=%d\n", mid[0], mid[-1], mid[1]);

    return 0;
}
