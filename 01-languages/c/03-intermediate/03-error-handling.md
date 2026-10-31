# Error Handling

C has no exceptions. Errors are reported through return values and `errno`, and **you** must check them, propagate them, and release resources on the way out. Consistent conventions are what make C code reliable.

**Prerequisites:** [Functions](../01-fundamentals/08-functions.md), [control flow](../01-fundamentals/07-control-flow.md) (`goto` cleanup), [memory management](../02-core-concepts/08-memory-management.md).

## Four common reporting styles

| Style | Example | Notes |
|---|---|---|
| Sentinel return value | `fopen` → `NULL`; `malloc` → `NULL`; `getchar` → `EOF` | Simple; the sentinel must be impossible as real data |
| Status code + out-parameter | `int parse(const char *s, int *out)` returns 0 or an error | Unambiguous; recommended for new APIs |
| `errno` | Set by library calls on failure | Details below |
| Error enum | `typedef enum { OK, ERR_NOMEM, ERR_IO } status_t;` | Lets callers distinguish causes |

```c
typedef enum { ST_OK = 0, ST_NOMEM, ST_IO, ST_PARSE } status_t;

status_t read_config(const char *path, config_t *out);   /* data via out-param, error via return */
```

Pick one style per project and document it for each function: what it returns on failure, whether `errno` is set, and who owns allocated results.

## `errno`

```c
#include <errno.h>
#include <string.h>

FILE *f = fopen(path, "r");
if (f == NULL) {
    fprintf(stderr, "open %s: %s\n", path, strerror(errno));
    return -1;
}
```

Rules:

- `errno` is meaningful **only right after a call that reported failure.** Successful calls may leave it changed, so never test `errno` alone to detect an error.
- For functions that can legitimately return any value (`strtol`), set `errno = 0` first, then check it afterward.
- Copy it immediately if you call anything else (including `printf`, `free`) before using it.
- `errno` is thread-local on modern systems.
- `perror("msg")` prints `msg: <description>` to `stderr`. `strerror` may return a shared buffer, so use `strerror_r` (POSIX) in multithreaded code.

```c
errno = 0;
long v = strtol(s, &end, 10);
if (end == s || *end != '\0') return ST_PARSE;
if (errno == ERANGE)          return ST_PARSE;     /* out of range */
```

## Check everything that can fail

Calls whose failure you should handle: `malloc`/`calloc`/`realloc`, `fopen`/`fclose`/`fread`/`fwrite`, `scanf` family, `snprintf` truncation, `strtol`, system calls (`read`, `write`, `close`), thread and socket functions. Ignoring them turns a clean failure into corruption three functions later.

When ignoring a result is intentional, say so: `(void)fclose(f);` or a comment.

## Cleanup with `goto`

Release resources in reverse order via a single exit path:

```c
status_t load(const char *path, char **out) {
    status_t st = ST_IO;
    char *buf = NULL;
    FILE *f = fopen(path, "rb");
    if (f == NULL) goto done;

    buf = malloc(4096);
    if (buf == NULL) { st = ST_NOMEM; goto done; }

    if (fread(buf, 1, 4096, f) == 0 && ferror(f)) goto done;

    *out = buf;
    buf = NULL;                 /* ownership transferred to caller */
    st = ST_OK;

done:
    free(buf);                  /* free(NULL) is safe */
    if (f != NULL) fclose(f);
    return st;
}
```

Initialize every resource variable to `NULL` so the cleanup block is always safe.

## Propagating errors

```c
status_t run(void) {
    status_t st;
    if ((st = step_one()) != ST_OK) return st;
    if ((st = step_two()) != ST_OK) return st;
    return ST_OK;
}
```

Add context at the boundary where you can describe what was being attempted ("loading config /etc/app.conf: ...").

## Libraries vs applications

- **Applications** may print an error and exit.
- **Libraries** should **never** call `exit`, `abort`, or print on their own; return an error and let the caller decide. Avoid global error state (or make it thread-local).

## Terminating

| Function | Effect |
|---|---|
| `return` from `main` / `exit(status)` | Normal exit: runs `atexit` handlers, flushes and closes streams |
| `abort()` | Abnormal termination (`SIGABRT`), may dump core; no cleanup |
| `_Exit(status)` | Immediate exit, no cleanup |
| `atexit(fn)` | Registers a function to run at normal exit |

Use `exit(EXIT_FAILURE)` for fatal application errors, after printing a message to `stderr`.

## Other mechanisms

- **`setjmp`/`longjmp`** give non-local jumps, a crude exception substitute. See [setjmp/longjmp](../04-advanced/06-setjmp-longjmp.md). They skip cleanup, so use them rarely.
- **Signals** report asynchronous faults (`SIGSEGV`), not recoverable errors. See [signals](../04-advanced/09-posix/03-signals.md).
- **Assertions** check programmer errors, not runtime errors. See [assertions](./04-assertions-defensive-coding.md).

## Common mistakes

| Mistake | Effect |
|---|---|
| Not checking `malloc` / `fopen` | Crash far from the cause |
| Testing `errno` without a failed call | False errors |
| Calling other functions before reading `errno` | Value overwritten |
| Leaking resources on early return | Use the `goto done` pattern |
| Library code calling `exit` | Callers can't recover |
| Returning ambiguous values (`0` as both valid result and error) | Use out-parameters |
| Error messages to `stdout` | Pollute piped output; use `stderr` |

## Summary

- Report errors through return values (preferably status + out-param); use `errno` for library failures.
- Check every fallible call; clean up through one exit path.
- Libraries return errors; applications decide to exit.
- Assertions are for bugs, error returns are for the environment.

**Next:** [Assertions and defensive coding](./04-assertions-defensive-coding.md)
