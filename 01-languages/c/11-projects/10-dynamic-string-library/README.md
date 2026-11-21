# Project 10: High-Performance Dynamic String Library (`dstr`)

> An ABI-stable, binary-safe dynamic string library in C modeled after Redis SDS (Simple Dynamic Strings), featuring $O(1)$ length inspection, binary safety, and automatic buffer growth.

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `10-dynamic-string-library` |
| **Language Baseline** | C11 / Standard Architecture |
| **Complexity** | Intermediate / Data Structures |
| **Core Systems Concepts** | Header Prefixing, Binary Safety, Amortized $O(1)$ Reallocation |

---

## 1. Architectural Overview & Component Breakdown

`dstr` replaces standard error-prone null-terminated C strings (`char*`):
1. **Prefix Header Architecture:** Stores `len` and `cap` in a header struct placed immediately *before* the character buffer pointer. The returned pointer is a standard `char*`, directly compatible with `printf` and `strlen`.
2. **Binary Safety:** Strings can hold arbitrary binary payloads containing embedded `\0` null bytes because length is tracked explicitly in metadata.
3. **Amortized Constant-Time Appending:** Allocates double capacity on buffer growth, avoiding quadratic memory reallocation bottlenecks.
4. **Pointer Re-Assignment Discipline:** Modifying operations return an updated string pointer, handling internal `realloc()` moves seamlessly.

```
       +-----------------------------------------------------------+
       |               Dynamic String Memory Layout                |
       +-----------------------------------------------------------+
       | Header: [ size_t len (8B) ] [ size_t cap (8B) ]          |
       +-----------------------------------------------------------+
       ^                                                 ^
       | Internal struct DStrHeader                      | Returned char* pointer
                                                         v
                                       [ 'H' 'e' 'l' 'l' 'o' '\0' ]
```

---

## 2. Complete Project Source Code

### Header: `include/dstr.h`

```c
#ifndef DSTR_H
#define DSTR_H

#include <stddef.h>
#include <stdbool.h>

typedef char* dstr;

/* Lifecycle */
dstr dstr_new(const char *init);
dstr dstr_new_len(const void *init, size_t len);
void dstr_free(dstr s);

/* Metrics */
size_t dstr_len(const dstr s);
size_t dstr_avail(const dstr s);

/* Mutating Operations */
dstr dstr_cat(dstr s, const char *str);
dstr dstr_cat_len(dstr s, const void *data, size_t len);
void dstr_clear(dstr s);

#endif /* DSTR_H */
```

### Source: `src/dstr.c`

```c
#include "dstr.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    size_t len;
    size_t cap;
    char buf[]; /* Flexible array member */
} DStrHeader;

#define DSTR_HDR(s) ((DStrHeader*)((char*)(s) - sizeof(DStrHeader)))

dstr dstr_new_len(const void *init, size_t len) {
    DStrHeader *hdr = malloc(sizeof(DStrHeader) + len + 1);
    if (!hdr) return NULL;
    hdr->len = len;
    hdr->cap = len;
    if (init && len > 0) {
        memcpy(hdr->buf, init, len);
    }
    hdr->buf[len] = '\0';
    return hdr->buf;
}

dstr dstr_new(const char *init) {
    size_t len = init ? strlen(init) : 0;
    return dstr_new_len(init, len);
}

void dstr_free(dstr s) {
    if (!s) return;
    free(DSTR_HDR(s));
}

size_t dstr_len(const dstr s) {
    return s ? DSTR_HDR(s)->len : 0;
}

size_t dstr_avail(const dstr s) {
    if (!s) return 0;
    DStrHeader *hdr = DSTR_HDR(s);
    return hdr->cap - hdr->len;
}

dstr dstr_cat_len(dstr s, const void *data, size_t add_len) {
    if (!s || !data || add_len == 0) return s;
    DStrHeader *hdr = DSTR_HDR(s);
    size_t avail = hdr->cap - hdr->len;

    if (avail < add_len) {
        size_t new_cap = (hdr->len + add_len) * 2;
        DStrHeader *new_hdr = realloc(hdr, sizeof(DStrHeader) + new_cap + 1);
        if (!new_hdr) return NULL;
        new_hdr->cap = new_cap;
        hdr = new_hdr;
    }

    memcpy(hdr->buf + hdr->len, data, add_len);
    hdr->len += add_len;
    hdr->buf[hdr->len] = '\0';
    return hdr->buf;
}

dstr dstr_cat(dstr s, const char *str) {
    return dstr_cat_len(s, str, str ? strlen(str) : 0);
}

void dstr_clear(dstr s) {
    if (!s) return;
    DStrHeader *hdr = DSTR_HDR(s);
    hdr->len = 0;
    hdr->buf[0] = '\0';
}
```

### Source: `src/main.c`

```c
#include "dstr.h"
#include <stdio.h>

int main(void) {
    printf("===========================================\n");
    printf("   Simple Dynamic String (SDS-Style) Lib   \n");
    printf("===========================================\n");

    dstr s = dstr_new("Hello");
    printf("Initial string: \"%s\" (Len: %zu, Avail: %zu)\n", s, dstr_len(s), dstr_avail(s));

    s = dstr_cat(s, ", World!");
    printf("After concatenation: \"%s\" (Len: %zu, Avail: %zu)\n", s, dstr_len(s), dstr_avail(s));

    s = dstr_cat(s, " Fast C Strings.");
    printf("After second concatenation: \"%s\" (Len: %zu, Avail: %zu)\n", s, dstr_len(s), dstr_avail(s));

    dstr_free(s);
    printf("Dynamic string cleanly deallocated.\n");
    return 0;
}
```

---

## 3. Build Configuration (`Makefile`)

```makefile
CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -O2 -Iinclude
SRC = src/main.c src/dstr.c
OBJ = $(SRC:.c=.o)
TARGET = dstr-demo

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $@

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f src/*.o $(TARGET)

.PHONY: all clean
```

---

[← Return to Projects Index](../README.md)
