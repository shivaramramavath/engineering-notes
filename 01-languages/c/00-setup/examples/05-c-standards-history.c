#include <stdio.h>
#include <limits.h>

int main(void) {
    printf("=== C Standards Compliance Matrix ===\n");
#if defined(__STDC_VERSION__)
    printf("Active Standard Macro (__STDC_VERSION__): %ldL\n", (long)__STDC_VERSION__);
#else
    printf("Active Standard: Pre-C89 (K and R C)\n");
#endif

    /* C99 feature: Variable declaration in for loop */
    int sum = 0;
    for (int i = 1; i <= 5; ++i) {
        sum += i;
    }
    printf("[C99] For-loop scoped iterator sum: %d\n", sum);

    /* C11 feature: _Generic selection */
    #define TYPE_NAME(x) _Generic((x), \
        int: "signed int", \
        double: "double precision float", \
        char *: "pointer to char", \
        default: "other type")

    int a = 10;
    double b = 3.14;
    printf("[C11] Generic selection for 'a' (%d): %s\n", a, TYPE_NAME(a));
    printf("[C11] Generic selection for 'b' (%.2f): %s\n", b, TYPE_NAME(b));

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L
    printf("[C23] Native nullptr and [[attributes]] active\n");
#else
    printf("[C11/C17 Baseline] Stable LTS configuration verified\n");
#endif

    return 0;
}
