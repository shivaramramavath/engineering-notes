#include <stdio.h>

/* Internal linkage (file-scoped static function) */
static void greet_internal(void) {
    printf("[Internal Linkage] Hello from translation unit private function!\n");
}

/* External linkage (can be called across translation units) */
void greet_external(void) {
    printf("[External Linkage] Public API function invoked.\n");
}

int main(void) {
    printf("=== Translation Unit & Linkage Structure Demo ===\n");
    greet_internal();
    greet_external();
    return 0;
}
