# LocalDate, LocalTime, and LocalDateTime

> Human calendar concepts in Java: date arithmetic, temporal adjusters, ChronoField manipulation, and boundary calculations.

| Metadata | Value |
|---|---|
| **Level** | Intermediate |
| **Prerequisites** | [00_java-time-overview.md](00_java-time-overview.md) |
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

In real-world business domains, many concepts describe dates and times without reference to any specific geographic timezone:
- A user's birthday: "May 20, 1995" is May 20th regardless of whether the user is in London, Tokyo, or San Francisco.
- Store operating hours: A supermarket that opens at "08:00:00" opens at 8:00 AM local time for whatever store branch is being visited.
- An invoice payment due date: "2026-12-31".

Using a timezone-bound timestamp for these domain concepts introduces bugs: if you store a birthday as an instant and display it in a different timezone, the date can shift backward by a day!

The `LocalDate`, `LocalTime`, and `LocalDateTime` classes represent human calendar dates and times strictly within the ISO-8601 proleptic calendar system without any timezone offset.

---

## 2. Core Concepts

### The Three Local Temporal Classes
1. **`LocalDate`**: Represents a date (year, month, day of month) without a time or timezone (e.g., `2026-11-21`).
2. **`LocalTime`**: Represents a time of day (hour, minute, second, nanosecond) without a date or timezone (e.g., `16:45:30.123456789`).
3. **`LocalDateTime`**: Combines `LocalDate` and `LocalTime` into a composite date-time representation (e.g., `2026-11-21T16:45:30`).

### Temporal Adjusters (`TemporalAdjusters`)
Manipulating calendar dates frequently requires complex domain rules: "the last day of the month", "the first Monday of next month", "the next business day".
`java.time.temporal.TemporalAdjusters` provides a suite of pre-packaged mathematical adjusters:
- `firstDayOfMonth()`, `lastDayOfMonth()`
- `firstDayOfNextMonth()`, `firstDayOfYear()`, `lastDayOfYear()`
- `next(DayOfWeek)`, `previous(DayOfWeek)`
- `nextOrSame(DayOfWeek)`, `previousOrSame(DayOfWeek)`
- `firstInMonth(DayOfWeek)`, `lastInMonth(DayOfWeek)`

---

## 3. How It Works Internally

### Compact Memory Layout
`LocalDate` stores date fields as compact primitive integers:
```java
public final class LocalDate implements Temporal, ... {
    private final int year;
    private final short month;
    private final short day;
}
```
`LocalTime` stores time fields down to nanoseconds:
```java
public final class LocalTime implements Temporal, ... {
    private final byte hour;
    private final byte minute;
    private final byte second;
    private final int nano;
}
```
`LocalDateTime` is a composite object holding references to both:
```java
public final class LocalDateTime implements Temporal, ... {
    private final LocalDate date;
    private final LocalTime time;
}
```

```
+-------------------------------------------------------+
| LocalDateTime Object Structure                        |
|                                                       |
| LocalDateTime                                         |
|  |-- LocalDate (year: int, month: short, day: short)  |
|  |-- LocalTime (hour: byte, min: byte, sec: byte, nano: int)
+-------------------------------------------------------+
```

### Proleptic ISO-8601 Calendar
The ISO-8601 calendar system is proleptic: it extends the modern Gregorian calendar rules backwards in time before its historical adoption in 1582. This ensures mathematically consistent date arithmetic regardless of historical calendar reforms.

---

## 4. Syntax & Usage

### 1. Date Arithmetic and Queries
```java
import java.time.DayOfWeek;
import java.time.LocalDate;
import java.time.Month;
import java.time.temporal.ChronoUnit;

public class LocalDateOperations {
    public static void main(String[] args) {
        LocalDate today = LocalDate.of(2026, Month.APRIL, 15);

        // Date arithmetic (Immutable: returns new instance)
        LocalDate nextWeek = today.plusWeeks(1);
        LocalDate lastYear = today.minusYears(1);

        // Queries
        DayOfWeek dow = today.getDayOfWeek(); // WEDNESDAY
        int dayOfYear = today.getDayOfYear();  // 105
        boolean isLeap = today.isLeapYear();   // false (2026 is not leap)

        // Elapsed time between dates
        LocalDate endOfYear = LocalDate.of(2026, 12, 31);
        long daysUntilEnd = ChronoUnit.DAYS.between(today, endOfYear);

        System.out.println("Days until end of year: " + daysUntilEnd); // 260
    }
}
```

