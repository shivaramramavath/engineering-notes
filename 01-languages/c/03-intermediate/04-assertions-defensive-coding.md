# Assertions and Defensive Coding

Not every problem is a runtime error to recover from. Some are **bugs**: a function was called with an impossible argument, or an invariant no longer holds. Assertions catch these early and loudly; defensive coding handles the failures you cannot prevent.

**Prerequisites:** [Error handling](./03-error-handling.md).

## `assert`

```c
#include <assert.h>

double average(const int *a, size_t n) {
    assert(a != NULL);
    assert(n > 0);
    long sum = 0;
    for (size_t i = 0; i < n; i++) sum += a[i];
    return (double)sum / (double)n;
}
```

If the condition is false, the program prints the expression, file, line and function to `stderr`, then calls `abort()`.

**Disabled by `NDEBUG`:** compiling with `-DNDEBUG` (typical for release builds) turns `assert(...)` into nothing. Therefore:

```c
assert(fclose(f) == 0);     /* WRONG: the call vanishes in release builds */

int rc = fclose(f);
assert(rc == 0);            /* the side effect always happens */
(void)rc;                   /* avoid an unused-variable warning when NDEBUG is set */
```

Never put side effects inside `assert`.

## Assertion or error handling?

| Situation | Use |
|---|---|
| Caller passed `NULL` to a function documented as non-null | `assert` (programmer error) |
| An internal invariant (sorted list is sorted, index in range) | `assert` |
| File not found, network down, malformed user input | Return an error: never `assert` |
| `malloc` returned `NULL` | Handle it: never `assert` |
| Impossible `default:` branch in a `switch` | `assert(0)` or unreachable marker |

Rule of thumb: **assertions document what must be true if the program is correct; errors report what can go wrong even when it is correct.** External input is never trusted by an assertion.

Public library functions often validate arguments at runtime and return an error code rather than aborting, because the library can't control its callers. Internal helpers can simply assert.

## Compile-time assertions

Catch impossible configurations at build time, with zero runtime cost:

```c
#include <assert.h>                       /* C11: static_assert macro; C23: keyword */

static_assert(sizeof(int) == 4, "this code assumes 32-bit int");
static_assert(sizeof(struct header) == 16, "wire header layout changed");
```

C11 spells the keyword `_Static_assert`; `<assert.h>` provides the `static_assert` macro, and C23 makes it a keyword. The condition must be a constant expression. Use it for struct sizes, array lengths, enum ranges, and platform assumptions.

## Marking unreachable code

```c
switch (kind) {
case A: return 1;
case B: return 2;
default:
    assert(!"unreachable");               /* message appears in the failure output */
    return -1;                            /* still return something for NDEBUG builds */
}
```

C23 adds `unreachable()` in `<stddef.h>`, and GCC/Clang offer `__builtin_unreachable()`. Both promise the compiler the path is never taken; if you're wrong, behavior is undefined, so pair them with an assert in debug builds.

## Defensive programming

Assertions find bugs during development. Defensive code keeps the program safe against inputs and states you can't control.

- **Validate at trust boundaries:** command-line arguments, files, network data, environment variables. Check lengths, ranges, and formats before use. See [input validation](../07-performance-security/05-input-validation.md).
- **Initialize everything**; set pointers to `NULL` after `free`.
- **Use bounded functions** (`snprintf`, `fgets`) and pass buffer sizes along with pointers.
- **Check arithmetic** that feeds allocation sizes and indexes. See [integer representation](../01-fundamentals/03-integer-representation.md).
- **Fail fast:** when state is corrupt, stop rather than continue producing wrong results.
- **Keep functions small and their contracts explicit:** document preconditions (what callers must ensure), postconditions (what you guarantee), and ownership.
- **Compile with warnings** (`-Wall -Wextra`) and **test with sanitizers** ([sanitizers](../06-debugging-testing/04-sanitizers-valgrind.md)).

Don't over-defend: checking the same condition at every layer clutters code and can mask real bugs. Decide where validation happens and trust data after it has been validated.

## A custom check macro

```c
#define REQUIRE(cond)                                                        \
    do {                                                                     \
        if (!(cond)) {                                                       \
            fprintf(stderr, "%s:%d: requirement failed: %s\n",               \
                    __FILE__, __LINE__, #cond);                              \
            abort();                                                         \
        }                                                                    \
    } while (0)
```

Unlike `assert`, `REQUIRE` stays active in release builds, appropriate for checks that guard against security-relevant corruption. Use sparingly.

## Common mistakes

| Mistake | Effect |
|---|---|
| Side effects inside `assert` | Behavior differs between debug and release |
| Asserting on user or file input | A crash instead of an error message |
| Assuming asserts run in production | They don't if `NDEBUG` is defined |
| Using `assert` instead of handling `malloc` failure | Release builds dereference `NULL` |
| `assert(0)` as the only code in a branch | Falls through when disabled; add a safe return |

## Summary

- `assert` checks programmer assumptions; it vanishes under `NDEBUG`, so never rely on its side effects.
- Errors from the outside world get error handling, not assertions.
- `static_assert` verifies assumptions at compile time.
- Validate at boundaries, fail fast on corruption, and let the tooling catch the rest.

**Next:** [Bit manipulation](./05-bit-manipulation.md)
