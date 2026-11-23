# JavaScript Date & Temporal: Legacy Pitfalls, Timezones, ISO 8601 & Modern Datetime

> Complete technical comparison and deep dive into ECMAScript legacy `Date` object flaws, UTC/local timezone mechanics, ISO-8601 parsing, and the modern TC39 Stage 3 `Temporal` API.

| Metadata | Details |
| :--- | :--- |
| **Level** | Core / Production Architecture |
| **Prerequisites** | [09_built-in-objects/02_number-and-bigint.md](02_number-and-bigint.md) |
| **Time to Read** | 27 minutes |
| **Status** | Complete |
| **ECMAScript Baseline** | ES5 / ES2015 (Date ISO extensions) / TC39 Stage 3 `Temporal` (Node `--harmony-temporal`) |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts: Legacy Date vs Modern Temporal](#2-core-concepts-legacy-date-vs-modern-temporal)
3. [How It Works: Epoch Representation & Timezone Resolution](#3-how-it-works-epoch-representation--timezone-resolution)
4. [Syntax & API Reference](#4-syntax--api-reference)
5. [Practical Examples](#5-practical-examples)
6. [Edge Cases, Pitfalls & Gotchas](#6-edge-cases-pitfalls--gotchas)
7. [Common Mistakes & Anti-Patterns](#7-common-mistakes--anti-patterns)
8. [Performance Characteristics](#8-performance-characteristics)
9. [Engine Internals: V8 JSDate Representation & Host Timezone Caching](#9-engine-internals-v8-jsdate-representation--host-timezone-caching)
10. [Best Practices & Design Guidelines](#10-best-practices--design-guidelines)
11. [Real-World Case Study: Multi-Timezone International Flight Booking Engine](#11-real-world-case-study-multi-timezone-international-flight-booking-engine)
12. [Interview Questions & Deep Dives](#12-interview-questions--deep-dives)
13. [Exercises & Challenges](#13-exercises--challenges)

---

## 1. What & Why

Handling dates, times, and timezones in JavaScript has historically been one of the most error-prone areas of web development. In 1995, Brendan Eich was forced to copy `java.util.Date` from Java 1.0 under extreme time pressure. That implementation was already considered flawed by the Java team (who deprecated most of it in Java 1.1 in 1997), but JavaScript was stuck with it for backward compatibility.

### The Fundamental Flaws of Legacy `Date`:
1. **Mutability:** `Date` objects are mutable; calling `date.setHours()` mutates the instance in-place, causing catastrophic side effects when passed across functions.
2. **0-Indexed Months:** Months are indexed from $0$ to $11$ (`0 = January`, `11 = December`), while days of the month are indexed from $1$ to $31$.
3. **Ambiguous Parsing:** `Date.parse()` behaves inconsistently across runtimes for non-standard date strings.
4. **Lack of True Timezone Support:** `Date` only understands two timezones: UTC and the host computer's local timezone. It cannot represent a date in Tokyo while running on a server in New York without manual offset arithmetic.
5. **No Wall-Clock Date/Time Types:** `Date` always represents a timestamp with an exact millisecond count. It cannot represent an abstract birthday ("May 15th") or opening hour ("09:00 AM") without binding it to a specific point in time.

To resolve these architectural issues permanently, TC39 standardized the **`Temporal` API** (Stage 3): an immutable, domain-specific, timezone-aware datetime suite.

---

## 2. Core Concepts: Legacy Date vs Modern Temporal

### 2.1 The Legacy `Date` Architecture
A legacy `Date` object wraps a single integer: **the number of milliseconds that have elapsed since midnight on January 1, 1970, UTC (the Unix Epoch)**.
- Stored internally as a 64-bit IEEE-754 float.
- Maximum range: $\pm 100,000,000$ days relative to the epoch ($\approx 273,790$ years BC to AD).

```
+-----------------------------------------------------------+
|                     Legacy JS Date                        |
+-----------------------------------------------------------+
| Internal Slot: [[DateValue]] = 1732384800000 ms (UTC)     |
+-----------------------------------------------------------+
         |                                           |
         v                                           v
   UTC Getters:                               Local Getters:
   - getUTCFullYear()                         - getFullYear()
   - getUTCHours()                            - getHours()
   (Returns pure UTC)                         (Queries OS Local Timezone)
```

### 2.2 The Modern `Temporal` Architecture
Instead of overloading a single class, `Temporal` separates datetime domains into dedicated, immutable types:

```
                              Temporal
                                 |
         +-----------------------+-----------------------+
         |                                               |
  Exact Time (Global)                             Wall-Clock Time (Local)
         |                                               |
+-----------------+                             +--------------------+
| Temporal.Instant| (UTC nanoseconds)           | Temporal.PlainDate | (2026-11-23)
+-----------------+                             +--------------------+
         |                                      | Temporal.PlainTime | (14:30:00)
         v                                      +--------------------+
+-------------------------+                     | Temporal.PlainDateTime |
| Temporal.ZonedDateTime  |                     +--------------------+
| (Instant + IANA Timezone|
|  + Calendar System)     |                     Durations & Units:
+-------------------------+                     +--------------------+
                                                | Temporal.Duration  | (PT2H30M)
                                                +--------------------+
```

---

## 3. How It Works: Epoch Representation & Timezone Resolution

### 3.1 Milliseconds vs Nanoseconds
- **Legacy `Date`:** Stored in **milliseconds** ($10^{-3}\text{ s}$).
- **`Temporal.Instant`:** Stored in **nanoseconds** ($10^{-9}\text{ s}$) using a 128-bit integer layout internally, eliminating floating-point rounding errors and supporting precision scientific computing.

### 3.2 Timezone & Daylight Saving Time (DST) Transitions
When a local date is converted to an instant or vice versa, engines consult the **IANA Time Zone Database** (e.g., `America/New_York`, `Europe/London`).
Because governments frequently change daylight saving boundaries:
- A day is **not always 24 hours** (it can be 23 or 25 hours during clock shifts).
- Simple arithmetic like `date.getTime() + 24 * 60 * 60 * 1000` frequently introduces 1-hour errors during DST shifts.
- `Temporal.ZonedDateTime` handles DST disambiguation automatically via `disambiguation: 'compatible' | 'earlier' | 'later' | 'reject'`.

---

## 4. Syntax & API Reference

### 4.1 Legacy `Date` API
```javascript
// Current timestamp:
const now = new Date();
const timestamp = Date.now(); // Milliseconds since epoch (number, no object allocation)

// Specific date: (Warning: Month is 0-indexed!)
const d = new Date(2026, 10, 23, 14, 30, 0); // Month 10 = November!

// Formatting:
console.log(d.toISOString());     // "2026-11-23T09:00:00.000Z" (UTC ISO 8601)
console.log(d.toUTCString());     // "Mon, 23 Nov 2026 09:00:00 GMT"
console.log(d.toLocaleDateString("en-US")); // "11/23/2026"
```

### 4.2 Modern `Temporal` API (Node 22+ `--harmony-temporal` / Polyfill)
```javascript
// Exact universal instant:
const instant = Temporal.Now.instant();
console.log(instant.toString()); // "2026-11-23T09:00:00.123456789Z"

// Timezone-aware ZonedDateTime:
const tokyoTime = Temporal.Now.zonedDateTimeISO("Asia/Tokyo");
console.log(tokyoTime.hour);     // e.g. 18
console.log(tokyoTime.timeZoneId); // "Asia/Tokyo"

// Abstract Plain Date (Zero timezone ambiguity):
const birthDate = Temporal.PlainDate.from("1995-12-17");
const nextYear = birthDate.add({ years: 1 });
console.log(nextYear.toString()); // "1996-12-17"

// Precise Durations:
const duration = Temporal.Duration.from({ hours: 2, minutes: 45 });
const totalMinutes = duration.total({ unit: "minute" });
console.log(totalMinutes); // 165
```

---

## 5. Practical Examples

### Example 1: Preventing Daylight Saving Arithmetic Errors
```javascript
// WRONG: Adding 86,400,000 ms to a Date across a DST transition
const beforeDst = new Date("2026-11-01T01:00:00-04:00"); // EDT
const nextDayWrong = new Date(beforeDst.getTime() + 24 * 60 * 60 * 1000);
// Result shifts to 00:00 or 02:00 due to 25-hour day!

// RIGHT with Legacy Date:
const nextDaySafe = new Date(beforeDst);
nextDaySafe.setDate(nextDaySafe.getDate() + 1); // Let the engine recalculate DST

// RIGHT with Temporal:
// const zonedDateTime = Temporal.ZonedDateTime.from("2026-11-01T01:00:00-04:00[America/New_York]");
// const nextDay = zonedDateTime.add({ days: 1 }); // Guaranteed 01:00 on the next calendar day
```

### Example 2: Calculating Calendar Age Without Leaps / Offsets
```javascript
function calculateAge(birthDateString) {
  const birth = new Date(birthDateString);
  const today = new Date();

  let age = today.getFullYear() - birth.getFullYear();
  const monthDiff = today.getMonth() - birth.getMonth();

  if (monthDiff < 0 || (monthDiff === 0 && today.getDate() < birth.getDate())) {
    age--;
  }
  return age;
}

console.log("Age:", calculateAge("1995-05-20"));
```

### Example 3: ISO-8601 UTC Serialization for APIs
```javascript
class AuditEvent {
  constructor(userId, action) {
    this.userId = userId;
    this.action = action;
    // Always record universal UTC timestamp:
    this.timestamp = new Date().toISOString();
  }

  toJSON() {
    return {
      userId: this.userId,
      action: this.action,
      timestamp: this.timestamp
    };
  }
}

const event = new AuditEvent(42, "PASSWORD_RESET");
console.log(JSON.stringify(event));
// {"userId":42,"action":"PASSWORD_RESET","timestamp":"2026-11-23T09:00:00.000Z"}
```

### Example 4: High-Resolution Performance Profiling
```javascript
// Date.now() has low resolution (~1ms) and can step backwards if system clock synchronizes (NTP)
// performance.now() provides monotonic nanosecond-precision timestamps:
function measureExecution(fn) {
  const start = performance.now();
  fn();
  const end = performance.now();
  return (end - start).toFixed(4); // Milliseconds with sub-millisecond precision
}

const duration = measureExecution(() => {
  let acc = 0;
  for (let i = 0; i < 1_000_000; i++) acc += i;
});
console.log(`Execution took ${duration} ms`);
```

### Example 5: Safe Month-End Date Arithmetic
```javascript
// Adding 1 month to January 31 in legacy Date overflows into March!
const jan31 = new Date(2026, 0, 31);
jan31.setMonth(jan31.getMonth() + 1);
console.log("Legacy overflow:", jan31.toISOString()); // 2026-03-03! (Feb only has 28 days)

// Safe month-end clamp helper:
function addMonthsSafe(date, months) {
  const result = new Date(date);
  const targetMonth = result.getMonth() + months;
  result.setMonth(targetMonth);

  // If month overshot due to shorter target month, rewind to last day of target month:
  if (result.getMonth() !== ((targetMonth % 12) + 12) % 12) {
    result.setDate(0); // Sets to last day of previous month
  }
  return result;
}

const safeFeb = addMonthsSafe(new Date(2026, 0, 31), 1);
console.log("Safely clamped:", safeFeb.toLocaleDateString("en-US")); // 2/28/2026
```

---

## 6. Edge Cases, Pitfalls & Gotchas

### 6.1 The 0-Indexed Month Gotcha
```javascript
// Intended: October 15, 2026
const d = new Date(2026, 10, 15);
console.log(d.getMonth()); // 10 -> Actually NOVEMBER!
```
Always remember: `0` is January, `11` is December.

### 6.2 ISO String Parsing: UTC vs Local Trap
In ECMAScript, date-only strings (`YYYY-MM-DD`) are parsed as **UTC**, whereas date-time strings (`YYYY-MM-DDTHH:mm:ss`) without a timezone offset are parsed as **Local Time**:
```javascript
// Date only -> UTC:
const utcDate = new Date("2026-11-23");
console.log(utcDate.getUTCDate()); // 23
// If local timezone is US Pacific (UTC-8), local date will display as Nov 22!
console.log(utcDate.getDate());    // 22 (In Pacific timezone!)

// Date + Time without Z -> LOCAL:
const localDate = new Date("2026-11-23T00:00:00");
console.log(localDate.getDate()); // 23
```

### 6.3 Mutating Dates Passed as Arguments
Because `Date` is an object, passing it to a function passes a reference. Mutating it in a helper function silently corrupts the caller's state:
```javascript
function addGracePeriod(date) {
  date.setDate(date.getDate() + 7); // MUTATES CALLER DATE!
  return date;
}

const invoiceDate = new Date("2026-11-01");
const dueDate = addGracePeriod(invoiceDate);
console.log(invoiceDate === dueDate); // true (Both are now Nov 8!)
```

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Bad Practice | Idiomatic Solution | Reason |
| :--- | :--- | :--- | :--- |
| **Instantiating Date to get timestamp** | `new Date().getTime()` | `Date.now()` | `Date.now()` avoids allocating a heap object. |
| **Direct Mutation of Dates** | `date.setHours(0)` | `const copy = new Date(date); copy.setHours(0);` | Avoids silent mutation bugs in shared application state. |
| **Hardcoding 86,400,000 for days** | `ts + 24 * 3600 * 1000` | `date.setDate(date.getDate() + 1)` | Days are not always 24 hours due to DST clock shifts. |
| **Parsing non-standard strings** | `new Date("23/11/2026")` | Use ISO-8601 `new Date("2026-11-23T00:00:00Z")` | Non-standard string formats behave inconsistently across browsers. |
| **Assuming Months are 1-12** | `new Date(2026, 1, 1)` for January | Pass `0` for January or use ISO string | Months in legacy `Date` are 0-indexed. |
| **Using `Date` for wall-clock time** | Storing store opening hours as `new Date()` | Use string `"09:00:00"` or `Temporal.PlainTime` | Binds abstract recurring hours to a specific historical epoch. |

---

## 8. Performance Characteristics

1. **`Date.now()` Fast Path:**
   - `Date.now()` is mapped directly to the OS clock syscall (e.g., `clock_gettime(CLOCK_REALTIME)` on Linux). It takes $\approx 15\text{ ns}$ and allocates $0$ bytes of memory.
2. **`new Date()` Heap Overhead:**
   - Every `new Date()` allocates a `JSDate` object on the V8 heap (~40 bytes) with a prototype link. Creating millions of `Date` instances inside a loop causes GC pressure.
3. **`performance.now()` Monotonicity:**
   - Unlike `Date.now()` (which reads wall-clock time and can jump backwards if NTP updates the system clock), `performance.now()` reads from a monotonic clock that never steps backward.

---

## 9. Engine Internals: V8 JSDate Representation & Host Timezone Caching

In V8, a `Date` instance is an internal C++ object: `v8::internal::JSDate`.

```
                  +-----------------------------------+
                  |         v8::internal::JSDate      |
                  +-----------------------------------+
                  | - Shape / Map pointer             |
                  | - value: Float64 (UTC millis)     |
                  | - year, month, day cache fields   |
                  +-----------------------------------+
```

### Host Timezone Cache:
1. Converting UTC milliseconds to local time requires checking the local timezone offset and DST tables.
2. Because querying the OS timezone on every `.getHours()` call would be prohibitively slow, V8 maintains an internal **`TimezoneCache`**.
3. When the host OS timezone changes (e.g., user travels to another timezone), the OS sends a signal to the engine process, which invalidates the V8 timezone cache.

---

## 10. Best Practices & Design Guidelines

1. **Store and Transmit Exclusively in UTC ISO-8601:** Always save timestamps in database tables and JSON API payloads as UTC strings (`2026-11-23T09:00:00.000Z`) or epoch milliseconds.
2. **Localize at the Presentation Layer:** Only convert UTC dates to local time at the final user interface rendering step using `Intl.DateTimeFormat`.
3. **Never Mutate Date Instances:** Treat `Date` objects as strictly read-only. Always clone before modifying (`new Date(d.getTime())`).
4. **Prepare for `Temporal`:** For new projects, use the official `@js-temporal/polyfill` to write forward-compatible, immutable datetime logic.

---

## 11. Real-World Case Study: Multi-Timezone International Flight Booking Engine

### Problem
An airline booking platform allows users to book flights between London (GMT/BST) and Tokyo (JST). During checkout, scheduled departures and arrivals were stored using legacy `Date` strings. During European Daylight Saving transitions, arrival times in Tokyo were calculated with a 1-hour error, resulting in missed airport transfers.

### Solution
Implement an encapsulated Flight Scheduling domain service using strict ISO UTC instants and localized timezone projections:

```javascript
class FlightScheduleService {
  // Calculates arrival given departure UTC and flight duration:
  static calculateArrival(departureIsoString, durationMinutes) {
    const departureDate = new Date(departureIsoString);
    const arrivalTimeMs = departureDate.getTime() + durationMinutes * 60 * 1000;
    return new Date(arrivalTimeMs).toISOString();
  }

  // Formats time in the airport's specific IANA timezone:
  static formatAirportTime(utcIsoString, timeZone, locale = "en-US") {
    const formatter = new Intl.DateTimeFormat(locale, {
      timeZone: timeZone,
      year: "numeric",
      month: "short",
      day: "numeric",
      hour: "2-digit",
      minute: "2-digit",
      timeZoneName: "short"
    });
    return formatter.format(new Date(utcIsoString));
  }
}

// Flight: London Heathrow (LHR) to Tokyo Haneda (HND)
// Departs: 2026-10-24 11:30 UTC
// Flight duration: 14 hours 15 minutes (855 minutes)
const departureUtc = "2026-10-24T11:30:00.000Z";
const arrivalUtc = FlightScheduleService.calculateArrival(departureUtc, 855);

console.log("Universal UTC Departure:", departureUtc);
console.log("Universal UTC Arrival:  ", arrivalUtc);

console.log("Local LHR Departure:    ", FlightScheduleService.formatAirportTime(departureUtc, "Europe/London"));
console.log("Local HND Arrival:      ", FlightScheduleService.formatAirportTime(arrivalUtc, "Asia/Tokyo"));

// Outputs:
// Local LHR Departure:     Oct 24, 2026, 12:30 PM GMT+1
// Local HND Arrival:       Oct 25, 2026, 10:45 AM GMT+9
```

---

## 12. Interview Questions & Deep Dives

<details>
<summary><strong>Q1: Why does <code>new Date("2026-11-23").getDate()</code> sometimes return 22 in Western timezones?</strong></summary>

Under the ECMAScript specification, date-only ISO strings (`"YYYY-MM-DD"`) are parsed as midnight UTC (`2026-11-23T00:00:00.000Z`). When `getDate()` is called, it returns the day of the month according to the **local** timezone. In timezones west of UTC (e.g., US Eastern UTC-5 or US Pacific UTC-8), midnight UTC corresponds to 7:00 PM or 4:00 PM on November 22.
</details>

<details>
<summary><strong>Q2: What is the difference between <code>Date.now()</code> and <code>performance.now()</code>?</strong></summary>

`Date.now()` returns the number of milliseconds since the Unix Epoch based on the system clock. It can be adjusted by the OS, user, or NTP time synchronization (meaning it can jump forward or backward). `performance.now()` returns milliseconds with sub-millisecond (microsecond) precision measured from an arbitrary fixed origin (process start). It is **monotonic**, meaning it is guaranteed to never decrease.
</details>

<details>
<summary><strong>Q3: Why was the <code>Temporal</code> API introduced instead of fixing <code>Date</code>?</strong></summary>

`Date` could not be modified without breaking billions of lines of legacy web code that depend on its quirks (such as 0-indexed months and mutable setters). `Temporal` provides a clean slate with immutable objects, nanosecond precision, distinct types for wall-clock vs exact time, and first-class IANA timezone support.
</details>

<details>
<summary><strong>Q4: How does <code>Temporal.PlainDate</code> differ from <code>Temporal.ZonedDateTime</code>?</strong></summary>

`Temporal.PlainDate` represents a calendar date without a time of day and without any reference to a timezone (e.g., `2026-11-23`). It is completely independent of where the user is located. `Temporal.ZonedDateTime` represents an exact universal moment in time bound to a specific IANA timezone (e.g., `2026-11-23T14:30:00+09:00[Asia/Tokyo]`) with daylight saving rules applied.
</details>

<details>
<summary><strong>Q5: What happens when you pass <code>0</code> as the day of the month to <code>date.setDate(0)</code>?</strong></summary>

In JavaScript's `Date` arithmetic, setting the day to `0` rewinds the date to the **last day of the previous month**. For example, setting day `0` on a date in March sets it to February 28 (or 29 in a leap year).
</details>

<details>
<summary><strong>Q6: Why is <code>date.getTime() + 24 * 60 * 60 * 1000</code> dangerous when calculating "tomorrow"?</strong></summary>

Days do not always contain 24 hours. On the day daylight saving time starts, the day has 23 hours; on the day it ends, the day has 25 hours. Adding a hardcoded 86,400,000 milliseconds will cause the resulting time to shift forward or backward by one hour across DST transitions.
</details>

<details>
<summary><strong>Q7: What is the maximum date range supported by the JavaScript <code>Date</code> object?</strong></summary>

ECMA-262 specifies that `Date` can measure $\pm 100,000,000$ days relative to January 1, 1970 UTC. This corresponds to approximately 285,616 years before or after the epoch (spanning roughly 271,821 BC to 275,760 AD).
</details>

<details>
<summary><strong>Q8: How does <code>toJSON()</code> work on a <code>Date</code> instance?</strong></summary>

`Date.prototype.toJSON()` internally calls `this.toISOString()`. When an object containing a `Date` is passed to `JSON.stringify(obj)`, the date is automatically serialized into an ISO-8601 UTC string (`"YYYY-MM-DDTHH:mm:ss.sssZ"`).
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Business Days Calculator
Write a function `addBusinessDays(startDate, days)` that adds $N$ working days (excluding Saturdays and Sundays) to a given `Date` without mutating the original date.

<details>
<summary><strong>Solution</strong></summary>

```javascript
function addBusinessDays(startDate, daysToAdd) {
  const current = new Date(startDate);
  let added = 0;

  while (added < daysToAdd) {
    current.setDate(current.getDate() + 1);
    const dayOfWeek = current.getDay();
    // 0 = Sunday, 6 = Saturday
    if (dayOfWeek !== 0 && dayOfWeek !== 6) {
      added++;
    }
  }

  return current;
}

const friday = new Date("2026-11-20T00:00:00Z");
const tuesday = addBusinessDays(friday, 2);
console.log("Result day of week:", tuesday.getUTCDay()); // 2 (Tuesday)
```
</details>

### Exercise 2: Check If a Given Year is a Leap Year
Write a pure function `isLeapYear(year)` using standard `Date` arithmetic.

<details>
<summary><strong>Solution</strong></summary>

```javascript
function isLeapYear(year) {
  // February 29th check:
  // If year is leap, date 29 will remain in month 1 (Feb). If not, it rolls to March.
  return new Date(year, 1, 29).getMonth() === 1;
}

console.log(isLeapYear(2024)); // true
console.log(isLeapYear(2025)); // false
console.log(isLeapYear(2000)); // true
console.log(isLeapYear(1900)); // false
```
</details>

### Exercise 3: ISO Duration Formatter
Write a function `formatDurationMs(ms)` that converts milliseconds into a human-readable string like `"2h 15m 30s"`.

<details>
<summary><strong>Solution</strong></summary>

```javascript
function formatDurationMs(ms) {
  const totalSeconds = Math.floor(ms / 1000);
  const hours = Math.floor(totalSeconds / 3600);
  const minutes = Math.floor((totalSeconds % 3600) / 60);
  const seconds = totalSeconds % 60;

  const parts = [];
  if (hours > 0) parts.push(`${hours}h`);
  if (minutes > 0 || hours > 0) parts.push(`${minutes}m`);
  parts.push(`${seconds}s`);

  return parts.join(" ");
}

console.log(formatDurationMs(8130000)); // "2h 15m 30s"
console.log(formatDurationMs(45000));   // "45s"
```
</details>
