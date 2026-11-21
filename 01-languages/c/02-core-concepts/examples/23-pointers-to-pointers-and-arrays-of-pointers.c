#include <stdio.h>
#include <stdlib.h>

/* Modifying caller pointer via pointer-to-pointer out parameter */
static int allocate_string(char **out_str, size_t len) {
    if (!out_str) return 0;
    *out_str = malloc(len);
    return (*out_str != NULL);
}

int main(void) {
    printf("=== C Pointers to Pointers & Arrays of Pointers Demo ===\n");

    /* 1. Pointer-to-pointer mutation */
    char *msg = NULL;
    if (allocate_string(&msg, 32)) {
        sprintf(msg, "Allocated via double pointer");
        printf("msg: %s\n", msg);
        free(msg);
    }

    /* 2. Array of pointers (ragged string table) */
    const char *commands[] = {
        "help",
        "connect --host localhost",
        "status",
        "exit"
    };
    size_t cmd_count = sizeof(commands) / sizeof(commands[0]);
    printf("Command table (%zu entries):\n", cmd_count);
    for (size_t i = 0; i < cmd_count; ++i) {
        printf("  [%zu] %s\n", i, commands[i]);
    }

    return 0;
}
