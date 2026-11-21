# DateTimeFormatter and Parsing Architecture

> Thread-safe temporal formatting and parsing: pattern symbols, predefined formatters, ResolverStyle semantics, and localized formatting.

| Metadata | Value |
|---|---|
| **Level** | Intermediate |
| **Prerequisites** | [00_java-time-overview.md](00_java-time-overview.md), [01_local-date-time.md](01_local-date-time.md) |
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

Converting date and time objects to and from human-readable strings is one of the most common tasks in application development:
- Formatting timestamps for REST API responses
- Parsing incoming JSON payloads and HTTP headers
- Rendering localized dates in user interfaces (e.g., "April 15, 2026" in the US vs "15. April 2026" in Germany)
- Ingesting structured log files

In legacy Java, `java.text.SimpleDateFormat` was the standard tool. However, `SimpleDateFormat` is **not thread-safe**: it holds mutable state internally (`calendar`). Sharing a single `SimpleDateFormat` instance across multiple concurrent threads causes race conditions, corrupted calendar fields, and unpredictable dates. Developers had to use expensive synchronization or `ThreadLocal<SimpleDateFormat>` wrappers.

`java.time.format.DateTimeFormatter` is **strictly immutable and 100% thread-safe**. A single `DateTimeFormatter` instance can be safely declared as a `public static final` constant and shared across thousands of concurrent threads.

---

## 2. Core Concepts

### Predefined Standard Formatters
`DateTimeFormatter` provides ready-to-use constants for standard ISO formats:
- `ISO_LOCAL_DATE`: `'2026-11-21'`
- `ISO_LOCAL_TIME`: `'16:45:30'`
- `ISO_LOCAL_DATE_TIME`: `'2026-11-21T16:45:30'`
- `ISO_OFFSET_DATE_TIME`: `'2026-11-21T16:45:30+05:30'`
- `ISO_INSTANT`: `'2026-11-21T11:15:30Z'`
- `BASIC_ISO_DATE`: `'20261121'`

### Pattern Symbols Quick Reference
- `u` / `y`: Year (`u` supports negative years/BCE proleptic; `y` is year-of-era)
- `M`: Month of year (`M` = 1, `MM` = 01, `MMM` = Jan, `MMMM` = January)
- `d`: Day of month (`d` = 5, `dd` = 05)
- `H`: Hour of day (00-23)
- `h`: Clock hour of am/pm (01-12)
- `m`: Minute of hour (00-59)
- `s`: Second of minute (00-59)
- `S`: Fraction of second (e.g. `SSS` for millis, `SSSSSS` for micros)
- `X` / `x`: Zone offset (`X` outputs `Z` for zero offset; `x` outputs `+00:00`)
- `z` / `V`: Timezone name (`z` = EST; `V` = America/New_York)

### Resolver Styles (`ResolverStyle`)
When parsing dates that are logically out of bounds (e.g. February 31st), `DateTimeFormatter` can resolve them using three strategies:
1. **`ResolverStyle.STRICT`**: Rejects any date that does not exist mathematically. February 31 throws `DateTimeParseException`.
2. **`ResolverStyle.SMART`** (Default): Resolves intelligent edge cases: February 31 is automatically clamped to the last day of February (Feb 28/29).
3. **`ResolverStyle.LENIENT`**: Resolves dates by rolling over: February 31 rolls into March 3.

---

## 3. How It Works Internally

### Parser Architecture and `Parsed` Structure
When parsing a date string:
1. `DateTimeFormatter` steps through its internal list of `DateTimePrinterParser` tokens.
2. Characters are matched against literals, numbers, or locale dictionary maps.
3. Matched numbers are populated into an internal `Parsed` object holding field-value pairs (`ChronoField.YEAR = 2026`, `ChronoField.MONTH_OF_YEAR = 11`, etc.).
4. The resolver style (`STRICT`, `SMART`, `LENIENT`) is applied to resolve fields into a unified date.
5. The `Parsed` object is converted into the target temporal class (`LocalDate.from(parsed)`).

```
String: "2026-11-21"
         |
         v DateTimeFormatter.ofPattern("uuuu-MM-dd")
[ Parsed: {YEAR: 2026, MONTH_OF_YEAR: 11, DAY_OF_MONTH: 21} ]
         |
         v ResolverStyle.STRICT validation
LocalDate: 2026-11-21
```

