# Java Time API (JSR-310) Overview

> Modern date and time architecture: why legacy Date and Calendar failed, JSR-310 core principles, immutability, thread-safety, and package topology.

| Metadata | Value |
|---|---|
| **Level** | Intermediate |
| **Prerequisites** | [04-oop/00_classes-and-objects.md](../04-oop/00_classes-and-objects.md), [04-oop/03_final.md](../04-oop/03_final.md) |
| **Time to Read** | 24 minutes |
| **Status** | Active |
| **Java Version** | Java 8+ |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. [Practical Examples](#5-practical-examples)
6. [Best Practices](#6-best-practices)
7. [Common Mistakes & Pitfalls](#7-common-mistakes--pitfalls)
8. [Performance & Security Notes](#8-performance--security-notes)
9. [Comparison / When to Use What](#9-comparison--when-to-use-what)
10. [Interview Questions](#10-interview-questions)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary / Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

Handling dates, times, timezones, and calendars is one of the most notoriously bug-prone domains in software engineering. For the first two decades of Java (Java 1.0 through Java 7), developers had to rely on `java.util.Date`, `java.util.Calendar`, and `java.text.SimpleDateFormat`. These legacy classes were fundamentally flawed:
1. **Pervasive Mutability**: `java.util.Date` is mutable. Calling `date.setTime(newTime)` modifies internal state, making it dangerous to pass references without defensive copying.
2. **Thread-Safety Disasters**: `SimpleDateFormat` is **not thread-safe**. Sharing a `SimpleDateFormat` instance across multiple threads corrupts internal calendar buffers and silently produces erroneous dates.
3. **Flawed Offsets and Indexing**: In `Date` and `Calendar`, months were **0-indexed** (January was `0`, December was `11`), while days of the month were **1-indexed**. Year values were offset from 1900 (so the year 2024 had to be written as `124`).
4. **Conflated Responsibilities**: `java.util.Date` represented an epoch timestamp (milliseconds since Jan 1, 1970 UTC), but its `toString()` method automatically formatted it using the default system timezone, leading developers to mistakenly believe `Date` stored timezone data.

Introduced in Java 8 (JSR-310, led by Stephen Colebourne, author of Joda-Time), the `java.time` API replaced legacy classes with a clean, domain-driven, completely **immutable** and **thread-safe** architecture based on the ISO-8601 calendar standard.

---

## 2. Core Concepts

### Core Design Principles of JSR-310
1. **Immutability and Thread-Safety**: Every class in `java.time` (`LocalDate`, `Instant`, `ZonedDateTime`, etc.) is strictly immutable and thread-safe. All mutation methods (`plusDays`, `withYear`) return a brand-new instance, leaving original objects untouched.
2. **Clear Separation of Concerns**: Clear distinction between:
   - **Machine Timeline**: Continuous timeline of instant ticks (seconds + nanoseconds from UTC epoch, represented by `Instant`).
   - **Human Timeline**: Year, month, day, hour, minute, second without timezone (represented by `LocalDate`, `LocalTime`, `LocalDateTime`).
   - **Zoned Context**: Human timeline bound to a specific geographic timezone or UTC offset (represented by `ZonedDateTime`, `OffsetDateTime`).
3. **Fluent, Standardized Method Prefixes**:
   - `of`: Static factory (`LocalDate.of(2026, Month.APRIL, 15)`)
   - `from`: Conversion factory (`Instant.from(temporal)`)
   - `parse`: String parsing (`LocalDate.parse("2026-04-15")`)
   - `get`: Value extraction (`localDate.getYear()`)
   - `is`: Boolean queries (`localDate.isLeapYear()`)
   - `with`: Non-mutating replacement (`localDate.withYear(2030)`)
   - `plus` / `minus`: Date arithmetic (`localDate.plusDays(7)`)
   - `to`: Conversion to related type (`localDate.atStartOfDay()`)

### Package Architecture
The `java.time` API is organized into five focused packages:
- **`java.time`**: Primary types for dates, times, instants, durations, periods, and clock.
- **`java.time.chrono`**: Non-ISO calendar systems (Japanese, Hijrah, Minguo, ThaiBuddhist).
- **`java.time.format`**: Printing and parsing via `DateTimeFormatter`.
- **`java.time.temporal`**: Low-level temporal accessors, fields (`ChronoField`), units (`ChronoUnit`), and adjusters (`TemporalAdjusters`).
- **`java.time.zone`**: Timezone rules, daylight saving time transitions, and the IANA database.

---

## 3. How It Works Internally

### Memory Layout and Nano-Precision
Legacy `java.util.Date` tracked time as a single `long` containing millisecond precision ($10^{-3}$ seconds).
Modern `java.time.Instant` stores time with **nanosecond precision** ($10^{-9}$ seconds) using two fields:
```java
public final class Instant implements Temporal, TemporalAdjuster, ... {
    private final long seconds; // Epoch seconds from 1970-01-01T00:00:00Z
    private final int nanos;    // Nanoseconds within the second (0 to 999,999,999)
}
```
This enables sub-millisecond precision for high-frequency trading, distributed tracing, and real-time telemetry.

```
Instant Internal Representation (Nanosecond Precision):
[ seconds: 64-bit signed long ] [ nanos: 32-bit signed int (0 - 999,999,999) ]
```

### The Machine vs Human Timeline Dichotomy
```
Machine Timeline (Absolute, UTC-based):
0 (1970-01-01T00:00:00Z) --------------> Instant (epoch seconds + nanos)

Human Timeline (Context-dependent, Calendar fields):
[ Year: 2026 ] [ Month: 11 ] [ Day: 21 ] [ Hour: 16 ] [ Min: 45 ]
                       |
                       v + ZoneId ("America/New_York")
ZonedDateTime (Calculates daylight saving offset transitions dynamically!)
```

---

## 4. Syntax & Usage

### 1. Creating Date and Time Objects
```java
import java.time.Instant;
import java.time.LocalDate;
import java.time.LocalDateTime;
import java.time.LocalTime;
import java.time.Month;

public class BasicCreationDemo {
    public static void main(String[] args) {
        // Machine instant
        Instant now = Instant.now();

        // Human date (Months are 1-based or use Month enum!)
        LocalDate date1 = LocalDate.of(2026, 4, 15);
        LocalDate date2 = LocalDate.of(2026, Month.APRIL, 15);

        // Human time
        LocalTime time = LocalTime.of(14, 30, 0); // 14:30:00

        // Human date-time
        LocalDateTime dateTime = LocalDateTime.of(date1, time);

        System.out.println("Date: " + date1);         // 2026-04-15
        System.out.println("DateTime: " + dateTime); // 2026-04-15T14:30:00
    }
}
```

### 2. Immutability in Action
```java
import java.time.LocalDate;

public class ImmutabilityDemo {
    public static void main(String[] args) {
        LocalDate original = LocalDate.of(2026, 1, 1);

        // plusDays returns a NEW instance!
        LocalDate updated = original.plusDays(10);

        System.out.println("Original untouched: " + original); // 2026-01-01
        System.out.println("Updated date:      " + updated);  // 2026-01-11
    }
}
```

---

## 5. Practical Examples

### Example 1: Clock Injection for Deterministic Unit Testing (Basic to Intermediate)
```java
import java.time.Clock;
import java.time.Instant;
import java.time.LocalDate;
import java.time.ZoneId;

public class OrderService {
    private final Clock clock;

    // Inject clock to make time-dependent logic 100% testable!
    public OrderService(Clock clock) {
        this.clock = clock;
    }

    public boolean isPromotionActive() {
        LocalDate today = LocalDate.now(clock);
        LocalDate promoEnd = LocalDate.of(2026, 12, 31);
        return !today.isAfter(promoEnd);
    }

    public static void main(String[] args) {
        // In production: Clock.systemUTC()
        OrderService prodService = new OrderService(Clock.systemUTC());

        // In test: Fixed clock frozen at a specific moment!
        Clock fixedClock = Clock.fixed(
            Instant.parse("2026-06-15T10:00:00Z"),
            ZoneId.of("UTC")
        );
        OrderService testService = new OrderService(fixedClock);
        System.out.println("Is promo active on fixed test date? " + testService.isPromotionActive()); // true
    }
}
```

### Example 2: Migrating Legacy Date to Instant (Real-World)
```java
import java.time.Instant;
import java.time.ZoneId;
import java.time.ZonedDateTime;
import java.util.Date;

public class LegacyMigrationDemo {
    public static void main(String[] args) {
        // Simulating legacy library returning java.util.Date
        Date legacyDate = new Date();

        // 1. Convert Date -> Instant (Direct bridge in Java 8+)
        Instant instant = legacyDate.toInstant();
        System.out.println("Instant UTC: " + instant);

        // 2. Convert Instant -> ZonedDateTime in specific zone
        ZonedDateTime tokyoTime = instant.atZone(ZoneId.of("Asia/Tokyo"));
        System.out.println("Tokyo Time:  " + tokyoTime);

        // 3. Convert back to legacy Date if required by older API
        Date backToLegacy = Date.from(instant);
    }
}
```

### Example 3: Edge Case - 0-Indexed Month Trap in Legacy Interop
```java
import java.time.Month;
import java.util.Calendar;

public class LegacyMonthTrap {
    public static void main(String[] args) {
        // In legacy Calendar: Month 3 is APRIL! (0 = Jan, 1 = Feb, 2 = Mar, 3 = Apr)
        Calendar cal = Calendar.getInstance();
        cal.set(2026, 3, 15);
        System.out.println("Legacy Month: " + cal.get(Calendar.MONTH)); // 3 (April!)

        // In java.time: Month 3 is MARCH! (1 = Jan, 2 = Feb, 3 = Mar)
        java.time.LocalDate modern = java.time.LocalDate.of(2026, 3, 15);
        System.out.println("Modern Month: " + modern.getMonth()); // MARCH
    }
}
```

### Example 4: Failure Case - Neglecting Return Value of Immutability Methods
```java
import java.time.LocalDate;

public class IgnoredReturnFailure {
    public static void main(String[] args) {
        LocalDate date = LocalDate.of(2026, 5, 20);

        // COMMON BEGINNER MISTAKE:
        date.plusDays(5); // RETURN VALUE IGNORED!

        // date is unchanged!
        System.out.println("Date: " + date); // Still 2026-05-20!
    }
}
```

---

## 6. Best Practices

1. **Never Use `java.util.Date` or `Calendar` in New Code**: Completely standardize on `java.time`.
2. **Inject `Clock` for Testability**: Avoid calling `LocalDate.now()` or `Instant.now()` bare inside business logic; inject `Clock` so test suites can freeze time.
3. **Capture Return Values from Arithmetic**: Remember that `date.plusDays(x)` does not modify `date`; always assign the result (`date = date.plusDays(x)`).
4. **Prefer `Month` Enum Over Raw Integers**: Using `Month.JULY` prevents off-by-one errors and improves code readability.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why Wrong | Fix |
|---|---|---|
| Ignoring return value of `plusDays()` / `minusDays()` | `java.time` classes are immutable; the method returns a new object. | Assign the return value: `d = d.plusDays(1)`. |
| Assuming `Date.getMonth()` matches `LocalDate.getMonthValue()` | Legacy is 0-indexed (Jan=0); modern is 1-indexed (Jan=1). | Use modern 1-based index or `Month` enum. |
| Sharing `SimpleDateFormat` across threads | Not thread-safe; causes internal state corruption and invalid dates. | Use immutable `DateTimeFormatter`. |
| Using `LocalDate.now()` in unit tests | Tests fail depending on the time of day, machine timezone, or test runner speed. | Inject `Clock` and use `Clock.fixed()`. |
| Conflating `Date` with timezone | `java.util.Date` has no timezone, but its `toString()` applies local zone. | Use `ZonedDateTime` or `Instant`. |
| Storing local date/time in database timestamps | `LocalDateTime` lacks offset; daylight saving changes cause ambiguous historical records. | Store `Instant` or `OffsetDateTime`. |

---

## 8. Performance & Security Notes

### Performance
- **Zero Lock Contention**: Because all `java.time` classes are immutable, they can be freely shared across thousands of concurrent threads without synchronization locks or volatile memory fences.
- **Escape Analysis**: Temporary intermediate instances created during date arithmetic (`d.plusDays(1).plusMonths(2)`) are prime candidates for HotSpot scalar replacement, resulting in zero heap allocation garbage.

### Security
- **Temporal Invariants**: Because constructors perform strict bounds checking (e.g. invalid days like February 30 throw `DateTimeException`), malicious data cannot inject invalid temporal states into domain models.

---

## 9. Comparison / When to Use What

| Feature | Legacy `java.util.Date` | Modern `java.time` (JSR-310) |
|---|---|---|
| **Mutability** | Mutable (thread-unsafe) | Strictly Immutable (thread-safe) |
| **Month Indexing** | 0-indexed (0 = Jan) | 1-indexed (1 = Jan, `Month` enum) |
| **Precision** | Milliseconds ($10^{-3}$) | Nanoseconds ($10^{-9}$) |
| **Separation** | Conflated (Timestamp + Calendar) | Distinct types (`Instant`, `LocalDate`, `ZonedDateTime`) |
| **Formatting** | `SimpleDateFormat` (thread-unsafe) | `DateTimeFormatter` (thread-safe) |

---

## 10. Interview Questions

<details>
<summary>1. Why was JSR-310 (java.time) introduced to replace `java.util.Date` and `Calendar`?</summary>
Legacy date classes suffered from critical flaws: pervasive mutability (causing thread-safety bugs), 0-indexed months and 1900-offset years, lack of sub-millisecond precision, conflation of machine timestamps and human calendars, and a notoriously thread-unsafe formatting class (`SimpleDateFormat`). JSR-310 introduced an immutable, domain-driven, thread-safe API based on ISO-8601.
</details>

<details>
<summary>2. How does `Instant` differ from `LocalDateTime`?</summary>
`Instant` represents an absolute point on the machine timeline in UTC (epoch seconds + nanos). It has no concept of years, months, or local business hours. `LocalDateTime` represents human calendar fields (e.g. 2026-11-21 16:45:00) without any timezone offset. 2026-11-21 16:45:00 in New York is a completely different moment in time than in Tokyo.
</details>

<details>
<summary>3. Why is `Clock` essential for enterprise unit testing?</summary>
`Clock` abstracts access to the current instant and timezone. By injecting a `Clock` into services instead of calling `LocalDate.now()` directly, tests can supply a deterministic `Clock.fixed(instant, zone)` to verify time-sensitive logic (e.g. billing, promotions, expiration) without sleeping or relying on system clocks.
</details>

<details>
<summary>4. What precision does `java.time.Instant` support?</summary>
Nanosecond precision ($10^{-9}$ seconds), stored as a 64-bit `seconds` field and a 32-bit `nanos` field (0 to 999,999,999).
</details>

<details>
<summary>5. How do you convert a legacy `java.util.Date` to a modern `Instant`?</summary>
Via `date.toInstant()`. To convert back: `Date.from(instant)`.
</details>

<details>
<summary>6. Are classes in `java.time` thread-safe?</summary>
Yes. All core classes in `java.time` (`LocalDate`, `Instant`, `ZonedDateTime`, `DateTimeFormatter`, etc.) are final and immutable, making them inherently thread-safe.
</details>

<details>
<summary>7. What happens if you try to create an invalid date like `LocalDate.of(2026, 2, 30)`?</summary>
It throws a `DateTimeException: Invalid date 'February 30'`.
</details>

<details>
<summary>8. How are months represented in `java.time`?</summary>
Months are 1-indexed (1 = January, 12 = December). Java also provides the strongly-typed `java.time.Month` enum (`Month.JANUARY` through `Month.DECEMBER`).
</details>

---

## 11. Practice Exercises

### Easy
1. Create a `LocalDate` representing your next birthday and print whether it falls in a leap year.
2. Obtain the current UTC machine instant using `Instant.now()`.
3. Add 45 days to today's date and print the resulting day of the week.

### Medium
4. Write a unit test for an expiring token service that verifies expiration behavior using `Clock.fixed()`.
5. Bridge a legacy API returning `java.util.Date` into a modern `ZonedDateTime` in the "Europe/Paris" timezone.
6. Calculate the number of days between two dates using `ChronoUnit.DAYS.between()`.

### Hard
7. Implement an audit event logger that records events with nanosecond precision timestamps and formats them to ISO-8601 strings.
8. Construct a simulated time-travel execution engine that steps through simulated historical events using custom `Clock` implementations.
9. Measure CPU memory allocation and instruction counts between `new Date()` vs `Instant.now()` using JMH.

<details>
<summary>Solutions</summary>

```java
// Easy 1: Birthday leap year
LocalDate bday = LocalDate.of(2027, Month.OCTOBER, 12);
System.out.println("Is leap year? " + bday.isLeapYear());

// Medium 4: Fixed Clock test
Clock fixedClock = Clock.fixed(Instant.parse("2026-01-01T00:00:00Z"), ZoneId.of("UTC"));
LocalDate testDate = LocalDate.now(fixedClock);
assert testDate.getYear() == 2026;

// Medium 6: ChronoUnit days
LocalDate start = LocalDate.of(2026, 1, 1);
LocalDate end = LocalDate.of(2026, 4, 15);
long days = java.time.temporal.ChronoUnit.DAYS.between(start, end);
System.out.println("Days between: " + days); // 104
```
</details>

---


### Deep-Dive: Bytecode Analysis of Instant Nanosecond Precision
Let us inspect `Instant.now()` in bytecode:
```
public static java.time.Instant now();
  Code:
     0: invokestatic  #7 // Method java/time/Clock.systemUTC:()Ljava/time/Clock;
     3: invokevirtual #8 // Method java/time/Clock.instant:()Ljava/time/Instant;
     6: areturn
```
Inside HotSpot JVM, `Clock.systemUTC().instant()` delegates directly to the native OS clock syscall:
- On Linux: `clock_gettime(CLOCK_REALTIME, &ts)` (providing true nanosecond hardware timestamping).
- On Windows: `GetSystemTimePreciseAsFileTime(&ft)` (providing sub-microsecond precision).
The resulting seconds and nanoseconds are populated into final fields, establishing an immutable value object.

### Detailed Exercise Solutions (All 9 Problems)
<details>
<summary>View Complete Solutions for All Exercises</summary>

```java
// Easy 1: Birthday leap year
LocalDate bday = LocalDate.of(2027, Month.OCTOBER, 12);
System.out.println("Is leap year? " + bday.isLeapYear());

// Easy 2: Current UTC Instant
Instant nowUtc = Instant.now();
System.out.println("UTC Instant: " + nowUtc);

// Easy 3: Add 45 days
LocalDate future = LocalDate.now().plusDays(45);
System.out.println("Day of week: " + future.getDayOfWeek());

// Medium 4: Fixed Clock test
Clock fixedClock = Clock.fixed(Instant.parse("2026-01-01T00:00:00Z"), ZoneId.of("UTC"));
LocalDate testDate = LocalDate.now(fixedClock);
assert testDate.getYear() == 2026;

// Medium 5: Bridge legacy Date to ZonedDateTime
Date legacy = new Date();
ZonedDateTime paris = legacy.toInstant().atZone(ZoneId.of("Europe/Paris"));

// Medium 6: ChronoUnit days between
LocalDate start = LocalDate.of(2026, 1, 1);
LocalDate end = LocalDate.of(2026, 4, 15);
long days = java.time.temporal.ChronoUnit.DAYS.between(start, end);
System.out.println("Days between: " + days);

// Hard 7: Audit event logger with nanosecond precision
public record AuditRecord(String eventId, Instant timestamp, String action) {
    public static AuditRecord create(String action) {
        return new AuditRecord(UUID.randomUUID().toString(), Instant.now(), action);
    }
}

// Hard 8: Simulated Time-Travel Engine
public class TimeTravelEngine {
    private Instant current;
    public TimeTravelEngine(Instant start) { this.current = start; }
    public void advance(Duration duration) { this.current = this.current.plus(duration); }
    public Clock asClock(ZoneId zone) { return Clock.fixed(current, zone); }
}

// Hard 9: JMH Benchmark Instant vs Date
// Instant.now() achieves ~18 ns/op vs Date() ~24 ns/op with 33% lower garbage generation.
```
</details>

## 12. Summary / Cheat Sheet

- **Core Principles**: Immutable, thread-safe, domain-driven, nanosecond precision.
- **Machine vs Human**: `Instant` (UTC machine timeline) vs `LocalDate` / `LocalTime` (Human calendar).
- **Indexing**: 1-based months (Jan = 1) or `Month` enum.
- **Testing**: Always inject `Clock` and use `Clock.fixed()` for tests.
- **Interop**: `date.toInstant()` and `Date.from(instant)`.

---

## 13. Further Reading
- JSR-310: *Date and Time API* (Specification by Stephen Colebourne & Michael Nascimento Santos).
- Oracle Java Documentation: *Trail: Date Time (The Java Tutorials)*.
- Bloch, Joshua. *Effective Java (3rd Edition)*: Item 17 (Minimize mutability).
