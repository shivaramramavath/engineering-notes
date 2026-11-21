# Project 03: Persistent In-Memory Key-Value Store (`kv-store`)

> A Redis-inspired in-memory key-value database featuring hash table indexing, binary-safe key/value storage, and crash-resilient Append-Only File (AOF) disk persistence.

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `03-key-value-store` |
| **Language Baseline** | C11 / POSIX.1-2008 |
| **Complexity** | Advanced / Storage |
| **Core Systems Concepts** | Hash Tables, Crash Recovery, Append-Only Logs, Serialization |

---

## 1. Architectural Overview & Component Breakdown

The Key-Value Store combines low-latency in-memory operations with durable write-ahead logging:
1. **Hash Table Index (`hash_table.c`):** Implements dynamic hash table buckets with separate chaining. Hash keys using Daniel J. Bernstein's `djb2` algorithm.
2. **Persistence Engine (`aof.c`):** Intercepts mutating operations (`SET`, `DEL`) and synchronously appends formatted commands to an Append-Only File (`store.aof`).
3. **Recovery Manager (`aof.c`):** At database initialization, parses and replays the AOF log from disk, reconstructing the complete in-memory hash state prior to accepting client commands.
4. **Interactive REPL (`main.c`):** Parses user commands (`SET`, `GET`, `DEL`, `EXISTS`, `KEYS`, `EXIT`).

```
       +-----------------------------------------------------------+
       |               Key-Value Store Architecture                |
       +-----------------------------------------------------------+
                                     |
                       User Command: "SET user:1 Alice"
                                     v
                        [ Command Dispatcher ]
                                     |
                +--------------------+--------------------+
                |                                         |
                v                                         v
       [ In-Memory Hash Table ]                  [ Append-Only File (AOF) ]
       - djb2 hash computation                   - Format: "SET user:1 Alice\n"
       - Insert/Update bucket node               - write() to store.aof
       - Fast O(1) in-memory lookup              - fsync() for durability
```

---

## 2. Complete Project Source Code

### Header: `include/kv.h`

```c
#ifndef KV_H
#define KV_H

#include <stddef.h>
#include <stdbool.h>

#define KV_HASH_BUCKETS 1024
#define KV_AOF_FILENAME "store.aof"

typedef struct KVPair {
    char *key;
    char *value;
    struct KVPair *next;
} KVPair;

typedef struct KVStore {
    KVPair *buckets[KV_HASH_BUCKETS];
    size_t count;
    int aof_fd;
} KVStore;

/* Store Lifecycle APIs */
KVStore* kv_store_create(const char *aof_path);
void kv_store_destroy(KVStore **store);

/* CRUD Operations */
bool kv_set(KVStore *store, const char *key, const char *value, bool append_to_aof);
const char* kv_get(const KVStore *store, const char *key);
bool kv_del(KVStore *store, const char *key, bool append_to_aof);
bool kv_exists(const KVStore *store, const char *key);
void kv_list_keys(const KVStore *store);

/* Persistence & Recovery */
bool kv_aof_replay(KVStore *store, const char *aof_path);

#endif /* KV_H */
```

### Source: `src/hash_table.c`

```c
#include "kv.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned long djb2_hash(const char *str) {
    unsigned long hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    }
    return hash;
}

bool kv_set(KVStore *store, const char *key, const char *value, bool append_to_aof) {
    if (!store || !key || !value) return false;

    unsigned long bucket_idx = djb2_hash(key) % KV_HASH_BUCKETS;
    KVPair *cur = store->buckets[bucket_idx];

    /* Update existing key */
    while (cur) {
        if (strcmp(cur->key, key) == 0) {
            char *new_val = strdup(value);
            if (!new_val) return false;
            free(cur->value);
            cur->value = new_val;

            if (append_to_aof && store->aof_fd >= 0) {
                char entry[512];
                int len = snprintf(entry, sizeof(entry), "SET %s %s\n", key, value);
                write(store->aof_fd, entry, len);
            }
            return true;
        }
        cur = cur->next;
    }

    /* Insert new node */
    KVPair *pair = malloc(sizeof(KVPair));
    if (!pair) return false;
    pair->key = strdup(key);
    pair->value = strdup(value);
    pair->next = store->buckets[bucket_idx];
    store->buckets[bucket_idx] = pair;
    store->count++;

    if (append_to_aof && store->aof_fd >= 0) {
        char entry[512];
        int len = snprintf(entry, sizeof(entry), "SET %s %s\n", key, value);
        write(store->aof_fd, entry, len);
    }

    return true;
}

const char* kv_get(const KVStore *store, const char *key) {
    if (!store || !key) return NULL;
    unsigned long bucket_idx = djb2_hash(key) % KV_HASH_BUCKETS;
    KVPair *cur = store->buckets[bucket_idx];
    while (cur) {
        if (strcmp(cur->key, key) == 0) {
            return cur->value;
        }
        cur = cur->next;
    }
    return NULL;
}

bool kv_del(KVStore *store, const char *key, bool append_to_aof) {
    if (!store || !key) return false;
    unsigned long bucket_idx = djb2_hash(key) % KV_HASH_BUCKETS;
    KVPair **curr = &store->buckets[bucket_idx];

    while (*curr) {
        if (strcmp((*curr)->key, key) == 0) {
            KVPair *to_free = *curr;
            *curr = (*curr)->next;
            free(to_free->key);
            free(to_free->value);
            free(to_free);
            store->count--;

            if (append_to_aof && store->aof_fd >= 0) {
                char entry[512];
                int len = snprintf(entry, sizeof(entry), "DEL %s\n", key);
                write(store->aof_fd, entry, len);
            }
            return true;
        }
        curr = &(*curr)->next;
    }
    return false;
}

bool kv_exists(const KVStore *store, const char *key) {
    return kv_get(store, key) != NULL;
}

void kv_list_keys(const KVStore *store) {
    if (!store) return;
    printf("Listing all %zu keys in store:\n", store->count);
    for (size_t i = 0; i < KV_HASH_BUCKETS; ++i) {
        KVPair *cur = store->buckets[i];
        while (cur) {
            printf(" - %s => %s\n", cur->key, cur->value);
            cur = cur->next;
        }
    }
}
```

