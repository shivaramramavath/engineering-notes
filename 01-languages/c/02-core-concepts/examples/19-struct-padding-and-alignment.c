#include <stdio.h>
#include <stddef.h>
#include <stdint.h>

struct Suboptimal {
    char a;       /* 1 byte + 3 pad */
    int b;        /* 4 bytes */
    char c;       /* 1 byte + 7 pad */
    double d;     /* 8 bytes */
};

struct Optimized {
    double d;     /* 8 bytes */
    int b;        /* 4 bytes */
    char a;       /* 1 byte */
    char c;       /* 1 byte + 2 pad */
};

int main(void) {
    printf("=== C Struct Padding and Alignment Demo ===\n");
    printf("Suboptimal size: %zu bytes\n", sizeof(struct Suboptimal));
    printf("  offset a: %zu\n", offsetof(struct Suboptimal, a));
    printf("  offset b: %zu\n", offsetof(struct Suboptimal, b));
    printf("  offset c: %zu\n", offsetof(struct Suboptimal, c));
    printf("  offset d: %zu\n", offsetof(struct Suboptimal, d));

    printf("Optimized size:   %zu bytes\n", sizeof(struct Optimized));
    printf("  offset d: %zu\n", offsetof(struct Optimized, d));
    printf("  offset b: %zu\n", offsetof(struct Optimized, b));
    printf("  offset a: %zu\n", offsetof(struct Optimized, a));
    printf("  offset c: %zu\n", offsetof(struct Optimized, c));

    return 0;
}