### 2. Using Temporal Adjusters
```java
import java.time.DayOfWeek;
import java.time.LocalDate;
import java.time.temporal.TemporalAdjusters;

public class TemporalAdjusterDemo {
    public static void main(String[] args) {
        LocalDate date = LocalDate.of(2026, 11, 21); // Saturday

        // Find last day of current month
        LocalDate endOfMonth = date.with(TemporalAdjusters.lastDayOfMonth());
        System.out.println("End of month: " + endOfMonth); // 2026-11-30

        // Find next Monday
        LocalDate nextMonday = date.with(TemporalAdjusters.next(DayOfWeek.MONDAY));
        System.out.println("Next Monday: " + nextMonday); // 2026-11-23
    }
}
```

---

## 5. Practical Examples

### Example 1: Next Business Day Calculator (Basic to Intermediate)
```java
import java.time.DayOfWeek;
import java.time.LocalDate;
import java.time.temporal.TemporalAdjuster;

public class BusinessDayAdjuster {
    // Custom TemporalAdjuster: skips Saturday and Sunday
    public static TemporalAdjuster nextBusinessDay() {
        return temporal -> {
            LocalDate date = LocalDate.from(temporal);
            LocalDate next = date.plusDays(1);
            if (next.getDayOfWeek() == DayOfWeek.SATURDAY) {
                return next.plusDays(2); // Skip to Monday
            } else if (next.getDayOfWeek() == DayOfWeek.SUNDAY) {
                return next.plusDays(1); // Skip to Monday
            }
            return next;
        };
    }

    public static void main(String[] args) {
        LocalDate friday = LocalDate.of(2026, 11, 20); // Friday
        LocalDate businessDay = friday.with(nextBusinessDay());
        System.out.println("Next business day after Friday: " + businessDay);
        // 2026-11-23 (Monday)
    }
}
```

### Example 2: Financial Billing Cycle Schedule (Real-World)
```java
import java.time.LocalDate;
import java.time.temporal.TemporalAdjusters;
import java.util.ArrayList;
import java.util.List;

public class BillingScheduler {
    public static List<LocalDate> generateBillingDates(LocalDate start, int monthsCount) {
        List<LocalDate> billingDates = new ArrayList<>();
        LocalDate current = start;

        for (int i = 0; i < monthsCount; i++) {
            // Bill on the last day of each month
            LocalDate billDate = current.with(TemporalAdjusters.lastDayOfMonth());
            billingDates.add(billDate);
            current = current.plusMonths(1);
        }
        return billingDates;
    }

    public static void main(String[] args) {
        LocalDate start = LocalDate.of(2026, 1, 15);
        List<LocalDate> schedule = generateBillingDates(start, 4);
        System.out.println("Billing Schedule: " + schedule);
        // [2026-01-31, 2026-02-28, 2026-03-31, 2026-04-30]
    }
}
```

### Example 3: Edge Case - End of Month Overflow Clamping
```java
import java.time.LocalDate;

public class MonthEndClampingDemo {
    public static void main(String[] args) {
        // January 31st
        LocalDate jan31 = LocalDate.of(2026, 1, 31);

        // Add 1 month: February does NOT have 31 days!
        // LocalDate automatically clamps to the last valid day of February!
        LocalDate feb = jan31.plusMonths(1);
        System.out.println("Jan 31 + 1 month = " + feb); // 2026-02-28!
    }
}
```

### Example 4: Failure Case - Comparing `LocalDateTime` Without Zone Across Locations
```java
import java.time.LocalDateTime;

public class ZoneComparisonFailure {
    public static void main(String[] args) {
        // Meeting scheduled for 10:00 AM local time
        LocalDateTime meetingNY = LocalDateTime.of(2026, 6, 1, 10, 0);
        LocalDateTime meetingLondon = LocalDateTime.of(2026, 6, 1, 10, 0);

        // meetingNY.isEqual(meetingLondon) returns TRUE!
        // BUT IN REALITY: 10:00 AM in NY is 15:00 in London (5 hours apart)!
        System.out.println("Are local times equal? " + meetingNY.isEqual(meetingLondon)); // true
        // WARNING: Never use LocalDateTime to compare absolute global event times!
    }
}
```

