# Hash Tables

A hash table maps keys to values in O(1) average time. A **hash function** turns a key into a number, which selects a **bucket**; collisions (different keys, same bucket) are resolved by chaining or probing. C has no built-in hash table, so you'll write one or use a library.

**Prerequisites:** [Linked lists](./01-linked-lists.md), [strings](../../02-core-concepts/02-strings.md), [memory management](../../02-core-concepts/08-memory-management.md).

## How it works

```text
key "bob" ──hash──▶ 0x9F3A... ──% capacity──▶ bucket 2

buckets: [0] → NULL
         [1] → ("al", 3) → ("eve", 9) → NULL
         [2] → ("bob", 7) → NULL
         [3] → NULL
```

1. Compute `hash(key)`.
2. Reduce it to a bucket index: `hash % capacity` (or `hash & (capacity - 1)` when capacity is a power of two).
3. Look in that bucket for an entry whose key equals yours.

**Load factor** = entries / buckets. As it grows, chains lengthen and operations slow down, so tables **resize** (typically double) and rehash when the load factor passes a threshold (0.75 is common).

## A hash function for strings: FNV-1a

```c
#include <stdint.h>

static uint64_t hash_str(const char *s) {
    uint64_t h = 14695981039346656037ULL;        /* FNV offset basis */
    while (*s) {
        h ^= (unsigned char)*s++;
        h *= 1099511628211ULL;                   /* FNV prime */
    }
    return h;
}
```

Simple and decent for non-adversarial input. A good hash spreads similar keys across buckets and is cheap to compute.

## Implementation: chaining, string keys, `int` values

```c
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

typedef struct entry {
    char         *key;
    int           value;
    struct entry *next;
} entry_t;

typedef struct {
    entry_t **buckets;
    size_t    cap;
    size_t    len;
} map_t;

static char *dup_str(const char *s) {
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

bool map_init(map_t *m) {
    m->cap = 16;
    m->len = 0;
    m->buckets = calloc(m->cap, sizeof *m->buckets);      /* all NULL */
    return m->buckets != NULL;
}

static bool map_grow(map_t *m) {
    size_t ncap = m->cap * 2;
    entry_t **nb = calloc(ncap, sizeof *nb);
    if (nb == NULL) return false;

    for (size_t i = 0; i < m->cap; i++) {                 /* relink existing nodes, no new allocations */
        entry_t *e = m->buckets[i];
        while (e != NULL) {
            entry_t *next = e->next;
            size_t j = hash_str(e->key) % ncap;
            e->next = nb[j];
            nb[j] = e;
            e = next;
        }
    }
    free(m->buckets);
    m->buckets = nb;
    m->cap = ncap;
    return true;
}

bool map_put(map_t *m, const char *key, int value) {
    size_t i = hash_str(key) % m->cap;
    for (entry_t *e = m->buckets[i]; e != NULL; e = e->next) {
        if (strcmp(e->key, key) == 0) { e->value = value; return true; }   /* update */
    }

    if ((m->len + 1) * 4 > m->cap * 3) {                  /* load factor > 0.75 */
        if (!map_grow(m)) return false;
        i = hash_str(key) % m->cap;                       /* capacity changed: recompute */
    }

    entry_t *e = malloc(sizeof *e);
    if (e == NULL) return false;
    e->key = dup_str(key);
    if (e->key == NULL) { free(e); return false; }
    e->value = value;
    e->next = m->buckets[i];
    m->buckets[i] = e;
    m->len++;
    return true;
}

bool map_get(const map_t *m, const char *key, int *out) {
    size_t i = hash_str(key) % m->cap;
    for (const entry_t *e = m->buckets[i]; e != NULL; e = e->next) {
        if (strcmp(e->key, key) == 0) { *out = e->value; return true; }
    }
    return false;
}

void map_free(map_t *m) {
    for (size_t i = 0; i < m->cap; i++) {
        entry_t *e = m->buckets[i];
        while (e != NULL) {
            entry_t *next = e->next;
            free(e->key);
            free(e);
            e = next;
        }
    }
    free(m->buckets);
    *m = (map_t){0};
}
```

Design points worth noting:

- The map **owns** its copies of the keys (`dup_str`), so callers can pass temporary strings.
- Resizing relinks nodes instead of reallocating them; only the bucket array is new.
- `map_get` returns a `bool` and writes through an out-parameter, so any `int` can be a value ([error handling](../03-error-handling.md)).
- Deleting an entry means unlinking from its chain (the pointer-to-pointer technique from [linked lists](./01-linked-lists.md)).

## Open addressing (the alternative)

Store entries directly in one array. On collision, probe other slots (linear: `i+1, i+2, ...`; quadratic; double hashing).

| | Chaining | Open addressing |
|---|---|---|
| Memory layout | array of pointers + scattered nodes | one contiguous array: cache-friendly |
| Allocation per insert | yes | no (until resize) |
| Deletion | unlink a node | needs *tombstones* or backward-shift |
| Load factor limit | can exceed 1 | must stay below ~0.7 |
| Complexity to implement | simple | trickier (clustering, tombstones) |

High-performance tables (and most modern designs) use open addressing; chaining is easier to get right first.

## Choosing keys and hashes

- **Equality must agree with hashing:** equal keys must have equal hashes.
- **Don't hash pointers or padded structs byte-by-byte** unless you've zeroed padding; hash the meaningful fields.
- For integer keys use a mixing function (for example a multiply-shift or the finalizer from `splitmix64`), since `key % cap` on patterned integers clusters badly.
- **Security:** with a fixed, known hash function an attacker can craft keys that all collide and degrade the table to O(n) per lookup (hash flooding). For tables fed by untrusted input, use a keyed hash (SipHash) with a random seed.

## Complexity

| Operation | Average | Worst case |
|---|---|---|
| Insert / lookup / delete | O(1) | O(n) (all keys collide) |
| Resize | O(n), amortized away | |
| Iteration | O(capacity + n) | order is arbitrary |

## Common mistakes

| Mistake | Effect |
|---|---|
| Storing the caller's `char *` key without copying | Dangling pointer when the caller reuses the buffer |
| Forgetting to recompute the index after a resize | Entry lands in the wrong bucket and later lookups fail |
| Never resizing | Chains grow; performance collapses |
| Comparing keys with `==` instead of `strcmp` | Never matches |
| Mutating a key after insertion | Entry becomes unreachable |
| Hashing with `hash % cap` and a poor hash | Clustering |
| Freeing keys but not nodes (or vice versa) in `map_free` | Leaks |
| Relying on iteration order | It changes with resizing |

## Summary

- Hash → bucket → compare keys; resize at a load factor around 0.75.
- Own your keys, relink on resize, return status via out-parameters.
- Chaining is simplest; open addressing is faster but fiddlier.
- Use a seeded hash when keys come from untrusted sources.

**Next:** [Graphs](./05-graphs.md)
