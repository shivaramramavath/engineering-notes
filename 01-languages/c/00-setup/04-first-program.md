# First Program

> Dissecting the minimal C program: standard entry points, the C runtime startup sequence, exit status codes, stream buffering, and the atexit lifecycle.

| Metadata | Details |
|---|---|
| **Level** | Beginner |
| **Prerequisites** | [01 · Toolchain](./01-toolchain.md), [03 · Compilation Process](./03-compilation-process.md) |
| **Time to Read** | 20 minutes |
| **Status** | Complete |

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

Every hosted C program begins execution at a special function designated by the ISO standard: `main`. While writing `printf("Hello, world!\n");` requires only five lines of code, understanding what is occurring under the surface connects your code to the operating system kernel, the standard library runtime, and the process lifecycle.

Writing your first robust program in C requires understanding:
- Why the return type of `main` must strictly be `int`.
- How the shell and automation pipelines interpret process exit status codes.
- How standard streams (`stdin`, `stdout`, `stderr`) buffer data in memory.
- What code executes *before* `main` starts and *after* `main` returns.

---

## 2. Core Concepts

### Standard Signatures of `main`
The ISO C standard allows exactly two portable definitions for `main` in a hosted environment:

```c
/* 1. When the program accepts no command-line arguments */
int main(void) {
    /* ... */
    return 0;
}

/* 2. When the program accepts command-line arguments */
int main(int argc, char *argv[]) {
    /* ... */
    return 0;
}
```

> **C23 Modernization Note:** In C89, C99, C11, and C17, declaring `int main()` with an empty parameter list technically meant the function accepted an *unspecified number of arguments*. In **ISO C23**, an empty parameter list `int main()` is now strictly defined as equivalent to `int main(void)` (accepting zero arguments), matching C++ semantics.

### Exit Status Codes
When `main` finishes, it returns an integer status code to the operating system host environment:
- `0` or `EXIT_SUCCESS` (defined in `<stdlib.h>`): Signals clean, successful execution.
- Non-zero or `EXIT_FAILURE` (defined in `<stdlib.h>`): Signals an error condition.

The parent process (such as a Bash or PowerShell terminal, or a CI/CD runner) inspects this value:
```bash
./my_program
echo $?        # In Bash/Zsh: prints 0 on success, non-zero on failure
echo $LASTEXITCODE # In Windows PowerShell
```

Since C99, if execution reaches the closing brace `}` of `main` without encountering an explicit `return` statement, the compiler implicitly inserts `return 0;`. However, explicit returns are strongly recommended for readability.

---

## 3. How It Works Internally

### The C Runtime (CRT) Startup Lifecycle

`main` is **not** the first function executed when an operating system launches your binary. The actual entry point recognized by the operating system ELF loader is a low-level symbol named `_start` provided by the C Runtime Startup Object (`crt1.o`):

```
+-------------------------------------------------------------------------------+
|                       C RUNTIME PROCESS LIFECYCLE                             |
+-------------------------------------------------------------------------------+
|  1. OS Kernel execve()    | Creates address space, maps ELF segments          |
|                           | Sets up stack with argc, argv, environ pointers   |
|            │                                                                  |
|            ▼                                                                  |
|  2. _start (crt1.o)       | Platform assembly entry point                     |
|            │              | Clears base pointer frame (RBP = 0)               |
|            ▼                                                                  |
|  3. __libc_start_main()   | Initializes threading, locale, stack canary      |
|                           | Constructs stdin, stdout, stderr FILE streams     |
|            │              | Runs global constructors (.init / .init_array)    |
|            ▼                                                                  |
|  4. main(argc, argv)      | *** YOUR CODE EXECUTES HERE ***                   |
|            │                                                                  |
|            ▼                                                                  |
|  5. exit(status)          | Called with return value of main()                |
|                           | Executes functions registered via atexit()        |
|                           | Flushes and closes all open standard streams      |
|            │                                                                  |
|            ▼                                                                  |
|  6. sys_exit()            | Invokes kernel termination syscall (exit_group)   |
+-------------------------------------------------------------------------------+
```

