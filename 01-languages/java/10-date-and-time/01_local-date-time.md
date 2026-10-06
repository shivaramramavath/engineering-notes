# Local Date and Time

`LocalDate`, `LocalTime`, and `LocalDateTime` model what a **calendar and a wall clock show**, with no time zone and no offset. They answer questions like "what day is the invoice due?" or "what time does the shop open?", not "when did this happen?".

**Prerequisites:** [Java Time Overview](00_java-time-overview.md).

---

## 1. Creating values

```java
LocalDate d1 = LocalDate.of(2024, 3, 10);
LocalDate d2 = LocalDate.of(2024, Month.MARCH, 10);   // enum avoids month-number mistakes
LocalDate d3 = LocalDate.parse("2024-03-10");          // ISO-8601 only
LocalDate d4 = LocalDate.ofYearDay(2024, 70);          // 70th day of 2024
LocalDate d5 = LocalDate.now(ZoneId.of("Asia/Kolkata"));  // pass the zone explicitly

LocalTime t1 = LocalTime.of(9, 30);                    // 09:30
LocalTime t2 = LocalTime.of(9, 30, 15);
LocalTime t3 = LocalTime.parse("18:45");
LocalTime.MIDNIGHT; LocalTime.NOON;

LocalDateTime dt1 = LocalDateTime.of(2024, 3, 10, 9, 30);
LocalDateTime dt2 = LocalDateTime.of(d1, t1);
LocalDateTime dt3 = d1.atTime(9, 30);
LocalDateTime dt4 = d1.atStartOfDay();
LocalDateTime dt5 = LocalDateTime.parse("2024-03-10T09:30:00");
```

Invalid values fail fast with `DateTimeException`:

```java
LocalDate.of(2023, 2, 29);   // DateTimeException: 2023 is not a leap year
LocalDate.of(2024, 13, 1);   // DateTimeException: month must be 1-12
```

> `now()` with no argument uses the **system default zone**. That is fine in a toy program and a bug in a server. Prefer `now(zone)` or `now(clock)` ([Best Practices](05_date-time-best-practices.md)).

---

## 2. Reading fields

```java
LocalDate d = LocalDate.of(2024, 3, 10);

d.getYear();          // 2024
d.getMonth();         // MARCH (enum)
d.getMonthValue();    // 3
d.getDayOfMonth();    // 10
d.getDayOfWeek();     // SUNDAY (enum)
d.getDayOfYear();     // 70
d.isLeapYear();       // true
d.lengthOfMonth();    // 31
d.lengthOfYear();     // 366
```

`LocalTime` has `getHour()`, `getMinute()`, `getSecond()`, `getNano()`. `LocalDateTime` has all of both.

---

## 3. Arithmetic

Use `plusX` / `minusX` for relative changes and `withX` for absolute changes. Both return new objects.

```java
LocalDate d = LocalDate.of(2024, 3, 10);

d.plusDays(30);              // 2024-04-09
d.minusWeeks(2);             // 2024-02-25
d.plusMonths(1);             // 2024-04-10
d.withDayOfMonth(1);         // 2024-03-01
d.withMonth(12);             // 2024-12-10
d.plus(Period.ofMonths(2));  // 2024-05-10
d.plus(1, ChronoUnit.YEARS); // 2025-03-10
```

### Month-end behavior

When the target month is shorter, the day is **clamped to the last valid day**, and the order of operations matters:

```java
LocalDate jan31 = LocalDate.of(2024, 1, 31);

jan31.plusMonths(1);                  // 2024-02-29  (clamped; 2024 is a leap year)
jan31.plusMonths(1).plusMonths(1);    // 2024-03-29  (NOT 03-31, the "31" was lost)
jan31.plusMonths(2);                  // 2024-03-31

LocalDate.of(2024, 2, 29).plusYears(1);   // 2025-02-28
```

If you add months repeatedly (monthly billing), compute from the **original** date (`start.plusMonths(n)`) instead of chaining increments.

### Time wraps around

```java
LocalTime.of(23, 30).plusHours(2);    // 01:30   (wraps; the date is lost)
LocalDateTime.of(2024, 3, 10, 23, 30).plusHours(2);   // 2024-03-11T01:30
```

`LocalTime` can't carry into a date. Use `LocalDateTime` when overflow matters.

---

## 4. Comparing and measuring

```java
LocalDate a = LocalDate.of(2024, 3, 10);
LocalDate b = LocalDate.of(2024, 4, 1);

a.isBefore(b);       // true
a.isAfter(b);        // false
a.isEqual(b);        // false  (equals() also works for Local* types)
a.compareTo(b);      // negative

ChronoUnit.DAYS.between(a, b);     // 22   (end is exclusive)
ChronoUnit.MONTHS.between(a, b);   // 0    (full months only)
a.until(b);                        // P22D (a Period: see Duration and Period)
a.datesUntil(b);                   // Stream<LocalDate>, Java 9+: a (inclusive) to b (exclusive)
```

`ChronoUnit.X.between(start, end)` returns the number of **complete** units, truncated toward zero. Details and the `Period` vs `ChronoUnit` difference are in [Duration and Period](03_duration-and-period.md).

