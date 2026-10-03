# String Formatting

Formatting turns values into well-shaped text: aligned columns, fixed decimals, thousands separators, currency, dates. Java's `Formatter` syntax (printf-style) is the main tool; `NumberFormat` and `DecimalFormat` handle locale-aware numbers.

```java
String line = String.format("%-10s %5d %8.2f", "Pen", 10, 1.5);
//                            "Pen          10     1.50"
```

## Three ways to call the same engine

```java
String s1 = String.format("Hello %s, you are %d", name, age);   // returns a String
String s2 = "Hello %s, you are %d".formatted(name, age);        // Java 15+, same result
System.out.printf("Hello %s, you are %d%n", name, age);         // prints directly
```

## Format specifier syntax

```
%[argument_index$][flags][width][.precision]conversion

 %-8.2f      →  left-justified, width 8, 2 decimals, floating-point
 %2$s        →  the second argument as a string
```

### Conversions

| Conversion | Meaning | Example | Output |
|------------|---------|---------|--------|
| `%d` | Integer (`byte`/`short`/`int`/`long`/`BigInteger`) | `%d`, 42 | `42` |
| `%f` | Decimal floating-point (6 decimals by default) | `%f`, 3.14159 | `3.141590` |
| `%e` | Scientific | `%e`, 12345.6 | `1.234560e+04` |
| `%s` | String (calls `toString`; `null` prints `null`) | `%s`, "hi" | `hi` |
| `%S` | Uppercase string | `%S`, "hi" | `HI` |
| `%c` | Character | `%c`, 'A' | `A` |
| `%b` | Boolean | `%b`, true | `true` |
| `%x` / `%X` | Hexadecimal | `%x`, 255 | `ff` |
| `%o` | Octal | `%o`, 8 | `10` |
| `%h` | Hash code in hex | | |
| `%n` | Platform line separator | | newline |
| `%%` | A literal `%` | | `%` |
| `%tY`, `%tF`, ... | Date/time (prefer `DateTimeFormatter`) | | |

### Flags, width and precision

| Part | Meaning | Example | Output |
|------|---------|---------|--------|
| width | Minimum characters (pads with spaces) | `%5d`, 42 | `   42` |
| `-` | Left-justify | `%-5d\|`, 42 | `42   \|` |
| `0` | Pad with zeros | `%05d`, 42 | `00042` |
| `+` | Always show the sign | `%+d`, 5 | `+5` |
| `,` | Thousands separator (locale-aware) | `%,d`, 1234567 | `1,234,567` |
| space | Leading space for positives | `% d`, 5 | ` 5` |
| `(` | Negatives in parentheses | `%(d`, -5 | `(5)` |
| `#` | Alternate form | `%#x`, 255 | `0xff` |
| `.n` for floats | Decimal places | `%.2f`, 3.14159 | `3.14` |
| `.n` for strings | Maximum characters (truncates) | `%.3s`, "abcdef" | `abc` |

```java
String.format("%,.2f", 1234567.891);    // "1,234,567.89"
String.format("%08.3f", 3.14159);       // "0003.142"
String.format("%10.3s|", "abcdef");     // "       abc|"
String.format("%2$s %1$s", "world", "hello");   // "hello world"  (argument indexes)
String.format("%s %<s", "echo");        // "echo echo"  (< reuses the previous argument)
```

## Aligned tables

```java
String header = String.format("%-12s %6s %10s", "Item", "Qty", "Price");
String row1   = String.format("%-12s %6d %10.2f", "Notebook", 3, 4.5);
String row2   = String.format("%-12s %6d %,10.2f", "Laptop", 1, 1299.0);
```

```
Item            Qty      Price
Notebook          3       4.50
Laptop            1   1,299.00
```

Text: left-justify with `-`. Numbers: right-justify (default) with a fixed width.

## Locale

Number formatting depends on the **locale**: separators and the decimal mark differ.

```java
String.format(Locale.US, "%,.2f", 1234.5);        // "1,234.50"
String.format(Locale.GERMANY, "%,.2f", 1234.5);   // "1.234,50"
String.format(Locale.FRANCE, "%,.2f", 1234.5);    // "1 234,50" (uses a narrow no-break space)
```

| Situation | Locale to use |
|-----------|---------------|
| Showing numbers to a user | The user's locale |
| Machine-readable output (CSV, JSON, logs, protocols, tests) | `Locale.ROOT` or `Locale.US` |
| Unspecified | Default locale: output varies by machine, a common source of "works on my machine" bugs |

## Numbers, currency, percent: `NumberFormat`

```java
NumberFormat money = NumberFormat.getCurrencyInstance(Locale.US);
money.format(1234.5);                       // "$1,234.50"

NumberFormat percent = NumberFormat.getPercentInstance(Locale.US);
percent.format(0.256);                      // "26%"
percent.setMaximumFractionDigits(1);
percent.format(0.256);                      // "25.6%"

NumberFormat number = NumberFormat.getInstance(Locale.GERMANY);
number.format(1234567.891);                 // "1.234.567,891"
number.parse("1.234,5");                    // 1234.5  (parsing is locale-aware too)

NumberFormat compact = NumberFormat.getCompactNumberInstance(Locale.US, NumberFormat.Style.SHORT);
compact.format(1_500_000);                  // "2M"
```

