# Standard Input and Output (I/O) Streams

> A comprehensive deep dive into the C standard I/O library (`<stdio.h>`): streams (`stdin`, `stdout`, `stderr`), file buffering modes (`_IOFBF`, `_IOLBF`, `_IONBF`), bounds-safe formatted output (`snprintf`), robust line parsing (`fgets` + `sscanf`), and C11 Annex K bounds-checking interfaces.

| Metadata | Details |
|---|---|
| **Level** | Beginner → Intermediate |
| **Prerequisites** | [08-functions.md](./08-functions.md), [02-data-types.md](./02-data-types.md) |
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

In C, input and output operations are not language keywords; they are provided by the C Standard Library via `<stdio.h>`. Input and output in C are modeled as continuous byte **streams** abstracted over hardware devices, network sockets, and filesystem files.

Understanding standard I/O architecture is critical for systems developers:
1. **Buffer Overflow Catastrophes**: Historical functions like `gets()` and unconstrained `scanf("%s", buf)` read unbounded bytes into fixed buffers, granting attackers trivial remote code execution exploits. `gets()` was formally **removed** from standard C in C11.
2. **Buffering Invisibility & Deadlocks**: Standard output (`stdout`) is buffered in user space. If a program crashes or halts without flushing the buffer, log outputs are lost, misdiagnosing the crash location.
3. **Stream Mixing & Error Handling**: `stderr` is unbuffered by default so that diagnostic alarms appear immediately on the console, even if `stdout` redirection is pipelined to a disk file.

---

## 2. Core Concepts

### Standard Streams and Descriptors

Every hosted C program starts with three open standard streams:

| Stream Pointer | Typical POSIX File Descriptor | Default Buffering Mode | Target Destination |
|---|---|---|---|
| `stdin` | `0` (`STDIN_FILENO`) | Fully Buffered (if file) / Line Buffered (if terminal) | Keyboard / Pipe Input |
| `stdout` | `1` (`STDOUT_FILENO`)| Line Buffered (if terminal) / Fully Buffered (if redirected) | Terminal Screen / Pipe Output |
| `stderr` | `2` (`STDERR_FILENO`)| **Unbuffered (`_IONBF`)** | Error Console / Diagnostic Log |

```
+-------------------------------------------------------------------------+
|                         C I/O STREAM BUFFERING                          |
+-------------------------------------------------------------------------+
|  User Application: printf("Log...")                                     |
|       |                                                                 |
|       v                                                                 |
|  [ User-Space FILE Buffer in C Standard Library ]                       |
|       |                                                                 |
|       | Flush condition met (newline \n, buffer full 4096, fflush())   |
|       v                                                                 |
|  [ OS Kernel Buffer: write(2) system call ]                             |
|       |                                                                 |
|       v                                                                 |
|  [ Hardware: Disk Controller / Terminal Display / Network NIC ]         |
+-------------------------------------------------------------------------+
```

### The Three Buffering Modes

Configured via `setvbuf(FILE *stream, char *buffer, int mode, size_t size)`:
1. **`_IOFBF` (Fully Buffered)**: Data is accumulated until the internal buffer (typically 4096 or 8192 bytes) is completely filled before invoking a kernel `write()` system call.
2. **`_IOLBF` (Line Buffered)**: Data is flushed to the kernel as soon as a newline character (`\n`) is written or when input is requested from `stdin`.
3. **`_IONBF` (Unbuffered)**: Every individual byte or formatted string is written immediately to the kernel without intervening user-space accumulation (`stderr` default).

---

## 3. How It Works Internally

### `snprintf` vs `sprintf` Mechanics

`sprintf(buffer, "%s", input)` writes without checking destination boundaries. If `input` exceeds `buffer`, memory corruption is immediate.

`snprintf(buffer, size, format, ...)` guarantees safety:
- It writes at most `size - 1` characters, followed by a terminating null byte `\0`.
- **Return Value**: It returns the **total number of characters that would have been written** if `size` had been sufficiently large (excluding `\0`).
- If `ret >= size`, output was truncated! This allows callers to detect truncation and allocate larger buffers dynamically:

```c
size_t needed = (size_t)snprintf(NULL, 0, "%s: %d", name, id) + 1;
char *buf = malloc(needed);
snprintf(buf, needed, "%s: %d", name, id);
```

---

## 4. Syntax & Usage

### 1. Robust Line Reading with `fgets`
Never use `scanf("%s")` or `gets()` for user input. The canonical safe idiom is `fgets()` followed by newline removal:

