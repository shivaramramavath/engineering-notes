#include <stdio.h>
#include <stdlib.h>

static void cleanup_hook(void) {
    printf("[CRT] atexit registered handler executed successfully.\n");
}

int main(void) {
    /* Register cleanup function with C Runtime exit sequence */
    if (atexit(cleanup_hook) != 0) {
        fprintf(stderr, "Failed to register exit handler.\n");
        return EXIT_FAILURE;
    }

    printf("=== First Program: Execution & Lifecycle ===\n");
    printf("Greeting: Hello, robust C world!\n");
    printf("Standard Output: Line buffered on interactive terminals.\n");
    printf("Standard Error:  Unbuffered for critical diagnostics.\n");
    printf("Exit Code:       EXIT_SUCCESS (%d)\n", EXIT_SUCCESS);

    return EXIT_SUCCESS;
}
