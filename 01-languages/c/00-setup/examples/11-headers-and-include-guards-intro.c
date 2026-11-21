#include <stdio.h>

/* Demonstrating idempotent header inclusion semantics */
#ifndef DEMO_HEADER_H
#define DEMO_HEADER_H
typedef struct {
    int major;
    int minor;
} Version;
#endif

int main(void) {
    printf("=== Idempotent Header Guard Demonstration ===\n");
    Version v = { .major = 2, .minor = 1 };
    printf("Engine API Version: %d.%d\n", v.major, v.minor);
    return 0;
}
