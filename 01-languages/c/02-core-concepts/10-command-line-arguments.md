# Command-Line Arguments: argc, argv, and CLI Parsing

> Processing command-line inputs in C: memory layout of argument vectors, shell tokenization, robust type conversion with strtol, and flag parsing architectures.

| Metadata | Details |
| :--- | :--- |
| **Level** | Core Concepts |
| **Prerequisites** | [Strings](./02-strings.md), [Pointers](./03-pointers.md), [First Program](../00-setup/04-first-program.md) |
| **Time to Read** | 25 minutes |
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

Virtually all standard UNIX and Windows utility programs (`git`, `grep`, `gcc`, `ls`) receive their runtime configuration from parameters passed on the command line. In C, the operating system passes these tokens to the program's entry point:
```c
int main(int argc, char *argv[])
```
Understanding how the operating system constructs `argc` and `argv`, how shells perform quoting and wildcard expansion before passing arguments, and how to safely parse flags and numbers is vital for building robust CLI tools. Failing to validate arguments causes buffer overflows, null pointer dereferences, and security bypasses.

---

## 2. Core Concepts

### The Argument Vector Anatomy
- **`argc` (Argument Count)**: Non-negative integer representing the total count of arguments passed to the program, **including the executable name itself**.
- **`argv` (Argument Vector)**: An array of pointers to null-terminated character strings (`char **`).
- **`argv[0]`**: By convention, holds the name used to invoke the program (or the binary path).
- **`argv[1]` through `argv[argc - 1]`**: The actual arguments supplied by the caller.
- **`argv[argc] == NULL`**: Guaranteed by the ISO C standard. The element immediately following the last valid argument pointer is always a null pointer.

### Shell Preprocessing
Before your C program receives arguments, the **command shell** (Bash, Zsh, PowerShell, Windows CMD):
1. Splits command lines on whitespace.
2. Strips surrounding quotes (`"foo bar"` becomes a single argument `foo bar`).
3. Expands wildcard glob expressions (`*.c` is replaced with the list of matching filenames).
4. Resolves environment variable substitutions (`$HOME`).
Your C application never sees raw shell quotation marks or wildcards unless they were explicitly escaped by the user.

### Safe Numeric Conversion: Why `atoi` Must Be Avoided
Standard C provides `atoi(str)`, but **`atoi` is dangerous in production code**:
- It provides zero error detection: if parsing fails, it returns `0` (indistinguishable from the valid string `"0"`).
- If the number overflows `INT_MAX`, the behavior is strictly undefined.
Always use **`strtol()`** or **`strtoll()`** for integer parsing and **`strtod()`** for floating-point numbers.

---

## 3. How It Works Internally

### Memory Layout of `argv` in Process Virtual Memory

```
Process Stack (Initial Setup by Kernel execve):
                                                   Virtual Memory Strings
 argv (char**)                                    ┌─────────────────────┐
┌───────────┐      ┌──────────────┐               │ "my_program\0"      │
│  &argv[0] │ ───► │ argv[0]      │ ────────────► ├─────────────────────┤
└───────────┘      ├──────────────┤               │ "--count\0"         │
                   │ argv[1]      │ ────────────► ├─────────────────────┤
                   ├──────────────┤               │ "42\0"              │
                   │ argv[2]      │ ────────────► ├─────────────────────┤
                   ├──────────────┤               │ "input.txt\0"       │
                   │ argv[3]      │ ────────────► └─────────────────────┘
                   ├──────────────┤
                   │ argv[4]=NULL │ (ISO C Sentinel)
                   └──────────────┘
```

The pointer array `argv` and the contiguous strings it references are placed on the process stack by the operating system kernel during process initialization (via `execve()` on Linux or `CreateProcess()` on Windows) right before transferring execution control to `_start`.

---

## 4. Syntax & Usage

### Standard Signatures
```c
/* Standard hosted signature */
int main(int argc, char *argv[]);

/* Equivalent pointer-to-pointer syntax */
int main(int argc, char **argv);

/* Optional environment pointer (POSIX extension, not ISO C) */
int main(int argc, char *argv[], char *envp[]);
```