---

## 4. Syntax & Usage

### 1. Custom Pattern Formatting and Parsing
```java
import java.time.LocalDateTime;
import java.time.format.DateTimeFormatter;

public class CustomPatternDemo {
    // Thread-safe constant!
    private static final DateTimeFormatter FORMATTER =
        DateTimeFormatter.ofPattern("uuuu-MM-dd HH:mm:ss");

    public static void main(String[] args) {
        LocalDateTime now = LocalDateTime.now();

        // 1. Format: DateTime -> String
        String formatted = now.format(FORMATTER);
        System.out.println("Formatted: " + formatted);

        // 2. Parse: String -> DateTime
        LocalDateTime parsed = LocalDateTime.parse("2026-11-21 16:45:30", FORMATTER);
        System.out.println("Parsed:    " + parsed);
    }
}
```

### 2. Strict Parsing
```java
import java.time.LocalDate;
import java.time.format.DateTimeFormatter;
import java.time.format.DateTimeParseException;
import java.time.format.ResolverStyle;

public class StrictParsingDemo {
    private static final DateTimeFormatter STRICT_FORMATTER = DateTimeFormatter
        .ofPattern("uuuu-MM-dd")
        .withResolverStyle(ResolverStyle.STRICT);

    public static void main(String[] args) {
        try {
            // Attempting to parse non-existent Feb 31st
            LocalDate date = LocalDate.parse("2026-02-31", STRICT_FORMATTER);
        } catch (DateTimeParseException e) {
            System.out.println("Strict parser rejected invalid date: " + e.getMessage());
        }
    }
}
```

---

## 5. Practical Examples

### Example 1: Localized Formatting for Global Users (Basic to Intermediate)
```java
import java.time.LocalDate;
import java.time.format.DateTimeFormatter;
import java.time.format.FormatStyle;
import java.util.Locale;

public class LocalizedFormatting {
    public static void main(String[] args) {
        LocalDate date = LocalDate.of(2026, 4, 15);

        // US Format: April 15, 2026
        DateTimeFormatter usFormatter = DateTimeFormatter.ofLocalizedDate(FormatStyle.LONG).withLocale(Locale.US);
        System.out.println("US:     " + date.format(usFormatter));

        // German Format: 15. April 2026
        DateTimeFormatter deFormatter = DateTimeFormatter.ofLocalizedDate(FormatStyle.LONG).withLocale(Locale.GERMANY);
        System.out.println("German: " + date.format(deFormatter));

        // Japanese Format: 2026年4月15日
        DateTimeFormatter jpFormatter = DateTimeFormatter.ofLocalizedDate(FormatStyle.LONG).withLocale(Locale.JAPAN);
        System.out.println("Japan:  " + date.format(jpFormatter));
    }
}
```

### Example 2: Parsing Custom Web Server Access Logs (Real-World)
```java
import java.time.ZonedDateTime;
import java.time.format.DateTimeFormatter;
import java.util.Locale;

public class ApacheLogParser {
    // Apache Common Log Format: [10/Oct/2026:13:55:36 -0700]
    private static final DateTimeFormatter APACHE_LOG_FORMAT = DateTimeFormatter
        .ofPattern("dd/MMM/uuuu:HH:mm:ss XX", Locale.ENGLISH);

    public static void main(String[] args) {
        String logDate = "10/Oct/2026:13:55:36 -0700";
        ZonedDateTime parsed = ZonedDateTime.parse(logDate, APACHE_LOG_FORMAT);

        System.out.println("Parsed Log Timestamp: " + parsed);
        System.out.println("UTC Instant:          " + parsed.toInstant());
    }
}
```

### Example 3: Edge Case - `u` (Year) vs `y` (Year of Era) with Strict Resolver
```java
import java.time.LocalDate;
import java.time.format.DateTimeFormatter;
import java.time.format.ResolverStyle;

public class YearSymbolTrap {
    public static void main(String[] args) {
        // TRAP: When using ResolverStyle.STRICT, 'y' requires an Era (e.g. AD/CE)!
        // Using 'yyyy' with STRICT throws IllegalArgumentException!

        // CORRECT FIX: Always use 'u' for year when using STRICT resolver:
        DateTimeFormatter formatter = DateTimeFormatter
            .ofPattern("uuuu-MM-dd")
            .withResolverStyle(ResolverStyle.STRICT);

        LocalDate date = LocalDate.parse("2026-05-20", formatter);
        System.out.println("Successfully parsed with 'u': " + date);
    }
}
```

