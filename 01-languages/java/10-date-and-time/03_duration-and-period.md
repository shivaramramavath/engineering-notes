# Duration and Period

`Duration` and `Period` both represent an **amount of time**, but they measure different things:

- `Duration` is **time-based**: an exact number of seconds and nanoseconds ("2 hours 30 minutes").
- `Period` is **date-based**: years, months, and days on a calendar ("1 month and 3 days").

Months and days don't have a fixed length in seconds (a month is 28-31 days, and a day in a DST zone can be 23 or 25 hours), so Java keeps the two concepts separate. Both implement `TemporalAmount`.

**Prerequisites:** [Local Date and Time](01_local-date-time.md), [Instant and Zoned Date-Time](02_instant-and-zoned-date-time.md).

---

## 1. `Duration`

### Creating

```java
Duration d1 = Duration.ofHours(2);
Duration d2 = Duration.ofMinutes(90);
Duration d3 = Duration.ofSeconds(45);
Duration d4 = Duration.ofMillis(1500);
Duration d5 = Duration.ofDays(1);                 // exactly 24 hours, not "a calendar day"
Duration d6 = Duration.parse("PT1H30M");          // ISO-8601
Duration d7 = Duration.between(startInstant, endInstant);
```

### Measuring between two points

```java
Instant start = Instant.parse("2024-03-10T04:00:00Z");
Instant end   = Instant.parse("2024-03-10T06:45:30Z");

Duration elapsed = Duration.between(start, end);    // PT2H45M30S
```

`Duration.between` works for anything with a time component: `Instant`, `LocalTime`, `LocalDateTime`, `ZonedDateTime`. It **throws `UnsupportedTemporalTypeException` for `LocalDate`**, which has no time-of-day. Use `Period` or `ChronoUnit.DAYS.between` for dates.

For `LocalDateTime`, `Duration.between` ignores zones, so it can't account for DST. Use `Instant` or `ZonedDateTime` for real elapsed time.

### Reading values

```java
Duration d = Duration.ofMinutes(150);

d.toHours();           // 2    (total whole hours)
d.toMinutes();         // 150  (total)
d.getSeconds();        // 9000
d.toHoursPart();       // 2    (Java 9+: the hour component)
d.toMinutesPart();     // 30   (Java 9+: the minute component, 0-59)
d.toSecondsPart();     // 0
d.toString();          // PT2H30M
```

`toXxx()` gives the **total** in that unit; `toXxxPart()` gives the **component** you'd show on a clock face. Mixing them up is a common display bug:

```java
String label = "%dh %02dm".formatted(d.toHours(), d.toMinutesPart());   // "2h 30m"
```

### Arithmetic and comparison

```java
Duration total = d1.plus(d2);                 // PT3H30M
Duration half  = d1.dividedBy(2);             // PT1H
Duration triple = d1.multipliedBy(3);         // PT6H
Duration abs   = d1.minus(d2).abs();          // PT30M
d1.compareTo(d2);                             // < 0
d1.isNegative(); d1.isZero();
```

---

## 2. `Period`

### Creating

```java
Period p1 = Period.of(1, 2, 3);            // 1 year, 2 months, 3 days
Period p2 = Period.ofMonths(3);
Period p3 = Period.ofWeeks(2);             // stored as 14 days
Period p4 = Period.parse("P1Y2M3D");
Period p5 = Period.between(LocalDate.of(2024, 1, 1), LocalDate.of(2024, 3, 15));   // P2M14D
```

`Period.between` only takes `LocalDate`s.

### Components, not totals

```java
Period p = Period.between(LocalDate.of(2024, 1, 1), LocalDate.of(2024, 3, 15));   // P2M14D

p.getMonths();         // 2
p.getDays();           // 14   ← NOT the total number of days
p.toTotalMonths();     // 2    (years * 12 + months)
```

If you want total days, use `ChronoUnit.DAYS.between(from, to)` (here, 74).

### Applying

```java
LocalDate d = LocalDate.of(2024, 1, 31);
d.plus(Period.ofMonths(1));       // 2024-02-29 (month-end clamping, as in LocalDate)

Period.ofMonths(14).normalized(); // P1Y2M: carries months into years (does NOT touch days)
```

`Period.ofMonths(1)` and `Period.ofDays(30)` are different values: they are never converted into one another.

---

## 3. `Duration` vs `Period`

