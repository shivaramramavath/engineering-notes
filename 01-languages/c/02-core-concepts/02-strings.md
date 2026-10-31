# Strings

C has no string type. A string is an array of `char` that ends with a null byte `'\0'`. Every string function relies on that terminator, and most string bugs are a missing terminator, a too-small buffer, or modifying something that is read-only.

**Prerequisites:** [Arrays](./01-arrays.md), [input/output](../01-fundamentals/09-input-output.md).

## Representation

```c
char s[] = "hello";
```

```text
index:  0    1    2    3    4    5
       'h'  'e'  'l'  'l'  'o'  '\0'
```

Six bytes for five visible characters. `strlen(s)` is 5; `sizeof s` is 6. The buffer must always have room for the terminator.

## Literals vs arrays

```c
char s[] = "hello";          /* writable array, initialized with a copy */
const char *p = "hello";     /* pointer to a read-only literal */

s[0] = 'J';                  /* fine */
p[0] = 'J';                  /* undefined behavior (usually a crash) */
```

Declare pointers to literals as `const char *` so the compiler catches modification. Use `char buf[N]` when you need to change the contents.

## The core functions (`<string.h>`)

| Function | Does | Notes |
|---|---|---|
| `strlen(s)` | length without `'\0'` | Scans the string: O(n); don't call in a loop condition |
| `strcmp(a, b)` | compare | returns `<0`, `0`, `>0`; **not** a boolean |
| `strncmp(a, b, n)` | compare up to `n` chars | |
| `strchr(s, c)` / `strrchr` | first / last occurrence of a char | returns pointer or `NULL` |
| `strstr(s, sub)` | find substring | pointer or `NULL` |
| `strspn` / `strcspn` | length of a prefix made of / free of given chars | `strcspn(s, "\n")` finds the newline |
| `memcpy(d, s, n)` | copy `n` bytes | regions must not overlap |
| `memmove(d, s, n)` | copy `n` bytes | overlap allowed |
| `memset(p, v, n)` | fill bytes | |

```c
if (strcmp(a, b) == 0) { /* equal */ }     /* never use a == b for strings */
```

`a == b` compares **addresses**, not contents.

## Copying and building strings safely

`strcpy` and `strcat` don't check sizes; `strncpy` does not do what its name suggests.

```c
char dst[8];
strncpy(dst, "toolongvalue", sizeof dst);   /* NOT null-terminated if src is too long;
                                               also pads the rest with zeros */
```

The recommended approach is `snprintf`, which always terminates (when the size is non-zero) and reports truncation:

```c
char path[64];
int n = snprintf(path, sizeof path, "%s/%s", dir, file);
if (n < 0 || (size_t)n >= sizeof path) {
    /* output was truncated or an error occurred */
}
```

When appending, track the remaining space or build the whole string with one `snprintf`. If you must use `strncpy`, terminate manually: `dst[sizeof dst - 1] = '\0';`.

`strlcpy`/`strlcat` exist on BSD, macOS and recent glibc, but are not in ISO C.

## Duplicating

```c
char *copy = strdup(s);    /* malloc + copy; free(copy) later */
```

`strdup` is POSIX and was added to ISO C in C23. On older strict-standard builds, write it yourself with `malloc(strlen(s) + 1)` and `memcpy`.

## Common tasks

```c
/* Remove trailing newline from fgets */
s[strcspn(s, "\n")] = '\0';

/* Convert to number, with error checking */
char *end;
long v = strtol(s, &end, 10);

/* Iterate */
for (const char *p = s; *p != '\0'; p++) { /* use *p */ }

/* Split (modifies the string; not thread-safe) */
for (char *tok = strtok(line, ","); tok != NULL; tok = strtok(NULL, ",")) { ... }
```

`strtok` writes `'\0'` into your buffer and keeps hidden state. Never use it on a literal, and consider `strtok_r` (POSIX) or manual parsing. See [text processing](../03-intermediate/07-text-processing-parsing.md).

## Characters and encodings

- `char` holds one **byte**, not necessarily one character. With UTF-8, a character may be 1 to 4 bytes, so `strlen` returns bytes and `s[i]` may be half a character.
- Functions like `toupper`, `isalpha` need an argument cast to `unsigned char`: `isalpha((unsigned char)c)`.
- `'a'` is an `int` character constant; `"a"` is a 2-byte string (`'a'`, `'\0'`).

See [unicode and locale](../03-intermediate/08-unicode-locale.md).

## Common mistakes

| Mistake | Effect |
|---|---|
| Buffer sized for the text, not text + 1 | Terminator overwrites memory |
| `==` to compare strings | Compares addresses |
| Modifying a string literal | Undefined behavior |
| `strcpy(dst, src)` without a size check | Buffer overflow |
| Relying on `strncpy` to terminate | Unterminated string, later over-read |
| `strlen` in a loop condition | O(n²) |
| `char *s; strcpy(s, "x");` | Writes through an uninitialized pointer |
| Using `strtok` on a literal | Crash |

## Debugging

- If printing a string shows trailing garbage, it's probably missing its `'\0'`.
- AddressSanitizer reports the exact overflow location (`stack-buffer-overflow`, `heap-buffer-overflow`).
- In gdb, `print s` and `x/s s` show the string; `x/16xb s` shows raw bytes, including the terminator.

## Summary

- A string is a `char` array ending in `'\0'`; size buffers for length + 1.
- Compare with `strcmp`; build with `snprintf`; avoid `strcpy`, `strcat`, and unchecked `strncpy`.
- String literals are read-only; use `const char *` for them.
- `char` is a byte, not a Unicode character.

**Next:** [Pointers](./03-pointers.md)
