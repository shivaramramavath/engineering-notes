# Time and Random Numbers

Time functions measure and format dates; random functions generate numbers. Both have traps: wall-clock time can jump, `rand()` is low quality, and several standard functions use hidden static state.

**Prerequisites:** [Data types](../01-fundamentals/02-data-types.md), [error handling](./03-error-handling.md).

## Kinds of time

| Need | Use | Why |
|---|---|---|
| Current date/time ("wall clock") | `time()` | calendar time in seconds; can jump (NTP, manual change) |
| Measure elapsed time precisely | `clock_gettime(CLOCK_MONOTONIC)` (POSIX) | never goes backward |
| Measure CPU time consumed | `clock()` | processor time, not wall time |
| Portable sub-second wall time | `timespec_get` (C11) | resolution varies |
| Pause | `nanosleep` (POSIX) | `sleep` has 1-second granularity |

## Calendar time

```c
#include <time.h>

time_t now = time(NULL);                 /* seconds since the epoch (usually 1970-01-01 UTC) */

struct tm *utc   = gmtime(&now);         /* broken-down UTC time */
struct tm *local = localtime(&now);      /* broken-down local time (timezone, DST) */

printf("%04d-%02d-%02d\n", local->tm_year + 1900, local->tm_mon + 1, local->tm_mday);
```

`struct tm` fields: `tm_year` counts from 1900, `tm_mon` is **0-11**, `tm_mday` is 1-31, `tm_wday` is 0 (Sunday) - 6, `tm_isdst` is the daylight-saving flag.

**`gmtime` and `localtime` return a pointer to shared static storage**: a second call overwrites the first, and they're not thread-safe. Use `gmtime_r` / `localtime_r` (POSIX) with your own `struct tm`:

```c
struct tm tmv;
localtime_r(&now, &tmv);
```

### Formatting and parsing

```c
char buf[64];
size_t n = strftime(buf, sizeof buf, "%Y-%m-%d %H:%M:%S", localtime_r(&now, &tmv));
if (n == 0) { /* buffer too small */ }
```

Common specifiers: `%Y` year, `%m` month, `%d` day, `%H:%M:%S`, `%Z` zone name, `%z` offset, `%F` = `%Y-%m-%d`, `%T` = `%H:%M:%S`. `strftime` output depends on the locale; use ISO 8601 format for logs and data. `strptime` parses text into a `struct tm` but is POSIX, not ISO C.

`mktime(&tm)` converts local broken-down time to `time_t` and normalizes out-of-range fields (so you can add days by incrementing `tm_mday`). Set `tm_isdst = -1` to let it determine DST.

`difftime(t1, t0)` returns the difference in seconds as a `double`.

### Pitfalls

- `time_t` is a 32-bit signed integer on some older systems, which overflows in **2038**. Modern 64-bit platforms use 64 bits.
- Local time has DST gaps and overlaps; store and compare timestamps in UTC.
- Time zones and leap seconds are not C's strength; use a dedicated library for serious calendar logic.

## Measuring elapsed time

```c
#include <time.h>

static double now_seconds(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);              /* POSIX */
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

double t0 = now_seconds();
do_work();
double elapsed = now_seconds() - t0;
printf("%.3f s\n", elapsed);
```

Using `time()` for benchmarks gives 1-second resolution and breaks if the clock changes. `clock()` measures CPU time summed over threads: divide by `CLOCKS_PER_SEC`. For serious benchmarking see [profiling](../07-performance-security/01-profiling.md): run many iterations, warm up, and beware of the optimizer deleting the work you measure.

## Random numbers

### `rand()`: simple but weak

```c
#include <stdlib.h>

srand((unsigned)time(NULL));        /* seed once at startup */
int r = rand();                     /* 0 .. RAND_MAX (at least 32767) */
```

Problems: `RAND_MAX` may be only 32767, quality varies by implementation, the generator state is global, and the sequence is predictable. Without `srand`, the same sequence runs every time (useful for reproducible tests).

### Range bias

```c
int dice = rand() % 6 + 1;          /* slightly biased unless RAND_MAX+1 is divisible by 6 */
```

Modulo introduces bias when the range doesn't divide the generator's range evenly. Unbiased approach: reject values from the incomplete final block.

### A small, better PRNG

```c
#include <stdint.h>

static uint64_t rng_state = 88172645463325252ULL;     /* must be non-zero */

uint64_t rng_next(void) {                              /* xorshift64* */
    rng_state ^= rng_state >> 12;
    rng_state ^= rng_state << 25;
    rng_state ^= rng_state >> 27;
    return rng_state * 2685821657736338717ULL;
}

/* Unbiased integer in [0, n), n > 0 */
uint64_t rng_below(uint64_t n) {
    uint64_t threshold = (0 - n) % n;                  /* 2^64 mod n */
    uint64_t r;
    do { r = rng_next(); } while (r < threshold);
    return r % n;
}
```

Good for games, simulations, shuffling, and tests. For a reproducible simulation, seed explicitly and log the seed.

### Shuffle (Fisher-Yates)

```c
for (size_t i = n - 1; i > 0; i--) {
    size_t j = (size_t)rng_below(i + 1);
    int tmp = a[i]; a[i] = a[j]; a[j] = tmp;
}
```

### Security: never use `rand()` or xorshift for secrets

Passwords, tokens, keys and nonces need a **cryptographically secure** source from the OS:

```c
#include <sys/random.h>                 /* Linux (glibc 2.25+), also on recent BSDs */
unsigned char key[32];
if (getrandom(key, sizeof key, 0) != (ssize_t)sizeof key) { /* handle error */ }
```

Alternatives: `arc4random_buf` (BSD, macOS, glibc 2.36+), reading `/dev/urandom`, `BCryptGenRandom` on Windows, or a library like libsodium. Seeding with `time(NULL)` is guessable.

## Common mistakes

| Mistake | Effect |
|---|---|
| Using `localtime`'s result across two calls | Value overwritten |
| `tm_mon` or `tm_year` off by one / 1900 | Wrong dates |
| `time()` for benchmarking or timeouts | Coarse and not monotonic |
| Calling `srand(time(NULL))` repeatedly (in a loop) | Same value in the same second, so identical numbers |
| `rand() % n` for large `n` | Bias; and `RAND_MAX` may be tiny |
| `rand()` for tokens or passwords | Predictable → vulnerability |
| Storing local time instead of UTC | DST and timezone bugs |

## Summary

- Use `time` for timestamps (store UTC), `CLOCK_MONOTONIC` for durations.
- Prefer `localtime_r`/`gmtime_r`; format with `strftime` (ISO 8601 for data).
- `rand()` is fine for toys; use a better PRNG for simulations and the OS CSPRNG for security.
- Avoid modulo bias when mapping to a range.

**Next:** [Data structures: linked lists](./10-data-structures/01-linked-lists.md)
