# Standard string.h Functions and Bounds-Safe Alternatives

> Deep dive into C string manipulation: string inspection, copying, concatenation, tokenization, bounds-safe functions, and truncation detection.

| Metadata | Details |
| :--- | :--- |
| **Level** | Core Concepts |
| **Prerequisites** | [Strings](./02-strings.md), [Pointers](./03-pointers.md), [Memory Management](./08-memory-management.md) |
| **Time to Read** | 30 minutes |
| **Status** | Complete |
| **Standard** | ISO C11 / C17 / C23 |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. [Practical Examples](#5-practical-examples)
6. [Best Practices](#6-best-practices)
7. [Common Mistakes & Pitfalls](#7-common-mistakes--pitfalls)
8. [Performance & Security Notes](#8-performance--security-notes)
9. [Comparison: When to Use What](#9-comparison-when-to-use-what)
10. [Interview Questions](#10-interview-questions)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary & Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

In C, strings are not primitive objects with encapsulated length headers; they are **Null-Terminated Byte Strings (NTBS)**: contiguous character arrays terminated by a zero byte (`'\0'`). The C standard library `<string.h>` provides the core functions for operating on these byte sequences.

Historically, standard functions like `strcpy()`, `strcat()`, and `sprintf()` accepted destination pointers without specifying buffer capacities. When input exceeded the destination size, they silently smashed the stack, creating the primary vector for remote code execution vulnerabilities for decades.

Modern C programming mandates understanding both classic `<string.h>` mechanics and bounds-safe alternatives:
- Why `strncpy()` is **not** a safe replacement for `strcpy()`.
- How BSD `strlcpy()` / `strlcat()` and standard `snprintf()` ensure null-termination.
- The thread-safety pitfalls of global state in `strtok()` vs. reentrant `strtok_r()`.

---

## 2. Core Concepts

### Taxonomy of Standard `<string.h>` Operations
1. **Length and Examination**:
   - `size_t strlen(const char *s)`: Returns the number of characters preceding the terminating null character. Time complexity is strictly $O(N)$.
   - `size_t strnlen(const char *s, size_t maxlen)`: Returns length bounded by `maxlen`, preventing infinite reads on unterminated buffers.
2. **Copying and Concatenation**:
   - `char* strcpy(char *dest, const char *src)`: **UNSAFE**. Copies until `'\0'`; no bounds checking.
   - `char* strncpy(char *dest, const char *src, size_t n)`: Problematic. Does not null-terminate if `src >= n`, and zero-pads the remainder of the buffer up to $n$ bytes.
   - `int snprintf(char *buf, size_t size, const char *fmt, ...)`: **Safe and Standard**. Guaranteed to null-terminate (if `size > 0`) and returns the number of characters that would have been written.
3. **Comparison and Searching**:
   - `int strcmp(const char *s1, const char *s2)` / `strncmp(...)`: Lexicographical comparison.
   - `char* strchr(const char *s, int c)`: Finds first occurrence of character `c`.
   - `char* strstr(const char *haystack, const char *needle)`: Finds first occurrence of substring.
4. **Tokenization**:
   - `char* strtok(char *str, const char *delim)`: Destructively tokenizes strings using hidden internal static state (non-reentrant, thread-unsafe).

---

## 3. How It Works Internally

### The `strncpy` Flaw vs. `snprintf` Bounds Guarantees

```
Buffer Layout comparison (Destination Buffer Capacity = 6 Bytes, Input = "HELLOWORLD")

1. strcpy(dest, "HELLOWORLD"):
┌───┬───┬───┬───┬───┬───┐ ──► Smashes stack frame!
│ H │ E │ L │ L │ O │ W │ O │ R │ L │ D │\0 │
└───┴───┴───┴───┴───┴───┘

2. strncpy(dest, "HELLOWORLD", 6):
┌───┬───┬───┬───┬───┬───┐ ──► MISSING NULL TERMINATOR!
│ H │ E │ L │ L │ O │ W │ (Subsequent strlen/printf causes read overflow)
└───┴───┴───┴───┴───┴───┘

3. snprintf(dest, 6, "%s", "HELLOWORLD"):
┌───┬───┬───┬───┬───┬───┐ ──► SAFE: Always null-terminated!
│ H │ E │ L │ L │ O │\0 │ (Returns 10, signaling truncation occurred)
└───┴───┴───┴───┴───┴───┘
```

---

## 4. Syntax & Usage

```c
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

/* Robust string copy idiom using snprintf */
bool safe_copy(char *dest, size_t dest_size, const char *src) {
    if (!dest || dest_size == 0) return false;
    if (!src) {
        dest[0] = '\0';
        return true;
    }
    int needed = snprintf(dest, dest_size, "%s", src);
    /* If needed >= dest_size, string was truncated */
    return (needed >= 0 && (size_t)needed < dest_size);
}
```

> [!NOTE]
> ### ISO C11 Annex K ("Safe" Functions) vs. Reality
> ISO C11 introduced optional Annex K "Bounds-checking interfaces" (`strcpy_s`, `strcat_s`). However, Annex K is widely regarded as an architectural failure:
> - Major platforms (Linux glibc) explicitly refuse to implement it.
> - Microsoft's MSVC implementation deviates from the standard specification.
> Systems engineers universally prefer standard `snprintf()` or BSD `strlcpy()`/`strlcat()` over Annex K.

---

## 5. Practical Examples

### Complete Demonstration Program
The following program demonstrates truncation detection with `snprintf`, substring searching with `strstr`, and token parsing with `strtok`.

```c
#include <stdio.h>
#include <string.h>

int main(void) {
    printf("=== C string.h Functions and Safe Alternatives Demo ===\n");

    /* 1. Safe snprintf bounds truncation check */
    char buffer[16];
    const char *long_str = "Standard C Systems Programming";
    int needed = snprintf(buffer, sizeof buffer, "%s", long_str);
    printf("Destination buffer (cap %zu): [%s]\n", sizeof buffer, buffer);
    if (needed >= (int)sizeof buffer) {
        printf("Detected truncation! Needed %d chars (including null terminator: %d)\n",
               needed, needed + 1);
    }

    /* 2. Searching with strchr and strstr */
    const char *haystack = "kernel_panic_recovery_flag=true";
    const char *sub = strstr(haystack, "recovery");
    if (sub) {
        printf("Found substring '%s' at offset: %td\n", "recovery", sub - haystack);
    }

    /* 3. Tokenization with strtok */
    char csv_line[] = "alice,admin,1001,active";
    printf("Parsing CSV tokens:\n");
    char *tok = strtok(csv_line, ",");
    while (tok) {
        printf("  Token: [%s]\n", tok);
        tok = strtok(NULL, ",");
    }

    return 0;
}
```

```text
=== C string.h Functions and Safe Alternatives Demo ===
Destination buffer (cap 16): [Standard C Syst]
Detected truncation! Needed 30 chars (including null terminator: 31)
Found substring 'recovery' at offset: 13
Parsing CSV tokens:
  Token: [alice]
  Token: [admin]
  Token: [1001]
  Token: [active]
```

### Real-World Example 2: BSD `strlcpy` and `strlcat` Reference Implementation
For non-POSIX or older environments lacking BSD extensions, the reference implementation guarantees null-termination and detects truncation:

```c
#include <stddef.h>
#include <string.h>

/* Bounded copy guaranteed to null-terminate if size > 0 */
size_t my_strlcpy(char *dst, const char *src, size_t size) {
    size_t srclen = strlen(src);
    if (size > 0) {
        size_t copylen = (srclen >= size) ? size - 1 : srclen;
        memcpy(dst, src, copylen);
        dst[copylen] = '\0';
    }
    return srclen; /* Truncation detected if return value >= size */
}

/* Bounded concatenation guaranteed to null-terminate if size > dstlen */
size_t my_strlcat(char *dst, const char *src, size_t size) {
    size_t dstlen = strnlen(dst, size);
    size_t srclen = strlen(src);
    if (dstlen >= size) return size + srclen;
    
    size_t copylen = (dstlen + srclen >= size) ? size - dstlen - 1 : srclen;
    memcpy(dst + dstlen, src, copylen);
    dst[dstlen + copylen] = '\0';
    return dstlen + srclen;
}
```

### Real-World Example 3: Constant-Time String Comparison (Timing-Attack Resistant)
Standard `strcmp` terminates early on the first mismatched byte, creating timing side-channels in authentication systems. Cryptographic code uses constant-time comparison:

```c
#include <stddef.h>

/* Returns 0 if identical, non-zero if different, executing in constant time */
int safe_memcmp_const_time(const void *a, const void *b, size_t len) {
    const unsigned char *pa = (const unsigned char *)a;
    const unsigned char *pb = (const unsigned char *)b;
    unsigned char result = 0;
    
    for (size_t i = 0; i < len; ++i) {
        result |= (pa[i] ^ pb[i]);
    }
    return (int)result;
}
```


---

## 6. Best Practices

1. **Use `snprintf()` for Bounded Copying and Formatting**: `snprintf(dest, sizeof dest, "%s", src)` guarantees null-termination and detects truncation when `ret >= sizeof dest`.
2. **Never Use `strcpy()` or `strcat()`**: Ban them in static analysis rules (`-Wdeprecated-declarations`, `-D_FORTIFY_SOURCE=2`).
3. **Beware of `strncpy()` Nuances**: Remember that `strncpy` will **not** append a null byte if the source length equals or exceeds $n$. If using it, manually force `dest[n - 1] = '\0'`.
4. **Prefer `strtok_r()` for Reentrant Tokenization**: In multithreaded environments or nested parsing loops, standard `strtok()` corrupts token state because it uses an internal static pointer. Use POSIX `strtok_r()` or `strspn()`/`strcspn()`.
5. **Remember `strlen()` is $O(N)$**: Never write `for (size_t i = 0; i < strlen(s); i++)`. Calling `strlen` in a loop condition turns an $O(N)$ traversal into a disastrous $O(N^2)$ quadratic slowdown.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It's Wrong | The Fix |
| :--- | :--- | :--- |
| **Assuming `strncpy` null-terminates** | If source string length $\ge n$, `dest` is left unterminated, causing read overflows. | Set `dest[sizeof(dest) - 1] = '\0'` or use `snprintf`. |
| **Quadratic `strlen` in loop condition** | `for (int i=0; i < strlen(s); i++)` recalculates length on every iteration ($O(N^2)$). | Cache length before loop: `size_t len = strlen(s); for (...)`. |
| **Using `strtok()` in multithreaded code** | `strtok()` stores internal state in a global static variable; concurrent calls clobber state. | Use `strtok_r()` (POSIX) or custom pointer cursor parsing. |
| **Passing string literal to `strtok()`** | `strtok()` mutates the input string by writing `'\0'` delimiters. Literals are in read-only `.rodata` (`SIGSEGV`). | Copy the string literal to a mutable `char[]` buffer first. |
| **Off-by-one in buffer capacity** | Allocating `malloc(strlen(s))` forgets the null terminator byte `'\0'`, causing 1-byte heap corruption. | Always allocate `malloc(strlen(s) + 1)`. |
| **Misinterpreting `strcmp()` return** | `strcmp(a, b)` returns `0` on equality. Testing `if (strcmp(a, b))` evaluates to false when strings match! | Test `if (strcmp(a, b) == 0)`. |

---

## 8. Performance & Security Notes

### The Morris-Worm Legacy of `strcpy`
The very first Internet worm (the 1988 Morris Worm) exploited a buffer overflow in the UNIX `fingerd` daemon caused by unchecked `gets()` and `strcpy()`. Decades later, string buffer overflows remain a primary attack vector for arbitrary code execution. Modern compilers provide `-D_FORTIFY_SOURCE=2` to automatically replace calls to `strcpy` with bounds-checked variants at compile time.

### SIMD Acceleration in Modern `strlen`
Standard C libraries implement `strlen()`, `strcpy()`, and `memchr()` using 128-bit SSE2 or 256-bit AVX2 vector instructions. By checking 16 or 32 bytes simultaneously for the null byte `0x00` using bitwise masks (like `_mm256_cmpeq_epi8`), modern systems scan strings at memory bus bandwidth speeds ($> 20 \text{ GB/sec}$).

---

## 9. Comparison: When to Use What

| Function | Guaranteed Null-Term? | Returns Truncation Info? | Thread Safe? | Standard Portability |
| :--- | :--- | :--- | :--- | :--- |
| **`strcpy`** | No (Buffer overflow) | No | Yes | ISO C89+ (UNSAFE) |
| **`strncpy`** | No (If $len \ge n$) | No | Yes | ISO C89+ |
| **`snprintf`** | Yes (If $size > 0$) | Yes (`ret >= size`) | Yes | ISO C99+ |
| **`strlcpy`** | Yes (If $size > 0$) | Yes (`ret >= size`) | Yes | OpenBSD / POSIX (Not ISO C) |
| **`strtok`** | Destructive | N/A | **No** (Global state) | ISO C89+ |
| **`strtok_r`** | Destructive | N/A | Yes | POSIX.1-2001 |

---

## 10. Interview Questions

<details>
<summary><strong>1. Why is <code>strncpy</code> considered an inefficient and flawed string function?</strong></summary>

1. It does not guarantee null-termination if the source length is equal to or greater than the destination buffer capacity.
2. If the source string is shorter than $n$, `strncpy` zero-pads the remaining bytes up to $n$. When copying a 5-byte string into a 4096-byte buffer, it unnecessarily writes 4091 zero bytes to memory, destroying performance.
</details>

<details>
<summary><strong>2. What does <code>snprintf</code> return when the output is truncated?</strong></summary>

`snprintf(buf, size, fmt, ...)` returns the total number of characters (excluding the null terminator) that **would have been written** if the buffer had been sufficiently large. Truncation is reliably detected when `return_value >= size`.
</details>

<details>
<summary><strong>3. How does <code>strtok()</code> maintain state between calls without receiving a pointer?</strong></summary>

`strtok()` stores a private pointer to the remaining string in an internal file-scope static variable inside libc. When passed `NULL` on subsequent calls, it resumes scanning from this saved static pointer. This design makes `strtok()` non-reentrant and dangerous in multithreaded programs.
</details>

<details>
<summary><strong>4. What is the difference between <code>memcmp()</code> and <code>strcmp()</code>?</strong></summary>

`strcmp(s1, s2)` stops comparing as soon as it encounters a null terminator byte `'\0'`.
`memcmp(b1, b2, n)` compares exactly $n$ bytes regardless of whether null bytes are present, making it suitable for arbitrary binary data and structures.
</details>

<details>
<summary><strong>5. Why is <code>if (strcmp(s1, s2))</code> an anti-pattern?</strong></summary>

In C boolean logic, `0` is false and non-zero is true. `strcmp` returns `0` when strings are identical. Therefore, `if (strcmp(s1, s2))` evaluates to true when strings **differ**, which is unintuitive and leads to inverted logic bugs. Write `if (strcmp(s1, s2) == 0)` explicitly.
</details>

<details>
<summary><strong>6. What does <code>strspn()</code> and <code>strcspn()</code> do?</strong></summary>

- `strspn(s, accept)`: Returns the length of the initial segment of `s` containing only characters present in `accept`.
- `strcspn(s, reject)`: Returns the length of the initial segment of `s` containing no characters from `reject`. Commonly used to locate newlines: `s[strcspn(s, "\r\n")] = '\0'`.
</details>

<details>
<summary><strong>7. How does <code>strdup()</code> work and who is responsible for the allocated memory?</strong></summary>

`strdup(s)` computes the length of `s`, allocates `strlen(s) + 1` bytes on the heap via `malloc()`, copies the string, and returns the pointer. The caller is responsible for freeing the memory via `free()`. Standardized in POSIX and ISO C23.
</details>

<details>
<summary><strong>8. Why does passing a string literal to <code>strtok()</code> cause a Segmentation Fault?</strong></summary>

`strtok()` modifies its input buffer in-place by writing null bytes (`'\0'`) over delimiter characters. String literals are stored in read-only memory pages (`.rodata`). Attempting to overwrite a literal triggers an MMU hardware page write violation (`SIGSEGV`).
</details>

---

## 11. Practice Exercises

### Exercise 1: Clean Newline Stripper (Easy)
Write a function `void strip_newline(char *str)` using `strcspn` that replaces trailing `\r` and `\n` characters with `\0`.

<details>
<summary>View Solution</summary>

```c
#include <string.h>

void strip_newline(char *str) {
    if (str) {
        str[strcspn(str, "\r\n")] = '\0';
    }
}
```
</details>

### Exercise 2: Safe String Concatenation (Medium)
Implement `bool safe_strcat(char *dest, size_t dest_cap, const char *src)` using `strlen` and `snprintf` that safely appends `src` to `dest` without buffer overflow.

<details>
<summary>View Solution</summary>

```c
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

bool safe_strcat(char *dest, size_t dest_cap, const char *src) {
    if (!dest || !src || dest_cap == 0) return false;
    size_t cur_len = strlen(dest);
    if (cur_len >= dest_cap) return false;

    int written = snprintf(dest + cur_len, dest_cap - cur_len, "%s", src);
    return (written >= 0 && (size_t)written < dest_cap - cur_len);
}
```
</details>

### Exercise 3: Thread-Safe Reentrant String Splitter (Hard)
Implement your own reentrant string tokenizer `char* my_strtok_r(char *str, const char *delim, char **saveptr)` without using any standard library tokenization functions.

<details>
<summary>View Solution</summary>

```c
#include <stdio.h>
#include <string.h>

char* my_strtok_r(char *str, const char *delim, char **saveptr) {
    char *s = str ? str : *saveptr;
    if (!s) return NULL;

    /* Skip leading delimiters */
    s += strspn(s, delim);
    if (*s == '\0') {
        *saveptr = NULL;
        return NULL;
    }

    char *token_start = s;
    /* Find end of token */
    s += strcspn(s, delim);
    if (*s != '\0') {
        *s = '\0';
        *saveptr = s + 1;
    } else {
        *saveptr = NULL;
    }

    return token_start;
}
```
</details>

---

## 12. Summary & Cheat Sheet

```c
/* String length */
size_t len = strlen(s);              /* O(N) traversal */
size_t bounded = strnlen(s, max);    /* Safe length check */

/* Safe bounds copy */
int needed = snprintf(buf, sizeof buf, "%s", src);
if (needed >= (int)sizeof buf) { /* Truncation occurred */ }

/* Searching */
char *p = strchr(s, 'x');            /* Find first char */
char *sub = strstr(haystack, needle);/* Find substring */

/* Comparison */
if (strcmp(a, b) == 0) { /* Identical */ }
if (strncmp(a, b, n) == 0) { /* First n match */ }
```

---

## 13. Further Reading
- ISO/IEC 9899:2018 (C17 Standard), §7.24 "String handling `<string.h>`".
- ISO/IEC 9899:2024 (C23 Standard), §7.24 "String handling" (C23 `strdup`, `strndup`).
- SEI CERT C Coding Standard: "STR31-C: Guarantee that storage for strings has sufficient space for character data and the null terminator".
- US-CERT Advisory: *Historic Buffer Overflow Vulnerabilities in Standard C String Routines*.

---

[← Previous: Dynamic Arrays & Realloc](./14-dynamic-arrays-and-realloc.md) | [Section Index](./README.md) | [Next: Bit-Fields →](./16-bit-fields.md)