Because `exit()` is invoked upon returning from `main`, all buffered output streams are automatically flushed and all resources registered with `atexit()` are safely dispatched.

---

## 4. Syntax & Usage

### 1. Minimal Hello World
```c
#include <stdio.h>

int main(void) {
    printf("Hello, world!\n");
    return 0;
}
```
- `#include <stdio.h>`: Directs the preprocessor to include standard input/output function declarations, specifically `printf`.
- `\n`: The newline character. On interactive terminal devices, `stdout` is line-buffered; omitting `\n` means text may remain stuck in the internal buffer until the program exits.

### 2. Reading Input Safely with `fgets`
Never use the deprecated and removed `gets()` function. Always bound input reads using `fgets`:

```c
#include <stdio.h>
#include <string.h>

int main(void) {
    char name[64];

    printf("Enter your name: ");
    if (fgets(name, sizeof(name), stdin) != NULL) {
        /* Strip trailing newline retained by fgets */
        name[strcspn(name, "\n")] = '\0';
        printf("Hello, %s!\n", name);
    } else {
        fprintf(stderr, "Error reading input.\n");
        return 1;
    }

    return 0;
}
```

### 3. Registering Exit Cleanup Handlers (`atexit`)
```c
#include <stdio.h>
#include <stdlib.h>

void on_shutdown(void) {
    puts("Flushing application state and releasing locks...");
}

int main(void) {
    atexit(on_shutdown);
    puts("Application running...");
    return EXIT_SUCCESS;
}
```

---

## 5. Practical Examples

### Diagnostic Program: Complete Program Lifecycle & Exit Handlers
The companion source file is available at [`examples/04-first-program.c`](./examples/04-first-program.c).

#### Goal
Demonstrate standard stream buffering characteristics, portable exit status codes, and C Runtime teardown hooks using `atexit()`.

#### Code
```c
#include <stdio.h>
#include <stdlib.h>

static void cleanup_hook(void) {
    printf("[CRT] atexit registered handler executed successfully.\n");
}

int main(void) {
    /* Register cleanup function with C Runtime exit sequence */
    if (atexit(cleanup_hook) != 0) {
        fprintf(stderr, "Failed to register exit handler.\n");
        return EXIT_FAILURE;
    }

    printf("=== First Program: Execution & Lifecycle ===\n");
    printf("Greeting: Hello, robust C world!\n");
    printf("Standard Output: Line buffered on interactive terminals.\n");
    printf("Standard Error:  Unbuffered for critical diagnostics.\n");
    printf("Exit Code:       EXIT_SUCCESS (%d)\n", EXIT_SUCCESS);

    return EXIT_SUCCESS;
}
```

#### Compilation & Execution
```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -Werror examples/04-first-program.c -o run_04_firstprog
./run_04_firstprog
```

#### Real Verified Output
```text
=== First Program: Execution & Lifecycle ===
Greeting: Hello, robust C world!
Standard Output: Line buffered on interactive terminals.
Standard Error:  Unbuffered for critical diagnostics.
Exit Code:       EXIT_SUCCESS (0)
[CRT] atexit registered handler executed successfully.
```

---

## 6. Best Practices