### Defensive Numeric Parsing with `strtol`
```c
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

long parse_long(const char *str, int *success) {
    char *endptr = NULL;
    errno = 0;
    long val = strtol(str, &endptr, 10);
    
    /* Verification checks:
     * 1. errno != ERANGE (no overflow/underflow)
     * 2. endptr != str (at least one valid digit read)
     * 3. *endptr == '\0' (no trailing non-numeric garbage) */
    if (errno != 0 || endptr == str || *endptr != '\0') {
        if (success) *success = 0;
        return 0;
    }
    if (success) *success = 1;
    return val;
}
```

> [!NOTE]
> ### Windows vs. POSIX Command-Line Discrepancy
> On POSIX systems, the kernel passes parsed arrays of strings (`char *argv[]`). On Windows, the operating system passes a single raw unparsed command-line string via `GetCommandLineW()`. The C runtime library startup code (`crt0`) parses this string into `argc` and `argv` using Windows quotation rules.

---

## 5. Practical Examples

### Complete Demonstration Program
The following program demonstrates verifying `argv[argc] == NULL`, iterating argument tokens, and converting numeric arguments defensively using `strtol`.

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

int main(int argc, char *argv[]) {
    printf("=== C Command-Line Arguments Demo ===\n");
    printf("Argument count (argc): %d\n", argc);

    /* Guaranteed by ISO C: argv[argc] is NULL */
    if (argv[argc] == NULL) {
        printf("Verified: argv[argc] is NULL\n");
    }

    /* Iterate and print arguments */
    for (int i = 0; i < argc; ++i) {
        printf("argv[%d] = %s\n", i, argv[i]);
    }

    /* Simulate robust integer parsing if 2nd arg provided */
    if (argc >= 2) {
        char *endptr = NULL;
        errno = 0;
        long val = strtol(argv[1], &endptr, 10);
        if (errno == 0 && endptr != argv[1] && *endptr == '\0') {
            printf("Parsed valid numeric argument: %ld\n", val);
        } else {
            printf("Argument '%s' is not a valid clean integer\n", argv[1]);
        }
    }

    return 0;
}
```

```text
=== C Command-Line Arguments Demo ===
Argument count (argc): 3
Verified: argv[argc] is NULL
argv[0] = E:\Learning\engineering-notes\01-languages\c\02-core-concepts\examples\10-command-line-arguments.exe
argv[1] = 42
argv[2] = verbose
Parsed valid numeric argument: 42
```

---

## 6. Best Practices

1. **Always Verify `argc` Before Indexing `argv`**: Never access `argv[1]` without first verifying `argc >= 2`. Accessing out-of-bounds indices causes memory access violations.
2. **Never Use `atoi()` or `atof()` in Production**: Always use `strtol()`, `strtoll()`, or `strtod()` because they detect non-numeric characters, overflow (`ERANGE`), and empty inputs.
3. **Print Helpful Usage Instructions**: If the user provides incorrect arguments, print a concise usage string to `stderr` and exit with `EXIT_FAILURE`.
4. **Treat `argv` Strings as Read-Only**: Although `argv` strings reside in writable stack/heap memory, mutating them can interfere with system inspection tools (e.g., `ps` on Linux) and leads to maintenance bugs.
5. **Support `--help` and `--version` Flags**: Standard CLI conventions expect these flags to print documentation and exit with `0` (`EXIT_SUCCESS`).

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It's Wrong | The Fix |
| :--- | :--- | :--- |
| **Using `atoi(argv[1])` directly** | Fails silently on letters (returns 0); undefined behavior on integer overflow. | Use `strtol()` and check `errno` and `endptr`. |
| **Accessing `argv[1]` without checking `argc`** | If no arguments are passed, `argv[1]` is `NULL`; dereferencing causes immediate `SIGSEGV`. | Always check `if (argc < 2) { print_usage(); return 1; }`. |
| **Assuming `argv[0]` is always non-empty** | A parent process invoking `execve()` can pass an empty string `""` or even `NULL` as `argv[0]`. | Check `if (argc > 0 && argv[0])` before printing program name. |
| **Manual string comparisons for flags without bounds check** | `strcmp(argv[i], "-f")` when `argv[i]` is not validated can crash. | Validate pointers before passing to string functions. |
| **Modifying `argv` strings in place beyond length** | Writing past the null terminator of `argv[i]` smashes adjacent environment strings. | Copy arguments to dynamically allocated buffers before manipulation. |
| **Ignoring return codes in `main`** | Omitting `return` or returning random numbers misinforms shell scripts and CI pipelines. | Return `EXIT_SUCCESS` (0) or `EXIT_FAILURE` (1). |

---

## 8. Performance & Security Notes

### Security: Argument Injection and Privilege Escalation
Command-line arguments are completely untrusted user input. If a privileged setuid program accepts a filename argument and passes it unquoted to `system()`, an attacker can inject arbitrary shell commands (e.g., `./app "file.txt; rm -rf /"`). Always use `execv()` or `execvp()` rather than `system()` to ensure arguments are passed as isolated tokens without shell interpretation.

### Inspecting Process Arguments via `/proc`
On Linux, a program's command line is exposed publicly via `/proc/<pid>/cmdline`. Never pass sensitive credentials, passwords, or cryptographic tokens via command-line arguments, as any local user on the machine can read them via `ps aux` or `/proc`.

---

## 9. Comparison: When to Use What

| Method | Safety | Overflow Checked? | Flexibility | Typical Use Case |
| :--- | :--- | :--- | :--- | :--- |
| **`atoi` / `atol`** | Unsafe | No (UB) | Minimal | Quick prototypes, toy exercises |
| **`strtol` / `strtoll`** | High | Yes (`ERANGE`) | Full base control (hex, oct, dec) | Production CLI numerical arguments |
| **`sscanf`** | Moderate | Partial | Format strings | Structured argument formats (e.g., `x,y`) |
| **`getopt_long` (POSIX)** | High | Via helper | Standard UNIX flag parsing | Complex production command-line utilities |

---

## 10. Interview Questions

<details>
<summary><strong>1. Is <code>argv[argc]</code> guaranteed to be NULL in ISO C?</strong></summary>

Yes. ISO C (§5.1.2.2.1) explicitly guarantees that `argv[argc]` is a null pointer. This allows looping through arguments without using `argc` by iterating `for (char **p = argv; *p != NULL; p++)`.
</details>

<details>
<summary><strong>2. Why is <code>int main(int argc, char *argv[])</code> equivalent to <code>int main(int argc, char **argv)</code>?</strong></summary>

In C function parameter declarations, any parameter declared with array syntax `T arr[]` automatically decays into a pointer to its element type `T *`. Therefore, `char *argv[]` decays into `char **argv`.
</details>

<details>
<summary><strong>3. Can <code>argc</code> ever be zero in a hosted C environment?</strong></summary>

Under standard ISO C (§5.1.2.2.1), `argc` is non-negative, but can technically be 0 if the host environment cannot determine the program name. However, on POSIX and Windows hosted systems, `argc` is virtually always $\ge 1$, with `argv[0]` representing the invocation name.
</details>

<details>
<summary><strong>4. How does <code>strtol()</code> differentiate between the input <code>"0"</code> and an error?</strong></summary>

`strtol(str, &endptr, base)` sets `endptr` to point to the character where conversion stopped. If conversion fails because no digits were found, `endptr == str`. If the input was `"0"`, `endptr` points to the terminating null byte `'\0'`, confirming valid conversion.
</details>

<details>
<summary><strong>5. What is the third parameter of <code>main</code> on POSIX systems?</strong></summary>

On POSIX systems, `main` can optionally accept a third argument: `int main(int argc, char *argv[], char *envp[])`. `envp` is a null-terminated array of strings representing environment variables (`"KEY=VALUE"`). Standard ISO C recommends accessing the environment via the standard library function `getenv()` instead.
</details>

<details>
<summary><strong>6. How does POSIX <code>getopt()</code> handle flag parsing?</strong></summary>

`getopt(argc, argv, optstring)` parses short flags (e.g., `-v`, `-f filename`). It automatically permutes arguments, sets global variables `optarg` (for flag parameters) and `optind` (index of next argument), and returns `-1` when all options have been processed.
</details>

<details>
<summary><strong>7. What happens if a command line exceeds the operating system's maximum length?</strong></summary>

On Linux, the maximum size for `argv` and environment variables is governed by `ARG_MAX` (typically 2 MB). If a shell wildcard expansion exceeds this limit, the kernel rejects execution with `E2BIG` ("Argument list too long").
</details>

<details>
<summary><strong>8. Why should passwords never be accepted via command-line arguments?</strong></summary>

Arguments passed on the command line are stored in process memory visible to all users on the host system via `ps -ef` or `/proc/<pid>/cmdline`. Passwords should be read from environment variables, files with restricted permissions (0600), or interactively from standard input with terminal echo disabled (`termios`).
</details>

---

## 11. Practice Exercises

### Exercise 1: Custom Flag Checker (Easy)
Write a program that inspects `argv` and prints `"Verbose mode enabled"` if `-v` or `--verbose` is present anywhere in the arguments.

<details>
<summary>View Solution</summary>

```c
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

