#include <stdio.h>

/* Function returning an int */
static int add(int a, int b) { return a + b; }
static int sub(int a, int b) { return a - b; }

/* Array of function pointers */
typedef int (*BinaryOp)(int, int);

int main(void) {
    printf("=== C Complex Declarations Reading Guide Demo ===\n");

    /* Raw declaration of array of 2 function pointers: int (*ops[2])(int, int) */
    int (*ops[2])(int, int) = { add, sub };

    printf("ops[0](10, 5) [add] = %d\n", ops[0](10, 5));
    printf("ops[1](10, 5) [sub] = %d\n", ops[1](10, 5));

    /* Typedef equivalent */
    BinaryOp op_alias = add;
    printf("op_alias(20, 30) = %d\n", op_alias(20, 30));

    return 0;
}
