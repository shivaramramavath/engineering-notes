# Input and Output

C's standard I/O (`<stdio.h>`) works on **streams**: buffered sequences of bytes. `printf` and `scanf` look simple, but format strings are not type-checked at runtime, and `scanf` has enough traps that most production code avoids it.

**Prerequisites:** [Data types](./02-data-types.md), [type conversions](./06-type-conversions.md).

## The three standard streams

| Stream | Constant | Default | Use |
|---|---|---|---|
| Standard input | `stdin` | keyboard | reading data |
| Standard output | `stdout` | terminal | normal results |
| Standard error | `stderr` | terminal | errors and diagnostics |

Write errors to `stderr` so they don't pollute output that is piped to another program:

```c
fprintf(stderr, "error: cannot open %s\n", path);
```

## Output: `printf`

```c
printf("%s is %d years old, %.1f m tall\n", name, age, height);
```

### Common format specifiers

| Specifier | Type |
|---|---|
| `%d` / `%i` | `int` |
| `%u` | `unsigned int` |
| `%ld` / `%lu` | `long` / `unsigned long` |
| `%lld` / `%llu` | `long long` / `unsigned long long` |
| `%zu` / `%zd` | `size_t` / `ssize_t` (`ptrdiff_t` uses `%td`) |
| `%f` | `double` (`%.2f` = 2 decimals); `float` is promoted to `double` |
| `%e`, `%g` | scientific / shortest form |
| `%c` | `int` printed as a character |
| `%s` | `char *` (null-terminated string) |
| `%p` | `void *`; cast other pointers: `(void *)ptr` |
| `%x`, `%X`, `%o` | `unsigned int` in hex / octal |
| `%%` | a literal `%` |

Width and flags: `%5d` (pad to 5), `%-5d` (left-justify), `%05d` (zero-pad), `%.3s` (at most 3 characters), `%*d` (width from an argument).

For fixed-width types use the macros from `<inttypes.h>`:

```c
#include <inttypes.h>
uint64_t v = 42;
printf("%" PRIu64 "\n", v);
```

### The specifier must match the argument

A mismatch is **undefined behavior** (garbage output or a crash), and the compiler can't always tell you:

```c
printf("%d\n", 3.14);        /* wrong: double passed for %d */
printf("%d\n", sizeof(int)); /* wrong: size_t needs %zu */
printf("%s\n", 42);          /* crash: 42 treated as a pointer */
```

`-Wall` enables `-Wformat`, which checks literal format strings. Never ignore those warnings.

### Never pass user input as the format string

```c
printf(user_input);          /* format-string vulnerability: attacker can use %s, %n */
printf("%s", user_input);    /* correct */
```

### Related functions

```c
puts("text");                       /* writes text plus newline */
putchar('x');
fprintf(f, "...", ...);             /* any stream */
snprintf(buf, sizeof buf, "%d-%d", a, b);   /* bounded write into a buffer */
```

Prefer `snprintf` over `sprintf`, which can overflow the buffer. `snprintf` returns the length the full output **would** have had, so a return value `>= sizeof buf` means truncation.

## Buffering

```text
program ──▶ stdio buffer ──▶ OS ──▶ terminal / file / pipe
```

- To a **terminal**, `stdout` is line-buffered (flushes at `\n`).
- To a **file or pipe**, it is fully buffered; output may appear much later, and be lost if the program crashes.
- `stderr` is unbuffered.
- `fflush(stdout)` forces a flush. A prompt without `\n` needs one before reading input on some systems.

This is why debug output via `printf` can seem to "vanish" before a crash. Use `stderr` or flush.

## Input: reading safely

### Preferred pattern: `fgets` then parse

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

int main(void) {
    char line[128];

    printf("Enter a number: ");
    if (fgets(line, sizeof line, stdin) == NULL) {
        return 1;                              /* EOF or error */
    }
    line[strcspn(line, "\n")] = '\0';          /* strip trailing newline */

    char *end;
    errno = 0;
    long v = strtol(line, &end, 10);
    if (end == line || *end != '\0' || errno == ERANGE) {
        fputs("not a valid number\n", stderr);
        return 1;
    }
    printf("You entered %ld\n", v);
    return 0;
}
```

- `fgets` reads at most `size - 1` characters and always terminates the string. It stores the newline if it fit.
- If the line was longer than the buffer, the rest stays in the stream. Check whether the buffer contains `'\n'`.
- `strtol` / `strtod` parse numbers and report errors; `atoi` cannot report errors.
- `gets` was removed in C11. Never use it.

### `scanf`

```c
int x;
if (scanf("%d", &x) != 1) { /* parse failed or EOF */ }
```

Rules if you do use it:

- **Always check the return value** (number of items assigned, or `EOF`).
- Pass **addresses** (`&x`). `%s` takes an array and no `&`.
- **Always bound `%s`**: `char buf[64]; scanf("%63s", buf);`. Plain `%s` can overflow.
- `scanf` leaves the newline in the input, which breaks a following `fgets` or `%c`. A failed conversion also leaves the bad characters in the stream, so loops can spin forever.
- `%f` reads `float`; use `%lf` for `double` in `scanf` (unlike `printf`).

For anything beyond a throwaway program, use `fgets` + `strtol`/`sscanf`.

### Character-at-a-time

```c
int c;                                  /* int, not char: must hold EOF */
while ((c = getchar()) != EOF) {
    putchar(c);
}
```

Using `char c` breaks on platforms where `char` is unsigned (EOF never matches) or when a valid byte `0xFF` equals `EOF` on signed `char`.

## Binary vs text

Text I/O (`"r"`, `"w"`) may translate line endings on some platforms. For binary data use `"rb"` / `"wb"` with `fread` / `fwrite`. See [file I/O](../02-core-concepts/11-file-io.md).

## Common mistakes

| Mistake | Consequence |
|---|---|
| Wrong format specifier | Undefined behavior |
| `printf(str)` with external data | Format-string attack |
| `scanf("%s", buf)` without width | Buffer overflow |
| Not checking `scanf` / `fgets` return value | Using uninitialized data, infinite loops |
| `char c = getchar()` | EOF mishandled |
| Leftover `'\n'` after `scanf` | Next `fgets` returns an empty line |
| `sprintf` into a fixed buffer | Overflow; use `snprintf` |
| Missing `\n` or `fflush` | Output appears late or never after a crash |

## Summary

- Errors go to `stderr`; results to `stdout`.
- Format specifiers must exactly match argument types; use `%zu` for `size_t`.
- Read lines with `fgets`, convert with `strtol`/`strtod`, and check every return value.
- Use `snprintf`, never `sprintf` or `gets`; never pass untrusted text as a format string.

**Next:** [Core concepts: arrays](../02-core-concepts/01-arrays.md)
