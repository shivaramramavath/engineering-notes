# Date-Time Formatting and Parsing

`DateTimeFormatter` converts date-time objects to text (**format**) and text to date-time objects (**parse**). It is immutable and **thread-safe**, unlike the legacy `SimpleDateFormat`, so you can (and should) keep formatters in `static final` constants.

**Prerequisites:** [Local Date and Time](01_local-date-time.md), [Instant and Zoned Date-Time](02_instant-and-zoned-date-time.md).

---

## 1. Built-in ISO formatters

Most machine-to-machine text should be ISO-8601, and the types already know how to read and write it:

```java
LocalDate.of(2024, 3, 10).toString();                         // 2024-03-10
Instant.parse("2024-03-10T04:00:00Z");
OffsetDateTime.parse("2024-03-10T09:30:00+05:30");
ZonedDateTime.parse("2024-03-10T09:30:00+05:30[Asia/Kolkata]");
LocalDateTime.parse("2024-03-10T09:30:00");
```

Constants for explicit use:

| Formatter | Example output |
|---|---|
| `ISO_LOCAL_DATE` | `2024-03-10` |
| `ISO_LOCAL_DATE_TIME` | `2024-03-10T09:30:00` |
| `ISO_OFFSET_DATE_TIME` | `2024-03-10T09:30:00+05:30` |
| `ISO_ZONED_DATE_TIME` | `2024-03-10T09:30:00+05:30[Asia/Kolkata]` |
| `ISO_INSTANT` | `2024-03-10T04:00:00Z` |

```java
String s = zdt.format(DateTimeFormatter.ISO_OFFSET_DATE_TIME);
```

Use these for APIs, logs, and storage as text. Only reach for custom patterns when a human or an external format requires it.

---

## 2. Custom patterns

```java
private static final DateTimeFormatter DMY = DateTimeFormatter.ofPattern("dd/MM/yyyy");

LocalDate d = LocalDate.of(2024, 3, 10);
String text = d.format(DMY);                    // "10/03/2024"
LocalDate back = LocalDate.parse("10/03/2024", DMY);
```

### Pattern letters you'll actually use

| Letter | Meaning | Example |
|---|---|---|
| `yyyy` / `uuuu` | year | `2024` |
| `MM` | month number | `03` |
| `MMM` / `MMMM` | month name (short/full), locale-dependent | `Mar` / `March` |
| `dd` | day of month | `10` |
| `EEE` / `EEEE` | weekday name | `Sun` / `Sunday` |
| `HH` | hour, 24h (00-23) | `09` |
| `hh` | hour, 12h (01-12), **needs `a`** | `09` |
| `mm` | **minute** | `30` |
| `ss` | second | `15` |
| `SSS` | fraction of second (milliseconds) | `123` |
| `a` | AM/PM | `AM` |
| `VV` | zone ID | `Asia/Kolkata` |
| `z` | zone name | `IST` |
| `XXX` | offset (`Z` for zero) | `+05:30` |
| `xxx` | offset (`+00:00` for zero) | `+05:30` |

Wrap literal text in single quotes: `"yyyy-MM-dd'T'HH:mm"`. Brackets mark optional sections: `"yyyy-MM-dd[ HH:mm]"`.

### Pattern mistakes that compile fine and print wrong values

| Wrong | Problem | Right |
|---|---|---|
| `yyyy-mm-dd` | `mm` is **minutes**, not months | `yyyy-MM-dd` |
| `YYYY-MM-dd` | `YYYY` is the **week-based year**; `2024-12-30` can print as `2025-12-30` | `yyyy-MM-dd` |
| `hh:mm` without `a` | 12-hour clock with no AM/PM, so 15:00 prints as `03:00` | `HH:mm` or `hh:mm a` |
| `DD` | day of **year** | `dd` |

### `yyyy` vs `uuuu`

`yyyy` is *year-of-era*, `uuuu` is the proleptic year. In normal formatting and the default parsing mode they behave the same. They differ with **strict** parsing (section 5): `yyyy` then requires an era field, so use `uuuu` there.

---

## 3. Formatting by type

A formatter can only print fields the object actually has:

```java
DateTimeFormatter f = DateTimeFormatter.ofPattern("yyyy-MM-dd HH:mm");

LocalDate.of(2024, 3, 10).format(f);
// UnsupportedTemporalTypeException: Unsupported field: HourOfDay
```

The most frequent version of this is formatting an `Instant`, which has no calendar fields. Give the formatter a zone:

```java
DateTimeFormatter f = DateTimeFormatter.ofPattern("yyyy-MM-dd HH:mm")
                                       .withZone(ZoneId.of("Asia/Kolkata"));

f.format(Instant.parse("2024-03-10T04:00:00Z"));    // "2024-03-10 09:30"
```

Printing zone information (`VV`, `z`, `XXX`) requires an object that has one (`ZonedDateTime`, `OffsetDateTime`) or a formatter with `withZone`.

---

## 4. Locale

Month and weekday names, AM/PM text, and localized styles depend on the `Locale`. Pass it explicitly. Without it, the JVM's default locale decides, and that varies between machines.

```java
DateTimeFormatter f = DateTimeFormatter.ofPattern("EEEE, d MMMM yyyy", Locale.ENGLISH);
f.format(LocalDate.of(2024, 3, 10));                         // "Sunday, 10 March 2024"
f.withLocale(Locale.FRANCE).format(LocalDate.of(2024, 3, 10));  // "dimanche 10 mars 2024"
```

### Localized styles

```java
DateTimeFormatter.ofLocalizedDate(FormatStyle.MEDIUM).withLocale(Locale.US)
                 .format(LocalDate.of(2024, 3, 10));          // "Mar 10, 2024"
```

