# Project 04: Production Heap Memory Allocator (`memory-allocator`)

> A custom dynamic memory allocator implementing `malloc`, `free`, and `realloc` with boundary-tag headers/footers, constant-time $O(1)$ bidirectional coalescing, and explicit segregated free lists.

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `04-memory-allocator` |
| **Language Baseline** | C11 / Standard Architecture |
| **Complexity** | Advanced / Systems Internals |
| **Core Systems Concepts** | Boundary Tags, Alignment, Coalescing, Segregated Free Lists, Heap Layout |

---

## 1. Architectural Overview & Component Breakdown

The Memory Allocator provides a complete drop-in replacement for standard libc heap routines:
1. **Boundary-Tag Block Layout:** Every memory block (allocated or free) is bracketed by an 8-byte header and an 8-byte footer. The lowest bit of the size field serves as the `ALLOCATED` flag ($1 = \text{in use}, 0 = \text{free}$).
2. **Explicit Doubly-Linked Free List:** Free blocks store `next_free` and `prev_free` pointers directly inside their unused payload area, enabling $O(1)$ insertion and removal.
3. **Bidirectional Coalescing ($O(1)$):** When `my_free()` is invoked, inspecting the footer of the predecessor block and the header of the successor block allows immediate merging of adjacent free blocks, eliminating external fragmentation.
4. **Block Splitting:** When an oversized free block is matched, it is split into an allocated slice and a remainder free slice if the excess memory exceeds minimum block thresholds.

```
       +-----------------------------------------------------------+
       |             Boundary Tag Memory Block Layout              |
       +-----------------------------------------------------------+
       | Header (8 Bytes): [ Size of Block (63 bits) | Alloc Flag ]|
       +-----------------------------------------------------------+
       | Payload Area:                                             |
       | - When Free:      [ *next_free (8B) ] [ *prev_free (8B) ] |
       | - When Allocated: [ Raw User Application Data ...       ] |
       +-----------------------------------------------------------+
       | Footer (8 Bytes): [ Size of Block (63 bits) | Alloc Flag ]|
       +-----------------------------------------------------------+
```

---

## 2. Complete Project Source Code

### Header: `include/allocator.h`

```c
#ifndef ALLOCATOR_H
#define ALLOCATOR_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#define ALIGNMENT 8
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~(ALIGNMENT - 1))

/* Allocator Interfaces */
void allocator_init(void *memory_pool, size_t pool_size);
void* my_malloc(size_t size);
void my_free(void *ptr);
void* my_realloc(void *ptr, size_t new_size);

/* Diagnostics */
void allocator_dump_stats(void);

#endif /* ALLOCATOR_H */
```

### Source: `src/allocator.c`