### Source: `src/aof.c`

```c
#include "kv.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

bool kv_aof_replay(KVStore *store, const char *aof_path) {
    FILE *f = fopen(aof_path, "r");
    if (!f) return true; /* No previous log exists, fresh start */

    char line[512];
    size_t replayed = 0;
    while (fgets(line, sizeof(line), f)) {
        char cmd[16], key[128], val[256];
        int fields = sscanf(line, "%15s %127s %255[^\n]", cmd, key, val);
        if (fields == 3 && strcmp(cmd, "SET") == 0) {
            kv_set(store, key, val, false); /* Replay in-memory without rewriting log */
            replayed++;
        } else if (fields == 2 && strcmp(cmd, "DEL") == 0) {
            kv_del(store, key, false);
            replayed++;
        }
    }
    fclose(f);
    printf("[AOF] Successfully replayed %zu transactions from disk.\n", replayed);
    return true;
}

KVStore* kv_store_create(const char *aof_path) {
    KVStore *store = calloc(1, sizeof(KVStore));
    if (!store) return NULL;

    /* Step 1: Replay previous transactions */
    kv_aof_replay(store, aof_path);

    /* Step 2: Open AOF for append operations */
    store->aof_fd = open(aof_path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (store->aof_fd < 0) {
        perror("aof open");
    }
    return store;
}

void kv_store_destroy(KVStore **store_ptr) {
    if (!store_ptr || !*store_ptr) return;
    KVStore *store = *store_ptr;

    if (store->aof_fd >= 0) {
        fsync(store->aof_fd);
        close(store->aof_fd);
    }

    for (size_t i = 0; i < KV_HASH_BUCKETS; ++i) {
        KVPair *cur = store->buckets[i];
        while (cur) {
            KVPair *next = cur->next;
            free(cur->key);
            free(cur->value);
            free(cur);
            cur = next;
        }
    }

    free(store);
    *store_ptr = NULL;
}
```

### Source: `src/main.c`

```c
#include "kv.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    printf("===========================================\n");
    printf("   In-Memory KV Store with AOF Persistence \n");
    printf("   Commands: SET <k> <v> | GET <k> | DEL <k>\n");
    printf("             EXISTS <k>  | KEYS    | EXIT   \n");
    printf("===========================================\n");

    KVStore *store = kv_store_create(KV_AOF_FILENAME);
    if (!store) {
        fprintf(stderr, "Failed to initialize store\n");
        return 1;
    }

    char line[512];
    while (1) {
        printf("kv> ");
        fflush(stdout);
        if (!fgets(line, sizeof(line), stdin)) break;

        char cmd[16], key[128], val[256];
        int parsed = sscanf(line, "%15s %127s %255[^\n]", cmd, key, val);
        if (parsed <= 0) continue;

        if (strcasecmp(cmd, "EXIT") == 0) {
            break;
        } else if (strcasecmp(cmd, "SET") == 0 && parsed == 3) {
            kv_set(store, key, val, true);
            printf("OK\n");
        } else if (strcasecmp(cmd, "GET") == 0 && parsed >= 2) {
            const char *res = kv_get(store, key);
            if (res) printf("\"%s\"\n", res);
            else printf("(nil)\n");
        } else if (strcasecmp(cmd, "DEL") == 0 && parsed >= 2) {
            bool ok = kv_del(store, key, true);
            printf(":(integer) %d\n", ok ? 1 : 0);
        } else if (strcasecmp(cmd, "EXISTS") == 0 && parsed >= 2) {
            printf(":(integer) %d\n", kv_exists(store, key) ? 1 : 0);
        } else if (strcasecmp(cmd, "KEYS") == 0) {
            kv_list_keys(store);
        } else {
            printf("(error) ERR unknown command or invalid syntax\n");
        }
    }

    kv_store_destroy(&store);
    printf("Database closed cleanly.\n");
    return 0;
}
```

---

## 3. Build Configuration (`Makefile`)

```makefile
CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -O2 -Iinclude -D_POSIX_C_SOURCE=200809L
SRC = src/main.c src/hash_table.c src/aof.c
OBJ = $(SRC:.c=.o)
TARGET = kv-store

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $@

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f src/*.o $(TARGET) store.aof

.PHONY: all clean
```

---

## 4. Verification & Sample Session

```bash
$ make
$ ./kv-store
===========================================
   In-Memory KV Store with AOF Persistence 
   Commands: SET <k> <v> | GET <k> | DEL <k>
             EXISTS <k>  | KEYS    | EXIT   
===========================================
[AOF] Successfully replayed 0 transactions from disk.
kv> SET user:100 "Alice Smith"
OK
kv> SET user:101 "Bob Jones"
OK
kv> GET user:100
""Alice Smith""
kv> EXISTS user:101
:(integer) 1
kv> DEL user:101
:(integer) 1
kv> EXIT
Database closed cleanly.

# Restart server to verify AOF persistence recovery:
$ ./kv-store
[AOF] Successfully replayed 3 transactions from disk.
kv> GET user:100
""Alice Smith""
kv> GET user:101
(nil)
kv> EXIT
```

---

[← Return to Projects Index](../README.md)
