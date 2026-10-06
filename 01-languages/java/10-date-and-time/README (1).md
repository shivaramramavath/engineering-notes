# 10 · Date and Time

Date and time bugs are rarely loud: they show up as an off-by-one-day report, a meeting that moves an hour after a DST change, or a timestamp that is correct on your laptop and wrong on the server. This module teaches the `java.time` API (Java 8+) and, more importantly, the **mental model** that prevents those bugs.

## Contents

| # | Note | What you get |
|---|------|--------------|
| 00 | [Java Time Overview](00_java-time-overview.md) | Why `java.time` replaced `Date`/`Calendar`, the type map, naming conventions, legacy interop |
| 01 | [Local Date and Time](01_local-date-time.md) | `LocalDate`, `LocalTime`, `LocalDateTime`, `YearMonth`, adjusters, comparison |
| 02 | [Instant and Zoned Date-Time](02_instant-and-zoned-date-time.md) | Timeline vs calendar, `ZoneId`, `ZonedDateTime`, `OffsetDateTime`, DST gaps/overlaps |
| 03 | [Duration and Period](03_duration-and-period.md) | Time-based vs date-based amounts, arithmetic, `ChronoUnit.between` |
| 04 | [Formatting and Parsing](04_date-time-formatting-and-parsing.md) | `DateTimeFormatter`, pattern letters, locales, strict parsing |
| 05 | [Best Practices](05_date-time-best-practices.md) | Storage, APIs, testing with `Clock`, DST traps, misconceptions |

## Suggested path

Read **00 → 01 → 02** in order: 02 is where the model clicks. Then 03 and 04 are reference-style. Finish with 05 as a checklist before you ship date-handling code.

## One-minute decision guide

```text
A calendar date (birthday, due date)?               → LocalDate
A wall-clock time (opens at 09:00)?                 → LocalTime
Date + time, no zone (a form field, local schedule) → LocalDateTime
A moment that happened / will happen (created_at)?  → Instant
A moment shown in a specific region's rules?        → ZonedDateTime
A moment with a fixed offset (API/DB column)?       → OffsetDateTime
"2 hours 30 minutes"                                → Duration
"1 month and 3 days"                                → Period
```

## Prerequisites

[Variables and Data Types](../01-fundamentals/01_variables-and-data-types.md) and a basic idea of [immutability](../23-design-and-clean-code/04_immutability.md).

**Next module:** [I/O and Networking](../11-io-and-networking/README.md)
