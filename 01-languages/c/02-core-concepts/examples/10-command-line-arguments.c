#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

int main(int argc, char *argv[]) {
    printf("=== C Command-Line Arguments Demo ===\n");
    printf("Argument count (argc): %d\n", argc);

    /* Guaranteed by ISO C: argv[argc] is NULL */
    if (argv[argc] == NULL) {
        printf("Verified: argv[argc] is NULL\n");
    }

    /* Iterate and print arguments */
    for (int i = 0; i < argc; ++i) {
        printf("argv[%d] = %s\n", i, argv[i]);
    }

    /* Simulate robust integer parsing if 2nd arg provided */
    if (argc >= 2) {
        char *endptr = NULL;
        errno = 0;
        long val = strtol(argv[1], &endptr, 10);
        if (errno == 0 && endptr != argv[1] && *endptr == '\0') {
            printf("Parsed valid numeric argument: %ld\n", val);
        } else {
            printf("Argument '%s' is not a valid clean integer\n", argv[1]);
        }
    }

    return 0;
}
