#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* Demonstrate safe dynamic allocation, growth with realloc, and cleanup */

typedef struct Buffer {
    size_t capacity;
    size_t size;
    int *data;
} Buffer;

static Buffer* buffer_create(size_t initial_cap) {
    /* Guard against zero or extreme overflow */
    if (initial_cap == 0) initial_cap = 4;
    
    Buffer *buf = malloc(sizeof *buf);
    if (!buf) return NULL;

    buf->data = malloc(initial_cap * sizeof *(buf->data));
    if (!buf->data) {
        free(buf);
        return NULL;
    }
    buf->capacity = initial_cap;
    buf->size = 0;
    return buf;
}

static int buffer_push(Buffer *buf, int val) {
    if (buf->size >= buf->capacity) {
        size_t new_cap = buf->capacity * 2;
        /* Safe realloc idiom using temporary pointer */
        int *new_data = realloc(buf->data, new_cap * sizeof *new_data);
        if (!new_data) {
            return 0; /* Failed to grow, original buf->data is still valid */
        }
        buf->data = new_data;
        buf->capacity = new_cap;
    }
    buf->data[buf->size++] = val;
    return 1;
}

static void buffer_free(Buffer **buf_ptr) {
    if (!buf_ptr || !*buf_ptr) return;
    Buffer *b = *buf_ptr;
    free(b->data);
    b->data = NULL;
    free(b);
    *buf_ptr = NULL; /* Prevent dangling pointer at caller site */
}

int main(void) {
    printf("=== C Memory Management Demo ===\n");

    /* 1. Dynamic buffer lifecycle */
    Buffer *buf = buffer_create(2);
    if (!buf) {
        fprintf(stderr, "Failed to create buffer\n");
        return 1;
    }
    printf("Buffer allocated with capacity: %zu\n", buf->capacity);

    for (int i = 1; i <= 6; ++i) {
        buffer_push(buf, i * 10);
    }
    printf("Buffer populated: size=%zu, capacity=%zu\n", buf->size, buf->capacity);
    printf("Contents: ");
    for (size_t i = 0; i < buf->size; ++i) {
        printf("%d ", buf->data[i]);
    }
    printf("\n");

    /* 2. calloc zero-initialization check */
    size_t count = 5;
    int *zeroed = calloc(count, sizeof *zeroed);
    if (!zeroed) {
        buffer_free(&buf);
        return 1;
    }
    int all_zeros = 1;
    for (size_t i = 0; i < count; ++i) {
        if (zeroed[i] != 0) all_zeros = 0;
    }
    printf("calloc zero-check: %s\n", all_zeros ? "VERIFIED (all 0)" : "FAILED");
    free(zeroed);
    zeroed = NULL;

    /* 3. Safe cleanup */
    buffer_free(&buf);
    printf("Buffer safely freed, pointer is NULL: %s\n", buf == NULL ? "TRUE" : "FALSE");

    return 0;
}
