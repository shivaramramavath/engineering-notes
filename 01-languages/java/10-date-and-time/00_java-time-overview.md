# Java Time Overview

`java.time` (Java 8+, JSR-310) is the standard date/time API. It replaced `java.util.Date`, `Calendar`, and `SimpleDateFormat`, whose flaws caused decades of bugs. Every type in it is **immutable and thread-safe**, and each type represents *one specific idea* (a date, a moment, a duration) instead of one class that tries to be everything.

If you remember one thing from this module: **a date-time is only meaningful once you know which of these questions you are answering**: *"what does the calendar/clock say?"* or *"which moment on the universal timeline?"*

---

## 1. Why the old API was replaced

| Legacy problem | Example |
|---|---|
| Mutable, not thread-safe | `SimpleDateFormat` shared across threads corrupts output ([Thread Safety](../14-concurrency/02_thread-safety.md)) |
| `Date` isn't a date | It is an instant in milliseconds, and it prints using the JVM default zone |
| Confusing indexing | `Calendar.MONTH` is 0-based; `new Date(int, int, int)` (deprecated) uses year-1900 |
| No separation of concepts | No "date without time" or "time without zone" type (`java.sql.Date` is a hack) |
| Poor arithmetic and formatting | Manual millisecond math; lenient parsing that silently accepts garbage |

Don't write new code with the legacy classes. You will still meet them in old code and in some libraries, so know how to convert (section 5).

---

## 2. The type map

```text
                      Instant                       machine time: a point on the UTC timeline
                       ▲   │
              toInstant()  │ atZone(zone)
                       │   ▼
LocalDateTime ──atZone(zone)──► ZonedDateTime ──toOffsetDateTime()──► OffsetDateTime
 (no zone)   ◄─toLocalDateTime─  (zone rules: DST)                    (fixed offset)
   │   │
   │   └── toLocalTime()  → LocalTime   (wall-clock time)
   └────── toLocalDate()  → LocalDate   (calendar date)
```

| Type | Holds | Typical use |
|---|---|---|
| `LocalDate` | year-month-day | birthday, due date, holiday |
| `LocalTime` | hour-minute-second-nano | store opening time |
| `LocalDateTime` | date + time, **no zone** | user-entered local schedule, "wall-clock" values |
| `Instant` | nanoseconds since 1970-01-01T00:00:00Z | `created_at`, logs, event ordering |
| `ZonedDateTime` | date + time + zone **ID** + offset | meetings in a region, DST-aware math |
| `OffsetDateTime` | date + time + fixed offset | API payloads, DB timestamps |
| `ZoneId` / `ZoneOffset` | `Asia/Kolkata` / `+05:30` | zone identity / fixed shift |
| `Duration` | seconds + nanos | timeouts, elapsed time |
| `Period` | years + months + days | "3 months from now" |
| `Year`, `YearMonth`, `MonthDay` | partial dates | card expiry (`YearMonth`), birthday without year (`MonthDay`) |
| `DayOfWeek`, `Month` | enums | `DayOfWeek.MONDAY`, `Month.FEBRUARY` |
| `Clock` | source of "now" | injectable time for testing |

Packages: `java.time` (main types), `java.time.format` (formatting/parsing), `java.time.temporal` (fields, units, adjusters), `java.time.zone` (zone rules), `java.time.chrono` (non-ISO calendars).

---

## 3. First example

```java
LocalDate    date = LocalDate.of(2024, 3, 10);              // months are 1-12
LocalTime    time = LocalTime.of(9, 30);
LocalDateTime ldt = LocalDateTime.of(date, time);           // 2024-03-10T09:30

Instant      now  = Instant.now();                          // 2024-03-10T04:00:00.123456Z
ZonedDateTime ist = ZonedDateTime.now(ZoneId.of("Asia/Kolkata"));

ZonedDateTime meeting = ldt.atZone(ZoneId.of("Asia/Kolkata"));  // attach a zone → real moment
Instant       moment  = meeting.toInstant();                    // 2024-03-10T04:00:00Z
```

`LocalDateTime` is just fields on a calendar. It becomes a real moment only after you attach a zone.

---