```java
// Age in years
int age = Period.between(birthDate, LocalDate.now(zone)).getYears();

// Is the contract still active?
boolean active = !today.isBefore(start) && today.isBefore(end);   // [start, end)
```

---

## 5. Adjusters

`TemporalAdjusters` handle "first Monday of the month"-style logic without manual loops:

```java
import static java.time.temporal.TemporalAdjusters.*;

LocalDate d = LocalDate.of(2024, 3, 10);   // a Sunday

d.with(firstDayOfMonth());                    // 2024-03-01
d.with(lastDayOfMonth());                     // 2024-03-31
d.with(next(DayOfWeek.MONDAY));               // 2024-03-11
d.with(nextOrSame(DayOfWeek.SUNDAY));         // 2024-03-10
d.with(previous(DayOfWeek.FRIDAY));           // 2024-03-08
d.with(firstInMonth(DayOfWeek.MONDAY));       // 2024-03-04
d.with(lastInMonth(DayOfWeek.FRIDAY));        // 2024-03-29
```

You can write your own, since `TemporalAdjuster` is a functional interface:

```java
TemporalAdjuster nextWorkingDay = t -> {
    LocalDate date = LocalDate.from(t);
    do { date = date.plusDays(1); }
    while (date.getDayOfWeek() == DayOfWeek.SATURDAY || date.getDayOfWeek() == DayOfWeek.SUNDAY);
    return t.with(date);
};
LocalDate.of(2024, 3, 8).with(nextWorkingDay);   // Friday → Monday 2024-03-11
```

(This version ignores holidays; real business-day logic needs a holiday calendar.)

---

## 6. Partial dates: `YearMonth`, `MonthDay`, `Year`

```java
YearMonth ym = YearMonth.of(2024, 2);
ym.lengthOfMonth();        // 29
ym.atDay(1);               // 2024-02-01
ym.atEndOfMonth();         // 2024-02-29
ym.plusMonths(1);          // 2024-03

YearMonth.parse("2025-08").isBefore(YearMonth.now(zone));   // card expiry check

MonthDay birthday = MonthDay.of(Month.FEBRUARY, 29);
birthday.isValidYear(2023);    // false
birthday.atYear(2023);         // 2023-02-28 (adjusted)
```

Use these instead of storing a `LocalDate` with a fake day.

---

## 7. Converting between local types

```java
LocalDateTime ldt = LocalDateTime.of(2024, 3, 10, 9, 30);
ldt.toLocalDate();          // 2024-03-10
ldt.toLocalTime();          // 09:30
ldt.truncatedTo(ChronoUnit.HOURS);   // 2024-03-10T09:00

date.atStartOfDay();                 // LocalDateTime at 00:00
date.atStartOfDay(zone);             // ZonedDateTime: correct if midnight doesn't exist in that zone
```

To get a real moment from a `LocalDateTime`, you must supply a zone: `ldt.atZone(zone).toInstant()`. See [Instant and Zoned Date-Time](02_instant-and-zoned-date-time.md).

---

## When to use which

| Use | Don't use |
|---|---|
| `LocalDate` for dates that are the same everywhere (birthday, due date) | `LocalDate` for "when did it happen" |
| `LocalTime` for recurring wall-clock times | `LocalTime` when you need to carry into the next day |
| `LocalDateTime` for local schedules and form input, **paired with a zone** somewhere else | `LocalDateTime` as a stored timestamp ("created_at") |

---

## Common mistakes

| Mistake | Fix |
|---|---|
| Ignoring the result of `plusDays(...)` | `d = d.plusDays(1)` |
| `LocalDateTime` as `created_at` | Use `Instant` or `OffsetDateTime` |
| `LocalDate.now()` in server code | `LocalDate.now(zone)` or `LocalDate.now(clock)` |
| Chaining `plusMonths(1)` repeatedly from month-end | Compute from the start date: `start.plusMonths(n)` |
| Using `0`-based months | Months are 1-12 here; prefer the `Month` enum |
| Assuming `ChronoUnit.DAYS.between` includes the end date | End is exclusive |
| Comparing with `==` | Use `equals` / `isEqual` / `compareTo` |

### Debugging

- `DateTimeException: Invalid date 'February 29' as '2023' is not a leap year`: validate input, or catch `DateTimeException` at the boundary.
- Wrong "today" on a server: check `ZoneId.systemDefault()` and the `user.timezone` property; better, stop depending on it.
- Off-by-one in ranges: decide explicitly between half-open `[start, end)` and closed ranges, and document it.

---

## Quick Summary

- `Local*` types are calendar/clock values with **no zone**, so they identify no moment by themselves.
- Create with `of`/`parse`, change with `plusX`/`withX` (immutable, reassign).
- Month-end additions clamp to the last valid day, so don't chain them.
- `ChronoUnit.between` counts complete units and the end is exclusive.
- `TemporalAdjusters` and `YearMonth`/`MonthDay` remove most hand-rolled calendar code.

**Next:** [Instant and Zoned Date-Time](02_instant-and-zoned-date-time.md)