| | `Duration` | `Period` |
|---|---|---|
| Measures | seconds + nanos | years + months + days |
| Fixed length? | Yes: exact elapsed time | No: depends on the calendar |
| Works with | `Instant`, `LocalTime`, `LocalDateTime`, `ZonedDateTime` | `LocalDate`, `LocalDateTime`, `ZonedDateTime` |
| `between` accepts | anything with time | `LocalDate` only |
| DST-aware on `ZonedDateTime`? | Adds exact elapsed time | Adds to the local date |
| Use for | timeouts, TTLs, SLAs, elapsed time | ages, subscription terms, deadlines |

```java
ZonedDateTime start = ZonedDateTime.of(2024, 3, 9, 12, 0, 0, 0, ZoneId.of("America/New_York"));

start.plus(Period.ofDays(1));      // 2024-03-10T12:00-04:00   same clock time (23h elapsed)
start.plus(Duration.ofDays(1));    // 2024-03-10T13:00-04:00   exactly 24h elapsed
```

---

## 4. `ChronoUnit.between` as a third option

```java
LocalDate a = LocalDate.of(2024, 1, 31);
LocalDate b = LocalDate.of(2024, 3, 1);

ChronoUnit.DAYS.between(a, b);      // 30
ChronoUnit.MONTHS.between(a, b);    // 1   (complete months only)
ChronoUnit.HOURS.between(t1, t2);   // works on time-bearing types

Period.between(a, b);               // P1M1D
```

| Want | Use |
|---|---|
| Total whole days/hours/months | `ChronoUnit.X.between` |
| Years + months + days breakdown ("age", "tenure") | `Period.between` |
| Exact elapsed time as an object | `Duration.between` |

---

## 5. Practical usage

### Timeouts, TTLs, and expiry

```java
Duration ttl = Duration.ofMinutes(15);
Instant expiresAt = Instant.now().plus(ttl);

boolean expired = Instant.now().isAfter(expiresAt);
```

Use `Duration` in config and method signatures instead of bare `long timeoutMs`. It carries the unit and avoids seconds-vs-millis mistakes. Convert at the API boundary:

```java
Thread.sleep(ttl.toMillis());          // all versions
// Thread.sleep(ttl);                  // Duration overload exists since Java 19
future.get(ttl.toMillis(), TimeUnit.MILLISECONDS);
```

### Measuring elapsed time in code

```java
long t0 = System.nanoTime();
doWork();
Duration took = Duration.ofNanos(System.nanoTime() - t0);
```

Prefer `System.nanoTime()` for measuring elapsed time. It is monotonic, while wall-clock time (`Instant.now()`) can jump when NTP adjusts the clock. Use `Duration.between(Instant, Instant)` for the gap between two *real timestamps* (for example `created_at` to `processed_at`).

### Backoff delays

`Duration` pairs well with retry logic: `base.multipliedBy(1L << attempt)`. See [Retry and Backoff](../25-real-world-patterns/02_retry-and-backoff.md).

---

## Common mistakes

| Mistake | Fix |
|---|---|
| `Duration.between(LocalDate, LocalDate)` | `ChronoUnit.DAYS.between` or `Period.between` |
| Treating `period.getDays()` as the total days | `ChronoUnit.DAYS.between(from, to)` |
| `Duration.ofDays(30)` for "one month" | `Period.ofMonths(1)` on a date |
| Showing `d.toMinutes()` next to `d.toHours()` as if both were components | Use `toMinutesPart()` |
| `Duration` of `LocalDateTime`s across a DST change, expecting real elapsed time | Use `Instant` or `ZonedDateTime` |
| Measuring performance with `Instant.now()` differences | `System.nanoTime()` (or JMH for benchmarks) |
| Assuming `Period.ofMonths(14)` stays as 14 months after `normalized()` | It becomes `P1Y2M` |
| Raw `long` timeouts with unknown units | Accept a `Duration` |

---

## Quick Summary

- **`Duration`** = exact time (seconds/nanos). **`Period`** = calendar amount (y/m/d).
- `Duration.between` needs a time component. `Period.between` needs two `LocalDate`s.
- `toHours()` is a total; `toHoursPart()` (Java 9+) is a component. `period.getDays()` is a component, never a total.
- On `ZonedDateTime`, `Period` follows the calendar and `Duration` follows the stopwatch.
- Use `ChronoUnit.X.between` for a single total in one unit; use `System.nanoTime()` to time code.
- Prefer `Duration` over raw `long` timeouts in APIs.

**Next:** [Date-Time Formatting and Parsing](04_date-time-formatting-and-parsing.md)
