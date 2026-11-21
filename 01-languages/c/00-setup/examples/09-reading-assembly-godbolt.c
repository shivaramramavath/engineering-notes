#include <stdio.h>

/* Simple function for assembly translation inspection */
int add_and_scale(int a, int b) {
    return (a + b) * 4;
}

int main(void) {
    printf("=== Assembly Translation Target ===\n");
    int res = add_and_scale(3, 5);
    printf("add_and_scale(3, 5) = %d\n", res);
    return 0;
}