```c
#include <stdio.h>
#include <string.h>

int read_line(char *buffer, size_t capacity) {
    if (!fgets(buffer, (int)capacity, stdin)) {
        return -1; /* EOF or error */
    }
    /* Strip trailing newline character if present */
    size_t len = strlen(buffer);
    if (len > 0 && buffer[len - 1] == '\n') {
        buffer[len - 1] = '\0';
    } else {
        /* Line was truncated; clear excess characters from stdin stream */
        int ch;
        while ((ch = getchar()) != '\n' && ch != EOF) {}
    }
    return 0;
}
```

### 2. Format Specifier Precision & Width Bounds
```c
/* Specifying maximum field width in scanf prevents buffer overflows! */
char username[16];
/* Reads at most 15 non-whitespace chars, reserving 1 byte for '\0' */
scanf("%15s", username);
```

---

## 5. Practical Examples

### Standard I/O and Safe Formatting Demonstration Program

```c
/* File: 01-languages/c/01-fundamentals/examples/09-input-output.c */
#include <stdio.h>
#include <string.h>

int main(void) {
    printf("=== Standard I/O & Formatting Demo ===\n");
    
    /* snprintf bounds-checked formatting */
    char buffer[32];
    int bytes_needed = snprintf(buffer, sizeof(buffer), "DeviceID: %04X, State: %s", 0x42A, "READY");
    printf("Formatted buffer: '%s' (Bytes needed: %d, Buffer capacity: %zu)\n", 
           buffer, bytes_needed, sizeof(buffer));

    /* Simulated safe line parsing via sscanf */
    const char *csv_line = "SensorAlpha,24.5,1013";
    char name[16];
    float temp;
    int pressure;
    int matched = sscanf(csv_line, "%15[^,],%f,%d", name, &temp, &pressure);
    printf("Parsed %d fields -> Sensor: %s, Temp: %.1f C, Pressure: %d hPa\n",
           matched, name, (double)temp, pressure);

    return 0;
}
```

#### Compilation & Execution Output
```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -Werror 09-input-output.c -o run_09.exe
./run_09.exe
```
Output:
```text
=== Standard I/O & Formatting Demo ===
Formatted buffer: 'DeviceID: 042A, State: READY' (Bytes needed: 28, Buffer capacity: 32)
Parsed 3 fields -> Sensor: SensorAlpha, Temp: 24.5 C, Pressure: 1013 hPa
```

---

## 6. Best Practices

1. **Always Use `snprintf` Instead of `sprintf`**: `sprintf` provides zero bounds checking and must be considered obsolete.
2. **Always Check the Return Value of `scanf` and `sscanf`**: `scanf` returns the count of successfully matched and assigned items. If `sscanf(buf, "%d %d", &a, &b) != 2`, input was malformed!
3. **Explicitly Flush Streams Before Calling Fork or System Operations**: Call `fflush(stdout)` before performing interactive input or launching child processes to ensure buffers are emptied.
4. **Log Errors Exclusively to `stderr`**: Writing diagnostic or error output to `stdout` corrupts data pipelines when `stdout` is redirected to JSON or binary output files.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It's Wrong | Conforming Fix |
|---|---|---|
| Using `gets()` | `gets()` has no length limit; causes immediate stack-based buffer overflows. Removed in C11. | Use `fgets(buffer, sizeof(buffer), stdin)`. |
| Unbounded string read via `scanf("%s", buf)` | Reads arbitrary characters until whitespace, overflowing `buf`. | Specify maximum width: `scanf("%31s", buf)`. |
| Forgetting that `fgets()` retains the newline character `\n` | If input fits, `fgets()` stores `\n` right before `\0`, corrupting string lookups and file paths. | Explicitly strip trailing `\n` using `strlen` check. |
| Checking `feof()` before reading from a file stream | `feof()` returns true **only after** an input operation has already attempted to read past EOF and failed! | Check read return value first (`fgets != NULL` or `fread > 0`). |
| Printing untrusted strings as format format: `printf(user_str)` | An attacker entering `%x %s %n` can read stack memory or overwrite return addresses (Format String Vulnerability). | Always use literal format strings: `printf("%s", user_str)`. |
| Passing mismatched types to `printf` (e.g. `%d` for `int64_t`) | Reads incorrect byte sizes from the stack/registers, corrupting subsequent arguments. | Match format specifiers exactly; use `<inttypes.h>`. |

---

## 8. Performance & Security Notes