`FormatStyle` is `SHORT`, `MEDIUM`, `LONG`, `FULL`. The exact output comes from the JDK's locale data (CLDR is the default since Java 9) and **can change between JDK versions**. Don't use localized styles for anything that is parsed again or compared as text. They are for display only.

---

## 5. Parsing

```java
LocalDate d = LocalDate.parse("10/03/2024", DMY);
LocalDateTime ldt = LocalDateTime.parse("2024-03-10 09:30", DateTimeFormatter.ofPattern("yyyy-MM-dd HH:mm"));
```

Failure throws `DateTimeParseException` (an unchecked `DateTimeException`), with the offending text and index:

```java
try {
    LocalDate.parse("2024-13-01");
} catch (DateTimeParseException e) {
    e.getParsedString();   // "2024-13-01"
    e.getErrorIndex();     // position of the error
}
```

### The parsed text must contain what the target type needs

```java
ZonedDateTime.parse("2024-03-10 09:30", f);
// DateTimeParseException: Unable to obtain ZonedDateTime ...  (no zone in the text)

// Parse what you have, then attach what you know
ZonedDateTime z = LocalDateTime.parse("2024-03-10 09:30", f).atZone(ZoneId.of("Asia/Kolkata"));
```

### Resolver styles: how strict is "valid"?

```java
DateTimeFormatter lenientish = DateTimeFormatter.ofPattern("yyyy-MM-dd");   // default: SMART

LocalDate.parse("2023-02-30", lenientish);                  // 2023-02-28  ← silently clamped!
LocalDate.parse("2023-02-30");                              // DateTimeParseException (ISO formatter is STRICT)
```

| Style | Behavior |
|---|---|
| `SMART` (default for `ofPattern`) | Day-of-month 1-31 accepted; values past month-end are **clamped** to the last valid day |
| `STRICT` | Only fully valid dates; requires `uuuu` (or an era with `yyyy`) |
| `LENIENT` | Accepts out-of-range values and rolls over (e.g. month 13) |

For user input and external data, **make parsing strict** so bad dates fail instead of being quietly corrected:

```java
private static final DateTimeFormatter STRICT_DMY =
        DateTimeFormatter.ofPattern("dd/MM/uuuu").withResolverStyle(ResolverStyle.STRICT);

LocalDate.parse("30/02/2023", STRICT_DMY);   // DateTimeParseException
```

### Optional parts and defaults

```java
DateTimeFormatter f = DateTimeFormatter.ofPattern("yyyy-MM-dd[ HH:mm]");   // time optional
```

For richer cases (default values for missing fields, case-insensitive month names) use `DateTimeFormatterBuilder` with `parseCaseInsensitive()` and `parseDefaulting(...)`. Reach for it only when `ofPattern` can't express the rule.

---

## 6. Practical usage

- **APIs and storage:** use ISO formatters (`Instant.toString()`, `OffsetDateTime`) and let your JSON library handle them ([Jackson](../17-json-and-data-formats/01_jackson.md)).
- **UI display:** format an `Instant` through `withZone(userZone)` and `withLocale(userLocale)` at the edge. Don't store the formatted string.
- **Non-ISO external formats (CSV, legacy files):** define one `static final` formatter per format, with explicit locale and strict resolver style.
- **Never parse dates to compare them.** Compare the parsed objects.

```java
// One place knows the file's format
private static final DateTimeFormatter FILE_TS =
        DateTimeFormatter.ofPattern("dd-MMM-uuuu HH:mm:ss", Locale.ENGLISH)
                         .withResolverStyle(ResolverStyle.STRICT);

Instant parse(String s) {
    return LocalDateTime.parse(s, FILE_TS).atZone(ZoneId.of("Asia/Kolkata")).toInstant();
}
```

---

## Common mistakes

| Mistake | Fix |
|---|---|
| `mm` for month, `MM` for minutes | `MM` = month, `mm` = minute |
| `YYYY` instead of `yyyy` | `yyyy` (or `uuuu`) |
| Formatting an `Instant` without a zone | `formatter.withZone(zone)` or convert to `ZonedDateTime` first |
| Relying on the default locale for names | `ofPattern(pattern, Locale.X)` |
| Default `SMART` parsing of user input | `ResolverStyle.STRICT` with `uuuu` |
| Creating a new `DateTimeFormatter` per call in a hot path | Constant; it is thread-safe |
| Parsing localized-style output (`MEDIUM`) later | Use ISO or an explicit pattern for round-trips |
| Storing formatted strings in the database | Store real date-time types |

### Debugging

- `DateTimeParseException: Text '...' could not be parsed at index N`: compare the text at index `N` with the pattern letter expected there (separator, width, month name language).
- `Unable to obtain X from TemporalAccessor`: the text lacks fields the target type needs (zone for `ZonedDateTime`, time for `LocalDateTime`). Parse into a smaller type, then enrich it.
- `Unsupported field: ...` while formatting: the object lacks that field. Convert to a richer type or set a zone on the formatter.
- Month names fail only on some machines: the default locale differs. Pass a `Locale`.

---

## Quick Summary

- Prefer ISO-8601 (`toString`, `parse`, `ISO_*` formatters) for anything machines read.
- `DateTimeFormatter` is immutable and thread-safe: keep it in a `static final` field.
- Remember `yyyy-MM-dd HH:mm:ss`: `MM` = month, `mm` = minute, `HH` = 24h, `yyyy` not `YYYY`.
- Formatting an `Instant` needs `withZone`. Parsing into a type requires all the fields it needs.
- Default `ofPattern` parsing is `SMART` and clamps invalid days: use `STRICT` + `uuuu` for validation.
- Always pass a `Locale` when output contains names.

**Next:** [Date-Time Best Practices](05_date-time-best-practices.md)
