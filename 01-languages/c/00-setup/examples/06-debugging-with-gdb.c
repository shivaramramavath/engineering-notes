#include <stdio.h>

static int compute_factorial(int n) {
    if (n <= 1) {
        return 1;
    }
    return n * compute_factorial(n - 1);
}

int main(void) {
    int input = 5;
    printf("=== GDB Debugging Target Program ===\n");
    printf("Tracing call stack for factorial calculation...\n");
    
    int result = compute_factorial(input);
    printf("Factorial of %d is %d\n", input, result);
    return 0;
}
