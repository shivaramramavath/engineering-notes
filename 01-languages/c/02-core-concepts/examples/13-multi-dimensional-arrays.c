#include <stdio.h>
#include <stddef.h>

#define ROWS 3
#define COLS 4

/* Function taking pointer to array of COLS integers */
static void print_matrix(int (*mat)[COLS], size_t rows) {
    for (size_t r = 0; r < rows; ++r) {
        printf("Row %zu: ", r);
        for (size_t c = 0; c < COLS; ++c) {
            printf("%2d ", mat[r][c]);
        }
        printf("\n");
    }
}

int main(void) {
    printf("=== C Multi-Dimensional Arrays Demo ===\n");

    /* 2D array row-major contiguous memory */
    int grid[ROWS][COLS] = {
        { 1,  2,  3,  4},
        { 5,  6,  7,  8},
        { 9, 10, 11, 12}
    };

    printf("Total matrix sizeof: %zu bytes (%zu elements * %zu bytes)\n",
           sizeof(grid), (size_t)(ROWS * COLS), sizeof(int));
    printf("Row 0 sizeof: %zu bytes\n", sizeof(grid[0]));

    /* Verify row-major contiguous layout via flat pointer */
    int *flat_ptr = &grid[0][0];
    printf("Flat traversal: ");
    for (size_t i = 0; i < ROWS * COLS; ++i) {
        printf("%d ", *(flat_ptr + i));
    }
    printf("\n");

    /* Matrix function call */
    print_matrix(grid, ROWS);

    return 0;
}
