#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

/* Initialized global -> .data segment */
int g_initialized = 42;

/* Uninitialized global -> .bss segment (zero-initialized) */
int g_uninitialized;

int main(void) {
    printf("=== C Process Memory Layout Demo ===\n");

    /* Local variable -> Stack */
    int stack_var = 100;

    /* Dynamic allocation -> Heap */
    int *heap_ptr = malloc(sizeof(int));
    if (!heap_ptr) return 1;
    *heap_ptr = 200;

    /* String literal -> .rodata */
    const char *rodata_str = "Read Only String";

    printf(".text (main fn):       %p\n", (void*)(uintptr_t)main);
    printf(".rodata (string lit):  %p\n", (void*)rodata_str);
    printf(".data (global init):   %p (val: %d)\n", (void*)&g_initialized, g_initialized);
    printf(".bss (global uninit):  %p (val: %d)\n", (void*)&g_uninitialized, g_uninitialized);
    printf("Heap (malloc block):   %p\n", (void*)heap_ptr);
    printf("Stack (local var):     %p\n", (void*)&stack_var);

    free(heap_ptr);
    return 0;
}
