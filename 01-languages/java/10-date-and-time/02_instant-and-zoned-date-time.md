# Instant and Zoned Date-Time

This is the note where the date/time model clicks. There are two ways to describe "a point in time":

- **Machine time:** a count of nanoseconds on one universal timeline → `Instant`.
- **Human time:** what a clock on a specific wall in a specific region shows → `ZonedDateTime`, `OffsetDateTime`.

Bugs happen when the two are mixed up. Zones matter because the same instant shows different local times around the world, and because the *rules* for a region (daylight saving) change what "add one day" means.

**Prerequisites:** [Java Time Overview](00_java-time-overview.md), [Local Date and Time](01_local-date-time.md).

---

## 1. `Instant`: a point on the timeline

An `Instant` is the number of seconds plus nanoseconds since the epoch, `1970-01-01T00:00:00Z`. It has no zone and no calendar.

```java
Instant now  = Instant.now();                               // e.g. 2024-03-10T04:00:00.123456Z
Instant a    = Instant.parse("2024-03-10T04:00:00Z");
Instant b    = Instant.ofEpochMilli(1_710_000_000_000L);
Instant c    = Instant.ofEpochSecond(1_710_000_000L);

a.toEpochMilli();             // millis since epoch
a.getEpochSecond();           // seconds since epoch

a.plusSeconds(90);
a.plus(Duration.ofHours(2));
a.plus(1, ChronoUnit.DAYS);   // allowed: exactly 24h
a.isBefore(b); a.isAfter(b);
```

Points to know:

- `toString()` always prints UTC with a trailing `Z`. That is a *display choice*, not a stored zone.
- **Supported units go up to `DAYS`.** `instant.plus(1, ChronoUnit.MONTHS)` throws `UnsupportedTemporalTypeException`, because months have no fixed length without a calendar and zone.
- Precision is up to nanoseconds, but what `Instant.now()` actually returns depends on the OS/JDK clock (commonly microseconds on modern JDKs). Values you store may be truncated by the database ([Best Practices](05_date-time-best-practices.md)).
- The Java time-scale smooths leap seconds, so there is no `23:59:60`.

Use `Instant` for: timestamps, `created_at`, log events, ordering, expiry checks, anything compared across systems.

---

## 2. Time zones: `ZoneId` and `ZoneOffset`

```java
ZoneId kolkata = ZoneId.of("Asia/Kolkata");     // region ID: carries rules (DST history, future rules)
ZoneId paris   = ZoneId.of("Europe/Paris");
ZoneId system  = ZoneId.systemDefault();        // JVM default: avoid relying on it
ZoneOffset ist = ZoneOffset.of("+05:30");       // fixed offset: no rules, no DST
ZoneOffset utc = ZoneOffset.UTC;
```

| | `ZoneId` (region) | `ZoneOffset` |
|---|---|---|
| Example | `Europe/Paris` | `+02:00` |
| Knows DST? | Yes: offset varies through the year | No: constant |
| Identifies | A place and its rules | A shift from UTC |

A **region ID** tells you *where* (so the offset can be looked up for any date). A **fixed offset** only tells you the shift at one moment: `+02:00` could be Paris in summer or a country that never changes.

Avoid 3-letter abbreviations like `IST` or `CST`: they're ambiguous, and `ZoneId.of("IST")` throws unless you pass `ZoneId.SHORT_IDS`. Use `Area/City` IDs. List available IDs with `ZoneId.getAvailableZoneIds()`.

---

## 3. `ZonedDateTime`: moment + region rules

```java
ZoneId ny = ZoneId.of("America/New_York");

ZonedDateTime z1 = ZonedDateTime.now(ny);
ZonedDateTime z2 = ZonedDateTime.of(2024, 3, 10, 9, 30, 0, 0, ny);
ZonedDateTime z3 = LocalDateTime.of(2024, 3, 10, 9, 30).atZone(ny);
ZonedDateTime z4 = Instant.parse("2024-03-10T04:00:00Z").atZone(ny);

z2.getOffset();            // -04:00 (EDT on that date)
z2.toInstant();            // the same moment on the UTC timeline
z2.toLocalDateTime();      // 2024-03-10T09:30
```

