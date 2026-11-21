#include <stdio.h>
#include "mathutil.h"

int main(void) {
    printf("=== C Multi-File Modular Compilation Demo ===\n");
    int a = 24, b = 36;
    printf("GCD(%d, %d) = %d\n", a, b, math_gcd(a, b));
    printf("LCM(%d, %d) = %d\n", a, b, math_lcm(a, b));
    printf("Total math operations tracked: %d\n", g_mathutil_ops_count);
    return 0;
}