1. **Always Use `int main(void)` or `int main(int argc, char *argv[])`:** Never use non-standard extensions like `void main()`. `void main()` is non-conforming on hosted implementations and can leave garbage in the CPU return register (`eax`), signaling spurious failures to parent shells.
2. **Always Terminate Terminal Output with `\n`:** Terminal streams (`stdout`) are line-buffered by default. If your string lacks a trailing `\n`, the characters remain in the C library user-space buffer and may not appear on the screen immediately.
3. **Send Diagnostics to `stderr`:** Diagnostic messages, warnings, and errors must be printed to `stderr` using `fprintf(stderr, ...)` or `fputs(..., stderr)`. `stderr` is unbuffered, ensuring critical error messages are visible even if the program terminates abnormally immediately afterward.
4. **Use `<stdlib.h>` Constants for Exit Status:** Use `EXIT_SUCCESS` and `EXIT_FAILURE` to guarantee portable status reporting across both POSIX and non-POSIX environments (such as OpenVMS).

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Breaks | Fix |
|---|---|---|
| `void main()` | The OS expects an integer return code in the `eax` register; returning `void` causes undefined exit behavior. | Strictly declare `int main(void)`. |
| Missing `#include <stdio.h>` | Triggers `implicit declaration of built-in function 'printf'`. | Always include headers for library functions you call. |
| Using `gets(buf)` | `gets()` has no buffer length parameter and was officially removed from C in C11 due to universal buffer overflow vulnerability. | Use `fgets(buf, sizeof(buf), stdin)`. |
| Running `hello` instead of `./hello` | On Unix systems, the current working directory `.` is intentionally excluded from `PATH` for security reasons. | Execute using the explicit path `./hello`. |
| Forgetting that `fgets()` retains `\n` | `fgets` copies the newline into your buffer, corrupting string comparisons or file paths. | Strip newline: `buf[strcspn(buf, "\n")] = '\0';`. |

---

## 8. Performance & Security Notes

- **Stream Buffering Performance:**
  - `stdout` connected to a terminal is **Line-Buffered**: it flushes whenever `\n` is printed or user input is requested.
  - `stdout` redirected to a file or pipe (`./app > out.txt`) automatically switches to **Block-Buffered** ($4\text{ KB}$–$8\text{ KB}$ chunks). This minimizes expensive `write()` system calls, increasing throughput by orders of magnitude.
  - `stderr` is always **Unbuffered**: every byte is dispatched immediately via a write system call.
- **Controlling Buffering with `setvbuf()`:**
  If high-throughput logging requires custom buffer sizing:
  ```c
  char custom_buf[65536];
  setvbuf(stdout, custom_buf, _IOFBF, sizeof(custom_buf));
  ```

---

## 9. Comparison / When to Use What

### Console Output Functions

| Function | Usage | Newline Added? | Target Stream | Safety |
|---|---|---|---|---|
| `printf("format %d", x)` | Formatted output with specifiers (`%d`, `%s`) | No | `stdout` | Vulnerable if format string is user-controlled (`printf(str)`) |
| `puts("text")` | Fast output of static or pre-formatted strings | **Yes** (automatic `\n`) | `stdout` | Safe and fast (no format parsing overhead) |
| `fputs("text", stderr)` | Clean string output to specific stream | No | Specified `FILE*` | Safe and explicit |

---

## 10. Interview Questions

<details>
<summary><strong>1. Is it legal for a C program to call <code>main()</code> recursively?</strong></summary>

**Yes, in C, calling `main()` recursively is completely valid and conforming.** 

Unlike C++ (where ISO C++ [basic.start.main] explicitly forbids calling `main()` or taking its address), ISO C allows `main` to be called recursively or referenced via a function pointer just like any other user-defined function. However, doing so is generally considered bad practice because it complicates stack termination and re-initialization.
</details>

<details>
<summary><strong>2. What is the exact difference between returning from <code>main</code> and calling <code>exit()</code>?</strong></summary>

In `main`, executing `return status;` and calling `exit(status);` produce the identical result: both invoke all registered `atexit()` handlers, flush all open standard I/O streams, close temporary files created with `tmpfile()`, and pass `status` to the OS.

However, in any **helper function** down the call stack:
- `return status;` only pops the current stack frame and returns control to the caller.
- `exit(status);` immediately halts execution of the entire program from any nesting depth, running all cleanup handlers without unwinding preceding stack frames.
</details>

