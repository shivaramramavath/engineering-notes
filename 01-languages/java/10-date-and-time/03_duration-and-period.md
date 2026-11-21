# Duration and Period: Measuring Amounts of Time

> Time-based amounts vs date-based amounts in Java: Duration mechanics, Period calendar intervals, ISO-8601 string representations, and conversion rules.

| Metadata | Value |
|---|---|
| **Level** | Intermediate |
| **Prerequisites** | [00_java-time-overview.md](00_java-time-overview.md), [01_local-date-time.md](01_local-date-time.md), [02_instant-and-zoned-date-time.md](02_instant-and-zoned-date-time.md) |
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

In software systems, we frequently need to model an **amount of time**:
- "The HTTP connection timed out after 30 seconds."
- "The user subscription renews every 3 months."
- "The batch job completed in 450 milliseconds."
- "The patient is 28 years, 4 months, and 12 days old."

In legacy Java, developers often represented elapsed time as raw primitive integers or longs (`int timeout = 30;`). This caused catastrophic unit mismatches (e.g. one method expects seconds, but caller passes milliseconds).

Java distinguishes between two fundamentally different types of time amounts:
1. **`Duration`**: A **time-based** amount measured on the machine timeline in seconds and nanoseconds (e.g. 10.5 seconds, 500 milliseconds, 48 hours).
2. **`Period`**: A **date-based** amount measured in human calendar units: years, months, and days (e.g. "1 year, 2 months, and 5 days").

---

## 2. Core Concepts

### `java.time.Duration`
- Measures elapsed machine time in **seconds and nanoseconds**.
- Operates on time-based objects (`Instant`, `LocalTime`, `LocalDateTime`, `ZonedDateTime`).
- **Cannot** be applied to a `LocalDate` because a `LocalDate` has no time components.
- Output format follows ISO-8601: `PT20.345S` (Period Time 20.345 Seconds), `PT15M` (15 Minutes), `PT8H` (8 Hours).

### `java.time.Period`
- Measures calendar time in **years, months, and days**.
- Operates on date-based objects (`LocalDate`).
- **Cannot** be applied to `Instant` or `LocalTime` because machine time has no calendar months or years (months vary from 28 to 31 days).
- Output format follows ISO-8601: `P1Y2M15D` (Period 1 Year, 2 Months, 15 Days).

```
+-------------------------------------------------------+
| Fundamental Distinction                              |
|                                                       |
| Concept            | Class    | Units                 |
|--------------------|----------|-----------------------|
| Machine Time Amount| Duration | Seconds, Nanoseconds  |
| Human Date Amount  | Period   | Years, Months, Days   |
+-------------------------------------------------------+
```

---

## 3. How It Works Internally

### Memory Layout
`Duration` stores time as a signed 64-bit `seconds` and 32-bit `nanos` field:
```java
public final class Duration implements TemporalAmount, Comparable<Duration>, ... {
    private final long seconds;
    private final int nanos;
}
```
`Period` stores calendar intervals as three 32-bit integers:
```java
public final class Period implements ChronoPeriod, ... {
    private final int years;
    private final int months;
    private final int days;
}
```

### The 24-Hour vs Calendar Day Difference
Because daylight saving time can create days with 23 or 25 hours:
- `date.plus(Duration.ofDays(1))` advances the clock by **exactly 86,400 seconds**.
- `date.plus(Period.ofDays(1))` advances the calendar by **one day**, keeping the local clock time identical even if the physical day was 23 hours long.

---

## 4. Syntax & Usage

### 1. Working with `Duration`
```java
import java.time.Duration;
import java.time.Instant;
import java.time.LocalTime;

public class DurationDemo {
    public static void main(String[] args) {
        // Factory creation
        Duration d1 = Duration.ofSeconds(30);
        Duration d2 = Duration.ofMinutes(5);
        Duration d3 = Duration.ofMillis(250);

        // Parsing ISO-8601 strings
        Duration parsed = Duration.parse("PT15M30S"); // 15 minutes, 30 seconds

        // Measuring between two temporals
        LocalTime t1 = LocalTime.of(10, 0);
        LocalTime t2 = LocalTime.of(11, 45);
        Duration elapsed = Duration.between(t1, t2);
        System.out.println("Elapsed minutes: " + elapsed.toMinutes()); // 105
    }
}
```

