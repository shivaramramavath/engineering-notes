#include <stdio.h>
#include <limits.h>

int main(void) {
    printf("=== The Triad of Non-Deterministic Behaviors ===\n");
    
    /* 1. Implementation-Defined Behavior: Character signedness & integer width */
#if CHAR_MIN < 0
    printf("[Implementation-Defined] 'char' is SIGNED on this target\n");
#else
    printf("[Implementation-Defined] 'char' is UNSIGNED on this target\n");
#endif
    printf("[Implementation-Defined] sizeof(int) = %zu bytes, sizeof(long) = %zu bytes\n",
           sizeof(int), sizeof(long));

    /* 2. Unspecified Behavior: Evaluation order of function arguments */
    printf("[Unspecified] Evaluation order of operands in expressions is compiler-dependent\n");

    /* 3. Undefined Behavior: Guarding against signed overflow */
    int a = INT_MAX;
    printf("[Defined Guard] INT_MAX is %d; adding 1 without check would be Undefined Behavior!\n", a);
    
    return 0;
}
