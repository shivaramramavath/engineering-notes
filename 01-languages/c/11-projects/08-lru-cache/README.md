# Project 08: Constant-Time $O(1)$ LRU Cache (`lru-cache`)

> A high-performance Least Recently Used (LRU) memory cache in C combining open-addressed hash tables with an intrusive doubly-linked list for guaranteed $O(1)$ lookup, insertion, and eviction.

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `08-lru-cache` |
| **Language Baseline** | C11 / Standard Architecture |
| **Complexity** | Intermediate / Data Structures |
| **Core Systems Concepts** | Hash Indexing, Intrusive Lists, Cache Eviction Policies, $O(1)$ Guarantees |

---

## 1. Architectural Overview & Component Breakdown

The LRU Cache couples two distinct data structures:
1. **Hash Table:** Provides $O(1)$ mapping from string keys to list node pointers.
2. **Doubly-Linked List:** Maintains access recency. The Most Recently Used (MRU) node sits at the head; the Least Recently Used (LRU) sits at the tail.
3. **Promote on Hit:** Whenever `lru_get()` is called, the node is spliced out of its current position and inserted at the head ($O(1)$).
4. **Evict on Overflow:** When `lru_put()` exceeds cache capacity, the node at the tail is purged from both the linked list and hash table ($O(1)$).

```
       +-----------------------------------------------------------+
       |                  LRU Cache Architecture                   |
       +-----------------------------------------------------------+
                                     |
               Hash Table Buckets (O(1) Direct Key Index)
               [ "session:A" ] ----+
               [ "session:B" ] ----|---------+
                                   |         |
                                   v         v
        +---------------------------------------------------------+
        | MRU Node (Head) <-> Node B <-> LRU Node (Tail)          |
        +---------------------------------------------------------+
              ^                                   |
              |                                   v
        lru_get() promotes to MRU         Overflow evicts from LRU
```

---

## 2. Complete Project Source Code

### Header: `include/lru.h`

```c
#ifndef LRU_H
#define LRU_H

#include <stddef.h>
#include <stdbool.h>

typedef struct LRUNode {
    char *key;
    char *value;
    struct LRUNode *prev;
    struct LRUNode *next;
    struct LRUNode *hash_next;
} LRUNode;

typedef struct LRUCache {
    size_t capacity;
    size_t count;
    LRUNode *head;
    LRUNode *tail;
    LRUNode **buckets;
    size_t num_buckets;
    size_t hits;
    size_t misses;
} LRUCache;

/* Public Interfaces */
LRUCache* lru_cache_create(size_t capacity);
void lru_cache_destroy(LRUCache **cache_ptr);
bool lru_cache_put(LRUCache *cache, const char *key, const char *value);
const char* lru_cache_get(LRUCache *cache, const char *key);
void lru_cache_dump(const LRUCache *cache);

#endif /* LRU_H */
```

### Source: `src/lru.c`

