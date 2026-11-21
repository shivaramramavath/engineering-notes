# File I/O: Streams, Buffering, and Binary Serialization

> Working with files in C: standard I/O streams, user-space buffering strategies, text translation pitfalls, binary serialization, and robust error recovery.

| Metadata | Details |
| :--- | :--- |
| **Level** | Core Concepts |
| **Prerequisites** | [Pointers](./03-pointers.md), [Strings](./02-strings.md), [Memory Management](./08-memory-management.md) |
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

Virtually all persistent computer state—logs, configuration files, databases, network streams, and multimedia files—relies on file input and output. The C standard library provides the **Standard I/O Library (`<stdio.h>`)**, a battle-tested abstraction built around the `FILE *` stream object.

Standard I/O provides two key advantages over raw operating system system calls (`read()`, `write()`):
1. **User-Space Buffering**: Dramatically reduces costly kernel context switches by batching character reads and writes in process memory buffers.
2. **Portability**: Provides a uniform interface across Linux, Windows, macOS, and embedded RTOS systems, transparently handling platform differences like line endings.

Understanding stream buffering modes, distinguishing text from binary modes, and correctly handling End-Of-File (`EOF`) conditions are critical competencies for any systems developer.

---

## 2. Core Concepts

### The Stream Abstraction (`FILE *`)
In C, a file is treated as a linear sequence of bytes accessed through an opaque handle: `FILE *`. The `FILE` structure maintains internal stream state:
- Current file position offset indicator.
- Internal buffer pointer and byte count.
- Error indicator (`ferror()`).
- End-Of-File indicator (`feof()`).
- Associated OS kernel file descriptor (e.g., `fileno(f)` on POSIX).

### File Opening Modes
The `fopen(path, mode)` function accepts mode strings specifying access intent:
- `"r"`: Read-only; fails if the file does not exist.
- `"w"`: Write-only; **truncates existing file to zero length** or creates a new file.
- `"a"`: Append-only; all writes occur at the end of the file regardless of seeks.
- `"r+"`: Read and write; file must already exist.
- `"w+"`: Read and write; creates file or truncates existing file to zero.
- `"wx"` (C11): Exclusive write; fails if file already exists (prevents race conditions).
- Binary mode flag (`"b"`): Append `"b"` (e.g., `"rb"`, `"wb"`) to disable text mode line-ending translations.

### Buffering Modes
Standard I/O streams operate under one of three buffering disciplines configured via `setvbuf()`:
1. **Fully Buffered (`_IOFBF`)**: Bytes are written to disk only when the buffer fills (standard for disk files).
2. **Line Buffered (`_IOLBF`)**: Bytes are written to disk whenever a newline `\n` character is written (standard for interactive terminals / `stdout`).
3. **Unbuffered (`_IONBF`)**: Every single read or write immediately issues an OS system call without caching (standard for `stderr`).

---

## 3. How It Works Internally

### Standard I/O vs. Kernel OS System Calls

```
 User Application Space
 ┌─────────────────────────────────────────────────────────────┐
 │ fputc() / fprintf() / fwrite()                              │
 └──────────────────────────────┬──────────────────────────────┘
                                │
                                ▼
 Standard C Library (libc) User-Space Buffer
 ┌─────────────────────────────────────────────────────────────┐
 │ Buffer (e.g., 4096 or 8192 bytes allocated via setvbuf)     │
 └──────────────────────────────┬──────────────────────────────┘
                                │ Buffer full / fflush() / \n
 ═══════════════════════════════╪═══════════════════════════════ Kernel Boundary
 Operating System Kernel Space  ▼
 ┌─────────────────────────────────────────────────────────────┐
 │ System Call: sys_write() -> Page Cache -> Physical Storage  │
 └─────────────────────────────────────────────────────────────┘
```

Without user-space buffering, writing 1,000,000 characters with `write(fd, &ch, 1)` causes 1,000,000 expensive user-to-kernel CPU context switches. With libc buffering, `fputc(ch, f)` stores bytes in user-space memory, issuing only $\approx 244$ system calls for 4 KB buffers—achieving a $100\times$ speedup.

