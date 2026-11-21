#include <stdio.h>
#include <stdlib.h>

typedef struct IntVector {
    size_t size;
    size_t capacity;
    int *items;
} IntVector;

static IntVector* vector_create(size_t initial_cap) {
    if (initial_cap == 0) initial_cap = 4;
    IntVector *v = malloc(sizeof *v);
    if (!v) return NULL;
    v->items = malloc(initial_cap * sizeof *(v->items));
    if (!v->items) {
        free(v);
        return NULL;
    }
    v->capacity = initial_cap;
    v->size = 0;
    return v;
}

static int vector_push(IntVector *v, int val) {
    if (v->size >= v->capacity) {
        size_t new_cap = v->capacity * 2; /* 2.0x geometric growth */
        int *new_items = realloc(v->items, new_cap * sizeof *new_items);
        if (!new_items) return 0;
        v->items = new_items;
        v->capacity = new_cap;
    }
    v->items[v->size++] = val;
    return 1;
}

static void vector_free(IntVector **v_ptr) {
    if (!v_ptr || !*v_ptr) return;
    IntVector *v = *v_ptr;
    free(v->items);
    free(v);
    *v_ptr = NULL;
}

int main(void) {
    printf("=== C Dynamic Arrays and Realloc Demo ===\n");

    IntVector *v = vector_create(2);
    if (!v) return 1;

    printf("Initial vector capacity: %zu\n", v->capacity);
    for (int i = 1; i <= 7; ++i) {
        vector_push(v, i * 11);
    }
    printf("Vector populated: size=%zu, capacity=%zu\n", v->size, v->capacity);
    printf("Elements: ");
    for (size_t i = 0; i < v->size; ++i) {
        printf("%d ", v->items[i]);
    }
    printf("\n");

    vector_free(&v);
    printf("Vector cleanly freed: %s\n", v == NULL ? "YES" : "NO");

    return 0;
}