### 2. Working with `Period`
```java
import java.time.LocalDate;
import java.time.Period;

public class PeriodDemo {
    public static void main(String[] args) {
        // Factory creation
        Period p1 = Period.ofDays(10);
        Period p2 = Period.of(1, 2, 15); // 1 year, 2 months, 15 days

        // Parsing ISO-8601 strings
        Period parsed = Period.parse("P2Y6M"); // 2 years, 6 months

        // Measuring between two dates
        LocalDate birthDate = LocalDate.of(1995, 5, 20);
        LocalDate today = LocalDate.of(2026, 11, 21);
        Period age = Period.between(birthDate, today);

        System.out.printf("Age: %d years, %d months, %d days\n",
            age.getYears(), age.getMonths(), age.getDays());
        // Age: 31 years, 6 months, 1 day
    }
}
```

---

## 5. Practical Examples

### Example 1: Robust Rate Limiting Window (Basic to Intermediate)
```java
import java.time.Duration;
import java.time.Instant;

public class RateLimiterWindow {
    private final Duration windowDuration;
    private final int maxRequests;
    private int requestCount = 0;
    private Instant windowStart = Instant.now();

    public RateLimiterWindow(Duration windowDuration, int maxRequests) {
        this.windowDuration = windowDuration;
        this.maxRequests = maxRequests;
    }

    public synchronized boolean allowRequest() {
        Instant now = Instant.now();
        if (Duration.between(windowStart, now).compareTo(windowDuration) > 0) {
            // Window expired; reset
            windowStart = now;
            requestCount = 0;
        }
        if (requestCount < maxRequests) {
            requestCount++;
            return true;
        }
        return false;
    }

    public static void main(String[] args) {
        RateLimiterWindow limiter = new RateLimiterWindow(Duration.ofSeconds(1), 2);
        System.out.println("Req 1: " + limiter.allowRequest()); // true
        System.out.println("Req 2: " + limiter.allowRequest()); // true
        System.out.println("Req 3: " + limiter.allowRequest()); // false (exceeded)
    }
}
```

### Example 2: Subscription Expiration Warning (Real-World)
```java
import java.time.LocalDate;
import java.time.Period;

public class SubscriptionService {
    public static boolean isRenewalDueSoon(LocalDate expiryDate, LocalDate currentDate) {
        Period remaining = Period.between(currentDate, expiryDate);
        // Alert if within 0 years, 0 months, and <= 7 days
        return remaining.getYears() == 0 && remaining.getMonths() == 0 &&
               remaining.getDays() >= 0 && remaining.getDays() <= 7;
    }

    public static void main(String[] args) {
        LocalDate today = LocalDate.of(2026, 11, 21);
        LocalDate expiry = LocalDate.of(2026, 11, 25);
        System.out.println("Renewal due soon? " + isRenewalDueSoon(expiry, today)); // true
    }
}
```

### Example 3: Edge Case - `Period.getDays()` is NOT Total Days!
```java
import java.time.LocalDate;
import java.time.Period;
import java.time.temporal.ChronoUnit;

public class PeriodDaysTrap {
    public static void main(String[] args) {
        LocalDate start = LocalDate.of(2026, 1, 1);
        LocalDate end = LocalDate.of(2026, 3, 10);

        Period period = Period.between(start, end);
        // TRAP: period.getDays() returns ONLY the remaining day field!
        System.out.println("period.getDays(): " + period.getDays()); // 9 (2 months and 9 days!)

        // FIX: For total days, use ChronoUnit.DAYS.between()
        long totalDays = ChronoUnit.DAYS.between(start, end);
        System.out.println("Total days:       " + totalDays);       // 68 days!
    }
}
```