### Text Mode vs. Binary Mode Mechanics
- **POSIX / Linux / macOS**: Text mode and binary mode are 100% identical. The newline character is a single byte `\n` (`0x0A`).
- **Windows (MS-DOS Heritage)**:
  - Text mode (`"r"`, `"w"`): Reading translates physical CRLF `\r\n` (`0x0D 0x0A`) into a single logical `\n`. Writing translates `\n` into physical `\r\n`. Additionally, byte `0x1A` (`Ctrl+Z`) is treated as premature EOF.
  - Binary mode (`"rb"`, `"wb"`): Raw bytes are transferred bit-for-bit with zero translation.

---

## 4. Syntax & Usage

```c
#include <stdio.h>
#include <stdlib.h>

/* Safe line-by-line reading idiom */
void process_lines(const char *filename) {
    FILE *f = fopen(filename, "r");
    if (!f) {
        perror("fopen failed");
        return;
    }

    char buffer[256];
    while (fgets(buffer, sizeof buffer, f) != NULL) {
        /* Process buffer... */
    }

    if (ferror(f)) {
        perror("Error reading file");
    }

    fclose(f);
}
```

> [!NOTE]
> ### ISO C11/C23 Updates: `"x"` Mode and `<stdio.h>` Bounds Safety
> - **Exclusive Open Mode `"x"`**: Added in C11 (`"wx"` or `"w+x"`). Opens the file in exclusive creation mode (equivalent to POSIX `O_CREAT | O_EXCL`). If the file already exists, `fopen` fails safely, closing a classic Time-Of-Check to Time-Of-Use (TOCTOU) security vulnerability.
> - **ISO C23 Attribute `[[nodiscard]]` on File Functions**: Directs compilers to warn if return values of resource allocation functions like `fopen` or critical checks are ignored.

---

## 5. Practical Examples

### Complete Demonstration Program
The following program demonstrates opening, writing, line-by-line reading with `fgets`, and binary record serialization using `fwrite` and `fread`.

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Record {
    int id;
    char name[20];
    double score;
} Record;

int main(void) {
    printf("=== C File I/O Demo ===\n");
    const char *text_filename = "batch8_temp_test.txt";
    const char *bin_filename = "batch8_temp_records.bin";

    /* 1. Formatted Text Write */
    FILE *ft = fopen(text_filename, "w");
    if (!ft) {
        perror("Failed to open text file for writing");
        return 1;
    }
    fprintf(ft, "Line 1: ISO C17 Standard\nLine 2: Systems Programming\n");
    fclose(ft);

    /* 2. Text Read via fgets */
    ft = fopen(text_filename, "r");
    if (!ft) {
        perror("Failed to open text file for reading");
        return 1;
    }
    char buffer[128];
    printf("Reading text file:\n");
    while (fgets(buffer, sizeof buffer, ft)) {
        buffer[strcspn(buffer, "\r\n")] = '\0'; /* Strip newline */
        printf("  Read: [%s]\n", buffer);
    }
    fclose(ft);
    remove(text_filename);

    /* 3. Binary Write and Read */
    Record out_rec = {101, "Alice Engineer", 98.5};
    FILE *fb = fopen(bin_filename, "wb");
    if (!fb) {
        perror("Failed to open binary file for writing");
        return 1;
    }
    fwrite(&out_rec, sizeof(Record), 1, fb);
    fclose(fb);

    Record in_rec = {0, "", 0.0};
    fb = fopen(bin_filename, "rb");
    if (!fb) {
        perror("Failed to open binary file for reading");
        return 1;
    }
    size_t items_read = fread(&in_rec, sizeof(Record), 1, fb);
    fclose(fb);
    remove(bin_filename);

    if (items_read == 1) {
        printf("Binary read verified: ID=%d, Name=%s, Score=%.1f\n",
               in_rec.id, in_rec.name, in_rec.score);
    }

    return 0;
}
```

```text
=== C File I/O Demo ===
Reading text file:
  Read: [Line 1: ISO C17 Standard]
  Read: [Line 2: Systems Programming]
