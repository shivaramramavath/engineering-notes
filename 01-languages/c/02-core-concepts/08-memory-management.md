# Memory Management

C leaves memory management to you: no garbage collector, no automatic cleanup of heap memory. You request memory with `malloc`, and you are responsible for releasing it exactly once. Doing this reliably is the central discipline of C programming.

**Prerequisites:** [Pointers](./03-pointers.md), [scope and storage classes](./07-scope-storage-classes.md), [structs](./06-structs-unions-enums.md).

## Stack vs heap

| | Stack | Heap |
|---|---|---|
| Allocation | automatic on function entry | explicit (`malloc`) |
| Release | automatic on return | explicit (`free`) |
| Size | small (often 1-8 MB) | limited by the system |
| Speed | very fast | slower |
| Lifetime | the function call | until `free` |

Use the stack for small, short-lived data; use the heap when size is unknown at compile time, data is large, or it must outlive the function.

## The allocation functions (`<stdlib.h>`)

```c
void *malloc(size_t size);               /* uninitialized block */
void *calloc(size_t n, size_t size);     /* zero-initialized, checks n * size overflow */
void *realloc(void *ptr, size_t size);   /* resize a block (may move it) */
void  free(void *ptr);                   /* release; free(NULL) is a no-op */
```

### The standard pattern

```c
int *arr = malloc(n * sizeof *arr);      /* sizeof *arr stays correct if the type changes */
if (arr == NULL) {
    /* out of memory: handle it */
    return -1;
}
/* ... use arr[0 .. n-1] ... */
free(arr);
arr = NULL;                              /* avoids dangling use and double free */
```

Rules:

- **Always check for `NULL`.** Allocation can fail, especially for large sizes.
- **Don't cast** the result of `malloc` in C. It's unnecessary, and a cast can hide a missing `#include <stdlib.h>`.
- Use `sizeof *ptr` rather than `sizeof(type)`.
- `malloc` returns **uninitialized** memory. Use `calloc` or `memset` if you need zeros.
- The block is suitably aligned for any standard type.
- `malloc(0)` may return `NULL` or a unique pointer; don't rely on either.

### Overflow in size calculations

```c
size_t bytes = n * sizeof *arr;          /* can wrap around if n is huge → tiny allocation, heap overflow */
```

When `n` comes from outside, validate it, or use `calloc(n, sizeof *arr)`, which fails safely on overflow. This is a classic vulnerability.

## Growing a buffer with `realloc`

```c
int *tmp = realloc(arr, new_cap * sizeof *arr);
if (tmp == NULL) {
    /* arr is still valid and still owned by you */
    free(arr);
    return -1;
}
arr = tmp;
```

**Never** write `arr = realloc(arr, ...)`: if it fails, you lose the only pointer to the old block and leak it. `realloc` may move the block, so all other pointers into the old block become invalid. Grow geometrically (double the capacity) to get amortized O(1) appends.

```c
typedef struct { int *data; size_t len, cap; } vec_t;

int vec_push(vec_t *v, int x) {
    if (v->len == v->cap) {
        size_t cap = v->cap ? v->cap * 2 : 8;
        int *tmp = realloc(v->data, cap * sizeof *tmp);
        if (tmp == NULL) return -1;
        v->data = tmp;
        v->cap = cap;
    }
    v->data[v->len++] = x;
    return 0;
}
```

## Ownership

Every heap block should have exactly one **owner** responsible for freeing it. Document it in the API:

- **Create/destroy pairs:** `thing_new()` returns an owned object; the caller calls `thing_free()`.
- **Borrowing:** functions taking `const T *` use the data but don't free it.
- **Transfer:** a function that stores the pointer takes ownership, and says so in its docs.

```c
typedef struct { char *name; int *scores; } student_t;

void student_free(student_t *s) {
    if (s == NULL) return;
    free(s->name);
    free(s->scores);
    free(s);
}
```

Free inner allocations before the outer one.

## The classic bugs

| Bug | Cause | Symptom |
|---|---|---|
| **Memory leak** | Block never freed | Memory use grows over time |
| **Use after free** | Access after `free` | Corruption, crashes, exploits |
| **Double free** | `free` twice | Heap corruption, abort |
| **Buffer overflow** | Write past the end | Corruption, exploits |
| **Invalid free** | `free` of a stack/global/interior pointer | Crash or abort |
| **Uninitialized read** | Using `malloc` memory before writing | Nondeterministic behavior |

```c
char *s = malloc(4);
strcpy(s, "hello");          /* overflow: needs 6 bytes */
free(s);
free(s);                     /* double free */
```

## Tools

```bash
gcc -g -fsanitize=address,undefined prog.c -o prog && ./prog   # catches most of the above
valgrind --leak-check=full ./prog                              # Linux; slower, needs -g for line numbers
```

AddressSanitizer reports the allocation, the free, and the bad access with stack traces. Run your tests under it routinely. See [sanitizers and Valgrind](../06-debugging-testing/04-sanitizers-valgrind.md) and [common bugs](../06-debugging-testing/06-common-bugs.md).

## When the process exits

The OS reclaims all memory when a process ends, so freeing everything right before `exit` isn't strictly required. But leak checkers can't distinguish deliberate from accidental leaks, and long-running programs and libraries must free properly. Free what you allocate.

## Beyond `malloc`

For many small, same-lifetime allocations, arena or pool allocators are faster and simpler to manage; see [custom allocators](../04-advanced/03-custom-allocators.md). For how `malloc` works inside, see [malloc internals](../05-internals/06-malloc-internals.md).

## Common mistakes

- Not checking `malloc`'s return value.
- `p = realloc(p, n)` leaking on failure.
- Allocating with the wrong size: `malloc(sizeof p)` when `p` is a pointer.
- Returning a pointer to a stack array.
- Freeing in one branch but not on early returns (use the `goto cleanup` pattern from [control flow](../01-fundamentals/07-control-flow.md)).
- Using a pointer after `realloc` may have moved the block.
- Forgetting the terminator in a string allocation: `malloc(strlen(s) + 1)`.

## Summary

- Pair every allocation with exactly one `free`; define who owns each block.
- Check for `NULL`, use `sizeof *ptr`, validate sizes, and use a temporary with `realloc`.
- Set pointers to `NULL` after freeing.
- Run AddressSanitizer and Valgrind regularly.

**Next:** [Preprocessor](./09-preprocessor.md)