### Example 4: Failure Case - Passing LocalDate to Duration
```java
import java.time.Duration;
import java.time.LocalDate;
import java.time.temporal.UnsupportedTemporalTypeException;

public class DurationLocalDateFailure {
    public static void main(String[] args) {
        LocalDate d1 = LocalDate.of(2026, 1, 1);
        LocalDate d2 = LocalDate.of(2026, 1, 10);

        try {
            // LocalDate does NOT support ChronoUnit.SECONDS!
            Duration.between(d1, d2);
        } catch (UnsupportedTemporalTypeException e) {
            System.out.println("Caught expected: LocalDate cannot be measured with Duration!");
            // Fix: Use Period.between(d1, d2) or ChronoUnit.DAYS.between(d1, d2)
        }
    }
}
```

---

## 6. Best Practices

1. **Use `Duration` for Technical Latency and Timeouts**: Network connect timeouts, sleep delays, retry intervals, cache TTLs.
2. **Use `Period` for Human Lifecycle and Business Rules**: Age calculations, annual renewals, quarterly tax deadlines.
3. **Use `ChronoUnit.DAYS.between()` for Total Days**: Avoid using `Period.getDays()` if you need the total elapsed day count across months.
4. **Leverage Modern Configuration Strings**: In Spring Boot or microservices, map configuration properties using ISO-8601 strings (`timeout: PT30S`, `retention: P30D`).

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why Wrong | Fix |
|---|---|---|
| Calling `Duration.between(localDate1, localDate2)` | `LocalDate` has no seconds/nanos; throws `UnsupportedTemporalTypeException`. | Use `Period.between()` or `ChronoUnit.DAYS`. |
| Using `period.getDays()` to get total days | Returns only the day component of the (Year, Month, Day) tuple. | Use `ChronoUnit.DAYS.between(d1, d2)`. |
| Chaining `Period.ofYears(1).ofMonths(2)` | `ofYears` is a static method; chaining calls ignores the first call! | Use `Period.of(1, 2, 0)`. |
| Passing raw numeric timeouts (`int timeout`) | Ambiguous units; invites seconds vs milliseconds bugs. | Accept `Duration timeout` directly. |
| Neglecting negative durations | If `t1` is after `t2`, `Duration.between(t1, t2)` is negative. | Check `duration.isNegative()`. |
| Assuming 1 month has 30 days in `Duration` | Months vary from 28 to 31 days; `Duration` does not support `ChronoUnit.MONTHS`. | Use `Period` for months. |

---

## 8. Performance & Security Notes

### Performance
- **Compact Primitive Fields**: `Duration` stores a single `long` and `int`. Arithmetic operations execute as direct CPU integer math without heap churn when optimized by HotSpot escape analysis.

### Security
- **Defense Against Denial of Service via Timeout Inversion**: Always validate that durations parsed from external inputs or HTTP headers are positive (`!duration.isNegative() && duration.toMillis() > 0`) to prevent infinite timeout hangs.

---

## 9. Comparison / When to Use What

| Criteria | `Duration` | `Period` | `ChronoUnit.X.between()` |
|---|---|---|---|
| **Measurement Type** | Machine Time | Human Calendar | Single Unit Quantity |
| **Units Supported** | Seconds, Nanos, Millis, Hours | Years, Months, Days | Any specific unit (Days, Hours, Weeks) |
| **Compatible Types** | `Instant`, `LocalTime`, `LocalDateTime` | `LocalDate` | Any temporal supporting that unit |
| **Total Duration** | `toDays()`, `toHours()`, `toMillis()` | Components only (`getDays()`) | Returns total count directly |
| **ISO-8601 String** | Starts with `PT` (`PT15M`) | Starts with `P` (`P1Y2M`) | N/A |

---

## 10. Interview Questions

<details>
<summary>1. What is the fundamental difference between `Duration` and `Period`?</summary>
`Duration` measures time-based amounts on the machine timeline in seconds and nanoseconds (e.g. 500 ms, 30 seconds). `Period` measures date-based amounts on the human calendar in years, months, and days.
</details>

