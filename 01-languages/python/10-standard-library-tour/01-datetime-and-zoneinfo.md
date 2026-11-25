---
title: "Date, Time, and Timezones: datetime, time, and zoneinfo"
description: "Mastering time in Python: datetime primitives, naive vs aware objects, IANA timezones with zoneinfo (PEP 615), Daylight Saving Time ambiguous fold transitions, ISO 8601 parsing, and monotonic clocks."
module: "10-standard-library-tour"
order: 1
tags:
  - python
  - datetime
  - time
  - zoneinfo
  - timezone
  - dst
  - fold
  - iso8601
difficulty: "Intermediate"
cpython_version: "3.12+"
last_verified: "2026-03"
---

# Date, Time, and Timezones: datetime, time, and zoneinfo

[<- Back to Module Index](./README.md)

---

## Table of Contents
1. [What & Why](#what--why)
2. [Core Concepts & Mental Model](#core-concepts--mental-model)
3. [How It Works Internally](#how-it-works-internally)
4. [Syntax & API Reference](#syntax--api-reference)
5. [Code Examples](#code-examples)
6. [Edge Cases, Pitfalls & Anti-Patterns](#edge-cases-pitfalls--anti-patterns)
7. [Performance & Memory Characteristics](#performance--memory-characteristics)
8. [CPython Internals & Bytecode](#cpython-internals--bytecode)
9. [Best Practices & Idiomatic Patterns](#best-practices--idiomatic-patterns)
10. [Real-World Case Study: Distributed Global Flight Booking & Scheduling Engine](#real-world-case-study-distributed-global-flight-booking--scheduling-engine)
11. [Interview Questions & Deep-Dive Scenarios](#interview-questions--deep-dive-scenarios)
12. [Practical Exercises & Code Katas](#practical-exercises--code-katas)

---

## What & Why

Handling temporal data is one of the most error-prone areas of software engineering. Earth rotates unevenly, political entities redraw timezone borders, daylight saving time (DST) creates missing or duplicate wall-clock hours, and leap seconds periodically adjust planetary coordination.

Python addresses these challenges through a modern standard library suite:
- **`datetime`**: Object-oriented models for calendar dates (`date`), times of day (`time`), combined points in time (`datetime`), and temporal intervals (`timedelta`).
- **`zoneinfo`**: Introduced in Python 3.9 (PEP 615), provides standard IANA (Olsen) timezone database support built directly into the standard library, permanently obsoleting external packages like `pytz`.
- **`time`**: Low-level C-library clock access, timestamps, sleep scheduling, and non-decreasing monotonic timers (`time.monotonic`, `time.perf_counter`).

### Motivation
A naive timestamp like `"2026-11-01 01:30:00"` is fundamentally ambiguous:
1. Which physical instant in the universe does it represent? (UTC, London, New York, Tokyo?)
2. During the autumn Daylight Saving Time "fall back" transition, 1:30 AM occurs **twice** in one night! Without timezone metadata and the `fold` attribute, it is impossible to determine whether a log line was emitted before or after the clock adjustment.
3. Measuring elapsed execution time using `time.time()` breaks if NTP (Network Time Protocol) steps the system clock backwards during the measurement window.

### Real-World Analogy
Consider coordinating an international rocket launch:
- If Mission Control in Houston tells the telemetry station in Madrid: "Fire thrusters at 2:00 AM", Madrid might fire an hour early or late depending on local daylight saving laws.
- The only unambiguous way to schedule the event is to specify a global physical instant: a UTC timestamp or an offset-aware ISO 8601 string (`2026-11-01T07:00:00Z`).
- Once the thrusters ignite, measuring how many seconds the burn lasts requires a mechanical stopwatch (`time.monotonic()`), not a wall clock whose hands can be moved by the room attendant.

### When to Use vs When Not to Use
- **Use `datetime` + `zoneinfo` for:**
  - Business logic, scheduling, appointments, calendar events, user-facing timestamps, and database ORM records.
- **Use `time.monotonic()` / `time.perf_counter()` for:**
  - Measuring elapsed execution time, rate-limiting windows, benchmark intervals, and task timeouts.
- **Do Not Use:**
  - Naive datetimes (`datetime.now()`) for distributed services (always use `datetime.now(timezone.utc)`).
  - Legacy `pytz` in modern Python (use `zoneinfo.ZoneInfo` instead).
  - `time.time()` for calculating benchmarking durations.

---

## Core Concepts & Mental Model

```
+-------------------------------------------------------------------------+
| Python Temporal Domain Model                                            |
+-------------------------------------------------------------------------+
       |                                                 |
       v                                                 v
+-------------------------------+             +---------------------------+
| Naive DateTime                |             | Aware DateTime            |
| (tzinfo is None)              |             | (tzinfo is ZoneInfo/UTC)  |
| - Pure numbers (year, month)  |             | - Exact physical point    |
| - Cannot resolve UTC instant  |             | - Accounts for offsets    |
| - Ambiguous during DST shifts |             | - Unambiguous comparison  |
+-------------------------------+             +---------------------------+
                                                         |
                               +-------------------------+
                               |
                               v
             +------------------------------------+
             | zoneinfo.ZoneInfo ("America/NY")  |
             | - IANA Database                    |
             | - Transitions table                |
             | - fold = 0 (Pre-transition)        |
             | - fold = 1 (Post-transition)       |
             +------------------------------------+
```

### Key Distinctions
1. **Naive vs. Aware Objects**: A `datetime` is **naive** if its `tzinfo` attribute is `None`. It represents abstract calendar and clock numbers. An **aware** `datetime` holds a concrete `tzinfo` object (such as `timezone.utc` or `ZoneInfo("Europe/London")`), anchoring it to a specific physical instant.
2. **The IANA Timezone Database**: A global registry of geographic regions (e.g. `America/New_York`, `Asia/Tokyo`, `Europe/Berlin`) containing historical and projected daylight saving laws and offset adjustments.
3. **The `fold` Attribute (PEP 495)**: An integer (0 or 1) attached to a `datetime` object to disambiguate the repeated wall-clock hour during an autumn DST transition. `fold=0` denotes the earlier (daylight) time, and `fold=1` denotes the later (standard) time.

---

## How It Works Internally

### 1. `zoneinfo` Architecture (PEP 615)
In Python 3.9+, `zoneinfo.ZoneInfo` looks up compiled timezone data (TZif binary files) from the host system's standard IANA database locations:
- On Linux/macOS: `/usr/share/zoneinfo`, `/usr/lib/zoneinfo`, `/etc/zoneinfo`.
- On Windows: Because Windows does not include an IANA database, Python searches for the official `tzdata` package on PyPI if installed.
When `ZoneInfo("America/New_York")` is instantiated, CPython parses the binary transition table once and caches the instance. Subclasses of `datetime.tzinfo` implement the methods:
- `utcoffset(dt)`: Returns the current offset from UTC as a `timedelta`.
- `dst(dt)`: Returns the daylight saving adjustment (typically 1 hour or 0).
- `tzname(dt)`: Returns the string abbreviation (e.g. `"EST"` or `"EDT"`).

### 2. Monotonic Clocks vs Wall Clocks
- `time.time()` queries the operating system's real-time clock (`CLOCK_REALTIME` on POSIX). It can jump forwards or backwards if the system administrator adjusts the clock or NTP synchronizes network time.
- `time.monotonic()` queries `CLOCK_MONOTONIC`. It cannot jump backwards, guaranteeing that `t2 - t1` is strictly non-negative and accurate regardless of NTP adjustments.
- `time.perf_counter()` queries the highest-resolution hardware clock available (e.g. TSC on x86), designed for micro-benchmarking.

---

## Syntax & API Reference

### Primary Constructors and Factories
```python
from datetime import datetime, date, time, timedelta, timezone
from zoneinfo import ZoneInfo

# Constructing Aware Datetimes (Modern standard)
utc_now = datetime.now(timezone.utc)
ny_now = datetime.now(ZoneInfo("America/New_York"))

# Constructing Specific Points in Time
dt = datetime(2026, 11, 25, 14, 30, 0, tzinfo=timezone.utc)

# Timezone Conversions (astimezone)
tokyo_dt = dt.astimezone(ZoneInfo("Asia/Tokyo"))

# Timedelta Arithmetic
one_week_later = dt + timedelta(days=7, hours=3, minutes=15)
duration: timedelta = tokyo_dt - dt  # Subtracting two aware datetimes yields timedelta
```

### Parsing and Formatting
```python
# ISO 8601 (Fastest & Preferred)
iso_str = dt.isoformat()                 # '2026-11-25T14:30:00+00:00'
dt_parsed = datetime.fromisoformat(iso_str)

# Unix Epoch Timestamps
ts: float = dt.timestamp()               # Seconds since Jan 1 1970 00:00:00 UTC
dt_from_ts = datetime.fromtimestamp(ts, tz=timezone.utc)

# Custom Formatting (C strftime/strptime syntax)
formatted = dt.strftime("%Y-%m-%d %H:%M:%S %Z")
custom_dt = datetime.strptime("2026-03-30 09:15", "%Y-%m-%d %H:%M")
```

---

## Code Examples

### Example 1: Naive vs Aware Datetimes and UTC Best Practice
```python
from datetime import datetime, timezone
from zoneinfo import ZoneInfo

def run_awareness_demo():
    # ANTI-PATTERN: Naive datetime (missing timezone context)
    naive_dt = datetime.now()
    print("Naive datetime:", naive_dt)
    print("Is naive aware?", naive_dt.tzinfo is not None)

    # BEST PRACTICE: Aware UTC datetime
    aware_utc = datetime.now(timezone.utc)
    print("Aware UTC datetime:", aware_utc)
    print("Timezone object:", aware_utc.tzinfo)

    # Convert UTC instant to local timezones seamlessly
    london_tz = ZoneInfo("Europe/London")
    sydney_tz = ZoneInfo("Australia/Sydney")

    london_time = aware_utc.astimezone(london_tz)
    sydney_time = aware_utc.astimezone(sydney_tz)

    print("London representation:", london_time.strftime("%Y-%m-%d %H:%M:%S %Z%z"))
    print("Sydney representation:", sydney_time.strftime("%Y-%m-%d %H:%M:%S %Z%z"))

    # Both representations represent the EXACT same instant in the universe:
    print("Are London and Sydney instants equal?", london_time == sydney_time)

run_awareness_demo()
# Output:
# Naive datetime: 2026-11-25 ...
# Is naive aware? False
# Aware UTC datetime: 2026-11-25 ...+00:00
# Timezone object: UTC
# London representation: ... GMT+0000 (or BST)
# Sydney representation: ... AEDT+1100
# Are London and Sydney instants equal? True
```

### Example 2: Resolving Daylight Saving Time Disambiguation with `fold`
```python
from datetime import datetime
from zoneinfo import ZoneInfo

def run_dst_fold_demo():
    # In New York, on Nov 1, 2026, clocks fall back from 02:00 EDT to 01:00 EST.
    # Therefore, 01:30 AM occurs twice!
    ny_tz = ZoneInfo("America/New_York")

    # fold = 0: The FIRST occurrence (before fall-back, EDT, UTC-4)
    dt_first = datetime(2026, 11, 1, 1, 30, fold=0, tzinfo=ny_tz)
    
    # fold = 1: The SECOND occurrence (after fall-back, EST, UTC-5)
    dt_second = datetime(2026, 11, 1, 1, 30, fold=1, tzinfo=ny_tz)

    print("Fold 0 (EDT):", dt_first.strftime("%Y-%m-%d %H:%M:%S %Z %z"))
    print("Fold 1 (EST):", dt_second.strftime("%Y-%m-%d %H:%M:%S %Z %z"))

    # Difference in UTC elapsed time
    delta = dt_second.astimezone(ZoneInfo("UTC")) - dt_first.astimezone(ZoneInfo("UTC"))
    print("Physical time elapsed between identical wall-clock readings:", delta)

run_dst_fold_demo()
# Output:
# Fold 0 (EDT): 2026-11-01 01:30:00 EDT -0400
# Fold 1 (EST): 2026-11-01 01:30:00 EST -0500
# Physical time elapsed between identical wall-clock readings: 1:00:00
```

### Example 3: ISO 8601 High-Performance Serialization and Parsing
```python
from datetime import datetime, timezone

def run_iso8601_demo():
    # Generate aware datetime
    event_time = datetime(2026, 6, 15, 18, 45, 30, 500000, tzinfo=timezone.utc)

    # Fast ISO 8601 string formatting
    iso_string = event_time.isoformat()
    print("ISO 8601 Output:", iso_string)

    # Parsing back (Python 3.11+ supports 'Z' suffix directly)
    parsed_dt = datetime.fromisoformat("2026-06-15T18:45:30.500000Z")
    print("Parsed Datetime:", parsed_dt)
    print("Timezone of parsed:", parsed_dt.tzinfo)

    assert parsed_dt == event_time

run_iso8601_demo()
# Output:
# ISO 8601 Output: 2026-06-15T18:45:30.500000+00:00
# Parsed Datetime: 2026-06-15 18:45:30.500000+00:00
# Timezone of parsed: UTC
```

### Example 4: Accurate Benchmark Timing with `time.perf_counter`
```python
import time
import math

def benchmark_computation():
    # NEVER use time.time() for benchmarking! Always use time.perf_counter()
    start_wall = time.time()
    start_perf = time.perf_counter()

    # Heavy calculation
    total = sum(math.sqrt(i) for i in range(1_000_000))

    elapsed_perf = time.perf_counter() - start_perf
    elapsed_wall = time.time() - start_wall

    print(f"Computed total: {total:.2f}")
    print(f"Monotonic High-Res Elapsed: {elapsed_perf * 1000:.3f} ms")
    print(f"Wall-Clock Elapsed:        {elapsed_wall * 1000:.3f} ms")

benchmark_computation()
# Output:
# Computed total: 666666166.46
# Monotonic High-Res Elapsed: ~85.412 ms
# Wall-Clock Elapsed:        ~85.420 ms
```

### Example 5: Date Math and Calendar Boundary Calculations
```python
from datetime import date, timedelta
import calendar

def get_last_day_of_month(target_date: date) -> date:
    """Calculates the exact last calendar day of the month for any given date."""
    _, last_day = calendar.monthrange(target_date.year, target_date.month)
    return target_date.replace(day=last_day)

def run_calendar_math_demo():
    # Leap year handling (2028 is a leap year)
    d1 = date(2028, 2, 10)
    last_feb = get_last_day_of_month(d1)
    print(f"Last day of February 2028: {last_feb} (Leap year: {calendar.isleap(2028)})")

    # Adding arbitrary business days
    start = date(2026, 10, 1)
    end = start + timedelta(days=45)
    print(f"45 days after {start}: {end}")

run_calendar_math_demo()
# Output:
# Last day of February 2028: 2028-02-29 (Leap year: True)
# 45 days after 2026-10-01: 2026-11-15
```

---

## Edge Cases, Pitfalls & Anti-Patterns

### 1. Mixing Naive and Aware Datetimes
Comparing or subtracting a naive datetime and an aware datetime raises `TypeError: can't compare offset-naive and offset-aware datetimes`.
- **Rule**: Keep all domain entities strictly **aware** (anchored to `timezone.utc`). Convert to local user timezones only at the final display layer.

### 2. The Legacy `pytz` Trap (`datetime(..., tzinfo=pytz_tz)`)
In the obsolete third-party library `pytz`, passing `tzinfo` directly to the `datetime` constructor resulted in incorrect historical offsets (often LMT—Local Mean Time, off by minutes!). Developers had to remember to call `pytz_tz.localize(dt)`.
- **Modern Solution**: Use standard library `zoneinfo.ZoneInfo`. You can pass `ZoneInfo` directly to `datetime(...)` without any localization gymnastics!

### 3. Measuring Durations with `time.time()`
`time.time()` queries system real-time clock. If an NTP daemon synchronizes the system clock backwards by 500 ms while an operation is running, `t2 - t1` can produce a **negative duration**!
- **Rule**: Always use `time.monotonic()` or `time.perf_counter()` for measuring durations and timeouts.

### Common Mistakes Table

| Bad Code | Good Code | Why | Severity |
| :--- | :--- | :--- | :--- |
| `datetime.now()` for storage | `datetime.now(timezone.utc)` | Naive datetime loses all timezone context; breaks in distributed systems. | Critical |
| `elapsed = time.time() - start` | `elapsed = time.perf_counter() - start` | `time.time()` can jump backwards on NTP sync, reporting negative time. | High |
| Naive string replace for timezone | `dt.astimezone(target_tz)` | `dt.replace(tzinfo=...)` changes timezone without shifting wall-clock numbers. | High |
| `import pytz` in Python 3.9+ | `from zoneinfo import ZoneInfo` | `pytz` is non-standard and requires error-prone `.localize()` calls. | Medium |
| `datetime.utcnow()` | `datetime.now(timezone.utc)` | `datetime.utcnow()` produces a **naive** datetime (deprecated in 3.12!). | High |
| Hardcoding `+05:00` for cities | `ZoneInfo("Asia/Karachi")` | Hardcoded offsets ignore daylight saving rules and political transitions. | Medium |

---

## Performance & Memory Characteristics

### Computational & Memory Complexity
- `datetime.now(timezone.utc)`: $\approx 120 \text{ ns}$ (pure C-accelerated system call).
- `datetime.fromisoformat()`: Highly optimized C parser in Python 3.11+, up to **5x faster** than `strptime()`.
- Memory size of `datetime` object: 48 bytes in CPython.
- Memory size of `ZoneInfo` instance: Shared cached instances, minimal heap overhead.

---

## CPython Internals & Bytecode

In CPython:
- The `datetime` module is implemented in C (`Modules/_datetimemodule.c`).
- `PyDateTime_DateTime` struct stores:
  - `data[4]`: 2 bytes year, 1 byte month, 1 byte day.
  - `data[3]`: 1 byte hour, 1 byte minute, 1 byte second.
  - `data[3]`: 3 bytes microsecond.
  - `tzinfo`: Pointer to `PyObject*`.
  - `fold`: 1-bit flag inside `hashstate`.

Bytecode for `datetime.now`:
```python
import dis
from datetime import datetime, timezone

def get_current_time():
    return datetime.now(timezone.utc)

dis.dis(get_current_time)
```
```text
  1           0 RESUME                   0
  2           2 LOAD_GLOBAL              1 (datetime + NULL)
             12 LOAD_ATTR                3 (now)
             22 LOAD_GLOBAL              5 (timezone + NULL)
             32 LOAD_ATTR                7 (utc)
             42 CALL                     1
             50 RETURN_VALUE
```

---

## Best Practices & Idiomatic Patterns

1. **Storage Rule: UTC Everywhere**:
   Always store timestamps in UTC (ISO 8601 strings or Unix epoch integers) in databases, logs, and APIs.
2. **Deprecation Notice: Avoid `datetime.utcnow()`**:
   Deprecated in Python 3.12 because it returns a naive datetime. Use `datetime.now(timezone.utc)`.
3. **Use `ZoneInfo` for Named Geographies**:
   Never assume fixed offsets for cities. Use `ZoneInfo("America/Chicago")` so the engine calculates historical and future DST shifts automatically.
4. **Use `fromisoformat()` Over `strptime()`**:
   `fromisoformat()` is standardized, handles timezone offsets natively, and executes significantly faster.

---

## Real-World Case Study: Distributed Global Flight Booking & Scheduling Engine

The following engine calculates flight departure and arrival times across international timezone borders, handling daylight saving transitions, layover durations, and UTC storage.

```python
from datetime import datetime, timedelta, timezone
from zoneinfo import ZoneInfo
from typing import NamedTuple

class FlightSchedule(NamedTuple):
    flight_number: str
    origin_airport: str
    origin_tz: ZoneInfo
    departure_local: datetime
    duration: timedelta
    destination_airport: str
    destination_tz: ZoneInfo

    def calculate_arrival(self) -> datetime:
        """
        Calculates local arrival time at destination, accounting for:
        1. Local departure conversion to UTC physical instant.
        2. Adding flight flight duration.
        3. Converting resulting UTC instant to destination local timezone.
        """
        # Step 1: Ensure departure is aware
        aware_departure = self.departure_local.replace(tzinfo=self.origin_tz)
        
        # Step 2: Convert to global UTC instant
        utc_departure = aware_departure.astimezone(timezone.utc)
        
        # Step 3: Add elapsed flight duration
        utc_arrival = utc_departure + self.duration
        
        # Step 4: Convert to local destination timezone
        local_arrival = utc_arrival.astimezone(self.destination_tz)
        return local_arrival

# Verification
if __name__ == "__main__":
    # Flight: London (LHR) to San Francisco (SFO)
    # Departs London on March 29, 2026 at 14:00 (during UK DST transition period)
    london_tz = ZoneInfo("Europe/London")
    sf_tz = ZoneInfo("America/Los_Angeles")

    flight = FlightSchedule(
        flight_number="BA-287",
        origin_airport="LHR",
        origin_tz=london_tz,
        departure_local=datetime(2026, 3, 29, 14, 0),
        duration=timedelta(hours=11, minutes=15),
        destination_airport="SFO",
        destination_tz=sf_tz
    )

    arrival_time = flight.calculate_arrival()

    print(f"Flight {flight.flight_number} Summary:")
    print(f"  Departure: {flight.departure_local.strftime('%Y-%m-%d %H:%M')} ({flight.origin_airport} - {london_tz})")
    print(f"  Duration:  {flight.duration}")
    print(f"  Arrival:   {arrival_time.strftime('%Y-%m-%d %H:%M %Z %z')} ({flight.destination_airport})")
```

---

## Interview Questions & Deep-Dive Scenarios

<details>
<summary><strong>1. Junior: Why is <code>datetime.utcnow()</code> deprecated in Python 3.12?</strong></summary>

`datetime.utcnow()` returns a **naive** datetime representing UTC time, with its `tzinfo` attribute set to `None`. This easily confuses developers into treating it as a local time or comparing it with other naive datetimes representing different timezones without error. Python 3.12 deprecated it in favor of `datetime.now(timezone.utc)`, which produces an explicitly **aware** datetime.
</details>

<details>
<summary><strong>2. Junior: What is the difference between a naive datetime and an aware datetime?</strong></summary>

A naive datetime contains no timezone information (`tzinfo=None`). It represents abstract calendar dates and clock times (e.g. "Christmas morning at 9 AM") and cannot be converted to a specific point on the universal timeline. An aware datetime contains a `tzinfo` object (such as `ZoneInfo` or `timezone.utc`) that anchors it to a concrete physical instant in time, enabling unambiguous comparisons and conversions.
</details>

<details>
<summary><strong>3. Mid: What is the purpose of the <code>fold</code> attribute on Python datetime objects (PEP 495)?</strong></summary>

During an autumn Daylight Saving Time "fall back" transition, the clocks are set backward by one hour (e.g. from 2:00 AM back to 1:00 AM), causing the times between 1:00 AM and 2:00 AM to occur twice. The `fold` attribute disambiguates these identical wall-clock readings: `fold=0` represents the first occurrence (before the transition, under daylight saving time), while `fold=1` represents the second occurrence (after the transition, under standard time).
</details>

<details>
<summary><strong>4. Mid: Why should you use <code>time.monotonic()</code> instead of <code>time.time()</code> to measure execution duration?</strong></summary>

`time.time()` queries the operating system's system clock (wall-clock time). If an NTP synchronization occurs or a system administrator changes the time during execution, the clock can jump forward or backward, resulting in inaccurate or even negative duration measurements. `time.monotonic()` is guaranteed to never move backward, providing reliable elapsed time measurements.
</details>

<details>
<summary><strong>5. Senior: What advantages does the standard library <code>zoneinfo</code> module offer over the legacy third-party <code>pytz</code> library?</strong></summary>

1. Standard Library Integration (PEP 615): No external dependencies required.
2. Standard DateTime Construction: `ZoneInfo` objects can be passed directly to `datetime(..., tzinfo=ZoneInfo(...))` and used with standard methods like `.astimezone()`. `pytz` required using custom non-standard methods like `tz.localize()` and `tz.normalize()`, and passing `pytz` timezones directly to constructors caused subtle, incorrect historical timezone offsets.
3. Native `fold` support: Fully integrates with PEP 495 ambiguous time handling.
</details>

<details>
<summary><strong>6. Senior: How does Python 3.11+ optimize ISO 8601 parsing in <code>datetime.fromisoformat()</code>?</strong></summary>

In Python 3.11, `fromisoformat()` was rewritten in pure C (`_datetimemodule.c`) to support almost all valid ISO 8601 strings (including `'Z'` UTC suffixes, arbitrary timezone offsets, and space separators) without invoking Python's slower regex engine or `strptime()` parsing tables. This resulted in an approximate 4x–5x performance speedup for JSON/API deserialization pipelines.
</details>

<details>
<summary><strong>7. Staff: How would you architect scheduling for recurring calendar events (e.g. "Every Monday at 9:00 AM local time") across international daylight saving transitions?</strong></summary>

1. Do NOT store future recurring occurrences as static UTC timestamps, because political entities may change DST laws or dates in the future.
2. Store the **recurrence rule** and the **local wall-clock time** with its IANA timezone identifier: e.g. `{time: "09:00", day: "Monday", tz: "America/Chicago"}`.
3. Compute concrete UTC instances dynamically over a rolling window (e.g. 14 days in advance) by creating `datetime(..., tzinfo=ZoneInfo("America/Chicago"))` and converting to UTC for queuing.
4. If a scheduled wall-clock time falls into a non-existent spring-forward gap (e.g. 2:30 AM), advance it by the DST delta (to 3:30 AM).
</details>

<details>
<summary><strong>8. Staff: How does the Linux kernel track monotonic time, and what is the difference between <code>CLOCK_MONOTONIC</code> and <code>CLOCK_MONOTONIC_RAW</code>?</strong></summary>

`CLOCK_MONOTONIC` represents monotonic time since an arbitrary point (usually boot). It is subject to NTP frequency adjustments (frequency slewing via `adjtime`), meaning it speeds up or slows down slightly to stay synchronized with physical time without jumping. `CLOCK_MONOTONIC_RAW` provides direct access to hardware timer counters (like TSC) with zero NTP slewing adjustments. Python's `time.monotonic()` maps to `CLOCK_MONOTONIC`, while `time.perf_counter()` uses the highest-resolution clock available.
</details>

---

## Practical Exercises & Code Katas

### Exercise 1: Aware Timezone Converter
**Requirement:** Write a function `convert_timezone(dt: datetime, target_tz_name: str) -> datetime` that converts any aware datetime into the target IANA timezone, raising `ValueError` if the input is naive.

<details>
<summary>Show Solution</summary>

```python
from datetime import datetime
from zoneinfo import ZoneInfo

def convert_timezone(dt: datetime, target_tz_name: str) -> datetime:
    if dt.tzinfo is None or dt.tzinfo.utcoffset(dt) is None:
        raise ValueError("Input datetime must be offset-aware")
    target_zone = ZoneInfo(target_tz_name)
    return dt.astimezone(target_zone)

# Test
from datetime import timezone
utc_time = datetime(2026, 7, 1, 12, 0, tzinfo=timezone.utc)
tokyo_time = convert_timezone(utc_time, "Asia/Tokyo")
print("Tokyo time (UTC+9):", tokyo_time)
assert tokyo_time.hour == 21
```
</details>

### Exercise 2: Business Day Duration Calculator
**Requirement:** Write a function that calculates the number of business days (Monday to Friday inclusive) between two `date` objects.

<details>
<summary>Show Solution</summary>

```python
from datetime import date, timedelta

def count_business_days(start_date: date, end_date: date) -> int:
    if start_date > end_date:
        start_date, end_date = end_date, start_date
    
    current = start_date
    business_days = 0
    while current <= end_date:
        # Monday is 0, Sunday is 6
        if current.weekday() < 5:
            business_days += 1
        current += timedelta(days=1)
    return business_days

# Test: Monday to next Monday (6 business days: Mon, Tue, Wed, Thu, Fri, Mon)
d1 = date(2026, 11, 2)   # Monday
d2 = date(2026, 11, 9)   # Next Monday
print("Business days:", count_business_days(d1, d2))
assert count_business_days(d1, d2) == 6
```
</details>

### Exercise 3: Elapsed Execution Timer Context Manager
**Requirement:** Create a context manager `Timer` that records the execution time of a code block in milliseconds using `time.perf_counter()`.

<details>
<summary>Show Solution</summary>

```python
import time
import contextlib

class Timer:
    def __init__(self):
        self.elapsed_ms: float = 0.0
        self._start: float = 0.0

    def __enter__(self):
        self._start = time.perf_counter()
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.elapsed_ms = (time.perf_counter() - self._start) * 1000.0

# Verification
with Timer() as t:
    time.sleep(0.05)

print(f"Elapsed: {t.elapsed_ms:.2f} ms")
assert 45.0 <= t.elapsed_ms <= 75.0
```
</details>

### Exercise 4: ISO 8601 Timestamp Normalizer
**Requirement:** Write a function that accepts variable ISO 8601 strings (e.g. with `'Z'`, `'+00:00'`, or offset numbers) and normalizes them into a canonical UTC ISO 8601 string ending in `'Z'`.

<details>
<summary>Show Solution</summary>

```python
from datetime import datetime, timezone

def normalize_to_utc_iso(iso_string: str) -> str:
    # Parse ISO string
    dt = datetime.fromisoformat(iso_string)
    # Ensure aware
    if dt.tzinfo is None:
        raise ValueError("Cannot normalize naive datetime string")
    # Convert to UTC
    utc_dt = dt.astimezone(timezone.utc)
    # Return formatted with Z
    return utc_dt.strftime("%Y-%m-%dT%H:%M:%S.%fZ")

# Test
res1 = normalize_to_utc_iso("2026-05-10T14:30:00+02:00")
print("Normalized:", res1)
assert res1.startswith("2026-05-10T12:30:00")
```
</details>

### Exercise 5: DST Transition Detection
**Requirement:** Write a function `is_dst_active(dt: datetime, tz_name: str) -> bool` that determines whether daylight saving time is active for a given datetime in a specified timezone.

<details>
<summary>Show Solution</summary>

```python
from datetime import datetime
from zoneinfo import ZoneInfo

def is_dst_active(dt: datetime, tz_name: str) -> bool:
    tz = ZoneInfo(tz_name)
    aware_dt = dt.replace(tzinfo=tz)
    # dst() returns timedelta(hours=1) if active, timedelta(0) if inactive
    dst_offset = aware_dt.dst()
    return dst_offset is not None and dst_offset.total_seconds() != 0

# Test: New York in July (DST) vs December (Standard)
print("NY in July (DST):", is_dst_active(datetime(2026, 7, 1, 12, 0), "America/New_York"))
print("NY in Dec (Standard):", is_dst_active(datetime(2026, 12, 1, 12, 0), "America/New_York"))
assert is_dst_active(datetime(2026, 7, 1, 12, 0), "America/New_York") is True
assert is_dst_active(datetime(2026, 12, 1, 12, 0), "America/New_York") is False
```
</details>

### Exercise 6: Next Occurrence Calculator
**Requirement:** Write a function `get_next_weekday(start_date: date, target_weekday: int) -> date` that calculates the date of the upcoming target weekday (where 0=Monday, 6=Sunday).

<details>
<summary>Show Solution</summary>

```python
from datetime import date, timedelta

def get_next_weekday(start_date: date, target_weekday: int) -> date:
    days_ahead = target_weekday - start_date.weekday()
    if days_ahead <= 0:
        days_ahead += 7
    return start_date + timedelta(days=days_ahead)

# Test: If today is Wednesday (2), next Friday (4) is 2 days ahead
today = date(2026, 11, 4)  # Wednesday
next_friday = get_next_weekday(today, 4)
print(f"Today ({today}) -> Next Friday: {next_friday}")
assert next_friday.weekday() == 4
assert (next_friday - today).days == 2
```
</details>