int main(int argc, char *argv[]) {
    bool verbose = false;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            verbose = true;
            break;
        }
    }
    printf("Verbose mode: %s\n", verbose ? "ENABLED" : "DISABLED");
    return 0;
}
```
</details>

### Exercise 2: Strict Sum of Integers CLI (Medium)
Write a program that takes any number of command-line arguments, validates that each argument is a strict decimal integer, and outputs their sum. If any argument is invalid, print an error and exit with code 1.

<details>
<summary>View Solution</summary>

```c
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

int main(int argc, char *argv[]) {
    long sum = 0;
    for (int i = 1; i < argc; ++i) {
        char *endptr = NULL;
        errno = 0;
        long val = strtol(argv[i], &endptr, 10);
        if (errno != 0 || endptr == argv[i] || *endptr != '\0') {
            fprintf(stderr, "Error: '%s' is not a valid integer\n", argv[i]);
            return 1;
        }
        sum += val;
    }
    printf("Total Sum: %ld\n", sum);
    return 0;
}
```
</details>

### Exercise 3: Key-Value Option Parser (Hard)
Implement an option parser that parses arguments formatted as `--key=value` (e.g., `--port=8080`, `--host=localhost`), extracting the keys and values into a dynamic lookup structure.

<details>
<summary>View Solution</summary>

```c
#include <stdio.h>
#include <string.h>

