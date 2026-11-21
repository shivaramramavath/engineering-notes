#include <stdio.h>
#include <stddef.h>
#include <stdalign.h>

struct Unoptimized {
    char a;      /* 1 byte */
                 /* 3 padding bytes */
    int b;       /* 4 bytes */
    char c;      /* 1 byte */
                 /* 3 padding bytes */
};

struct Optimized {
    int b;       /* 4 bytes */
    char a;      /* 1 byte */
    char c;      /* 1 byte */
                 /* 2 padding bytes */
};

int main(void) {
    printf("=== Sizeof & Memory Alignment Verification ===\n");
    printf("sizeof(struct Unoptimized): %zu bytes (Alignment: %zu)\n", 
           sizeof(struct Unoptimized), alignof(struct Unoptimized));
    printf("sizeof(struct Optimized):   %zu bytes (Alignment: %zu)\n", 
           sizeof(struct Optimized), alignof(struct Optimized));
    printf("alignof(max_align_t):       %zu bytes\n", alignof(max_align_t));
    return 0;
}