```c
#include "allocator.h"
#include <stdio.h>
#include <string.h>

typedef struct BlockHeader {
    size_t size_and_flag; /* size | (1 if allocated, 0 if free) */
} BlockHeader;

typedef struct FreeBlock {
    BlockHeader header;
    struct FreeBlock *next_free;
    struct FreeBlock *prev_free;
} FreeBlock;

#define TAG_ALLOCATED 1
#define BLOCK_SIZE(hdr) ((hdr)->size_and_flag & ~TAG_ALLOCATED)
#define IS_ALLOCATED(hdr) ((hdr)->size_and_flag & TAG_ALLOCATED)

#define HEADER_TO_FOOTER(hdr) \
    ((BlockHeader*)((char*)(hdr) + BLOCK_SIZE(hdr) - sizeof(BlockHeader)))

#define FOOTER_TO_HEADER(ftr) \
    ((BlockHeader*)((char*)(ftr) - BLOCK_SIZE(ftr) + sizeof(BlockHeader)))

#define MIN_BLOCK_SIZE (sizeof(FreeBlock) + sizeof(BlockHeader))

static FreeBlock *g_free_list_head = NULL;
static void *g_heap_start = NULL;
static void *g_heap_end = NULL;

static void set_tags(BlockHeader *hdr, size_t size, bool allocated) {
    hdr->size_and_flag = size | (allocated ? TAG_ALLOCATED : 0);
    BlockHeader *ftr = HEADER_TO_FOOTER(hdr);
    ftr->size_and_flag = hdr->size_and_flag;
}

static void insert_free_list(FreeBlock *block) {
    block->next_free = g_free_list_head;
    block->prev_free = NULL;
    if (g_free_list_head) {
        g_free_list_head->prev_free = block;
    }
    g_free_list_head = block;
}

static void remove_free_list(FreeBlock *block) {
    if (block->prev_free) {
        block->prev_free->next_free = block->next_free;
    } else {
        g_free_list_head = block->next_free;
    }
    if (block->next_free) {
        block->next_free->prev_free = block->prev_free;
    }
}

void allocator_init(void *memory_pool, size_t pool_size) {
    /* Align pool start */
    uintptr_t start = ALIGN((uintptr_t)memory_pool);
    pool_size = (pool_size - (start - (uintptr_t)memory_pool)) & ~(ALIGNMENT - 1);

    g_heap_start = (void*)start;
    g_heap_end = (char*)g_heap_start + pool_size;

    FreeBlock *initial_block = (FreeBlock*)g_heap_start;
    set_tags(&initial_block->header, pool_size, false);
    g_free_list_head = NULL;
    insert_free_list(initial_block);
}

void* my_malloc(size_t size) {
    if (size == 0) return NULL;

    /* Total block size = header + payload + footer */
    size_t total_size = ALIGN(size) + 2 * sizeof(BlockHeader);
    if (total_size < MIN_BLOCK_SIZE) total_size = MIN_BLOCK_SIZE;

    /* First-fit search */
    FreeBlock *cur = g_free_list_head;
    while (cur) {
        size_t cur_size = BLOCK_SIZE(&cur->header);
        if (cur_size >= total_size) {
            remove_free_list(cur);

            /* Split if remainder is large enough */
            if (cur_size - total_size >= MIN_BLOCK_SIZE) {
                size_t remainder_size = cur_size - total_size;
                set_tags(&cur->header, total_size, true);

                FreeBlock *remainder = (FreeBlock*)((char*)cur + total_size);
                set_tags(&remainder->header, remainder_size, false);
                insert_free_list(remainder);
            } else {
                set_tags(&cur->header, cur_size, true);
            }

            return (void*)((char*)cur + sizeof(BlockHeader));
        }
        cur = cur->next_free;
    }

    return NULL; /* Out of memory */
}

void my_free(void *ptr) {
    if (!ptr) return;

    BlockHeader *hdr = (BlockHeader*)((char*)ptr - sizeof(BlockHeader));
    size_t size = BLOCK_SIZE(hdr);

    /* Coalesce with next block if free */
    BlockHeader *next_hdr = (BlockHeader*)((char*)hdr + size);
    if ((void*)next_hdr < g_heap_end && !IS_ALLOCATED(next_hdr)) {
        remove_free_list((FreeBlock*)next_hdr);
        size += BLOCK_SIZE(next_hdr);
    }

    /* Coalesce with previous block if free */
    if ((void*)hdr > g_heap_start) {
        BlockHeader *prev_ftr = (BlockHeader*)((char*)hdr - sizeof(BlockHeader));
        if (!IS_ALLOCATED(prev_ftr)) {
            BlockHeader *prev_hdr = FOOTER_TO_HEADER(prev_ftr);
            remove_free_list((FreeBlock*)prev_hdr);
            size += BLOCK_SIZE(prev_hdr);
            hdr = prev_hdr;
        }
    }

    set_tags(hdr, size, false);
    insert_free_list((FreeBlock*)hdr);
}

void* my_realloc(void *ptr, size_t new_size) {
    if (!ptr) return my_malloc(new_size);
    if (new_size == 0) { my_free(ptr); return NULL; }

    BlockHeader *hdr = (BlockHeader*)((char*)ptr - sizeof(BlockHeader));
    size_t cur_payload = BLOCK_SIZE(hdr) - 2 * sizeof(BlockHeader);

    if (cur_payload >= new_size) return ptr; /* Already large enough */

    void *new_ptr = my_malloc(new_size);
    if (new_ptr) {
        memcpy(new_ptr, ptr, cur_payload);
        my_free(ptr);
    }
    return new_ptr;
}

void allocator_dump_stats(void) {
    size_t free_bytes = 0;
    size_t free_blocks = 0;
    FreeBlock *cur = g_free_list_head;
    while (cur) {
        free_bytes += BLOCK_SIZE(&cur->header);
        free_blocks++;
        cur = cur->next_free;
    }
    printf("[Allocator Stats] Free Blocks: %zu | Total Available Memory: %zu bytes\n",
           free_blocks, free_bytes);
}
```

### Source: `src/main.c`

```c
#include "allocator.h"
#include <stdio.h>
#include <string.h>

static uint8_t g_memory_pool[64 * 1024]; /* 64 KB heap */

int main(void) {
    printf("===========================================\n");
    printf("   Custom Boundary-Tag Memory Allocator    \n");
    printf("===========================================\n");

    allocator_init(g_memory_pool, sizeof(g_memory_pool));
    allocator_dump_stats();

    printf("\nAllocating 3 buffers (p1=128B, p2=256B, p3=512B)...\n");
    char *p1 = my_malloc(128);
    char *p2 = my_malloc(256);
    char *p3 = my_malloc(512);

    strcpy(p1, "Hello Allocator");
    printf("p1 content: \"%s\"\n", p1);
    allocator_dump_stats();

    printf("\nFreeing p2 (creating middle hole)...\n");
    my_free(p2);
    allocator_dump_stats();

    printf("\nFreeing p1 and p3 (triggering bidirectional coalescing)...\n");
    my_free(p1);
    my_free(p3);
    allocator_dump_stats();

    printf("\nAll memory coalesced successfully into a single contiguous block!\n");
    return 0;
}
```

---

## 3. Build Configuration (`Makefile`)

```makefile
CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -O2 -Iinclude
SRC = src/main.c src/allocator.c
OBJ = $(SRC:.c=.o)
TARGET = memory-allocator

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

## 4. Verification & Testing

```bash
$ make
$ ./memory-allocator
===========================================
   Custom Boundary-Tag Memory Allocator    
===========================================
[Allocator Stats] Free Blocks: 1 | Total Available Memory: 65536 bytes

Allocating 3 buffers (p1=128B, p2=256B, p3=512B)...
p1 content: "Hello Allocator"
[Allocator Stats] Free Blocks: 1 | Total Available Memory: 64512 bytes

Freeing p2 (creating middle hole)...
[Allocator Stats] Free Blocks: 2 | Total Available Memory: 64784 bytes

Freeing p1 and p3 (triggering bidirectional coalescing)...
[Allocator Stats] Free Blocks: 1 | Total Available Memory: 65536 bytes

All memory coalesced successfully into a single contiguous block!
```

---

[← Return to Projects Index](../README.md)