### Same instant, different zone vs same local time, different zone

```java
ZonedDateTime meetingNy = ZonedDateTime.of(2024, 6, 1, 9, 0, 0, 0, ny);

meetingNy.withZoneSameInstant(ZoneId.of("Asia/Kolkata"));
// 2024-06-01T18:30+05:30[Asia/Kolkata]   ← same moment, different clock reading

meetingNy.withZoneSameLocal(ZoneId.of("Asia/Kolkata"));
// 2024-06-01T09:00+05:30[Asia/Kolkata]   ← same clock reading, DIFFERENT moment
```

Converting a moment to someone else's zone is almost always `withZoneSameInstant`. `withZoneSameLocal` is for fixing data whose zone was wrongly attached.

---

## 4. DST: why `ZonedDateTime` exists

On 2024-03-10 in New York, clocks jump from 02:00 to 03:00 (spring forward). On 2024-11-03 they fall back from 02:00 to 01:00.

```text
Spring forward (gap)                  Fall back (overlap)
local:  01:59 ──► 03:00               local:  01:59 ──► 01:00 ──► 01:59 ──► 02:00
        (02:xx never exists)                  (01:xx happens twice)
```

### Gap: a local time that doesn't exist

```java
LocalDateTime.of(2024, 3, 10, 2, 30).atZone(ny);
// 2024-03-10T03:30-04:00[America/New_York]   ← shifted forward by the length of the gap
```

### Overlap: a local time that happens twice

```java
ZonedDateTime first = LocalDateTime.of(2024, 11, 3, 1, 30).atZone(ny);
// 2024-11-03T01:30-04:00  ← default: the EARLIER offset

first.withLaterOffsetAtOverlap();
// 2024-11-03T01:30-05:00  ← the second occurrence
```

### "Add one day" vs "add 24 hours"

`ZonedDateTime` arithmetic differs by unit type:

- Date-based units (`plusDays`, `plusMonths`, `Period`) work on the **local timeline**: same clock time, next date.
- Time-based units (`plusHours`, `Duration`) work on the **instant timeline**: exact elapsed time.

```java
ZonedDateTime start = ZonedDateTime.of(2024, 3, 9, 12, 0, 0, 0, ny);   // 12:00 EST, day before DST

start.plusDays(1);
// 2024-03-10T12:00-04:00   ← still noon, but only 23 hours elapsed

start.plus(Duration.ofHours(24));
// 2024-03-10T13:00-04:00   ← exactly 24h elapsed, so the clock reads 13:00
```

Neither is "wrong". Use days for "same time tomorrow" (a daily reminder at 9 AM) and hours/`Duration` for "exactly 24 hours later" (token expiry). More in [Duration and Period](03_duration-and-period.md).

---

## 5. `OffsetDateTime`: moment + fixed offset

```java
OffsetDateTime o = OffsetDateTime.parse("2024-03-10T09:30:00+05:30");
o.getOffset();                  // +05:30
o.toInstant();                  // 2024-03-10T04:00:00Z
Instant.now().atOffset(ZoneOffset.UTC);
```

It keeps the offset it was given but knows **no region rules**, so `plusDays(1)` can't adjust for DST. That makes it a good fit for **data interchange**: JSON payloads, database `timestamp with time zone` columns, JDBC 4.2 (which requires drivers to support `OffsetDateTime`; `ZonedDateTime` and `Instant` support is driver-dependent).

### Which one?

| Need | Use |
|---|---|
| Store/compare/order moments | `Instant` |
| Send/receive a timestamp with offset (APIs, DB) | `OffsetDateTime` (or `Instant` in UTC) |
| Display to a user in a region; DST-aware arithmetic; recurring local events | `ZonedDateTime` |
| Business rule "9 AM in the customer's city" | `LocalDateTime` + `ZoneId`, resolved when needed |