```c
#include "lru.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned long hash_str(const char *str) {
    unsigned long h = 5381;
    int c;
    while ((c = *str++)) h = ((h << 5) + h) + c;
    return h;
}

static void detach_node(LRUCache *cache, LRUNode *node) {
    if (node->prev) node->prev->next = node->next;
    else cache->head = node->next;

    if (node->next) node->next->prev = node->prev;
    else cache->tail = node->prev;
}

static void attach_head(LRUCache *cache, LRUNode *node) {
    node->next = cache->head;
    node->prev = NULL;
    if (cache->head) cache->head->prev = node;
    cache->head = node;
    if (!cache->tail) cache->tail = node;
}

LRUCache* lru_cache_create(size_t capacity) {
    if (capacity == 0) return NULL;
    LRUCache *c = calloc(1, sizeof(LRUCache));
    c->capacity = capacity;
    c->num_buckets = capacity * 2;
    c->buckets = calloc(c->num_buckets, sizeof(LRUNode*));
    return c;
}

const char* lru_cache_get(LRUCache *cache, const char *key) {
    if (!cache || !key) return NULL;
    size_t idx = hash_str(key) % cache->num_buckets;
    LRUNode *cur = cache->buckets[idx];

    while (cur) {
        if (strcmp(cur->key, key) == 0) {
            /* Hit: Move to head */
            detach_node(cache, cur);
            attach_head(cache, cur);
            cache->hits++;
            return cur->value;
        }
        cur = cur->hash_next;
    }

    cache->misses++;
    return NULL;
}

bool lru_cache_put(LRUCache *cache, const char *key, const char *value) {
    if (!cache || !key || !value) return false;
    size_t idx = hash_str(key) % cache->num_buckets;
    LRUNode *cur = cache->buckets[idx];

    while (cur) {
        if (strcmp(cur->key, key) == 0) {
            free(cur->value);
            cur->value = strdup(value);
            detach_node(cache, cur);
            attach_head(cache, cur);
            return true;
        }
        cur = cur->hash_next;
    }

    /* Evict tail if full */
    if (cache->count >= cache->capacity && cache->tail) {
        LRUNode *lru = cache->tail;
        detach_node(cache, lru);

        /* Remove from hash bucket */
        size_t lru_idx = hash_str(lru->key) % cache->num_buckets;
        LRUNode **h_cur = &cache->buckets[lru_idx];
        while (*h_cur) {
            if (*h_cur == lru) {
                *h_cur = lru->hash_next;
                break;
            }
            h_cur = &(*h_cur)->hash_next;
        }

        free(lru->key);
        free(lru->value);
        free(lru);
        cache->count--;
    }

    /* Insert new node */
    LRUNode *node = calloc(1, sizeof(LRUNode));
    node->key = strdup(key);
    node->value = strdup(value);
    node->hash_next = cache->buckets[idx];
    cache->buckets[idx] = node;

    attach_head(cache, node);
    cache->count++;
    return true;
}

void lru_cache_dump(const LRUCache *cache) {
    if (!cache) return;
    printf("[LRU Cache] Count: %zu/%zu | Hits: %zu, Misses: %zu\n",
           cache->count, cache->capacity, cache->hits, cache->misses);
    LRUNode *cur = cache->head;
    printf("MRU -> ");
    while (cur) {
        printf("[%s: %s] -> ", cur->key, cur->value);
        cur = cur->next;
    }
    printf("LRU\n");
}

void lru_cache_destroy(LRUCache **cache_ptr) {
    if (!cache_ptr || !*cache_ptr) return;
    LRUCache *c = *cache_ptr;
    LRUNode *cur = c->head;
    while (cur) {
        LRUNode *next = cur->next;
        free(cur->key);
        free(cur->value);
        free(cur);
        cur = next;
    }
    free(c->buckets);
    free(c);
    *cache_ptr = NULL;
}
```

### Source: `src/main.c`

```c
#include "lru.h"
#include <stdio.h>

int main(void) {
    printf("===========================================\n");
    printf("   Constant-Time O(1) LRU Memory Cache     \n");
    printf("===========================================\n");

    LRUCache *cache = lru_cache_create(3);

    printf("\nInserting 3 items (A, B, C)...\n");
    lru_cache_put(cache, "A", "Alpha");
    lru_cache_put(cache, "B", "Beta");
    lru_cache_put(cache, "C", "Gamma");
    lru_cache_dump(cache);

    printf("\nAccessing key 'A' (Promotes A to MRU)...\n");
    lru_cache_get(cache, "A");
    lru_cache_dump(cache);

    printf("\nInserting key 'D' (Triggers eviction of LRU key 'B')...\n");
    lru_cache_put(cache, "D", "Delta");
    lru_cache_dump(cache);

    lru_cache_destroy(&cache);
    printf("Cache destroyed successfully.\n");
    return 0;
}
```

---

## 3. Build Configuration (`Makefile`)

```makefile
CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -O2 -Iinclude
SRC = src/main.c src/lru.c
OBJ = $(SRC:.c=.o)
TARGET = lru-cache

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
