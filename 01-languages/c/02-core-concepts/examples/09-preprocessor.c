#include <stdio.h>
#include <stdint.h>

/* Safe multi-statement macro idiom */
#define SWAP(a, b, type) do { \
    type _tmp = (a);          \
    (a) = (b);                \
    (b) = _tmp;               \
} while (0)

/* Stringification operator # */
#define PRINT_INT(expr) printf(#expr " = %d\n", (expr))

/* Token pasting operator ## */
#define MAKE_FN_NAME(prefix, suffix) prefix##_##suffix

static int MAKE_FN_NAME(math, add)(int a, int b) {
    return a + b;
}

/* Variadic macro */
#define LOG_INFO(fmt, ...) printf("[INFO] " fmt "\n", __VA_ARGS__)

int main(void) {
    printf("=== C Preprocessor Demo ===\n");

    /* 1. Safe macro with do-while(0) */
    int x = 10, y = 20;
    SWAP(x, y, int);
    printf("After SWAP: x=%d, y=%d\n", x, y);

    /* 2. Stringification */
    PRINT_INT(x + y * 2);

    /* 3. Token pasting */
    int sum = math_add(30, 40);
    printf("Token-pasted math_add result: %d\n", sum);

    /* 4. Variadic macro */
    LOG_INFO("Status code: %d, Message: %s", 200, "OK");

    return 0;
}