---

## 6. Converting between them

```java
Instant instant = Instant.now();

ZonedDateTime  z = instant.atZone(ZoneId.of("Asia/Kolkata"));
OffsetDateTime o = instant.atOffset(ZoneOffset.UTC);
LocalDateTime  l = LocalDateTime.ofInstant(instant, ZoneId.of("Asia/Kolkata"));

Instant back1 = z.toInstant();
Instant back2 = o.toInstant();
Instant back3 = l.atZone(ZoneId.of("Asia/Kolkata")).toInstant();   // zone required: lossy otherwise
```

Going from `LocalDateTime` to `Instant` always needs a zone. If you find yourself writing `l.toInstant(ZoneOffset.UTC)` on data that was *not* UTC, that is a bug that works in tests and fails when the server zone changes.

---

## 7. Equality and comparison

```java
ZonedDateTime a = ZonedDateTime.of(2024, 6, 1, 9, 0, 0, 0, ZoneId.of("Europe/Paris"));
ZonedDateTime b = a.withZoneSameInstant(ZoneId.of("Asia/Kolkata"));

a.equals(b);       // false: different zone/offset/local time
a.isEqual(b);      // true:  same instant
a.compareTo(b);    // compares instant first, then local date-time, then zone ID: not "same moment == 0"
a.toInstant().equals(b.toInstant());   // true
```

`ZonedDateTime.equals` compares the local date-time, the offset, **and** the zone, so two values for the same moment in different zones are *not equal*. Use `isEqual`, `isBefore`, `isAfter`, or compare `Instant`s. This also matters for `Set`/`Map` keys ([equals and hashCode](../04-oop/14_equals-and-hashcode.md)): keys of different zone representations won't match.

---

## Common mistakes

| Mistake | Fix |
|---|---|
| Storing `LocalDateTime` for events | Store `Instant`/UTC `OffsetDateTime` |
| `ldt.toInstant(ZoneOffset.UTC)` on non-UTC data | `ldt.atZone(actualZone).toInstant()` |
| Using `withZoneSameLocal` to "convert" a zone | `withZoneSameInstant` |
| `instant.plus(1, MONTHS)` | Convert to `ZonedDateTime`/`LocalDate` first |
| Fixed-offset `OffsetDateTime` for future recurring events | Store `LocalDateTime` + region `ZoneId` |
| `ZonedDateTime.equals` to compare moments | `isEqual` or compare `Instant`s |
| Relying on `ZoneId.systemDefault()` | Pass the zone explicitly |
| Using `IST`-style abbreviations | Use `Asia/Kolkata`-style IDs |
| Assuming every day has 24 hours | DST days have 23 or 25 (in zones that observe DST) |

### Debugging

- Times are "off by an hour" only on certain dates → DST. Print `zdt.getOffset()` and `zdt.getZone()`.
- Times are off by the same constant (e.g. 5:30) → a zone was assumed (often UTC or system default) on one side and not the other. Log the `Instant`.
- `UnsupportedTemporalTypeException: Unsupported unit: Months` → you called a calendar operation on an `Instant`.
- Time-zone data is shipped with the JDK. When governments change DST rules, you need an updated JDK or tzdata, or your zone math will be wrong for the affected region.

---

## Quick Summary

- `Instant` = a point on the UTC timeline (machine time). `ZonedDateTime` = that point as seen in a region, with DST rules. `OffsetDateTime` = fixed shift, good for interchange.
- Convert moments between zones with `withZoneSameInstant`; `withZoneSameLocal` changes the moment.
- DST creates gaps and overlaps. `plusDays` keeps the clock time, `plusHours` keeps elapsed time.
- `ZonedDateTime.equals` includes the zone, so use `isEqual` or compare `Instant`s.
- A `LocalDateTime` becomes a moment only when you supply the *correct* zone.

**Next:** [Duration and Period](03_duration-and-period.md)
