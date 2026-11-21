#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

/* Safe integer parsing using strtol with strict validation */
typedef enum {
    PARSE_SUCCESS,
    PARSE_EMPTY_STRING,
    PARSE_OUT_OF_RANGE,
    PARSE_INVALID_CHARACTERS
} ParseStatus;

static ParseStatus parse_long(const char *str, long *out_val) {
    if (!str || *str == '\0') {
        return PARSE_EMPTY_STRING;
    }

    /* Skip leading whitespace */
    while (isspace((unsigned char)*str)) {
        str++;
    }
    if (*str == '\0') {
        return PARSE_EMPTY_STRING;
    }

    char *endptr = NULL;
    errno = 0;
    long val = strtol(str, &endptr, 10);

    if (errno == ERANGE) {
        return PARSE_OUT_OF_RANGE;
    }
    if (endptr == str) {
        return PARSE_INVALID_CHARACTERS;
    }

    /* Skip trailing whitespace */
    while (isspace((unsigned char)*endptr)) {
        endptr++;
    }
    if (*endptr != '\0') {
        return PARSE_INVALID_CHARACTERS;
    }

    *out_val = val;
    return PARSE_SUCCESS;
}

/* Safe CSV line tokenizer without destructive strtok */
static void parse_csv_line(const char *line) {
    printf("Parsing CSV line: \"%s\"\n", line);
    const char *p = line;
    int field_idx = 0;

    while (*p != '\0') {
        const char *start = p;
        while (*p != ',' && *p != '\0') {
            p++;
        }
        size_t len = (size_t)(p - start);
        printf("  Field %d: \"%.*s\"\n", field_idx++, (int)len, start);
        if (*p == ',') {
            p++;
        }
    }
}

int main(void) {
    printf("=== Text Processing and Parsing ===\n");

    const char *test_numbers[] = {"12345", "  -42  ", "999999999999999999999999999", "123abc", "   ", "xyz"};
    size_t count = sizeof(test_numbers) / sizeof(test_numbers[0]);

    for (size_t i = 0; i < count; i++) {
        long result = 0;
        ParseStatus status = parse_long(test_numbers[i], &result);
        printf("Input: \"%-28s\" -> ", test_numbers[i]);
        switch (status) {
            case PARSE_SUCCESS:
                printf("SUCCESS (val: %ld)\n", result);
                break;
            case PARSE_EMPTY_STRING:
                printf("ERROR: Empty or whitespace-only string\n");
                break;
            case PARSE_OUT_OF_RANGE:
                printf("ERROR: Numeric overflow/underflow (ERANGE)\n");
                break;
            case PARSE_INVALID_CHARACTERS:
                printf("ERROR: Invalid non-numeric trailing character\n");
                break;
        }
    }

    printf("\n");
    parse_csv_line("Alice,Engineering,45000,Senior Dev");
    return 0;
}