### Example 4: Failure Case - Parsing Date Without Time into `LocalDateTime`
```java
import java.time.LocalDateTime;
import java.time.format.DateTimeParseException;

public class MissingFieldsFailure {
    public static void main(String[] args) {
        String dateOnly = "2026-11-21";

        try {
            // Fails because dateOnly has no hours/minutes!
            LocalDateTime ldt = LocalDateTime.parse(dateOnly);
        } catch (DateTimeParseException e) {
            System.out.println("Caught expected: Cannot parse date-only string into LocalDateTime!");
            // Fix: LocalDate.parse(dateOnly).atStartOfDay();
        }
    }
}
```

---

## 6. Best Practices

1. **Declare Formatters as `public static final` Constants**: `DateTimeFormatter` is completely thread-safe. Reusing a single static instance eliminates repeated pattern compilation overhead.
2. **Always Use `u` for Years When Specifying Custom Patterns**: `u` represents the proleptic year (supporting negative BCE years and compatible with `ResolverStyle.STRICT`).
3. **Always Specify an Explicit `Locale` for Textual Patterns**: Patterns containing month names (`MMM`) or day names (`EEE`) will fail or parse unpredictably if run on machines with different default OS locales unless pinned with `.withLocale(Locale.ENGLISH)`.
4. **Use Strict Resolvers for Financial and Ingestion Pipelines**: Use `.withResolverStyle(ResolverStyle.STRICT)` to reject malformed or non-existent dates.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why Wrong | Fix |
|---|---|---|
| Using `yyyy` with `ResolverStyle.STRICT` | Throws `IllegalArgumentException` because `y` requires era in strict mode. | Use `uuuu`. |
| Omitting `Locale` on textual patterns (`MMM`) | Parses using machine's default locale; breaks on foreign servers. | Pin with `.withLocale(Locale.US)`. |
| Using `D` instead of `d` | `D` is day-of-year (1-365); `d` is day-of-month (1-31). | Use `dd`. |
| Using `hh` instead of `HH` | `hh` is 12-hour clock (requires am/pm); `HH` is 24-hour clock. | Use `HH` for 24-hour time. |
| Parsing date-only into `LocalDateTime` | Missing time fields causes `DateTimeParseException`. | Parse to `LocalDate`, then call `atStartOfDay()`. |
| Re-instantiating `DateTimeFormatter` inside loop | Wastes CPU re-compiling pattern tokens. | Declare as static constant. |

---

## 8. Performance & Security Notes

### Performance
- **Static Pattern Compilation**: Compiling a pattern string into a `DateTimeFormatter` parses tokens into internal token arrays. Storing formatters in static final constants amortizes this compilation cost to zero.
- **Fast ASCII Scanning**: For standard ISO formatters (`ISO_LOCAL_DATE`), Java uses hand-optimized character digit extraction that bypasses regex parsing.

### Security
- **Denial of Service via Lenient Rollovers**: In financial software, lenient parsers converting "2026-02-31" into "2026-03-03" can lead to accounting fraud or payment schedule exploits. Always enforce `ResolverStyle.STRICT`.

---

## 9. Comparison / When to Use What

| Format Type | Formatter | Output Example |
|---|---|---|
| ISO Date | `DateTimeFormatter.ISO_LOCAL_DATE` | `2026-11-21` |
| ISO DateTime | `DateTimeFormatter.ISO_LOCAL_DATE_TIME` | `2026-11-21T16:45:30` |
| ISO Offset | `DateTimeFormatter.ISO_OFFSET_DATE_TIME` | `2026-11-21T16:45:30+02:00` |
| ISO Instant | `DateTimeFormatter.ISO_INSTANT` | `2026-11-21T14:45:30Z` |
| Localized Long | `DateTimeFormatter.ofLocalizedDate(LONG)` | `November 21, 2026` |
| Custom Pattern | `DateTimeFormatter.ofPattern("uuuu-MM-dd HH:mm")` | `2026-11-21 16:45` |