void parse_key_value(const char *arg) {
    if (strncmp(arg, "--", 2) != 0) return;
    const char *eq = strchr(arg + 2, '=');
    if (!eq) return;

    char key[64] = {0};
    char val[128] = {0};

    size_t key_len = eq - (arg + 2);
    if (key_len >= sizeof(key)) key_len = sizeof(key) - 1;
    strncpy(key, arg + 2, key_len);

    strncpy(val, eq + 1, sizeof(val) - 1);
    printf("Parsed Option: Key=[%s], Value=[%s]\n", key, val);
}

int main(int argc, char *argv[]) {
    for (int i = 1; i < argc; ++i) {
        parse_key_value(argv[i]);
    }
    return 0;
}
```
</details>

---

## 12. Summary & Cheat Sheet

```c
/* Standard main */
int main(int argc, char *argv[])

/* Iteration */
for (int i = 1; i < argc; ++i) { /* Process argv[i] */ }
while (*argv) { /* Process *argv; argv++; */ }

/* Robust integer conversion */
char *endptr;
errno = 0;
long val = strtol(argv[1], &endptr, 10);
if (errno != 0 || endptr == argv[1] || *endptr != '\0') {
    /* Invalid number */
}

/* Return codes */
return EXIT_SUCCESS; /* 0 */
return EXIT_FAILURE; /* 1 */
```

---

## 13. Further Reading
- ISO/IEC 9899:2018 (C17 Standard), §5.1.2.2.1 "Program startup".
- POSIX.1-2017: `getopt`, `getopt_long` specifications.
- *The Linux Programming Interface* by Michael Kerrisk (Chapter 6: "Processes", Command-Line Arguments).
- GNU C Library Reference Manual: "Parsing Program Options".

---

[← Previous: Preprocessor](./09-preprocessor.md) | [Section Index](./README.md) | [Next: File I/O →](./11-file-io.md)