Binary read verified: ID=101, Name=Alice Engineer, Score=98.5
```

---

## 6. Best Practices

1. **Always Check the Return of `fopen()`**: Never assume opening a file succeeded. If `fopen` returns `NULL`, dereferencing it causes an immediate fatal crash.
2. **Every Successful `fopen()` Must Have a Matching `fclose()`**: File handles are limited operating system resources (governed by `RLIMIT_NOFILE`). Leaking file descriptors causes subsequent open operations to fail.
3. **Always Open Non-Text Files with `"b"` Mode**: Always append `"b"` (`"rb"`, `"wb"`, `"ab"`) when reading images, binaries, network protocols, or serialized structs to prevent Windows CRLF data corruption.
4. **Never Use `while (!feof(f))` as a Read Loop Condition**: `feof()` only returns true **after** an I/O operation attempts to read past the end of the file. Testing `!feof(f)` upfront processes the final byte/line twice.
5. **Always Check the Return Value of `fwrite()` and `fread()`**: Verify that the number of items written or read matches expectations. Disk full conditions or severed pipes result in partial writes.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It's Wrong | The Fix |
| :--- | :--- | :--- |
| **Using `while (!feof(f))`** | `feof()` is only set *after* a read fails; causes the loop body to execute one extra time on garbage data. | Loop on the read function itself: `while (fgets(...) != NULL)`. |
| **Storing `fgetc()` result in a `char`** | `EOF` is `-1`. If `char` is unsigned, `(unsigned char)EOF` becomes `255`, creating an infinite loop. | Store return of `fgetc()` in an `int`: `int c; while ((c = fgetc(f)) != EOF)`. |
| **Opening binary files in text mode on Windows** | Windows CR-LF translation corrupts byte offsets, and byte `0x1A` is treated as premature EOF. | Always include `"b"` in the mode string: `"rb"`, `"wb"`. |
| **Assuming `fclose()` cannot fail** | When writing to disk, buffered data is flushed during `fclose()`. If the disk is full, `fclose()` fails. | Check `if (fclose(f) != 0) { perror("fclose failed"); }`. |
| **Writing structs with pointers via `fwrite`** | Writing a struct containing pointers writes raw memory addresses, which are invalid when read back. | Serialize struct fields independently; serialize data, not pointers. |
| **Missing `fflush()` before reading on `"r+"` streams** | ISO C forbids switching between reading and writing without an intervening seek or flush. | Call `fflush()` or `fseek()` before changing I/O direction. |

---

## 8. Performance & Security Notes

### Buffer Sizing with `setvbuf()`
By default, libc allocates a standard buffer (usually 4 KB or 8 KB). When reading large gigabyte-scale datasets sequentially, increasing the buffer size to 64 KB or 128 KB via `setvbuf()` significantly reduces system call overhead:
```c
char large_buf[65536];
setvbuf(f, large_buf, _IOFBF, sizeof large_buf);
```

### Security: Path Traversal and Symlink Race Conditions
- **Path Traversal**: Never open user-supplied paths directly without sanitizing against `../` directory traversal attacks.
- **TOCTOU Race Conditions**: Calling `access(path, W_OK)` followed by `fopen(path, "w")` is vulnerable to symlink race attacks. Use C11 exclusive mode `"wx"` or POSIX `open(path, O_CREAT | O_EXCL | O_WRONLY)`.

---

## 9. Comparison: When to Use What

| Function | Unit of Operation | Buffering? | Error Detection | Typical Use Case |
| :--- | :--- | :--- | :--- | :--- |
| `fgetc` / `fputc` | 1 Character (`int`) | Yes | `EOF` | Tokenizers, lexers, byte filters |
| `fgets` / `fputs` | 1 Line (`char[]`) | Yes | `NULL` | Config files, log parsers |
| `fread` / `fwrite` | Arbitrary Byte Blocks | Yes | Return item count | Binary data, media files, bulk I/O |
| `read` / `write` (POSIX) | Raw Byte Slices | No (Kernel direct) | `-1` with `errno` | Network sockets, device drivers |

---

## 10. Interview Questions

<details>
<summary><strong>1. Why must the return value of <code>fgetc()</code> be stored in an <code>int</code> rather than a <code>char</code>?</strong></summary>

`fgetc()` must be capable of returning all possible unsigned byte values (`0` to `255`) plus a distinct sentinel value indicating End-Of-File (`EOF`, typically `-1`). If stored in a `char`, on systems where `char` is signed, a valid byte `0xFF` would be indistinguishable from `EOF`. On systems where `char` is unsigned, `EOF` is converted to `255`, resulting in an infinite loop.
</details>

<details>
<summary><strong>2. What is the difference between <code>fseek()</code>/<code>ftell()</code> and <code>fgetpos()</code>/<code>fsetpos()</code>?</strong></summary>

`fseek()` and `ftell()` operate on `long` integer offsets. On 32-bit systems (and 64-bit Windows), `long` is 32 bits, limiting file seeking to 2 GB files.
`fgetpos()` and `fsetpos()` operate on `fpos_t`, an opaque type designed to support arbitrarily large 64-bit file offsets and complex multibyte state across all platforms.
</details>

<details>
<summary><strong>3. Why is <code>gets()</code> completely removed from ISO C, and why is <code>fgets()</code> safe?</strong></summary>

`gets(buf)` accepted no buffer size parameter, reading standard input until a newline was reached. Any input longer than the buffer smashed the stack frame, making it the source of countless historic buffer overflow exploits. It was deprecated in C99 and removed completely in C11. `fgets(buf, size, stream)` requires specifying the maximum buffer capacity, guaranteeing no more than `size - 1` bytes are written.
</details>

<details>
<summary><strong>4. What does <code>fflush()</code> do on an input stream?</strong></summary>

In standard ISO C (§7.21.5.2), `fflush()` is only defined for output streams or update streams where the last operation was an output. Calling `fflush(stdin)` on an input stream is **strictly undefined behavior**, although some implementations (like MSVC) define it as discarding unread input.
</details>

<details>
<summary><strong>5. How do you detect whether a read operation failed due to EOF vs. an I/O hardware error?</strong></summary>

When a read function like `fgets()` returns `NULL` or `fread()` returns fewer items than requested, call `feof(f)` and `ferror(f)`:
- `feof(f) != 0`: Reached the end of the file normally.
- `ferror(f) != 0`: A hardware, disk, or network I/O error occurred (inspect `errno`).
</details>

<details>
<summary><strong>6. What is the difference between line buffering and full buffering?</strong></summary>

Line buffering (`_IOLBF`) flushes buffered characters to the OS whenever a newline `\n` character is output or when input is requested from an interactive stream. Full buffering (`_IOFBF`) only flushes when the entire buffer fills or when `fflush()`/`fclose()` is explicitly invoked.
</details>

<details>
<summary><strong>7. What happens if a program crashes without calling <code>fclose()</code>?</strong></summary>

When a process terminates normally via `exit()` or returning from `main()`, the C runtime flushes and closes all open standard I/O streams. However, if the process crashes abnormally (e.g., `SIGSEGV`, `abort()`, or power loss), user-space buffers residing in volatile memory are lost without being written to disk.
</details>

<details>
<summary><strong>8. Why is serializing raw C structs directly with <code>fwrite(&s, sizeof s, 1, f)</code> not portable?</strong></summary>

Raw struct serialization writes memory padding bytes, which vary across compiler flags, architectures, and ABIs. Additionally, integer endianness (big-endian vs little-endian) differs between CPU architectures. Serialized files created on one machine will be corrupt when read on another.
</details>

---

## 11. Practice Exercises

### Exercise 1: File Line Counter (Easy)
Write a program that opens a text file specified on the command line and counts the total number of lines (`\n` characters) it contains.

<details>
<summary>View Solution</summary>

```c
#include <stdio.h>

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <filename>\n", argv[0]);
        return 1;
    }
    FILE *f = fopen(argv[1], "r");
    if (!f) {
        perror("Error opening file");
        return 1;
    }
    int c;
    size_t lines = 0;
    while ((c = fgetc(f)) != EOF) {
        if (c == '\n') lines++;
    }
    fclose(f);
    printf("Total lines: %zu\n", lines);
    return 0;
}
```
</details>

### Exercise 2: Binary File Copy Utility (Medium)
Implement a fast binary file copy utility `int copy_file(const char *src, const char *dst)` that transfers data in 8 KB blocks using `fread` and `fwrite`, handling errors defensively.

<details>
<summary>View Solution</summary>

```c
#include <stdio.h>

