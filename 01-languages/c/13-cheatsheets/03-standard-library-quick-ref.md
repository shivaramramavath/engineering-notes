# Standard Library Quick Reference (`03-standard-library-quick-ref`)

## `<stdlib.h>`
- `malloc(size)`: Uninitialized heap allocation.
- `calloc(num, size)`: Zero-initialized heap allocation.
- `realloc(ptr, new_size)`: Resizes allocation.
- `free(ptr)`: Deallocates memory (`free(NULL)` is safe).
- `abort()`: Abnormal termination (`SIGABRT`).
- `exit(status)`: Normal termination; flushes streams and runs `atexit` handlers.
- `strtol(str, &endptr, base)`: Safe integer parsing with error checking.
- `strtod(str, &endptr)`: Safe floating-point parsing.
- `qsort(base, num, size, compar)`: Quicksort array.
- `bsearch(key, base, num, size, compar)`: Binary search sorted array.

## `<string.h>`
- `strlen(s)`: String length up to `\0` ($O(N)$).
- `memcpy(dst, src, n)`: Fast byte copy; **buffers must NOT overlap**.
- `memmove(dst, src, n)`: Safe byte copy for overlapping memory.
- `memset(dst, c, n)`: Fills buffer with byte value `c`.
- `memcmp(s1, s2, n)`: Compares `n` bytes.
- `strcmp(s1, s2)`: Lexicographical string comparison.
- `strncmp(s1, s2, n)`: Bounded string comparison.
- `strchr(s, c)`: Locate first occurrence of character `c`.
- `strstr(haystack, needle)`: Locate substring.

## `<stdint.h>`
- Exact-width integers: `int8_t`, `uint8_t`, `int16_t`, `uint16_t`, `int32_t`, `uint32_t`, `int64_t`, `uint64_t`.
- Pointer-sized integers: `intptr_t`, `uintptr_t`.
- Maximum-width integers: `intmax_t`, `uintmax_t`.
- Limits: `INT32_MIN`, `INT32_MAX`, `UINT32_MAX`, `UINT64_MAX`, `SIZE_MAX`.
