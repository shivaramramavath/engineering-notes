#include <stdio.h>
#include <stdint.h>

/* Global initialized variable: stored in .data section */
int global_counter = 42;

/* Global uninitialized variable: stored in .bss section */
int uninitialized_buffer[64];

/* Constant literal pointer: string literal stored in .rodata section */
static const char *const rodata_message = "Hello from .rodata!";

/* Function code: stored in .text section */
static void inspect_memory_segments(int stack_arg) {
    int local_stack_var = 100;

    printf("=== Process Memory Segment Inspection ===\n");
    printf(".text (Code Segment)       at address: %p\n", (void *)(uintptr_t)inspect_memory_segments);
    printf(".rodata (Read-Only Data)   at address: %p\n", (void *)rodata_message);
    printf(".data (Initialized Global) at address: %p (value = %d)\n", (void *)&global_counter, global_counter);
    printf(".bss (Uninitialized Block) at address: %p\n", (void *)uninitialized_buffer);
    printf("Stack (Parameter Address)  at address: %p (value = %d)\n", (void *)&stack_arg, stack_arg);
    printf("Stack (Local Variable)     at address: %p (value = %d)\n", (void *)&local_stack_var, local_stack_var);
}

int main(void) {
    inspect_memory_segments(7);
    return 0;
}
