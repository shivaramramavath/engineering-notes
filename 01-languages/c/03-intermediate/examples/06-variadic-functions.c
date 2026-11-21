#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

/* Custom variadic logger with log level prefix */
typedef enum {
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR
} LogLevel;

static void logger_log(LogLevel level, const char *fmt, ...) {
    const char *prefix = "UNKNOWN";
    switch (level) {
        case LOG_INFO:  prefix = "INFO"; break;
        case LOG_WARN:  prefix = "WARN"; break;
        case LOG_ERROR: prefix = "ERROR"; break;
    }

    printf("[%s] ", prefix);

    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);

    printf("\n");
}

/* Safe formatted string duplicator utilizing va_copy */
static char *string_format_alloc(const char *fmt, ...) {
    va_list args1, args2;
    va_start(args1, fmt);
    va_copy(args2, args1);

    /* Determine necessary buffer length (excluding null terminator) */
    int len = vsnprintf(NULL, 0, fmt, args1);
    va_end(args1);

    if (len < 0) {
        va_end(args2);
        return NULL;
    }

    char *buf = (char *)malloc((size_t)len + 1);
    if (!buf) {
        va_end(args2);
        return NULL;
    }

    vsnprintf(buf, (size_t)len + 1, fmt, args2);
    va_end(args2);
    return buf;
}

int main(void) {
    printf("=== Variadic Functions Demonstrations ===\n");

    logger_log(LOG_INFO, "System initialized successfully with %d workers", 4);
    logger_log(LOG_WARN, "Memory usage exceeded threshold: %.2f%%", 87.45);
    logger_log(LOG_ERROR, "Disk write error on partition '%s' (errno %d)", "/dev/sda1", 28);

    char *dyn_str = string_format_alloc("Payload ID: %04d, Hash: %s", 102, "a9f8c2b7");
    if (dyn_str) {
        printf("Allocated formatted string: %s\n", dyn_str);
        free(dyn_str);
    }

    return 0;
}