<details>
<summary>2. Why does `Period.between(LocalDate.of(2026, 1, 1), LocalDate.of(2026, 3, 15)).getDays()` return 14 instead of 73?</summary>
Because `Period` represents time in normalized calendar fields (e.g., 2 months and 14 days). `getDays()` returns only the day portion of the composite tuple. To obtain the total number of days across the entire span, use `ChronoUnit.DAYS.between()`.
</details>

<details>
<summary>3. Can you calculate `Duration.between(localDate1, localDate2)`?</summary>
No. `LocalDate` contains no time components (hours, minutes, seconds). Calling `Duration.between()` on two `LocalDate`s throws `UnsupportedTemporalTypeException: Unsupported unit: Seconds`.
</details>

<details>
<summary>4. What is the format of an ISO-8601 duration string?</summary>
It starts with `P` followed by date units, and an optional `T` section followed by time units. For example, `PT1H30M` is 1 hour 30 minutes, and `P2Y3M` is 2 years 3 months.
</details>

<details>
<summary>5. What happens if you write `Period p = Period.ofDays(5).ofMonths(2);`?</summary>
`ofMonths(2)` is a static method that returns a brand new `Period` representing 2 months, completely discarding the `ofDays(5)` call. The correct syntax is `Period.of(0, 2, 5)` or `Period.ofMonths(2).plusDays(5)`.
</details>

<details>
<summary>6. How does a 23-hour Daylight Saving Time day affect `Duration` vs `Period`?</summary>
Adding `Period.ofDays(1)` to 10:00 AM preserves 10:00 AM on the next calendar day. Adding `Duration.ofDays(1)` (which is strictly 86,400 seconds) advances the clock by 24 physical hours, resulting in 11:00 AM due to the missing hour.
</details>

<details>
<summary>7. How can you test if a `Duration` has expired?</summary>
Via `duration.isZero()` or `duration.isNegative()`.
</details>

<details>
<summary>8. How do you convert a `Duration` to milliseconds?</summary>
Via `duration.toMillis()`.
</details>

---

## 11. Practice Exercises

### Easy
1. Create a `Duration` of 2 hours and 30 minutes and print its total minutes.
2. Create a `Period` representing 3 years and 6 months and print its ISO-8601 string.
3. Check whether a `Duration` of -5 seconds is negative.

### Medium
4. Write a function that calculates a person's exact age in years, months, and days from their birthdate using `Period.between()`.
5. Implement an exponential backoff calculator that doubles a `Duration` with each retry up to a maximum duration.
6. Calculate the total number of days between two dates across leap year boundaries using `ChronoUnit.DAYS`.

### Hard
7. Implement an in-memory token bucket rate limiter that refills tokens using fractional `Duration` nanosecond calculations.
8. Design a human-friendly duration formatter that formats durations into strings like "2 hours, 15 minutes, 4 seconds" without third-party libraries.
9. Benchmark performance and allocation rates of `Duration.between()` vs raw primitive subtraction (`System.nanoTime()`) under JMH.

<details>
<summary>Solutions</summary>

```java
// Easy 1: Duration minutes
Duration d = Duration.ofHours(2).plusMinutes(30);
System.out.println("Total minutes: " + d.toMinutes()); // 150

// Medium 5: Exponential backoff
public static Duration nextBackoff(Duration current, Duration max) {
    Duration doubled = current.multipliedBy(2);
    return doubled.compareTo(max) > 0 ? max : doubled;
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Duration**: Time-based (seconds, nanos). Use for timeouts, latency, technical timestamps.
- **Period**: Date-based (years, months, days). Use for human calendar intervals.
- **ISO Format**: `Duration` starts with `PT` (`PT30S`); `Period` starts with `P` (`P1Y`).
- **Total Days**: Never use `period.getDays()`; always use `ChronoUnit.DAYS.between()`.
- **API Signatures**: Prefer `Duration timeout` over `int timeoutMs`.

---

## 13. Further Reading
- Oracle Java Documentation: *The Duration and Period Classes*.
- JSR-310: *Temporal Amount Specifications*.
- Bloch, Joshua. *Effective Java (3rd Edition)*: Item 50 (Make defensive copies when needed).
