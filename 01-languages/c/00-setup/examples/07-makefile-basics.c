#include <stdio.h>

int multiply(int a, int b) {
    return a * b;
}

int main(void) {
    printf("=== Makefile Target Execution ===\n");
    int x = 6, y = 7;
    printf("Result of %d * %d = %d\n", x, y, multiply(x, y));
    return 0;
}
