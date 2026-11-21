#include <stdio.h>

/* Pass-by-value demonstration */
static void swap_by_value(int a, int b) {
    int temp = a;
    a = b;
    b = temp;
    (void)a;
    (void)b;
}

/* Pass-by-reference simulation via pointers */
static void swap_by_pointer(int *a, int *b) {
    if (a && b) {
        int temp = *a;
        *a = *b;
        *b = temp;
    }
}

/* Function pointer definition */
typedef int (*binary_op_t)(int, int);

static int add(int x, int y) { return x + y; }
static int multiply(int x, int y) { return x * y; }

int main(void) {
    printf("=== Function Semantics & Call Dispatch Demo ===\n");
    int x = 10, y = 20;
    
    swap_by_value(x, y);
    printf("After swap_by_value: x = %d, y = %d (Unchanged!)\n", x, y);

    swap_by_pointer(&x, &y);
    printf("After swap_by_pointer: x = %d, y = %d (Swapped!)\n", x, y);

    /* Dynamic dispatch via function pointer */
    binary_op_t op = add;
    printf("Function pointer dispatch add(4, 5) = %d\n", op(4, 5));
    op = multiply;
    printf("Function pointer dispatch multiply(4, 5) = %d\n", op(4, 5));

    return 0;
}
