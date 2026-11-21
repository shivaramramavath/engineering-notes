#include <stdio.h>
#include <string.h>

int main(void) {
    printf("=== Safe String Mechanics Demo ===\n");
    
    /* String literal in .rodata */
    const char *greeting = "Hello, Systems C!";
    printf("String: '%s' (Length: %zu, Size with null: %zu)\n", 
           greeting, strlen(greeting), sizeof("Hello, Systems C!"));

    /* Safe bounded concatenation with snprintf */
    char dest[24];
    int written = snprintf(dest, sizeof(dest), "%s -> v%d", "Kernel", 6);
    printf("Formatted: '%s' (Written: %d, Cap: %zu)\n", dest, written, sizeof(dest));

    return 0;
}