int copy_file(const char *src, const char *dst) {
    FILE *in = fopen(src, "rb");
    if (!in) return -1;

    FILE *out = fopen(dst, "wb");
    if (!out) {
        fclose(in);
        return -2;
    }

    char buffer[8192];
    size_t bytes;
    while ((bytes = fread(buffer, 1, sizeof buffer, in)) > 0) {
        if (fwrite(buffer, 1, bytes, out) != bytes) {
            fclose(in);
            fclose(out);
            return -3; /* Write failure / disk full */
        }
    }

    fclose(in);
    return fclose(out) == 0 ? 0 : -4;
}
```
</details>

### Exercise 3: Dynamic Line Reader (Hard)
Implement a portable function `char* read_arbitrary_line(FILE *f)` that reads a line of unbounded length, dynamically reallocating memory as needed, and stripping the trailing newline.

<details>
<summary>View Solution</summary>

```c
#include <stdio.h>
#include <stdlib.h>

char* read_arbitrary_line(FILE *f) {
    if (!f) return NULL;
    size_t cap = 64;
    size_t len = 0;
    char *buf = malloc(cap);
    if (!buf) return NULL;

    int c;
    while ((c = fgetc(f)) != EOF && c != '\n') {
        if (c == '\r') continue; /* Handle Windows CRLF */
        if (len + 1 >= cap) {
            size_t new_cap = cap * 2;
            char *new_buf = realloc(buf, new_cap);
            if (!new_buf) {
                free(buf);
                return NULL;
            }
            buf = new_buf;
            cap = new_cap;
        }
        buf[len++] = (char)c;
    }

    if (len == 0 && c == EOF) {
        free(buf);
        return NULL;
    }

    buf[len] = '\0';
    return buf;
}
```
</details>

---

## 12. Summary & Cheat Sheet

```c
/* Opening files */
FILE *f = fopen("data.bin", "rb"); /* Binary read */
FILE *out = fopen("log.txt", "a"); /* Text append */

/* Reading / Writing text */
char line[128];
while (fgets(line, sizeof line, f)) { /* process */ }
fprintf(out, "Formatted: %d\n", val);

/* Reading / Writing binary */
fwrite(data_ptr, sizeof(Elem), count, out);
fread(data_ptr, sizeof(Elem), count, in);

/* Seeking */
fseek(f, 0, SEEK_END);   /* Seek to end */
long size = ftell(f);    /* Query file size */
rewind(f);               /* Seek to start */

/* Cleanup */
fclose(f);
```

---

## 13. Further Reading
- ISO/IEC 9899:2018 (C17 Standard), §7.21 "Input/output `<stdio.h>`".
- ISO/IEC 9899:2024 (C23 Standard), §7.21 "Input/output `<stdio.h>`".
- *Advanced Programming in the UNIX Environment* (3rd ed.) by Stevens & Rago (Chapter 5: "Standard I/O Library").
- *The Linux Programming Interface* by Michael Kerrisk (Chapter 13: "File I/O Buffering").

---

[← Previous: Command-Line Arguments](./10-command-line-arguments.md) | [Section Index](./README.md) | [Next: Pointer Arithmetic →](./12-pointer-arithmetic.md)
