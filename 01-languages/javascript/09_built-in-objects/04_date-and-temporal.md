# Date and Temporal

`Date` is the classic API for time. It works, but it has well-known quirks. **Temporal** is the modern replacement (check runtime support).

## Date basics

A `Date` wraps a single number: **milliseconds since 1970-01-01T00:00:00Z** (the Unix epoch, UTC).

```js
new Date();                          // now
Date.now();                          // now as a number (ms)
new Date(0);                         // epoch
new Date("2026-09-30T10:15:00Z");    // ISO string with Z (UTC)
new Date(2026, 8, 30, 10, 15);       // y, monthIndex (0-11!), d, h, m (LOCAL time)
new Date(NaN);                       // Invalid Date
```

## Getting parts

| Local | UTC | Notes |
|-------|-----|-------|
| `getFullYear()` | `getUTCFullYear()` | |
| `getMonth()` | `getUTCMonth()` | **0 to 11** |
| `getDate()` | `getUTCDate()` | day of month, 1 to 31 |
| `getDay()` | `getUTCDay()` | 0 = Sunday |
| `getHours/Minutes/Seconds/Milliseconds()` | `getUTC...()` | |
| `getTime()` / `valueOf()` | | ms since epoch |
| `getTimezoneOffset()` | | minutes behind UTC |

Setters (`setMonth`, `setDate`, ...) **mutate** the date and handle overflow:

```js
const d = new Date(2026, 0, 31);
d.setMonth(1);            // Feb 31 becomes Mar 3
d.setDate(d.getDate() + 30);   // adds 30 days, rolls the month
```

## Formatting

```js
d.toISOString();          // "2026-09-30T10:15:00.000Z" (UTC, best for storage and APIs)
d.toJSON();               // same as toISOString
d.toString();             // "Wed Sep 30 2026 15:45:00 GMT+0530 (India Standard Time)"
d.toDateString();
d.toLocaleDateString("en-GB");                  // "30/09/2026"
d.toLocaleTimeString("en-US");                  // "10:15:00 AM"
d.toLocaleString("en-US", { timeZone: "Asia/Kolkata", dateStyle: "medium", timeStyle: "short" });
new Intl.DateTimeFormat("de-DE", { dateStyle: "full" }).format(d);
```

## Parsing rules (a common trap)

| String | Interpreted as |
|--------|----------------|
| `"2026-09-30"` (date only, ISO) | **UTC** midnight |
| `"2026-09-30T10:15:00"` (no offset) | **local** time |
| `"2026-09-30T10:15:00Z"` / `+05:30` | exact instant |
| `"09/30/2026"`, `"Sep 30, 2026"` | implementation-defined, avoid |

```js
new Date("2026-09-30").getDate();   // may be 29 in negative UTC offsets!
```

Always send and store **ISO 8601 with an offset or `Z`**.

## Arithmetic

```js
const diffMs = end - start;                    // dates coerce to numbers
const days = diffMs / 86_400_000;

const addDays = (d, n) => { const r = new Date(d); r.setDate(r.getDate() + n); return r; };
const startOfDay = (d) => new Date(d.getFullYear(), d.getMonth(), d.getDate());
const isSameDay = (a, b) => a.toDateString() === b.toDateString();
```

Daylight saving: adding `24 * 60 * 60 * 1000` ms is **not** always "tomorrow". Use `setDate` (local calendar arithmetic) instead.

## Validation

```js
const isValidDate = (d) => d instanceof Date && !Number.isNaN(d.getTime());
new Date("nonsense").toString();      // "Invalid Date"
```

## Timing code

```js
const t0 = performance.now();      // monotonic, sub-millisecond
work();
console.log(performance.now() - t0);
```

Use `performance.now()` for durations, not `Date.now()` (which can jump when the clock changes).

## Date pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| Month is 0-based | Off-by-one bugs | Use named constants or Temporal |
| Date-only strings parse as UTC | Wrong local day | Use full ISO with offset |
| Setters mutate | Shared objects change | Copy first |
| `+ 24h` across DST | Wrong local time | Calendar arithmetic |
| Time zones limited to local and UTC | Cannot easily use "Asia/Tokyo" | `Intl` `timeZone` option, Temporal |
| Non-ISO string parsing | Engine-dependent | Parse manually or use ISO |
| Storing local times without offset | Ambiguity | Store UTC + zone name |
| Comparing with `==` | Compares references | `a.getTime() === b.getTime()` |

## Temporal

**Temporal** is the new date/time API designed to fix `Date`: immutable objects, 1-based months, real time zones, calendars and precise types. It is being rolled out across engines; **verify support** on MDN and use a polyfill (`@js-temporal/polyfill`) where needed.

```js
// Requires Temporal support or the polyfill
const now = Temporal.Now.instant();                    // exact point in time
const date = Temporal.PlainDate.from("2026-09-30");    // calendar date, no zone
const time = Temporal.PlainTime.from("10:15");         // wall-clock time
const local = Temporal.PlainDateTime.from("2026-09-30T10:15");
const zoned = Temporal.ZonedDateTime.from("2026-09-30T10:15[Asia/Kolkata]");
```

| Type | Represents |
|------|-----------|
| `Temporal.Instant` | exact UTC moment |
| `Temporal.ZonedDateTime` | instant + time zone + calendar |
| `Temporal.PlainDate` | calendar date (no time, no zone) |
| `Temporal.PlainTime` | wall-clock time |
| `Temporal.PlainDateTime` | date + time, no zone |
| `Temporal.PlainYearMonth` / `PlainMonthDay` | partial dates (birthdays, billing months) |
| `Temporal.Duration` | length of time |

```js
const d = Temporal.PlainDate.from("2026-01-31");
d.add({ months: 1 });                      // 2026-02-28 (clamped)
d.month;                                   // 1 (1-based!)
d.until("2026-03-01").days;                // days between
d.with({ day: 1 });                        // immutable update

const tokyo = Temporal.Now.zonedDateTimeISO("Asia/Tokyo");
tokyo.toString();                          // "2026-09-30T19:15:00+09:00[Asia/Tokyo]"
tokyo.withTimeZone("America/New_York");

Temporal.Instant.compare(a, b);            // -1, 0, 1
Temporal.Duration.from({ hours: 2, minutes: 30 }).total("minutes");   // 150
```

## Date vs Temporal

| | Date | Temporal |
|---|------|----------|
| Mutability | Mutable | Immutable |
| Months | 0-based | 1-based |
| Time zones | local/UTC only | any IANA zone |
| Date-only values | fake (midnight UTC) | `PlainDate` |
| Durations | manual math | `Duration` |
| Calendars | Gregorian | multiple |
| Precision | ms | ns |
| Availability | everywhere | check support / polyfill |

## Libraries (until Temporal is everywhere)

| Library | Notes |
|---------|-------|
| `date-fns` | functions over native `Date`, tree-shakable |
| `Luxon` | time zones via `Intl`, immutable |
| `Day.js` | small Moment-like API |
| `@js-temporal/polyfill` | Temporal itself |
| Moment.js | legacy, in maintenance mode |

## Key takeaways

- `Date` is ms since the epoch; months are 0-based; setters mutate
- Store and transmit ISO 8601 strings with `Z` or an offset
- Use `Intl.DateTimeFormat` for display and time zones
- Prefer Temporal (or a library) for real calendar and time zone logic

**Next:** [RegExp](./05_regexp.md)