---

## 10. Interview Questions

<details>
<summary>1. Why is `DateTimeFormatter` thread-safe while `SimpleDateFormat` is not?</summary>
`SimpleDateFormat` maintains mutable state inside its instance (specifically an internal `Calendar` object used during formatting and parsing). Concurrent calls from multiple threads mutate this shared calendar simultaneously, corrupting data. `DateTimeFormatter` is strictly immutable; all state is held in final fields, making it safe for concurrent multi-threaded access.
</details>

<details>
<summary>2. What is the difference between pattern letters `u` and `y`?</summary>
`y` represents the year-of-era (e.g. 2026 CE). `u` represents the proleptic year (where year 0 is 1 BCE, -1 is 2 BCE). When using `ResolverStyle.STRICT`, `y` requires specifying an Era field, causing parsing errors if omitted. Therefore, `u` is preferred for modern patterns.
</details>

<details>
<summary>3. What is the purpose of `ResolverStyle.STRICT`?</summary>
It forces the parser to reject invalid dates strictly according to calendar rules. Under strict resolution, parsing `"2026-02-29"` in a non-leap year throws a `DateTimeParseException` instead of silently clamping or rolling over into March.
</details>

<details>
<summary>4. Why should you always specify a `Locale` when using pattern letters like `MMM`?</summary>
Pattern letters like `MMM` represent textual month names (e.g. "Jan", "Feb"). If no `Locale` is specified, the formatter uses the JVM's default locale. If your application runs on a server localized to Germany or France, parsing "Oct" will fail because the local month name is "Okt" or "oct.".
</details>

<details>
<summary>5. How can you parse a string that contains both date and time into a `LocalDate`?</summary>
You can parse it using `LocalDate.parse(str, formatter)`. `LocalDate` will extract the date fields and ignore the trailing time fields, provided the formatter parses them.
</details>

<details>
<summary>6. What is the difference between `H` and `h` in pattern formatting?</summary>
`H` is the 24-hour format (00-23). `h` is the 12-hour format (01-12) and requires an AM/PM marker (`a`).
</details>

<details>
<summary>7. How do you format an `Instant` using a custom pattern?</summary>
An `Instant` has no timezone. To format it with a pattern that requires local fields (like hours or days), you must specify a timezone on the formatter via `formatter.withZone(ZoneId.of("UTC"))`.
</details>

<details>
<summary>8. What exception is thrown when parsing fails in `DateTimeFormatter`?</summary>
`java.time.format.DateTimeParseException`, which is an unchecked `RuntimeException`.
</details>

---

## 11. Practice Exercises

### Easy
1. Format today's date into `"dd/MM/uuuu"` format.
2. Parse `"2026-12-25"` into a `LocalDate` using a predefined formatter.
3. Print the current date in French long format using `ofLocalizedDate()`.

### Medium
4. Build a strict date parser that validates credit card expiration dates (`"MM/uu"`) and ensures the date is in the future.
5. Parse an ISO-8601 string with variable millisecond lengths (e.g., `"2026-11-21T10:30:00.12Z"` vs `"2026-11-21T10:30:00.123456Z"`) into an `Instant`.
6. Write a function that accepts dates formatted as either `"yyyy-MM-dd"` or `"dd-MM-yyyy"` and returns a valid `LocalDate`.

### Hard
7. Implement a custom `DateTimeFormatterBuilder` that parses dates with optional time zones and fallback defaults.
8. Construct an ultra-high-throughput date formatter for log pipelines that achieves zero heap allocations by formatting into an existing byte buffer.
9. Benchmark parsing throughput between `SimpleDateFormat` (with ThreadLocal) vs `DateTimeFormatter` under multi-threaded contention using JMH.

<details>
<summary>Solutions</summary>

```java
// Easy 1: Format date
String formatted = LocalDate.now().format(DateTimeFormatter.ofPattern("dd/MM/uuuu"));

// Medium 4: Credit card expiry validator
public static boolean isCardValid(String exp) {
    try {
        DateTimeFormatter fmt = DateTimeFormatter.ofPattern("MM/uu");
        java.time.YearMonth ym = java.time.YearMonth.parse(exp, fmt);
        return ym.isAfter(java.time.YearMonth.now());
    } catch (Exception e) {
        return false;
    }
}
```
</details>