---

## 6. Best Practices

1. **Use `LocalDate` for True Dates Without Times**: Birthdays, holidays, contract start dates, and invoice due dates must always be `LocalDate`.
2. **Never Use `LocalDateTime` for Global Timestamps**: If an event happened at a specific point in history across global servers, use `Instant` or `OffsetDateTime`.
3. **Leverage `TemporalAdjusters` for Complex Calendar Math**: Replace procedural while-loops with declarative adjusters (`TemporalAdjusters.next(DayOfWeek)`).
4. **Be Aware of End-of-Month Clamping**: `LocalDate.of(2026, 1, 31).plusMonths(1)` clamps to `2026-02-28`. If symmetric month roundtrips are needed, verify clamping behavior.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why Wrong | Fix |
|---|---|---|
| Using `LocalDateTime` as a database timestamp | Has no timezone or UTC offset; Daylight Saving Time causes duplicated or missing hours. | Use `Instant` or `OffsetDateTime`. |
| Assuming `date.plusMonths(1).minusMonths(1)` returns original date | Jan 31 + 1 month = Feb 28; Feb 28 - 1 month = Jan 28! | Be mindful of end-of-month clamping. |
| Creating invalid dates with raw numbers | `LocalDate.of(2026, 2, 29)` (non-leap year) throws `DateTimeException`. | Validate dates before constructing. |
| Mutating without reassigning | Calling `date.plusDays(10)` returns a new instance; original is unchanged. | Reassign: `date = date.plusDays(10)`. |
| Comparing local date-times across international offices | Local times match numerically but represent completely different moments in time. | Convert to `ZonedDateTime` or `Instant` before comparing. |
| Parsing non-padded strings | `LocalDate.parse("2026-4-5")` fails because standard ISO requires 2-digit padding ("2026-04-05"). | Use `DateTimeFormatter` or pad strings. |

---

## 8. Performance & Security Notes

### Performance
- **Zero Heap Overhead on Escape**: `LocalDate` only has 3 primitive fields. Inside local methods, modern JIT compilers completely eliminate object allocation via scalar replacement.
- **Clamping Math Simplicity**: Month-length lookup uses a compact 12-element byte table, computing leap year adjustments with simple bitwise arithmetic (`(year & 3) == 0 && ((year % 100) != 0 || (year % 400) == 0)`).

### Security
- **Strict Validation Guarantees**: `LocalDate` validates month (1-12) and day (1-31) ranges at instantiation, preventing buffer overflows or logic bypasses from invalid dates.

---

## 9. Comparison / When to Use What

| Type | Components | Contains Timezone? | Best Use Case |
|---|---|---|---|
| `LocalDate` | Year, Month, Day | No | Birthdays, holidays, due dates |
| `LocalTime` | Hour, Minute, Second, Nano | No | Store hours, daily alarm times |
| `LocalDateTime` | Date + Time | No | Airline flight departure (printed on local ticket) |
| `Instant` | Epoch seconds + Nanos | UTC (implicit) | Machine logs, database timestamps, distributed tracing |
| `OffsetDateTime` | Date + Time + Offset | Yes (Fixed) | REST API JSON payloads, database columns |

---

## 10. Interview Questions

<details>
<summary>1. When should you use `LocalDate` instead of `Instant`?</summary>
Use `LocalDate` when representing a calendar date that has no inherent connection to a timezone or time of day, such as a birthday, an annual holiday (Christmas), or an accounting reporting date. Use `Instant` when recording a physical moment on the global machine timeline (e.g. server transaction log).
</details>

<details>
<summary>2. What happens when you add one month to January 31st in a non-leap year?</summary>
`LocalDate.of(2026, 1, 31).plusMonths(1)` clamps the result to the last valid day of February: `2026-02-28`.
</details>

<details>
<summary>3. Why is `LocalDateTime` unsuitable for storing event timestamps in distributed systems?</summary>
Because `LocalDateTime` contains no timezone offset. Without knowing the offset or timezone, it is impossible to determine the absolute UTC instant when the event occurred, leading to confusion during Daylight Saving Time transitions or across servers in different regions.
</details>

