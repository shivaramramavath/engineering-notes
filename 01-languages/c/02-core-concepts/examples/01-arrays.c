#include <stdio.h>

int main(void) {
    printf("=== Arrays & Memory Layout Demo ===\n");
    
    int arr[5] = { 10, 20, 30, 40, 50 };
    printf("Array element count: %zu\n", sizeof(arr) / sizeof(arr[0]));
    printf("Base address &arr[0]: %p\n", (void *)&arr[0]);
    printf("Next address &arr[1]: %p (Step offset: %zu bytes)\n", 
           (void *)&arr[1], (size_t)((char *)&arr[1] - (char *)&arr[0]));

    /* 2D array row-major contiguous layout */
    int matrix[2][3] = { {1, 2, 3}, {4, 5, 6} };
    printf("Row-major element matrix[1][0] at %p\n", (void *)&matrix[1][0]);
    printf("Adjacent element matrix[0][2] at %p (Directly precedes matrix[1][0] in RAM!)\n", 
           (void *)&matrix[0][2]);

    return 0;
}
