#include "mathutil.h"

int g_mathutil_ops_count = 0;

static int internal_abs(int x) {
    return x < 0 ? -x : x;
}

int math_gcd(int a, int b) {
    g_mathutil_ops_count++;
    a = internal_abs(a);
    b = internal_abs(b);
    while (b != 0) {
        int t = a % b;
        a = b;
        b = t;
    }
    return a;
}

int math_lcm(int a, int b) {
    if (a == 0 || b == 0) return 0;
    return internal_abs(a * b) / math_gcd(a, b);
}
