#include <stdio.h>
#include <string.h>

int main(void) {
    printf("=== C string.h Functions and Safe Alternatives Demo ===\n");

    /* 1. Safe snprintf bounds truncation check */
    char buffer[16];
    const char *long_str = "Standard C Systems Programming";
    int needed = snprintf(buffer, sizeof buffer, "%s", long_str);
    printf("Destination buffer (cap %zu): [%s]\n", sizeof buffer, buffer);
    if (needed >= (int)sizeof buffer) {
        printf("Detected truncation! Needed %d chars (including null terminator: %d)\n",
               needed, needed + 1);
    }

    /* 2. Searching with strchr and strstr */
    const char *haystack = "kernel_panic_recovery_flag=true";
    const char *sub = strstr(haystack, "recovery");
    if (sub) {
        printf("Found substring '%s' at offset: %td\n", "recovery", sub - haystack);
    }

    /* 3. Tokenization with strtok */
    char csv_line[] = "alice,admin,1001,active";
    printf("Parsing CSV tokens:\n");
    char *tok = strtok(csv_line, ",");
    while (tok) {
        printf("  Token: [%s]\n", tok);
        tok = strtok(NULL, ",");
    }

    return 0;
}