<details>
<summary><strong>3. How does <code>_exit()</code> / <code>_Exit()</code> differ from standard <code>exit()</code>?</strong></summary>

- `exit()` is the standard library termination routine: it runs `atexit()` handlers and flushes standard I/O stream buffers before terminating.
- `_Exit()` (C99 standard) and `_exit()` (POSIX) terminate the process immediately at the kernel level: **no `atexit()` handlers are executed, and unwritten stdio buffers are NOT flushed**. It is used in child processes after `fork()` to prevent flushing shared file buffers twice.
</details>

---

## 11. Practice Exercises

### Exercise 1 (Easy): Standard Error Redirection
**Goal:** Write a program that prints "Normal log" to `stdout` and "Fatal alert" to `stderr`. Run it from the terminal and demonstrate redirecting only `stdout` to a file while letting `stderr` display on the screen.

<details>
<summary>View Solution</summary>

```c
#include <stdio.h>

int main(void) {
    fprintf(stdout, "Normal log message\\n");
    fprintf(stderr, "Fatal alert message\\n");
    return 0;
}
```
Run:
```bash
gcc -std=c17 ex1.c -o ex1
./ex1 > output.txt
```
*Output on screen: `Fatal alert message`.*  
*Content in `output.txt`: `Normal log message`.*
</details>

### Exercise 2 (Medium): Robust Command-Line Argument Validator
**Goal:** Write a program using `int main(int argc, char *argv[])` that requires exactly one command-line argument. If the argument is missing, print usage instructions to `stderr` and exit with `EXIT_FAILURE`.

<details>
<summary>View Solution</summary>

```c
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <target-name>\\n", argv[0]);
        return EXIT_FAILURE;
    }

    printf("Target configured: %s\\n", argv[1]);
    return EXIT_SUCCESS;
}
```
</details>

### Exercise 3 (Hard): Multiple LIFO Exit Cleanups
**Goal:** Write a program that registers three distinct cleanup functions using `atexit()`. Verify and demonstrate that registered handlers are executed in strict reverse order of registration (LIFO - Last In, First Out).

<details>
<summary>View Solution</summary>

```c
#include <stdio.h>
#include <stdlib.h>

void step_one(void)   { puts("Cleanup: Step 1 executed."); }
void step_two(void)   { puts("Cleanup: Step 2 executed."); }
void step_three(void) { puts("Cleanup: Step 3 executed."); }

int main(void) {
    atexit(step_one);
    atexit(step_two);
    atexit(step_three);

    puts("Main program work complete. Exiting...");
    return EXIT_SUCCESS;
}
```
*Output:*
```text
Main program work complete. Exiting...
Cleanup: Step 3 executed.
Cleanup: Step 2 executed.
Cleanup: Step 1 executed.
```
</details>

---

## 12. Summary / Cheat Sheet

```c
// Standard minimal entry point
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    puts("Hello, World!");
    return EXIT_SUCCESS;
}

// Entry point with arguments
int main(int argc, char *argv[]) {
    printf("Invoked as: %s with %d arguments\\n", argv[0], argc - 1);
    return EXIT_SUCCESS;
}
```

```bash
# Compile with strict flags
gcc -std=c17 -Wall -Wextra -Wpedantic -Werror main.c -o app

# Check exit code
./app; echo $?
```

---

## 13. Further Reading

- ISO/IEC 9899:2018 (C17 Standard), Section 5.1.2.2.1: "Program startup" and Section 7.22.4.4: "The exit function".
- Kerrisk, Michael. *The Linux Programming Interface*, Chapter 6: "Processes" and Chapter 13: "File I/O Buffering".
- SEI CERT C Coding Standard: *ENV32-C. All exit handlers must return normally*.

---

## Navigation
- **Previous:** [03 · Compilation Process](./03-compilation-process.md)
- **Section Index:** [00 · Setup Index](./README.md)
- **Next:** [05 · C Standards & History](./05-c-standards-history.md)