## 4. Naming conventions (learn once, use everywhere)

The API is consistent, so you can guess methods:

| Prefix | Meaning | Example |
|---|---|---|
| `now` | current value | `LocalDate.now()` |
| `of` | factory from components | `LocalDate.of(2024, 3, 10)` |
| `from` | conversion from another temporal | `LocalDate.from(zdt)` |
| `parse` | from text | `LocalDate.parse("2024-03-10")` |
| `format` | to text | `date.format(fmt)` |
| `get` | read a field | `date.getYear()` |
| `with` | **copy** with one field changed | `date.withDayOfMonth(1)` |
| `plus` / `minus` | **copy** after adding/subtracting | `date.plusDays(7)` |
| `to` | convert to another type | `ldt.toLocalDate()` |
| `at` | combine with something else | `date.atTime(9, 30)`, `ldt.atZone(zone)` |
| `is` | query | `a.isBefore(b)` |

### Immutability: the classic bug

Every `plus`/`with` returns a **new object**; the original never changes.

```java
LocalDate d = LocalDate.of(2024, 3, 10);
d.plusDays(1);              // result discarded: d is still 2024-03-10
d = d.plusDays(1);          // correct
```

---

## 5. Working with legacy code

| From | To | How |
|---|---|---|
| `java.util.Date` | `Instant` | `date.toInstant()` |
| `Instant` | `java.util.Date` | `Date.from(instant)` |
| `java.sql.Timestamp` | `LocalDateTime` | `ts.toLocalDateTime()` |
| `LocalDateTime` | `Timestamp` | `Timestamp.valueOf(ldt)` |
| `java.sql.Date` | `LocalDate` | `sqlDate.toLocalDate()` |
| `LocalDate` | `java.sql.Date` | `java.sql.Date.valueOf(localDate)` |
| `GregorianCalendar` | `ZonedDateTime` | `cal.toZonedDateTime()` |
| `TimeZone` | `ZoneId` | `tz.toZoneId()` |

```java
Date legacy = new Date();
LocalDateTime ldt = LocalDateTime.ofInstant(legacy.toInstant(), ZoneId.of("Asia/Kolkata"));
```

Watch out:

- `java.sql.Date.toInstant()` **throws `UnsupportedOperationException`**. Use `toLocalDate()` instead.
- `Timestamp.valueOf(LocalDateTime)` and `toLocalDateTime()` use the **JVM default zone** under the hood. Convert via `Instant` (`Timestamp.from(instant)`, `ts.toInstant()`) when you care about the exact moment.
- With modern JDBC drivers you usually don't need `Timestamp` at all: see [ResultSet and Data Mapping](../16-jdbc-and-databases/03_resultset-and-data-mapping.md).

---

## 6. Version notes

| Version | Addition |
|---|---|
| 8 | `java.time` introduced |
| 9 | `LocalDate.datesUntil`, `Duration.toXxxPart()` methods, more precise `now()` on supporting platforms |
| 17 | `InstantSource` interface (`Clock` implements it): handy for injecting time |

---

## Common misconceptions

- **"`LocalDateTime` is UTC / is a timestamp."** It has no zone at all, so it identifies no moment.
- **"`Instant` has a time zone."** It doesn't. Its `toString` ends in `Z` because it is printed in UTC.
- **"`Date` stores a date."** It stores epoch milliseconds.
- **"`now()` is the same everywhere."** `LocalDate.now()` and friends use the **system default zone**. A server in UTC and a laptop in IST give different dates for the same moment ([Best Practices](05_date-time-best-practices.md)).

---

## Quick Summary

- Use `java.time`; treat `Date`/`Calendar`/`SimpleDateFormat` as legacy.
- Pick the type by the question: *calendar value* (`Local*`), *moment* (`Instant`), *moment + region rules* (`ZonedDateTime`), *amount* (`Duration`/`Period`).
- All types are immutable. Reassign the result of `plus`/`with`.
- Naming is systematic: `of`, `now`, `parse`, `with*`, `plus*`, `at*`, `to*`, `is*`.
- Convert legacy types through `Instant` where possible.

**Next:** [Local Date and Time](01_local-date-time.md)
