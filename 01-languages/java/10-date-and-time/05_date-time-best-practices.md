# Date-Time Best Practices

The API in `java.time` is good. Most real-world date bugs come from *decisions around it*: what to store, which zone to assume, how to test "now". This note is the checklist to apply before shipping date-handling code.

**Prerequisites:** all earlier notes in this module, especially [Instant and Zoned Date-Time](02_instant-and-zoned-date-time.md).

---

## 1. The core rule: moments in UTC, zones at the edges

```text
 user input ──► parse in user's zone ──► Instant (UTC) ──► store / transmit / compare
                                                │
 user display ◄── format in user's zone ◄───────┘
```

- **Inside the system**, represent moments as `Instant` (or UTC `OffsetDateTime`).
- **At the boundary** (UI, reports, emails), convert to the user's `ZoneId` and `Locale` for display, and convert user input back to an `Instant` on the way in.
- Don't pass `LocalDateTime` around as if it were a moment. It carries no way to recover which one.

---

## 2. Choose the type by what the value *means*

| The value is... | Store | Reason |
|---|---|---|
| Something that **happened** (`created_at`, `paid_at`, log time) | `Instant` / `OffsetDateTime` (UTC) | A fixed moment |
| A **calendar date** with no time (birthday, invoice date) | `LocalDate` | Same everywhere; no zone needed |
| A **future local event** (meeting at 09:00 Paris time, next year) | `LocalDateTime` + `ZoneId` (as two fields) | The offset may change if the region's rules change |
| A **recurring wall-clock time** (store opens 09:00) | `LocalTime` (+ `ZoneId` of the store) | Must follow local clocks through DST |
| A **length of time** (timeout, TTL) | `Duration` | Explicit units |
| A **calendar amount** (billing every 1 month) | `Period` | Follows the calendar |

Why not store an `Instant` for the future meeting? If Paris changes its DST rules before then, "09:00 Paris" and the UTC instant you precomputed no longer agree. Storing local time + zone ID lets you resolve it using the latest rules at the moment you need it.

---

## 3. Never depend on the default zone

```java
LocalDate.now();                  // uses ZoneId.systemDefault()
LocalDateTime.now();              // same
ZoneId.systemDefault();           // depends on OS, container, and -Duser.timezone
```

The same code gives different results on a laptop (local zone) and a server or container (often UTC). Pass the zone, or better, a `Clock`:

```java
LocalDate today = LocalDate.now(ZoneId.of("Asia/Kolkata"));   // explicit
LocalDate today2 = LocalDate.now(clock);                       // explicit and testable
```

"Today" is not a global fact: it's "today in *which* zone?" Business rules ("orders before midnight ship today") should name the zone.

---

## 4. Make time injectable: `Clock`

Code that calls `Instant.now()` directly is hard to test. Inject a `Clock` instead:

```java
class SubscriptionService {
    private final Clock clock;

    SubscriptionService(Clock clock) { this.clock = clock; }

    boolean isExpired(Instant expiresAt) {
        return clock.instant().isAfter(expiresAt);
    }

    LocalDate today(ZoneId zone) { return LocalDate.now(clock.withZone(zone)); }
}
```

Production wiring uses `Clock.systemUTC()`. Tests pin or move time:

```java
Clock fixed = Clock.fixed(Instant.parse("2024-03-10T00:00:00Z"), ZoneOffset.UTC);
var service = new SubscriptionService(fixed);

assertTrue(service.isExpired(Instant.parse("2024-03-09T23:59:59Z")));
assertFalse(service.isExpired(Instant.parse("2024-03-10T00:00:01Z")));
```

Tests that use `Instant.now()` or `Thread.sleep` to simulate time are flaky or slow. Since Java 17 you can depend on the narrower `InstantSource` interface (which `Clock` implements) if you only need "now". See [Testing Patterns](../19-testing/04_testing-patterns.md).

---

## 5. Interchange: APIs, JSON, databases

### APIs and JSON

- Use ISO-8601 text with an offset or `Z`: `2024-03-10T04:00:00Z`.
- Don't invent formats like `10-Mar-24 09:30 AM`. If an external system forces one, confine it to one parsing/formatting class ([Formatting and Parsing](04_date-time-formatting-and-parsing.md)).
- With Jackson 2.x, `java.time` support needs the `jackson-datatype-jsr310` module registered, and you usually want to disable writing dates as numeric timestamps so you get ISO strings. See [Jackson](../17-json-and-data-formats/01_jackson.md).

### Databases

