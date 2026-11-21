# Java Date and Time API (`10-date-and-time`)

> Complete architectural reference for modern date and time in Java (JSR-310): local dates, instants, timezones, durations, periods, thread-safe formatting, and distributed production best practices.

---

## Module Overview

Prior to Java 8, handling dates and times in Java was notoriously error-prone due to mutable classes (`Date`, `Calendar`), thread-unsafe formatters (`SimpleDateFormat`), and 0-indexed months. JSR-310 replaced legacy classes with a modern, domain-driven, completely immutable and thread-safe architecture based on ISO-8601.

This module provides a comprehensive guide for engineering temporal systems in modern Java (Java 8 through Java 25 LTS).

---

## Notes Directory

| # | Note | Description | Level | Estimated Reading Time |
|---|---|---|---|---|
| 00 | [Java Time Overview (JSR-310)](00_java-time-overview.md) | Why Date and Calendar failed, JSR-310 core principles, immutability, thread-safety, and package topology. | Intermediate | 24 min |
| 01 | [LocalDate, LocalTime & LocalDateTime](01_local-date-time.md) | Human calendar concepts, date arithmetic, TemporalAdjusters, and end-of-month clamping. | Intermediate | 24 min |
| 02 | [Instant, ZoneId & ZonedDateTime](02_instant-and-zoned-date-time.md) | UTC machine timeline, IANA timezones, OffsetDateTime vs ZonedDateTime, and DST transitions. | Advanced | 25 min |
| 03 | [Duration and Period](03_duration-and-period.md) | Measuring time amounts: machine time Duration vs calendar Period, ISO-8601 formatting, and unit traps. | Intermediate | 24 min |
| 04 | [DateTimeFormatter & Parsing](04_date-time-formatting-and-parsing.md) | Thread-safe formatting, predefined formatters, ResolverStyle.STRICT, and localized parsing. | Intermediate | 24 min |
| 05 | [Date-Time Best Practices](05_date-time-best-practices.md) | Distributed system rules: UTC storage invariants, REST API schemas, JDBC 4.2 mappings, and Clock injection. | Advanced | 24 min |

---

## Core Principles Quick Reference

### The Golden Rules of Production Time
1. **Store UTC, Display Local**: Backend databases, message queues, and logs must operate strictly in UTC (`Instant` or `OffsetDateTime` with `Z`).
2. **Use `LocalDate` for True Dates**: Birthdays, holidays, and contract dates have no time or timezone.
3. **Use `OffsetDateTime` for APIs**: Format as ISO-8601 strings (`2026-11-21T16:30:00Z`).
4. **Use `ZonedDateTime` for Future Human Schedules**: Store `LocalTime` + `ZoneId` to respect future DST rule changes.
5. **Inject `Clock` for Testability**: Never call `Instant.now()` directly in business logic.

---

## Prerequisites
- [04-oop](../04-oop/README.md) - Object-Oriented Programming (Classes, Immutability)