<details>
<summary>4. What is a `TemporalAdjuster`?</summary>
A `TemporalAdjuster` is a functional strategy interface used to manipulate temporal objects. It allows complex business adjustments (e.g. `TemporalAdjusters.lastDayOfMonth()`, `next(DayOfWeek.FRIDAY)`) to be applied via `date.with(adjuster)`.
</details>

<details>
<summary>5. How do you calculate the difference in days between two `LocalDate` objects?</summary>
Via `ChronoUnit.DAYS.between(date1, date2)` or `Period.between(date1, date2).getDays()` (Note: `Period` gives calendar day component, while `ChronoUnit.DAYS` gives total days).
</details>

<details>
<summary>6. How does `LocalDate` determine whether a year is a leap year?</summary>
A year is a leap year if it is divisible by 4, except for end-of-century years, which must also be divisible by 400. Accessible via `localDate.isLeapYear()`.
</details>

<details>
<summary>7. How can you combine a `LocalDate` and a `LocalTime` into a `LocalDateTime`?</summary>
Via `date.atTime(time)` or `LocalDateTime.of(date, time)`.
</details>

<details>
<summary>8. Can you extract the day of the week from a `LocalDate`?</summary>
Yes, via `date.getDayOfWeek()`, which returns the strongly typed `DayOfWeek` enum (`MONDAY` through `SUNDAY`).
</details>

---

## 11. Practice Exercises

### Easy
1. Create a `LocalDate` representing the first day of next year.
2. Check if the current year is a leap year.
3. Print the day of the week for July 4th, 2026.

### Medium
4. Write a custom `TemporalAdjuster` that finds the next occurrence of Friday the 13th.
5. Given a user's birthdate, calculate their exact age in years today using `Period.between()`.
6. Write a function that counts the number of weekend days (Saturdays and Sundays) between two `LocalDate`s.

### Hard
7. Implement a complete recurring holiday calendar (e.g., US Thanksgiving: 4th Thursday of November) using `TemporalAdjusters`.
8. Implement an in-memory schedule conflict detector for room reservations using `LocalDateTime` intervals.
9. Measure performance differences between `ChronoUnit.DAYS.between()` vs manual epoch-day subtraction under JMH.

<details>
<summary>Solutions</summary>

```java
// Easy 3: Day of week
LocalDate july4 = LocalDate.of(2026, Month.JULY, 4);
System.out.println("July 4th 2026 is: " + july4.getDayOfWeek()); // SATURDAY

// Medium 4: Friday the 13th Adjuster
public static TemporalAdjuster nextFridayThe13th() {
    return temporal -> {
        LocalDate cur = LocalDate.from(temporal);
        while (true) {
            cur = cur.plusDays(1);
            if (cur.getDayOfMonth() == 13 && cur.getDayOfWeek() == DayOfWeek.FRIDAY) {
                return cur;
            }
        }
    };
}

// Medium 6: Count weekend days
public static long countWeekendDays(LocalDate start, LocalDate end) {
    return start.datesUntil(end.plusDays(1))
        .filter(d -> d.getDayOfWeek() == DayOfWeek.SATURDAY || d.getDayOfWeek() == DayOfWeek.SUNDAY)
        .count();
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Classes**: `LocalDate` (date only), `LocalTime` (time only), `LocalDateTime` (combined).
- **Timezone**: None! Represents pure human calendar concepts.
- **Arithmetic**: `plusDays()`, `minusMonths()`, `withYear()`. (Immutable!).
- **Adjusters**: `TemporalAdjusters.lastDayOfMonth()`, `next(DayOfWeek)`.
- **Elapsed Units**: `ChronoUnit.DAYS.between(d1, d2)`.
- **Clamping**: End-of-month math automatically clamps to last valid day.

---

## 13. Further Reading
- Oracle Java Documentation: *The LocalDate, LocalTime, and LocalDateTime Classes*.
- Colebourne, Stephen. *ThreeTen Design Principles*.
- Bloch, Joshua. *Effective Java (3rd Edition)*: Item 50 (Make defensive copies when needed).