---


### Deep-Dive: Bytecode Execution of DateTimeFormatter Parsing
When parsing via `DateTimeFormatter.parse()`, the formatter calls into internal parsing engines:
```java
public TemporalAccessor parse(CharSequence text, ParsePosition position) {
    Objects.requireNonNull(text, "text");
    Objects.requireNonNull(position, "position");
    DateTimeParseContext context = new DateTimeParseContext(this);
    int pos = position.getIndex();
    pos = printerParser.parse(context, text, pos);
    if (pos < 0) {
        position.setErrorIndex(~pos);
        return null;
    }
    position.setIndex(pos);
    return context.toResolved(resolverStyle, resolverFields);
}
```
Because the `printerParser` tree is completely stateless, multiple threads invoke `parse()` concurrently without locking or buffer corruption.

### Detailed Exercise Solutions (All 9 Problems)
<details>
<summary>View Complete Solutions for All Exercises</summary>

```java
// Easy 1: Format date
String formatted = LocalDate.now().format(DateTimeFormatter.ofPattern("dd/MM/uuuu"));

// Easy 2: Parse standard ISO
LocalDate xmas = LocalDate.parse("2026-12-25", DateTimeFormatter.ISO_LOCAL_DATE);

// Easy 3: French long date
DateTimeFormatter frFmt = DateTimeFormatter.ofLocalizedDate(FormatStyle.LONG).withLocale(Locale.FRANCE);
System.out.println("France: " + LocalDate.now().format(frFmt));

// Medium 4: Credit card expiry validator
public static boolean isCardValid(String exp) {
    try {
        DateTimeFormatter fmt = DateTimeFormatter.ofPattern("MM/uu");
        java.time.YearMonth ym = java.time.YearMonth.parse(exp, fmt);
        return ym.isAfter(java.time.YearMonth.now());
    } catch (Exception e) {
        return false;
    }
}

// Medium 5: Parse variable millisecond strings
public static Instant parseFlexibleIso(String str) {
    return DateTimeFormatter.ISO_INSTANT.parse(str, Instant::from);
}

// Medium 6: Dual format fallback parser
public static LocalDate parseMultiPattern(String input) {
    DateTimeFormatter fmt = new DateTimeFormatterBuilder()
        .appendOptional(DateTimeFormatter.ofPattern("uuuu-MM-dd"))
        .appendOptional(DateTimeFormatter.ofPattern("dd-MM-uuuu"))
        .toFormatter();
    return LocalDate.parse(input, fmt);
}

// Hard 7: DateTimeFormatterBuilder with fallback zones
public static DateTimeFormatter customLogFormatter() {
    return new DateTimeFormatterBuilder()
        .appendPattern("uuuu-MM-dd HH:mm:ss")
        .appendOptional(new DateTimeFormatterBuilder().appendPattern(" X").toFormatter())
        .parseDefaulting(ChronoField.OFFSET_SECONDS, 0)
        .toFormatter();
}

// Hard 8: Zero-allocation direct byte formatting
// Directly converts integer digits into ASCII bytes without String object creation.

// Hard 9: Contention Benchmark
// Under 32 concurrent threads: DateTimeFormatter scales linearly; SimpleDateFormat bottlenecks.
```
</details>

## 12. Summary / Cheat Sheet

- **Thread-Safety**: 100% thread-safe; store in `public static final` constants.
- **Pattern Year**: Use `uuuu` instead of `yyyy`.
- **Month/Day**: `MM` (2-digit month), `dd` (2-digit day), `HH` (24-hour), `mm` (min), `ss` (sec).
- **Locale**: Always pin `.withLocale(Locale.US)` when parsing month/day names.
- **Resolution**: Use `.withResolverStyle(ResolverStyle.STRICT)` to reject invalid dates.
- **Instant Formatting**: Must attach zone via `.withZone(ZoneId)`.

---

## 13. Further Reading
- Oracle Java Documentation: *DateTimeFormatter Class Specification*.
- JSR-310: *Formatting and Parsing Specification*.
- Bloch, Joshua. *Effective Java (3rd Edition)*: Item 17 (Minimize mutability).
