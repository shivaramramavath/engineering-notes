#include <stdio.h>

/* Complex typedef: Function pointer taking int and returning int */
typedef int (*math_func_t)(int);

static int square(int x) { return x * x; }
static int cube(int x) { return x * x * x; }

int main(void) {
    printf("=== Declarations & Complex Typedefs Demo ===\n");
    
    /* Array of function pointers */
    math_func_t pipeline[2] = { square, cube };
    int input = 3;
    
    int step1 = pipeline[0](input);
    int step2 = pipeline[1](input);
    printf("square(%d) = %d\n", input, step1);
    printf("cube(%d) = %d\n", input, step2);

    return 0;
}
