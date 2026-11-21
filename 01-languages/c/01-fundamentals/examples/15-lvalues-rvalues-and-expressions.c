#include <stdio.h>

int main(void) {
    printf("=== Lvalues, Rvalues, and Expressions ===\n");
    
    /* Modifiable lvalue */
    int x = 42;
    x = 100; /* x on left is lvalue; 100 on right is rvalue */
    printf("Modifiable lvalue x = %d\n", x);
    
    /* Compound literal is an lvalue */
    int *arr = (int[]){ 1, 2, 3 };
    arr[0] = 10; /* Mutating compound literal */
    printf("Compound literal mutated first element: %d\n", arr[0]);

    /* Non-modifiable lvalue */
    const int ro = 50;
    printf("Non-modifiable lvalue 'ro': %d (has address: %p)\n", ro, (const void *)&ro);

    return 0;
}
