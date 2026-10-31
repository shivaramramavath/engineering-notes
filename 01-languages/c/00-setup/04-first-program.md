# First Program

A complete C program needs one function named `main`. This note builds, runs and explains the smallest useful program, and covers the mistakes almost everyone makes in the first week.

**Prerequisites:** a working [toolchain](./01-toolchain.md).

## Hello, world

```c
#include <stdio.h>

int main(void) {
    printf("Hello, world!\n");
    return 0;
}
```

```bash
gcc -std=c17 -Wall -Wextra hello.c -o hello
./hello
```

```text
Hello, world!
```

On Windows (native), the output file is `hello.exe`, and you run it with `hello` or `.\hello`.

## Line by line

- `#include <stdio.h>`: pastes in the declarations for standard input/output, including `printf`. The preprocessor handles this (see [compilation process](./03-compilation-process.md)).
- `int main(void)`: the entry point. The OS starts your program by calling `main`. `int` is the return type; `void` means it takes no arguments.
- `printf("...\n")`: prints formatted text. `\n` is a newline; without it, the shell prompt appears on the same line.
- `return 0;`: the **exit status**. `0` means success; any non-zero value means failure.

Since C99, reaching the closing brace of `main` is equivalent to `return 0;`, so you may omit it. It is still common to write it explicitly, and it is required for other functions.

## Exit status

```bash
./hello
echo $?     # prints 0
```

Shells, scripts and CI systems use this number to decide whether a command succeeded. Use `EXIT_SUCCESS` and `EXIT_FAILURE` from `<stdlib.h>` when you want portable, readable code.

```c
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    FILE *f = fopen("config.txt", "r");
    if (f == NULL) {
        fputs("fatal: cannot open config.txt\n", stderr);
        return EXIT_FAILURE;
    }
    fclose(f);
    return EXIT_SUCCESS;
}
```

## Taking input

```c
#include <stdio.h>

int main(void) {
    char name[64];

    printf("Name: ");
    if (fgets(name, sizeof name, stdin) == NULL) {
        return 1;
    }
    printf("Hello, %s", name);   /* fgets keeps the newline */
    return 0;
}
```

Use `fgets` with a size limit. Never use `gets`: it cannot be made safe and was removed from the language in C11.

## How it runs

```text
shell ──▶ OS loads ./hello ──▶ C runtime startup code ──▶ main() ──▶ exit(status)
                                                                      │
                                      shell reads status via $? ◀─────┘
```

`main` is not the very first code to run. The C runtime initializes the environment, calls `main`, then calls `exit` with whatever `main` returned. That is why returning from `main` flushes buffered output and closes files.

## Common mistakes

| Mistake | Effect | Fix |
|---|---|---|
| Missing `\n` | Prompt glued to output; with piped output, text may appear late | End lines with `\n` |
| Forgetting `#include <stdio.h>` | "implicit declaration" error or warning | Include the header |
| Writing `int main()` | In C before C23, `()` means "unspecified arguments", not "none" | Write `int main(void)` |
| `void main()` | Not standard on hosted systems | Always `int main` |
| Running `hello` instead of `./hello` | `command not found`; the current directory isn't in `PATH` | Use `./hello` |
| Ignoring warnings | Bugs that compile silently | Always build with `-Wall -Wextra` |

## Debugging a first program

- **Nothing printed:** check you actually ran the new binary, and that the compile step succeeded without errors.
- **Output missing after a crash:** `stdout` is buffered when not attached to a terminal. Print debug messages to `stderr` (unbuffered) or call `fflush(stdout)`.
- **Strange characters in output:** a format specifier doesn't match its argument type (for example `%d` with a `double`). Warnings from `-Wall` catch this.

## Summary

- Every hosted C program starts at `int main(void)`.
- The return value is the exit status: `0` = success.
- Compile with `-std=c17 -Wall -Wextra` from day one.
- Use `fgets`, never `gets`.

**Next:** [C standards and history](./05-c-standards-history.md), then [program structure](../01-fundamentals/01-program-structure.md)
