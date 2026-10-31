# Command-Line Arguments

Programs receive their command-line arguments through `main`'s parameters. Handling them carefully (validating input, reporting errors, setting exit codes) is what makes a C program usable as a real command-line tool.

**Prerequisites:** [Strings](./02-strings.md), [pointers](./03-pointers.md), [first program](../00-setup/04-first-program.md).

## `argc` and `argv`

```c
#include <stdio.h>

int main(int argc, char *argv[]) {
    for (int i = 0; i < argc; i++) {
        printf("argv[%d] = %s\n", i, argv[i]);
    }
    return 0;
}
```

```bash
$ ./demo hello "two words" 42
argv[0] = ./demo
argv[1] = hello
argv[2] = two words
argv[3] = 42
```

```text
argc = 4
argv ──▶ [ ptr0 ][ ptr1 ][ ptr2 ][ ptr3 ][ NULL ]
            │       │       │       │
          "./demo" "hello" "two words" "42"
```

- `argc` is the number of arguments **including the program name**.
- `argv[0]` is usually the program name (not guaranteed to be a path or even non-empty).
- `argv[argc]` is always `NULL`.
- `argv` is an array of `char *`; its type is equivalent to `char **argv`.
- The **shell** splits the line, expands wildcards (`*.c`), and removes quotes before your program sees the arguments. Your program receives each argument as one string.

The strings may be modified, but you shouldn't count on their length; copy them if you need to change them.

## Validating arguments

```c
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "usage: %s <count>\n", argv[0]);
        return EXIT_FAILURE;
    }

    char *end;
    errno = 0;
    long n = strtol(argv[1], &end, 10);
    if (end == argv[1] || *end != '\0' || errno == ERANGE || n < 0) {
        fprintf(stderr, "%s: invalid count '%s'\n", argv[0], argv[1]);
        return EXIT_FAILURE;
    }

    printf("count = %ld\n", n);
    return EXIT_SUCCESS;
}
```

Conventions worth following:

- Print a **usage** message to `stderr` when arguments are wrong.
- Return a **non-zero exit status** on error, so scripts can detect failure (`echo $?`).
- Convert numbers with `strtol`/`strtod`, not `atoi`, which can't report errors.
- Treat arguments as untrusted input: check lengths before copying into fixed buffers.

## Options with `getopt` (POSIX)

For flags such as `-v` and `-n name`, use `getopt` instead of writing your own parser:

```c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>     /* getopt, optarg, optind (POSIX, not ISO C) */

int main(int argc, char *argv[]) {
    int verbose = 0;
    const char *name = "world";
    int opt;

    while ((opt = getopt(argc, argv, "vn:")) != -1) {
        switch (opt) {
        case 'v': verbose = 1;      break;
        case 'n': name = optarg;    break;     /* "n:" means -n takes a value */
        default:
            fprintf(stderr, "usage: %s [-v] [-n name] [file...]\n", argv[0]);
            return 2;
        }
    }

    if (verbose) fprintf(stderr, "verbose on\n");
    printf("hello, %s\n", name);

    for (int i = optind; i < argc; i++) {       /* remaining (non-option) arguments */
        printf("file: %s\n", argv[i]);
    }
    return 0;
}
```

- `getopt` stops at the first non-option argument (POSIX behavior), and handles grouped flags (`-vn bob`) and the `--` terminator.
- Long options (`--verbose`) need `getopt_long`, available on GNU/Linux, BSDs and macOS but not in POSIX itself.
- On Windows without a POSIX layer, `getopt` is not available. Use a small library or write a loop.

### Manual parsing, for very simple cases

```c
for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--help") == 0) { usage(); return 0; }
    else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) { out = argv[++i]; }
    else { in = argv[i]; }
}
```

Always check `i + 1 < argc` before reading an option's value.

## Environment variables

```c
const char *home = getenv("HOME");     /* returns NULL if unset; don't modify the returned string */
if (home != NULL) printf("%s\n", home);
```

`getenv` is standard C. Some systems also pass a third `envp` argument to `main`, which is non-standard; prefer `getenv`, or the POSIX `environ` variable.

## Exit status

Return `EXIT_SUCCESS` (0) or `EXIT_FAILURE` from `main`, or call `exit(status)`. Many tools reserve specific codes; `2` for usage errors is a common convention. See [first program](../00-setup/04-first-program.md).

## Common mistakes

| Mistake | Effect |
|---|---|
| Reading `argv[1]` without checking `argc` | Reads `NULL` → crash |
| `atoi(argv[1])` | Silent `0` on bad input |
| `strcpy(buf, argv[1])` into a fixed buffer | Buffer overflow |
| Assuming `argv[0]` is the program's path | Not guaranteed |
| Forgetting that the shell expands `*` | Your program gets many arguments |
| Printing usage to `stdout` | Pollutes piped output; use `stderr` |
| Option value read at `argv[i+1]` with no bounds check | Out-of-bounds read |

## Summary

- `argc` counts the program name; `argv[argc]` is `NULL`.
- Validate count and content; use `strtol`; report errors on `stderr` with a non-zero exit code.
- Use `getopt` for flags on POSIX systems.
- Treat all arguments as untrusted data.

**Next:** [File I/O](./11-file-io.md)
