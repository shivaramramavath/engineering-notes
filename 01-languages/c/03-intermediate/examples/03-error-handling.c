#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

typedef enum StatusCode {
    STATUS_OK = 0,
    STATUS_ERR_NOMEM = -1,
    STATUS_ERR_INVALID = -2,
    STATUS_ERR_IO = -3
} StatusCode;

/* Clean resource management using goto cleanup */
static StatusCode process_records(const char *header, size_t count, int **out_buffer) {
    if (!header || count == 0 || !out_buffer) {
        return STATUS_ERR_INVALID;
    }

    StatusCode status = STATUS_OK;
    int *buf1 = NULL;
    int *buf2 = NULL;

    buf1 = malloc(count * sizeof *buf1);
    if (!buf1) {
        status = STATUS_ERR_NOMEM;
        goto cleanup;
    }

    buf2 = malloc(count * sizeof *buf2);
    if (!buf2) {
        status = STATUS_ERR_NOMEM;
        goto cleanup;
    }

    /* Simulate processing */
    for (size_t i = 0; i < count; ++i) {
        buf1[i] = (int)(i * 10);
        buf2[i] = buf1[i] + 5;
    }

    *out_buffer = buf1;
    buf1 = NULL; /* Transfer ownership to caller */

cleanup:
    free(buf1);
    free(buf2);
    return status;
}

int main(void) {
    printf("=== C Error Handling & Goto Cleanup Demo ===\n");

    int *result = NULL;
    StatusCode rc = process_records("TestHeader", 4, &result);
    printf("Operation status code: %d (%s)\n",
           rc, rc == STATUS_OK ? "SUCCESS" : "FAILED");

    if (rc == STATUS_OK && result) {
        printf("Result data: [%d, %d, %d, %d]\n",
               result[0], result[1], result[2], result[3]);
        free(result);
    }

    return 0;
}