- **The Format String Vulnerability (CWE-134)**: Passing user input directly as the first argument to `printf` allows attackers to read arbitrary stack memory via `%x` or write arbitrary memory addresses using `%n`. Enable `-Wformat=2 -Werror=format-security` to convert this flaw into a fatal compile error.
- **System Call Overhead**: Every unbuffered read/write (`read(2)`, `write(2)`) incurs a CPU context switch from user space to kernel space (hundreds of clock cycles). Standard I/O buffers (`_IOFBF`) aggregate thousands of character writes into a single bulk kernel operation.

---

## 9. Comparison / When to Use What

| Function | Primary Use Case | Bounds Safety | Return Value |
|---|---|---|---|
| `printf` | Standard formatted output to `stdout` | N/A (Output) | Number of characters printed |
| `fprintf` | Formatted output to specific file or `stderr` | N/A (Output) | Number of characters printed |
| `sprintf` | Formatted string in memory | **Dangerous (Unbounded)** | Characters written |
| `snprintf` | Formatted string with buffer capacity check | **100% Safe** | Characters that would be written |
| `fgets` | Safe line reading from stream | **Safe (Bounded)** | Buffer pointer or `NULL` on EOF |
| `scanf` | Interactive input parsing | Risky (unless width set) | Number of successfully matched fields |

---

## 10. Interview Questions

<details>
<summary><strong>1. Why was `gets()` completely removed from the ISO C11 standard?</strong></summary>

Because `gets()` has no parameter specifying the size of the destination buffer. It is mathematically impossible to use `gets()` safely on arbitrary user input because any input line longer than the allocated buffer inevitably causes a buffer overflow. It was deprecated in C99 and permanently abolished in C11.
</details>

<details>
<summary><strong>2. What is the return value of `snprintf()` when the destination buffer is too small?</strong></summary>

`snprintf()` returns the **total number of characters that would have been written** had the buffer been sufficiently large (excluding the terminating null byte `\0`). If the return value is greater than or equal to the buffer size, output truncation occurred.
</details>

<details>
<summary><strong>3. Why is `while (!feof(fp))` considered an anti-pattern in C?</strong></summary>

`feof()` returns true only **after** a read function attempts to read past the end of the file. Checking `feof()` at the top of a loop means the loop will execute one extra time after the last valid item has been read, processing stale or invalid data. The loop should test the success of the read operation directly (e.g. `while (fgets(...) != NULL)`).
</details>

<details>
<summary><strong>4. How do the buffering modes of `stdout` and `stderr` differ by default?</strong></summary>

When connected to an interactive terminal, `stdout` is **line-buffered** (`_IOLBF`), meaning data is written to the terminal upon encountering a newline `\n`. If `stdout` is redirected to a file, it becomes **fully buffered** (`_IOFBF`). In contrast, `stderr` is **unbuffered** (`_IONBF`) by default in all environments, ensuring diagnostic and crash messages appear immediately.
</details>

<details>
<summary><strong>5. What is a Format String Attack, and how do you prevent it?</strong></summary>

A Format String Attack occurs when user input is supplied directly as the format string argument to a `printf`-family function (e.g. `printf(user_input);`). Attackers supply format specifiers like `%x` to dump stack memory or `%n` to write integers to arbitrary memory addresses. It is prevented by always hardcoding literal format strings: `printf("%s", user_input);`.
</details>

<details>
<summary><strong>6. How does `fgets()` handle newlines compared to `gets()`?</strong></summary>

`gets()` read until a newline and discarded the newline character entirely. `fgets()` reads up to `size - 1` characters or until a newline, but **retains the newline character** in the destination buffer, appending `\0` immediately after it.
</details>

<details>
<summary><strong>7. What does `fflush(stdin)` do according to the ISO C standard?</strong></summary>

According to the ISO C standard (Section 7.21.5.2), calling `fflush()` on an input stream like `stdin` is **undefined behavior**. While some legacy MS-DOS/Windows compilers implemented it to clear the keyboard buffer, standard conforming code must read and discard characters using a `getchar()` loop.
</details>

<details>
<summary><strong>8. What is C11 Annex K (Bounds-checking interfaces)?</strong></summary>

Annex K defines optional bounds-checked functions like `printf_s`, `scanf_s`, `strcpy_s`, and `gets_s`. These functions enforce buffer sizes and call runtime constraint handlers upon error. However, because Annex K is optional (compilers define `__STDC_LIB_EXT1__` only if supported) and widely criticized for design flaws, it is rarely implemented outside Microsoft Visual C++.
</details>

---

<details>
<summary><strong>9. How does POSIX `getline()` compare to standard C `fgets()`?</strong></summary>

Standard C `fgets()` requires a pre-allocated fixed-size buffer, which truncates lines exceeding that capacity. The POSIX `getline(&buf, &capacity, fp)` function automatically resizes the heap buffer using `realloc()` as needed to read arbitrarily long lines without truncation. While standardized in POSIX.1-2008, it is not part of ISO C17 (though widely supported as an extension).
</details>

