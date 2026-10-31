# Standard Library

The C standard library is small and lives in a set of headers. Knowing what it contains saves you from rewriting basics, and knowing what to *avoid* saves you from its worst functions. This note is a map; follow the links for details.

**Prerequisites:** [Core concepts](../02-core-concepts/01-arrays.md).

## Header map

| Header | Contents | Notes |
|---|---|---|
| `<stdio.h>` | streams, `printf`/`scanf` families, `fopen`, `fgets`, `snprintf` | [I/O](../01-fundamentals/09-input-output.md), [files](../02-core-concepts/11-file-io.md) |
| `<stdlib.h>` | `malloc`/`free`, `exit`, `qsort`, `bsearch`, `strtol`, `rand`, `getenv`, `abs` | [memory](../02-core-concepts/08-memory-management.md) |
| `<string.h>` | `str*` and `mem*` functions | [strings](../02-core-concepts/02-strings.md) |
| `<ctype.h>` | `isalpha`, `isdigit`, `toupper`, ... | cast `char` to `unsigned char` first |
| `<math.h>` | `sqrt`, `pow`, `sin`, `floor`, `fabs`, `isnan` | link with `-lm` on many systems |
| `<limits.h>`, `<float.h>` | `INT_MAX`, `CHAR_BIT`, `DBL_EPSILON` | type limits |
| `<stdint.h>`, `<inttypes.h>` | fixed-width ints, `PRIu64` format macros | [data types](../01-fundamentals/02-data-types.md) |
| `<stddef.h>` | `size_t`, `ptrdiff_t`, `NULL`, `offsetof` | |
| `<stdbool.h>` | `bool`, `true`, `false` | keywords in C23 |
| `<assert.h>` | `assert`, `static_assert` | [assertions](./04-assertions-defensive-coding.md) |
| `<errno.h>` | `errno` and error constants | [error handling](./03-error-handling.md) |
| `<stdarg.h>` | variadic function support | [variadic functions](./06-variadic-functions.md) |
| `<time.h>` | `time`, `strftime`, `clock` | [time](./09-time-and-random.md) |
| `<signal.h>` | `signal`, `raise` | [signals](../04-advanced/09-posix/03-signals.md) |
| `<setjmp.h>` | `setjmp`, `longjmp` | [setjmp/longjmp](../04-advanced/06-setjmp-longjmp.md) |
| `<locale.h>`, `<wchar.h>`, `<uchar.h>` | locale, wide/Unicode chars | [Unicode](./08-unicode-locale.md) |
| `<stdatomic.h>`, `<threads.h>` | atomics, threads (both optional in C11) | [concurrency](../04-advanced/08-concurrency/01-pthreads.md) |
| `<stdbit.h>`, `<stdckdint.h>` | bit utilities, checked arithmetic (C23) | |

POSIX adds more (`<unistd.h>`, `<pthread.h>`, `<sys/socket.h>`, ...), but those aren't ISO C. See the [POSIX quick reference](../12-reference/05-posix-quick-reference.md).

## Functions worth knowing

### Sorting and searching (`<stdlib.h>`)

```c
qsort(arr, n, sizeof arr[0], cmp);            /* comparator: see function pointers */
void *hit = bsearch(&key, arr, n, sizeof arr[0], cmp);
```

See [function pointers](./01-function-pointers.md) for comparator rules.

### Numeric conversion (`<stdlib.h>`)

`strtol`, `strtoul`, `strtoll`, `strtod`: convert with error detection. Use these instead of `atoi`/`atof`.

### Integer helpers

`abs`, `labs`, `llabs` (undefined for the most negative value); `div`/`ldiv` return quotient and remainder together.

### Math (`<math.h>`)

```c
#include <math.h>
double r = sqrt(2.0);
double p = pow(2.0, 10.0);                    /* 1024.0 */
```

Compile with `-lm` where required. Domain errors (like `sqrt(-1)`) produce `NaN` and may set `errno`; test with `isnan`/`isfinite`. Compare doubles with a tolerance, not `==`. See [data types](../01-fundamentals/02-data-types.md).

### Character classification (`<ctype.h>`)

```c
if (isdigit((unsigned char)c)) { ... }
```

These work on single bytes in the current locale, not on Unicode.

### Program control (`<stdlib.h>`)

`exit`, `atexit`, `abort`, `getenv`. `system(cmd)` runs a shell command; it's convenient and dangerous with any untrusted input (command injection).

### Memory block functions (`<string.h>`)

`memcpy` (no overlap), `memmove` (overlap OK), `memset`, `memcmp`, `memchr`.

## Functions to avoid, and what to use instead

| Avoid | Problem | Use |
|---|---|---|
| `gets` | cannot be used safely; removed in C11 | `fgets` |
| `strcpy`, `strcat` | no bounds checking | `snprintf`, or `memcpy` with a checked length |
| `sprintf` | no bounds checking | `snprintf` |
| `scanf("%s", ...)` | unbounded write | `fgets` + parse, or `%63s` with a width |
| `atoi`, `atof` | no error reporting | `strtol`, `strtod` |
| `strncpy` | may not terminate; pads with zeros | `snprintf` or manual termination |
| `strtok` | modifies input, hidden global state | `strtok_r`, `strsep`, or manual parsing |
| `rand` for anything important | weak and predictable | a better PRNG, or the OS CSPRNG for security |
| `signal` (portable code) | behavior varies across systems | `sigaction` (POSIX) |
| `system` with external input | command injection | `fork`/`exec` with an argument vector |
| `tmpnam`, `mktemp` | race conditions | `mkstemp` (POSIX) |
| `gmtime`, `localtime`, `strerror` in threads | shared static buffer | `_r` variants |

## Annex K (`_s` functions)

C11 defines optional "bounds-checking" functions such as `strcpy_s` and `fopen_s`. Almost no implementation outside MSVC supports them, and they are widely considered a failed experiment. Don't rely on them in portable code.

## Standard library vs. platform and third-party code

- The standard library has **no** containers (lists, maps), no networking, no threads on many compilers, no Unicode handling, and no filesystem traversal. Use POSIX, platform APIs, or libraries for those.
- Everyday needs beyond libc: **zlib** (compression), **OpenSSL/libsodium** (cryptography), **libcurl** (HTTP), **SQLite** (database), **ICU** (Unicode), **jansson/cJSON** (JSON).
- A library function's behavior may differ between libc implementations (glibc, musl, BSD libc, MSVC CRT). Check the standard or the manual (`man 3 function`) instead of assuming.

## Finding documentation

```bash
man 3 qsort          # section 3 = library functions
man 2 open           # section 2 = system calls
man 7 signal-safety  # overview pages
```

Online: the C standard drafts (N-documents), cppreference's C section, and the POSIX specification. See the [libc quick reference](../12-reference/04-libc-quick-reference.md) and [resources](../12-reference/10-resources.md).

## Summary

- Know the header map; most things you need are in `stdio`, `stdlib`, `string`, `ctype`, and `math`.
- Prefer `fgets`, `snprintf`, `strtol`, `memmove`/`memcpy` with checked sizes.
- Avoid the dangerous functions in the table above.
- For containers, networking, threads, and Unicode, use POSIX or libraries.

**Next:** [Advanced: undefined behavior](../04-advanced/01-undefined-behavior.md)
