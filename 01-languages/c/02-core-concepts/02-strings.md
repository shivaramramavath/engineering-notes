# String Architecture and Safe Manipulation Idioms

> A comprehensive reference on C string mechanics: null-terminated byte strings (NTBS), memory placement (mutable arrays vs read-only string literals in `.rodata`), standard library functions (`strlen`, `strcmp`), bounds-safe alternatives (`snprintf`, `strlcpy`), and UTF-8 handling.

| Metadata | Details |
|---|---|
| **Level** | Beginner → Intermediate |
| **Prerequisites** | [01-arrays.md](./01-arrays.md), [01-fundamentals/12-const-correctness-basics.md](../01-fundamentals/12-const-correctness-basics.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **Standard Baseline** | ISO C11 / C17 / C23 |

---

## Table of Contents
- [1. What & Why](#1-what--why)
- [2. Core Concepts](#2-core-concepts)
- [3. How It Works Internally](#3-how-it-works-internally)
- [4. Syntax & Usage](#4-syntax--usage)
- [5. Practical Examples](#5-practical-examples)
- [6. Best Practices](#6-best-practices)
- [7. Common Mistakes & Pitfalls](#7-common-mistakes--pitfalls)
- [8. Performance & Security Notes](#8-performance--security-notes)
- [9. Comparison / When to Use What](#9-comparison--when-to-use-what)
- [10. Interview Questions](#10-interview-questions)
- [11. Practice Exercises](#11-practice-exercises)
- [12. Summary / Cheat Sheet](#12-summary--cheat-sheet)
- [13. Further Reading](#13-further-reading)

---

## 1. What & Why

In C, there is no native primitive `string` data type. A "string" in C is defined by the ISO standard as a **Null-Terminated Byte String (NTBS)**: a contiguous sequence of characters terminating with a null byte (`'\0'`, ASCII value `0`).

This sentinel-byte design has profound engineering consequences:
1. **The Source of Historic Security Disasters**: Because string length is not tracked alongside the buffer, functions like `strcpy()`, `strcat()`, and `sprintf()` read and write memory until they happen to find a `\0` byte. Over the past four decades, unbounded string manipulation has accounted for the majority of exploitable buffer overflow vulnerabilities in systems software.
2. **$O(N)$ Traversal Overhead**: Determining string length via `strlen()` requires linear scanning of every single byte from index zero until `\0` is found. Naive concatenation loops that repeatedly call `strlen` degrade into quadratic $O(N^2)$ performance!
3. **Mutable Arrays vs Read-Only Literals**: Declaring `char *s = "hello";` creates a pointer to write-protected memory in `.rodata`. Modifying `s[0]` triggers an immediate crash.

---

## 2. Core Concepts

### Memory Representation of a C String

A string consists of printable character bytes followed immediately by the sentinel delimiter:

```
String: "Hello"
Characters:    'H'    'e'    'l'    'l'    'o'    '\0'
Hex Bytes:    0x48   0x65   0x6C   0x6C   0x6F   0x00
Array Index:  [ 0 ]  [ 1 ]  [ 2 ]  [ 3 ]  [ 4 ]  [ 5 ]
Total Length (strlen): 5 characters
Total Memory Footprint (sizeof): 6 bytes (including terminating null byte!)
```

### Character Array vs. String Literal Pointer

```c
/* Case 1: Mutable Character Array (Stack Allocation) */
char str_array[] = "Hello";
str_array[0] = 'M'; /* VALID: 'str_array' is allocated on the stack; becomes "Mello" */

/* Case 2: Read-Only String Literal Pointer (.rodata Allocation) */
const char *str_literal = "Hello";
/* str_literal[0] = 'M'; <-- COMPILER ERROR (or runtime SIGSEGV if cast away!) */
```

```
+-------------------------------------------------------------+
|                     STRING MEMORY LOCATION                  |
+-------------------------------------------------------------+
|  char str[] = "Hello";                                      |
|  [ Stack Frame Memory ] -> Allocated, Mutable, Copied at run|
|                                                             |
|  const char *p = "Hello";                                   |
|  [ .rodata Segment ]    -> Write-Protected, Shared, Static  |
+-------------------------------------------------------------+
```

---

## 3. How It Works Internally

### The Mechanics of Classic vs. Safe String Copying

```
1. Unsafe strcpy(dest, src):
   Scans src and writes to dest until '\0' is found.
   Zero bounds checks! Dest overflows into return address if src > dest capacity!

2. Flawed strncpy(dest, src, n):
   Copies at most n bytes.
   FATAL FLAW: If src >= n, it DOES NOT NULL-TERMINATE dest! Dest is left corrupt.

3. Robust snprintf(dest, sizeof(dest), "%s", src):
   Guarantees null termination!
   Detects truncation if return >= sizeof(dest).

4. BSD / POSIX strlcpy(dest, src, sizeof(dest)):
   Copies up to size - 1, guarantees null-termination, returns strlen(src).
```

### UTF-8 Multi-Byte Character Architecture

Standard C strings operate on raw 8-bit bytes (`char`). In modern internationalized software, characters are encoded using **UTF-8**:
- ASCII characters (A-Z, 0-9) occupy 1 byte (`0x00` - `0x7F`).
- Accented characters, Greek, Cyrillic occupy 2 bytes.
- Asian characters (CJK) occupy 3 bytes.
- Emojis occupy 4 bytes.

*Crucial Reality*: In UTF-8, `strlen("🚀")` returns **4 bytes**, NOT 1 character! String indexing `s[i]` indexes raw bytes, not visual characters (grapheme clusters).

---

## 4. Syntax & Usage

### 1. Robust String Copying with `snprintf`
```c
#include <stdio.h>
#include <stdbool.h>

bool safe_string_copy(char *dest, size_t dest_size, const char *src) {
    if (!dest || !src || dest_size == 0) return false;

    int written = snprintf(dest, dest_size, "%s", src);
    if (written < 0 || (size_t)written >= dest_size) {
        /* Truncation occurred */
        return false;
    }
    return true;
}
```

### 2. String Comparison with `strcmp`
```c
#include <string.h>

void evaluate_strings(const char *s1, const char *s2) {
    int res = strcmp(s1, s2);
    if (res == 0) {
        printf("Strings are lexicographically EQUAL.\n");
    } else if (res < 0) {
        printf("s1 precedes s2 in ASCII sort order.\n");
    } else {
        printf("s1 succeeds s2 in ASCII sort order.\n");
    }
}
```

---

## 5. Practical Examples

### String Mechanics and Safe Formatting Demonstration Program

```c
/* File: 01-languages/c/02-core-concepts/examples/02-strings.c */
#include <stdio.h>
#include <string.h>

int main(void) {
    printf("=== Safe String Mechanics Demo ===\n");
    
    /* String literal in .rodata */
    const char *greeting = "Hello, Systems C!";
    printf("String: '%s' (Length: %zu, Size with null: %zu)\n", 
           greeting, strlen(greeting), sizeof("Hello, Systems C!"));

    /* Safe bounded concatenation with snprintf */
    char dest[24];
    int written = snprintf(dest, sizeof(dest), "%s -> v%d", "Kernel", 6);
    printf("Formatted: '%s' (Written: %d, Cap: %zu)\n", dest, written, sizeof(dest));

    return 0;
}
```

#### Compilation & Execution Output
```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -Werror 02-strings.c -o run_02.exe
./run_02.exe
```
Output:
```text
=== Safe String Mechanics Demo ===
String: 'Hello, Systems C!' (Length: 17, Size with null: 18)
Formatted: 'Kernel -> v6' (Written: 12, Cap: 24)
```

---

## 6. Best Practices

1. **Always Declare String Literals as `const char *`**: Prevent accidental writes to read-only `.rodata` memory pages.
2. **Never Use `strcpy()` or `strcat()`**: Ban these functions completely from your codebase via compiler flags or linters.
3. **Always Account for the Terminating Null Byte**: When allocating memory for a string of length $L$, always allocate $L + 1$ bytes: `malloc(strlen(s) + 1)`.
4. **Beware of `strncpy()` Flaws**: If you must use `strncpy()`, always manually null-terminate: `dest[sizeof(dest) - 1] = '\0'`.
5. **Never Compare Strings with `==`**: Writing `if (str1 == str2)` compares memory pointer addresses, NOT character contents! Always use `strcmp()`.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It's Wrong | Conforming Fix |
|---|---|---|
| Comparing strings via `if (s1 == s2)` | Compares pointer memory addresses, not string text. | Use `if (strcmp(s1, s2) == 0)`. |
| Allocating string memory as `malloc(strlen(s))` | Forgets the terminating null byte `\0`; writes corrupt adjacent heap memory. | Allocate `malloc(strlen(s) + 1)`. |
| Modifying a string literal (`char *s = "hi"; s[0] = 'H';`) | String literals live in `.rodata`; triggers immediate hardware `SIGSEGV` crash. | Declare as mutable array: `char s[] = "hi";`. |
| Calling `strcat()` in a loop | Scans the destination string from the beginning on every call, causing $O(N^2)$ quadratic slowdown. | Track write pointer offset or use `snprintf()`. |
| Assuming `strncpy()` null-terminates | If source length $\ge$ size, `strncpy` leaves destination non-null-terminated! | Set `dest[size - 1] = '\0'` explicitly. |
| Assuming `sizeof(str_ptr)` gives string length | Returns pointer size (4 or 8 bytes), NOT string length! | Use `strlen(str_ptr)`. |

---

## 8. Performance & Security Notes

- **Quadratic String Concatenation ("Schlemiel the Painter's Algorithm")**: Calling `strcat(dest, src)` scans `dest` from index 0 until `\0`. Appending 1,000 strings in a loop rescans previously written characters repeatedly, burning millions of CPU cycles. Always maintain an end-pointer: `ptr += sprintf(ptr, ...)` or use dynamic string buffers.
- **Buffer Overflow Overwriting Stack Canaries**: Unbounded string copies overwrite stack frames, triggering `__stack_chk_fail` exceptions or remote shellcode execution. Compile with `-fstack-protector-strong`.

---

## 9. Comparison / When to Use What

| Function | Destination Bounds Checked? | Guarantees `\0` Termination? | Detection of Truncation? |
|---|---|---|---|
| `strcpy(dst, src)` | **NO** | Only if space permits | None |
| `strncpy(dst, src, n)` | Partial (Max $n$) | **NO (Fatal flaw)** | Difficult |
| `strlcpy(dst, src, n)` | **YES** | **YES** | Return $\ge n$ |
| `snprintf(dst, n, "%s", src)` | **YES** | **YES** | Return $\ge n$ |

---

## 10. Interview Questions

<details>
<summary><strong>1. Why is `char *s = "hello";` dangerous compared to `const char *s = "hello";`?</strong></summary>

String literals are placed in the read-only `.rodata` segment by the compiler. While historical C allowed `char *s` for backward compatibility, attempting to mutate `s[0] = 'H'` triggers an immediate hardware segmentation fault at runtime. Declaring it `const char *` allows the compiler to catch mutation attempts at compile time.
</details>

<details>
<summary><strong>2. Why is `strncpy()` considered flawed for safe string copying?</strong></summary>

If the source string's length is greater than or equal to the specified size parameter `n`, `strncpy()` **does not append a terminating null byte `\0`**. The destination buffer is left as an unterminated character array. If subsequently passed to `strlen()` or `printf("%s")`, an out-of-bounds read occurs.
</details>

<details>
<summary><strong>3. What is the difference between `sizeof("Hello")` and `strlen("Hello")`?</strong></summary>

- `strlen("Hello")` scans the characters at runtime and returns `5` (the count of characters preceding the null terminator).
- `sizeof("Hello")` is evaluated at compile time and returns `6` (the full size in bytes of the array, including the terminating null byte `\0`).
</details>

<details>
<summary><strong>4. How does `strcmp(s1, s2)` determine equality and ordering?</strong></summary>

`strcmp` compares the characters of both strings byte-by-byte as `unsigned char`. It returns:
- `0` if all characters match up to and including the terminating null byte.
- A negative integer if the first differing character in `s1` is less than in `s2`.
- A positive integer if the first differing character in `s1` is greater than in `s2`.
</details>

<details>
<summary><strong>5. What is "Schlemiel the Painter's Algorithm" in C string manipulation?</strong></summary>

An algorithmic anti-pattern where a program repeatedly scans string data from the beginning to find the end (such as calling `strcat()` in a loop). Because `strcat` traverses the entire accumulated string on each iteration, concatenating $N$ segments scales quadratically ($O(N^2)$) rather than linearly ($O(N)$).
</details>

<details>
<summary><strong>6. Why does `strlen()` return `size_t` rather than `int`?</strong></summary>

`size_t` is the unsigned integer type guaranteed to be wide enough to hold the size of any object in memory. On 64-bit systems, a string could mathematically exceed $2^{31}-1$ bytes (the limit of signed `int`), which would cause integer overflow if returned as `int`.
</details>

<details>
<summary><strong>7. How does UTF-8 encoding affect string length calculations in C?</strong></summary>

In standard C, `char` is an 8-bit byte. In UTF-8, non-ASCII characters occupy between 2 and 4 bytes. Therefore, `strlen()` returns the number of **bytes**, not the number of human-readable characters. A string containing a single emoji `"😀"` has a `strlen` of 4.
</details>

<details>
<summary><strong>8. What is the difference between `char str[] = "hello";` and `char *str = "hello";`?</strong></summary>

`char str[]` allocates an array on the execution stack and copies the bytes `"hello\0"` into it; the elements are mutable and can be modified. `char *str` creates a pointer pointing to the string literal stored directly in the read-only `.rodata` segment; modifying its elements causes a segmentation fault.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): In-Place String Reversal</strong></summary>

**Goal**: Implement a function `void reverse_string(char *s)` that reverses a null-terminated mutable character string in place.

**Solution**:
```c
#include <string.h>

void reverse_string(char *s) {
    if (!s) return;
    size_t len = strlen(s);
    if (len <= 1) return;

    size_t left = 0;
    size_t right = len - 1;
    while (left < right) {
        char temp = s[left];
        s[left] = s[right];
        s[right] = temp;
        left++;
        right--;
    }
}
```
</details>

<details>
<summary><strong>Exercise 2 (Medium): Safe Bounded String Concatenation</strong></summary>

**Goal**: Implement a safe string append function `bool safe_strcat(char *dest, size_t dest_cap, const char *src)` that guarantees null-termination and detects overflow.

**Solution**:
```c
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

bool safe_strcat(char *dest, size_t dest_cap, const char *src) {
    if (!dest || !src || dest_cap == 0) return false;

    size_t current_len = strlen(dest);
    if (current_len >= dest_cap) return false;

    int written = snprintf(dest + current_len, dest_cap - current_len, "%s", src);
    if (written < 0 || (size_t)written >= (dest_cap - current_len)) {
        return false; /* Truncation occurred */
    }
    return true;
}
```
</details>

<details>
<summary><strong>Exercise 3 (Hard): Linear-Time Multi-String Concatenation Buffer</strong></summary>

**Goal**: Author a dynamic string builder that concatenates an array of strings in linear $O(N)$ time by maintaining an active write pointer.

**Solution**:
```c
#include <stdlib.h>
#include <string.h>

char *concat_all(const char **strings, size_t count) {
    if (!strings || count == 0) return NULL;

    /* Pass 1: Compute total length */
    size_t total_len = 0;
    for (size_t i = 0; i < count; ++i) {
        if (strings[i]) {
            total_len += strlen(strings[i]);
        }
    }

    char *result = malloc(total_len + 1);
    if (!result) return NULL;

    /* Pass 2: Stream copy in O(N) linear time */
    char *write_ptr = result;
    for (size_t i = 0; i < count; ++i) {
        if (strings[i]) {
            size_t len = strlen(strings[i]);
            memcpy(write_ptr, strings[i], len);
            write_ptr += len;
        }
    }
    *write_ptr = '\0';
    return result;
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Strings are NTBS**: Character sequences ending in `'\0'`.
- `char s[] = "hi"` is mutable (stack); `const char *s = "hi"` is immutable (`.rodata`).
- Always allocate `strlen(s) + 1` for the terminating null byte.
- Never use `strcpy()` or `strcat()`; use `snprintf(dest, sizeof(dest), "%s", src)`.
- Never compare strings with `==`; use `strcmp(s1, s2) == 0`.
- UTF-8 characters occupy 1 to 4 bytes; `strlen()` measures bytes, not visual characters.

---

## 13. Further Reading

- ISO/IEC 9899:2018 (C17 Standard), Section 7.24: "String handling `<string.h>`".
- SEI CERT C Coding Standard: *STR31-C. Guarantee that storage for strings has sufficient space for character data and the null terminator*.
- Joel Spolsky: *The Absolute Minimum Every Software Developer Absolutely, Positively Must Know About Unicode and Character Sets*.

---

[← Previous: 01-arrays.md](./01-arrays.md) | [Section Index](./README.md) | [Next: 03-pointers.md](./03-pointers.md) →