<details>
<summary><strong>10. What does the return value of `fread(ptr, size, nmemb, fp)` indicate?</strong></summary>

`fread()` returns the **number of full elements (`nmemb`) successfully read**, NOT the number of bytes! If `fread()` returns a value less than `nmemb`, an error occurred or EOF was reached, which must then be distinguished using `ferror(fp)` and `feof(fp)`.
</details>

<details>
<summary><strong>11. Why is `gets_s()` in C11 Annex K not widely adopted?</strong></summary>

Annex K (Bounds-checking interfaces) is optional, and most major standard library implementations (glibc, musl, Apple libc) declined to implement it due to interface design issues, runtime constraint handlers adding overhead, and lack of cross-platform portability outside Windows MSVC.
</details>

<details>
<summary><strong>12. How does standard I/O interact with multi-threaded programs?</strong></summary>

Under POSIX and ISO C11 threads, every `FILE *` object has an associated internal recursive mutex lock. Standard functions like `printf()` and `fputs()` automatically acquire this lock to prevent interleaved characters from multiple threads. For high-performance single-threaded loops, POSIX provides unlocked variants like `getchar_unlocked()` that bypass locking.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Stripping Trailing Newlines from `fgets`</strong></summary>

**Goal**: Write a helper function `strip_newline(char *str)` that safely removes any trailing `\r` or `\n` characters from a string read by `fgets()`.

**Solution**:
```c
#include <string.h>

void strip_newline(char *str) {
    if (!str) return;
    size_t len = strlen(str);
    while (len > 0 && (str[len - 1] == '\n' || str[len - 1] == '\r')) {
        str[len - 1] = '\0';
        len--;
    }
}
```
</details>

<details>
<summary><strong>Exercise 2 (Medium): Dynamic String Formatting with `snprintf`</strong></summary>

**Goal**: Implement a function `char *format_message(const char *prefix, int code, const char *msg)` that uses a two-pass `snprintf()` pattern to dynamically allocate exactly the required number of bytes on the heap.

**Solution**:
```c
#include <stdio.h>
#include <stdlib.h>

char *format_message(const char *prefix, int code, const char *msg) {
    /* Pass 1: Query required length by passing NULL and 0 */
    int len = snprintf(NULL, 0, "[%s #%d] %s", prefix, code, msg);
    if (len < 0) return NULL;

    size_t size = (size_t)len + 1;
    char *buf = malloc(size);
    if (!buf) return NULL;

    /* Pass 2: Format directly into allocated buffer */
    snprintf(buf, size, "[%s #%d] %s", prefix, code, msg);
    return buf;
}
```
</details>

<details>
<summary><strong>Exercise 3 (Hard): Robust CSV Record Parser</strong></summary>

**Goal**: Author a function that reads a CSV line containing an ID (integer), a name (string with spaces), and a score (double), using `sscanf` with field widths and validating return codes.

**Solution**:
```c
#include <stdio.h>
#include <stdbool.h>

typedef struct {
    int id;
    char name[32];
    double score;
} Record;

bool parse_record(const char *line, Record *out) {
    if (!line || !out) return false;
    
    /* Format: %d, %31[^,], %lf */
    int matched = sscanf(line, "%d , %31[^,] , %lf", 
                         &out->id, out->name, &out->score);
    
    return (matched == 3);
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **`gets()` is DEAD**: Removed in C11; always use `fgets()`.
- **`sprintf` is OBSOLETE**: Always use `snprintf(buf, sizeof(buf), ...)`.
- Check `snprintf` return: `if (ret >= sizeof(buf))` indicates truncation.
- `stdout` is line-buffered; `stderr` is unbuffered (`_IONBF`).
- `fgets()` stores `\n`; strip trailing `\n` manually.
- Never write `printf(user_string)`; write `printf("%s", user_string)`.
- Never call `fflush(stdin)` (it is undefined behavior in ISO C).

---

## 13. Further Reading

- ISO/IEC 9899:2018 (C17 Standard), Section 7.21: "Input/output `<stdio.h>`".
- SEI CERT C Coding Standard: *FIO30-C. Exclude user input from format strings*.
- Kerrisk, Michael: *The Linux Programming Interface* (Standard I/O Buffering).

---

[← Previous: 08-functions.md](./08-functions.md) | [Section Index](./README.md) | [Next: 10-undefined-unspecified-implementation-defined.md](./10-undefined-unspecified-implementation-defined.md) →
