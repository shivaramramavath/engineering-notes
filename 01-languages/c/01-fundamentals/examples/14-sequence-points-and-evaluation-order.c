#include <stdio.h>

int main(void) {
    printf("=== Sequence Points & Sequenced Execution ===\n");
    
    /* Fully sequenced expression statements */
    int i = 0;
    i++;
    int next_val = i;
    printf("Sequenced statement evaluation: i = %d, next_val = %d\n", i, next_val);

    /* Logical short-circuit sequence point */
    int guard = 0;
    int evaluated = 0;
    int res = (guard != 0) && (++evaluated);
    printf("Short-circuit guarantee: res = %d, evaluated = %d\n", res, evaluated);

    return 0;
}
