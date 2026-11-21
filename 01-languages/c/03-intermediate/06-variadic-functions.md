# 06 Variadic Functions

> Master variadic arguments in C with `<stdarg.h>`, understand stack frame traversal and ABI calling conventions, write safe formatting wrappers, and prevent format string vulnerabilities.

| Metadata | Details |
| :--- | :--- |
| **Level** | Intermediate |
| **Prerequisites** | [01-fundamentals/08-functions.md](../01-fundamentals/08-functions.md), [02-core-concepts/03-pointers.md](../02-core-concepts/03-pointers.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **Standard** | C11 / C17 / C23 |

---

## Table of Contents
1. [1. What & Why](#1-what--why)
2. [2. Core Concepts](#2-core-concepts)
3. [3. How It Works Internally](#3-how-it-works-internally)
4. [4. Syntax & Usage](#4-syntax--usage)
5. [5. Practical Examples](#5-practical-examples)
6. [6. Best Practices](#6-best-practices)
7. [7. Common Mistakes & Pitfalls](#7-common-mistakes--pitfalls)
8. [8. Performance & Security Notes](#8-performance--security-notes)
9. [9. Comparison / When to Use What](#9-comparison--when-to-use-what)
10. [10. Interview Questions](#10-interview-questions)
11. [11. Practice Exercises](#11-practice-exercises)
12. [12. Summary / Cheat Sheet](#12-summary--cheat-sheet)
13. [13. Further Reading](#13-further-reading)

---

## 1. What & Why

In C, most functions take a predetermined, fixed number of arguments. However, many real-world tasks require varying quantities of inputs:
- **Formatted String Generation**: `printf()`, `snprintf()`, and custom enterprise loggers must accept arbitrary format strings followed by zero or dozens of values of differing types.
- **SQL / Query Builders**: Functions constructing database queries dynamically bind arbitrary parameters to prepared statements.
- **Data Aggregators**: Mathematical utilities calculating geometric means, maximums, or concatenating lists of strings where the item count varies at runtime.

The C standard provides the `<stdarg.h>` header, defining macros that allow a function to accept a variable argument list (commonly denoted by the ellipsis `...`). Understanding how variadic functions interact with compiler ABI calling conventions, default argument promotions, and runtime pointer walking is essential for systems programmers.

---

## 2. Core Concepts

### 2.1 The `<stdarg.h>` Primitives
Handling variable arguments relies on four primary macros and one opaque type:
- `va_list`: An opaque type that holds the state and position information necessary to retrieve variable arguments.
- `va_start(va_list ap, last_named_arg)`: Initializes `ap` to point to the first anonymous argument following `last_named_arg`.
- `va_arg(va_list ap, type)`: Retrieves the next argument as the specified `type` and advances `ap` to the subsequent argument.
- `va_end(va_list ap)`: Cleans up the `va_list` state. Calling `va_end` is **mandatory** before returning from the function.
- `va_copy(va_list dest, va_list src)`: Introduced in C99, duplicates the traversal state of `src` into `dest`. Essential when an argument list must be parsed multiple times.

### 2.2 Default Argument Promotions
When arguments are passed through an ellipsis `...`, normal prototype-based conversions are disabled. The compiler applies **default argument promotions**:
1. Any integer type smaller than `int` (`char`, `signed char`, `unsigned char`, `short`, `unsigned short`, `_Bool`) is promoted to `signed int` (or `unsigned int` if `int` cannot represent all values).
2. Any `float` is automatically promoted to `double`.

```text
Passed Type          Promoted Type in va_arg()
-----------------------------------------------
char, short, bool -> int
unsigned char     -> int (or unsigned int)
float             -> double
int, long, double -> unchanged
Pointers          -> unchanged
```
> [!CAUTION]
> Never pass `char`, `short`, or `float` as the type argument to `va_arg(ap, type)`. Doing so invokes **Undefined Behavior**. Always retrieve them as `int` or `double`.

### 2.3 Knowing When Arguments End
The C runtime **does not provide** a mechanism for a variadic function to determine how many arguments were passed. The developer must provide an explicit stopping mechanism:
1. **Count Parameter**: Pass an explicit integer argument specifying the number of following arguments: `sum(int count, ...)`.
2. **Format Specifiers**: Use a string containing tokens that dictate both the count and expected types: `printf("%d %s", 42, "data")`.
3. **Sentinel Terminator**: Terminate the list with a designated sentinel value, such as `NULL`, `-1`, or `(char *)0`: `concat_strings("a", "b", "c", NULL)`.

---

## 3. How It Works Internally

Under legacy 32-bit x86 cdecl calling conventions, all arguments were pushed onto the execution stack from right to left. `va_list` was simply a `char *` pointer that advanced through stack addresses by adding `sizeof(promoted_type)`.

On modern 64-bit architectures (System V AMD64 ABI on Linux/macOS, and Microsoft x64 on Windows), arguments are primarily passed in general-purpose and floating-point registers:
- **System V ABI (Linux/macOS)**: First 6 integer/pointer arguments in `rdi, rsi, rdx, rcx, r8, r9`. First 8 floating point arguments in `xmm0` through `xmm7`.
- **Microsoft x64 (Windows)**: First 4 arguments in `rcx, rdx, r8, r9` (or `xmm0-xmm3`).

```
System V AMD64 Variadic Stack Layout:
┌──────────────────────────────────────────┐
│ Stack Arguments (if > 6 int or > 8 float)│ [High Memory]
├──────────────────────────────────────────┤
│ Return Address                           │
├──────────────────────────────────────────┤
│ Saved Frame Pointer (%rbp)               │
├──────────────────────────────────────────┤
│ Register Save Area (RSA):                │
│   Spilled %rdi, %rsi, %rdx, %rcx, %r8, %r9│ [Spilled Registers]
│   Spilled %xmm0 .. %xmm7                 │
├──────────────────────────────────────────┤
│ va_list struct:                          │
│   gp_offset  (offset in RSA for int/ptr) │
│   fp_offset  (offset in RSA for float)   │
│   reg_save_area (pointer to RSA)         │
│   overflow_arg_area (pointer to stack)   │ [Low Memory]
└──────────────────────────────────────────┘
```

Because arguments reside in registers, the compiler emits a prologue in variadic functions that spills register arguments into a designated **Register Save Area (RSA)** on the stack. `va_list` under System V AMD64 is actually an array of a 24-byte structure tracking register save offsets and stack spill pointers.

---

## 4. Syntax & Usage

### 4.1 Forwarding with the `v*` Family
You should virtually never attempt to re-parse format strings manually when wrapping `printf`. Instead, delegate to the standard library's `v*` forwarding functions:
- `vprintf(fmt, ap)`: Prints formatted output to `stdout`.
- `vfprintf(stream, fmt, ap)`: Prints formatted output to a specific `FILE *`.
- `vsnprintf(buf, size, fmt, ap)`: Writes bounded formatted string into a buffer.
- `vsyslog(priority, fmt, ap)`: Writes to system log (POSIX).

### 4.2 C23 Modernization
In C11 and C17, a variadic function required at least one named parameter before the ellipsis:
```c
/* C11/C17: Requires at least one named argument */
void log_message(const char *prefix, ...);
```
ISO C23 removed this restriction, allowing purely variadic prototypes:
```c
/* C23: Parameterless ellipsis is valid */
void f(...);
```
Furthermore, in C23 `va_start` no longer strictly requires passing the second argument: `va_start(ap)` is valid.

---

## 5. Practical Examples

### Comprehensive Example: Custom Variadic Logger & Safe Dynamic Formatter
The following program implements an enterprise-style logging system and a dynamic string formatter utilizing `va_copy` and `vsnprintf`.

```c
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

/* Custom variadic logger with log level prefix */
typedef enum {
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR
} LogLevel;

static void logger_log(LogLevel level, const char *fmt, ...) {
    const char *prefix = "UNKNOWN";
    switch (level) {
        case LOG_INFO:  prefix = "INFO"; break;
        case LOG_WARN:  prefix = "WARN"; break;
        case LOG_ERROR: prefix = "ERROR"; break;
    }

    printf("[%s] ", prefix);

    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);

    printf("\n");
}

/* Safe formatted string duplicator utilizing va_copy */
static char *string_format_alloc(const char *fmt, ...) {
    va_list args1, args2;
    va_start(args1, fmt);
    va_copy(args2, args1);

    /* Determine necessary buffer length (excluding null terminator) */
    int len = vsnprintf(NULL, 0, fmt, args1);
    va_end(args1);

    if (len < 0) {
        va_end(args2);
        return NULL;
    }

    char *buf = (char *)malloc((size_t)len + 1);
    if (!buf) {
        va_end(args2);
        return NULL;
    }

    vsnprintf(buf, (size_t)len + 1, fmt, args2);
    va_end(args2);
    return buf;
}

int main(void) {
    printf("=== Variadic Functions Demonstrations ===\n");

    logger_log(LOG_INFO, "System initialized successfully with %d workers", 4);
    logger_log(LOG_WARN, "Memory usage exceeded threshold: %.2f%%", 87.45);
    logger_log(LOG_ERROR, "Disk write error on partition '%s' (errno %d)", "/dev/sda1", 28);

    char *dyn_str = string_format_alloc("Payload ID: %04d, Hash: %s", 102, "a9f8c2b7");
    if (dyn_str) {
        printf("Allocated formatted string: %s\n", dyn_str);
        free(dyn_str);
    }

    return 0;
}
```

#### Compilation & Verified Execution Output
```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -Werror 06-variadic-functions.c -o 06-variadic-functions.exe
./06-variadic-functions.exe
```

```text
=== Variadic Functions Demonstrations ===
[INFO] System initialized successfully with 4 workers
[WARN] Memory usage exceeded threshold: 87.45%
[ERROR] Disk write error on partition '/dev/sda1' (errno 28)
Allocated formatted string: Payload ID: 0102, Hash: a9f8c2b7
```

---

## 6. Best Practices

1. **Always Call `va_end` in Every Exit Path**: Omitting `va_end` leaves the execution stack or register save pointers in an invalid state, causing undefined behavior or stack corruption.
2. **Always Use `va_copy` for Multiple Passes**: Never read from the same `va_list` twice without re-initializing it via `va_copy` or another `va_start`.
3. **Use Compiler Format Attributes**: In GCC and Clang, attach `__attribute__((format(printf, fmt_idx, arg_idx)))` to custom logging functions. This prompts the compiler to perform static type checking against the format string at compile time.
4. **Enforce Sentinel Checks**: When using sentinel-terminated lists, ensure callers pass a typed pointer sentinel: `(char *)NULL` rather than a bare `0` or `NULL`, because `NULL` might expand to an untyped integer `0` which may not match pointer width on 64-bit systems.
5. **Respect Default Argument Promotions**: Always retrieve `char` and `short` as `int`, and `float` as `double` inside `va_arg()`.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It's Wrong | The Fix |
| :--- | :--- | :--- |
| `va_arg(ap, float)` | Floats are promoted to `double`. Fetching `float` reads mismatched byte width, corrupting the `va_list` cursor. | Use `va_arg(ap, double)`. |
| `va_arg(ap, char)` | Chars are promoted to `int`. Passing `char` triggers Undefined Behavior. | Use `va_arg(ap, int)`. |
| Reusing `va_list` without `va_copy` | Reading arguments advances internal offsets. The second pass will read garbage memory or crash. | Use `va_copy(ap_copy, ap);` before first traversal. |
| Missing `va_end(ap)` | Violates ISO C specification; leaks register resources on ABIs where `va_list` allocates state. | Ensure `va_end(ap)` is called before return. |
| `printf(user_input)` | Format string vulnerability: attacker provides `%x %s %n` to leak memory or overwrite arbitrary addresses. | Always use `printf("%s", user_input);`. |
| Passing bare `0` as sentinel pointer | On 64-bit platforms, `0` is a 32-bit `int`, but pointer arguments require 64 bits. | Cast sentinel explicitly: `(void *)0` or `(char *)NULL`. |

---

## 8. Performance & Security Notes

### 8.1 Format String Vulnerabilities (CWE-134)
The `%n` format specifier in `printf` writes the count of characters emitted so far back into an integer pointer passed as an argument. If untrusted user input is passed directly as the format argument:
```c
/* VULNERABLE CODE */
char user_data[128];
fgets(user_data, sizeof(user_data), stdin);
printf(user_data); /* Attacker supplies: %x%x%x%n to hijack control flow! */
```
Modern security frameworks disable `%n` by default or enforce `-Wformat-security`.

### 8.2 Execution Overhead
Variadic functions incur measurable overhead compared to normal functions:
- Register spilling: Compilers must emit code to write parameter registers into the stack's Register Save Area.
- Inability to inline: Compilers rarely inline variadic functions.
- Absence of compiler type checking unless vendor-specific attributes (`__attribute__((format...))`) are supplied.

---

## 9. Comparison / When to Use What

| Technique | Type Safety | Flexibility | Performance | Portability |
| :--- | :--- | :--- | :--- | :--- |
| **Variadic (`...`) + `va_list`** | None (Runtime checked) | High (Accepts heterogeneous types) | Moderate (Register spill overhead) | Universal ISO C |
| **Array of Structs / Tagged Union** | Complete | Moderate (Fixed set of types) | High (Direct contiguous memory) | Universal ISO C |
| **Variadic Macros (`__VA_ARGS__`)** | Compile-Time Checked | High (Code expansion) | High (Zero runtime call overhead) | C99 / C11 / C23 |
| **C++ Variadic Templates** | Complete | Very High | Optimal (Zero-cost compile abstraction) | C++ only (Not C) |

---

## 10. Interview Questions

<details>
<summary><strong>1. Why is `va_copy` necessary instead of simply assigning `va_list dest = src;`?</strong></summary>

Under the C standard, `va_list` may be an array type, a pointer, or a structure. On x86_64 System V ABI, `va_list` is defined as a one-element array of a struct: `typedef struct { ... } va_list[1];`. In C, array assignment is illegal (`dest = src` causes a compilation error). Even on architectures where `va_list` is a pointer, simple assignment copies the pointer, not the underlying traversal state, meaning operations on one list mutate the state of the other. `va_copy` guarantees a safe, deep duplication across all platforms.
</details>

<details>
<summary><strong>2. What are default argument promotions, and how do they impact `va_arg`?</strong></summary>

When a variadic function is called, the compiler does not know the expected types of the anonymous arguments. Therefore, it applies default argument promotions: integer types narrower than `int` (`char`, `short`, `bool`) are converted to `int`, and `float` values are converted to `double`. Inside the variadic function, attempting to invoke `va_arg(ap, char)` or `va_arg(ap, float)` invokes Undefined Behavior because the values on the stack or in registers are physically `int` (4 bytes) or `double` (8 bytes).
</details>

<details>
<summary><strong>3. How does the compiler know how many arguments were passed to a variadic function?</strong></summary>

The compiler does **not** know, and no runtime metadata is automatically passed. The callee must deduce the count and types through application logic: either an explicit count parameter, a format string (like `%d %s`), or a terminal sentinel value (like `NULL`). If the function calls `va_arg` more times than arguments were actually passed, it reads uninitialized stack memory, causing garbage data or segmentation faults.
</details>

<details>
<summary><strong>4. Why is passing `NULL` as a sentinel to a variadic function potentially dangerous on 64-bit systems?</strong></summary>

In C, `NULL` can be defined as `0` or `((void*)0)`. If a compiler defines `NULL` as integer literal `0`, it is a 32-bit `int`. On a 64-bit architecture where pointers occupy 8 bytes, passing `0` places 4 bytes on the stack or fills only half a 64-bit register. When the function reads `va_arg(ap, const char *)`, it reads an 8-byte pointer, incorporating 4 bytes of adjacent uninitialized stack garbage. The portable fix is to explicitly cast: `(char *)NULL`.
</details>

<details>
<summary><strong>5. Explain the purpose of GCC/Clang `__attribute__((format(printf, m, n)))`.</strong></summary>

Because C does not verify variadic types against format strings at compile time, passing mismatched arguments (e.g. `%s` with an integer) causes runtime crashes. The `__attribute__((format(printf, m, n)))` informs the compiler that argument `m` is a `printf`-style format string, and arguments starting at `n` correspond to the format specifiers. The compiler then checks the format string against the types of arguments passed at every call site, emitting warnings for mismatches.
</details>

<details>
<summary><strong>6. Can a variadic function forward its arguments to another variadic function directly?</strong></summary>

No. A variadic function cannot forward its `...` directly to another function expecting `...` (such as `printf(...)`). It can only forward the `va_list` handle to a companion function designed to accept `va_list` (such as `vprintf`, `vsnprintf`, or `vfprintf`).
</details>

<details>
<summary><strong>7. How does C23 modify the syntax of variadic functions?</strong></summary>

In ISO C23, the requirement for a named parameter before the ellipsis was eliminated. Prototypes such as `void f(...)` are now legal. Additionally, `va_start` was simplified so that the second parameter (the last named parameter) is optional: `va_start(ap);`.
</details>

<details>
<summary><strong>8. What is the security impact of the `%n` format specifier?</strong></summary>

`%n` does not output text; instead, it writes the number of characters written so far into the memory location pointed to by an `int *` argument. If an attacker controls the format string, they can use repeated format specifiers (`%1000u%n`) to write arbitrary integer values to arbitrary memory locations, allowing remote code execution and stack hijacking.
</details>

---

## 11. Practice Exercises

### Easy: Calculate Integer Average
Write a variadic function `double calculate_average(int count, ...)` that calculates the arithmetic mean of `count` integers.

<details>
<summary>Solution</summary>

```c
#include <stdarg.h>

double calculate_average(int count, ...) {
    if (count <= 0) return 0.0;

    va_list ap;
    va_start(ap, count);
    long long sum = 0;

    for (int i = 0; i < count; i++) {
        sum += va_arg(ap, int);
    }

    va_end(ap);
    return (double)sum / count;
}
```
</details>

### Medium: Concatenate Multiple Strings
Write a function `char *concat_strings(const char *first, ...)` that takes a NULL-terminated list of strings, allocates heap memory for the combined string, and returns it.

<details>
<summary>Solution</summary>

```c
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

char *concat_strings(const char *first, ...) {
    if (!first) return NULL;

    /* First pass: calculate total required length */
    va_list ap1, ap2;
    va_start(ap1, first);
    va_copy(ap2, ap1);

    size_t total_len = strlen(first);
    const char *str = NULL;
    while ((str = va_arg(ap1, const char *)) != NULL) {
        total_len += strlen(str);
    }
    va_end(ap1);

    char *result = (char *)malloc(total_len + 1);
    if (!result) {
        va_end(ap2);
        return NULL;
    }

    strcpy(result, first);
    while ((str = va_arg(ap2, const char *)) != NULL) {
        strcat(result, str);
    }
    va_end(ap2);

    return result;
}
```
</details>

### Hard: Minimal Formatted Printer
Implement `void mini_printf(const char *fmt, ...)` supporting `%d` (signed int), `%s` (string), and `%c` (char) using only `putchar()`.

<details>
<summary>Solution</summary>

```c
#include <stdio.h>
#include <stdarg.h>

static void print_int(int n) {
    if (n < 0) {
        putchar('-');
        /* Handle INT_MIN safely without overflow */
        unsigned int un = (unsigned int)(-(n + 1)) + 1u;
        if (un / 10) print_int((int)(un / 10));
        putchar((char)('0' + (un % 10)));
        return;
    }
    if (n / 10) print_int(n / 10);
    putchar((char)('0' + (n % 10)));
}

void mini_printf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);

    for (const char *p = fmt; *p != '\0'; p++) {
        if (*p != '%') {
            putchar(*p);
            continue;
        }
        p++;
        switch (*p) {
            case 'd': {
                int val = va_arg(ap, int);
                print_int(val);
                break;
            }
            case 's': {
                const char *s = va_arg(ap, const char *);
                if (!s) s = "(null)";
                while (*s) putchar(*s++);
                break;
            }
            case 'c': {
                /* char is promoted to int */
                int ch = va_arg(ap, int);
                putchar(ch);
                break;
            }
            case '%': {
                putchar('%');
                break;
            }
            default: {
                putchar('%');
                putchar(*p);
                break;
            }
        }
    }
    va_end(ap);
}
```
</details>

---

## 12. Summary / Cheat Sheet

| Primitive | Signature | Rule |
| :--- | :--- | :--- |
| `va_list` | Type | State tracker for variable arguments |
| `va_start` | `va_start(ap, last_param)` | Binds cursor to first variable argument |
| `va_arg` | `va_arg(ap, Type)` | Retrieves argument, advances cursor; type must be promoted |
| `va_end` | `va_end(ap)` | Cleans up cursor; mandatory before returning |
| `va_copy` | `va_copy(dst, src)` | Duplicates state for multi-pass traversals |
| `vsnprintf` | `vsnprintf(buf, sz, fmt, ap)` | Standard buffer formatter accepting `va_list` |

---

## 13. Further Reading

- ISO/IEC 9899:2018 (C17) §7.16 (`<stdarg.h>` Variable arguments).
- ISO/IEC 9899:2024 (C23) §7.16 (Variable arguments modernized).
- System V Application Binary Interface: AMD64 Architecture Processor Supplement (Section 3.5.7: Variable Argument Lists).
- SEI CERT C Coding Standard: FIO30-C (Exclude user input from format strings).
- SEI CERT C Coding Standard: DCL00-C (Const-qualify immutable parameter pointers).

---

[← Previous](05-bit-manipulation.md) | [Section Index](README.md) | [Next →](07-text-processing-parsing.md)