- Map directly to `java.time` types. JDBC 4.2 drivers support `LocalDate`, `LocalTime`, `LocalDateTime`, and `OffsetDateTime` through `getObject`/`setObject`. JPA 2.2+ and modern Hibernate map them too. Don't go through `java.sql.Timestamp`/`java.util.Date`.
- Understand your database's semantics: a `timestamp with time zone` column normally stores a UTC moment, while `timestamp without time zone` stores bare local values. Details differ per database and driver, so read yours.
- **Precision:** `Instant.now()` may carry microseconds or nanoseconds, while a column may keep milliseconds or microseconds. A value written and read back may not `equals` the original, which breaks naive assertions. Truncate (`instant.truncatedTo(ChronoUnit.MILLIS)`) before comparing or storing.

See [ResultSet and Data Mapping](../16-jdbc-and-databases/03_resultset-and-data-mapping.md).

---

## 6. Comparing and ordering

```java
a.isBefore(b);  a.isAfter(b);     // prefer these to compareTo for readability
```

- For moments, compare `Instant`s, or use `isEqual` on zoned types. `ZonedDateTime.equals` also compares the zone.
- Use half-open ranges `[start, end)` for intervals so adjacent intervals don't overlap or leave gaps.
- Don't compare formatted strings; only ISO strings in UTC sort correctly, and relying on that is fragile.

---

## 7. DST and calendar traps

| Trap | Guidance |
|---|---|
| "A day is 24 hours" | Not on DST days. Use `plusDays` for calendar days and `Duration` for elapsed time |
| Daily job at 02:30 local | That time doesn't exist on spring-forward day and happens twice on fall-back. Decide the intended behavior, or schedule in UTC |
| Midnight doesn't always exist | Some zones skip it on DST changes; use `date.atStartOfDay(zone)` rather than building it by hand |
| End-of-month arithmetic | `Jan 31 + 1 month = Feb 28/29`. Compute from the anchor date |
| Feb 29 | `plusYears(1)` gives Feb 28 |
| Week numbers and first day of week | Depend on locale (`WeekFields.of(locale)`) or ISO (`IsoFields`). `YYYY` in a pattern is the week-based year |
| Zone rules change | tzdata is part of the JDK. Keep JDKs patched, especially for regions that changed DST rules |
| Abbreviations (`IST`, `CST`) | Ambiguous. Use `Area/City` IDs |

---

## 8. Legacy interop

- Convert at the boundary and keep the rest of the code on `java.time`.
- Convert `Date` through `Instant` (`date.toInstant()`, `Date.from(instant)`).
- Never share a `SimpleDateFormat` between threads. Replace it with `DateTimeFormatter`.
- `java.sql.Date.toInstant()` throws. Use `toLocalDate()`.

Conversion table: [Java Time Overview](00_java-time-overview.md#5-working-with-legacy-code).

---

## 9. Common misconceptions

| Belief | Reality |
|---|---|
| "`LocalDateTime` is the modern `Date`" | `Date` was a moment. The modern equivalent is `Instant` |
| "Store everything in UTC and you're done" | Fine for past moments. Future local events need local time + zone ID |
| "`Instant.now()` is the same on every machine" | It's the same *moment* only if system clocks are in sync. Don't use it for cross-machine ordering without that assumption |
| "Server in UTC means no time-zone bugs" | Bugs move to user input, display, and "what is today" logic |
| "`Duration.ofDays(1)` = a calendar day" | It's exactly 24 hours |
| "`SMART` parsing validates dates" | It clamps invalid days; use `STRICT` |

---

## Pre-ship checklist

```text
[ ] Stored moments are Instant / UTC OffsetDateTime, not LocalDateTime
[ ] No now() without a zone or Clock in business logic
[ ] Time is injected (Clock / InstantSource) and tests don't use real time or sleeps
[ ] User-facing text is formatted with an explicit zone AND locale
[ ] External formats are parsed with strict, explicit formatters in one place
[ ] Future local events keep local time + region ZoneId
[ ] Intervals are half-open; day arithmetic uses plusDays, elapsed time uses Duration
[ ] Precision differences (DB vs Instant) are handled
[ ] No Date / Calendar / SimpleDateFormat in new code
```

---

## Quick Summary

- Keep moments as `Instant` (UTC) internally and convert to zones/locales only at the edges.
- Pick the type from the meaning of the value: moment, calendar date, local schedule, or amount.
- Never rely on the default zone. Pass a `ZoneId` or inject a `Clock` (which also makes tests deterministic).
- Use ISO-8601 for APIs, native `java.time` mappings for databases, and watch precision.
- Treat DST, month-ends, week-based years, and tzdata updates as real design inputs, not edge cases.

**Next module:** [I/O and Networking](../11-io-and-networking/00_io-streams-readers-writers.md)