`NumberFormat` and `DecimalFormat` are **not thread-safe**; create one per use or per thread.

### `DecimalFormat`: custom patterns

```java
new DecimalFormat("#,##0.00").format(1234567.891);   // "1,234,567.89"
new DecimalFormat("0.###").format(2.5);              // "2.5"
new DecimalFormat("000").format(7);                  // "007"
new DecimalFormat("0.00%").format(0.256);            // "25.60%"
```

| Pattern symbol | Meaning |
|----------------|---------|
| `0` | A digit, shown as `0` if absent |
| `#` | A digit, omitted if absent |
| `,` | Grouping separator |
| `.` | Decimal point |
| `%` | Multiply by 100 and append `%` |

Default rounding is `HALF_EVEN`; set `setRoundingMode(RoundingMode.HALF_UP)` when you need school rounding. For money amounts, round with `BigDecimal` first ([numeric precision](../01-fundamentals/08_numeric-precision-and-math.md)).

## Dates and times

Use `DateTimeFormatter`, not the `%t` conversions:

```java
LocalDate.now().format(DateTimeFormatter.ofPattern("dd MMM yyyy", Locale.US));
```

See [10-date-and-time/04_date-time-formatting-and-parsing.md](../10-date-and-time/04_date-time-formatting-and-parsing.md).

## Other formatting tools

| Tool | Purpose |
|------|---------|
| `MessageFormat.format("{0} has {1} items", name, n)` | Positional templates, plural/choice formats, i18n resource bundles |
| `String.valueOf`, `Integer.toString`, `Integer.toBinaryString` | Simple conversions |
| `" ".repeat(n)` | Manual padding |
| `Formatter` | Format into an `Appendable` (e.g., a `StringBuilder`) |
| Logging placeholders `log.info("User {}", id)` | Defer formatting until the log level is enabled ([22-production-engineering/00_logging.md](../22-production-engineering/00_logging.md)) |

## Formatting exceptions

| Exception | Cause |
|-----------|-------|
| `MissingFormatArgumentException` | More specifiers than arguments: `format("%d %d", 1)` |
| `IllegalFormatConversionException` | Type mismatch: `%d` with a `double`, `%f` with an `int` |
| `UnknownFormatConversionException` | Invalid conversion letter, or a stray `%` (use `%%`) |
| `IllegalFormatPrecisionException` | Precision on `%d`: `%.2d` |
| Extra arguments | **Ignored silently** |

```java
String.format("%d", 3.5);        // IllegalFormatConversionException: d != java.lang.Double
String.format("%.2f", 3);        // IllegalFormatConversionException: f != java.lang.Integer
String.format("100%");           // UnknownFormatConversionException
String.format("100%%");          // "100%"
```

## Concatenation or format?

| `+` / `StringBuilder` | `format` / `formatted` |
|-----------------------|------------------------|
| Short, simple joins | Templates with several values |
| Hot paths (a bit faster) | Alignment, padding, decimals, locale |
| No formatting needed | Messages that translators or product owners edit |

`format` parses the template on each call, so it is slower than plain concatenation. Do not worry about it unless a profiler says so ([20-performance](../20-performance/README.md)).

## Common mistakes

| Mistake | Symptom | Fix |
|---------|---------|-----|
| `%d` with a `double`, `%f` with an `int` | `IllegalFormatConversionException` | Match specifier and type, or cast |
| `\n` in format strings | Wrong line endings on Windows | `%n` |
| Forgetting `%%` for a literal percent | `UnknownFormatConversionException` | `%%` |
| Relying on the default locale | `3,14` on one machine, `3.14` on another | Pass a `Locale` explicitly |
| `double` for money, then formatting | Cent rounding errors | `BigDecimal`, then format |
| Too few arguments | `MissingFormatArgumentException` | Count them |
| Sharing a `DecimalFormat`/`NumberFormat` between threads | Garbled output | One per thread, or synchronize |
| Using `%s` with arrays | `[I@1b6d3586` | `Arrays.toString` |
| Building SQL or HTML with `format` and user input | Injection vulnerabilities | Prepared statements, output escaping ([21-security](../21-security/README.md)) |
| Expecting `%.2f` to truncate | It rounds (`HALF_UP`) | `BigDecimal.setScale` with the mode you need |

## Key takeaways

- `String.format`, `formatted` and `printf` share one syntax: `%[index$][flags][width][.precision]conversion`
- Use `-` to left-justify, `0` to zero-pad, `,` for grouping, `.n` for decimals; `%n` for newlines
- Always pass a `Locale` when output must be predictable; use `NumberFormat` for user-facing currency and percentages
- Match each conversion to the argument type, or you get an exception
- Never format money with `double`

**Next:** [Text Blocks](./05_text-blocks.md)
