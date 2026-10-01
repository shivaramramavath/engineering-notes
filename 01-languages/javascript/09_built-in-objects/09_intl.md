# Intl

`Intl` is the built-in internationalization API: locale-aware formatting, sorting, pluralization and text segmentation, with data supplied by the runtime (ICU/CLDR). It replaces hand-written formatting code.

```js
new Intl.NumberFormat("de-DE").format(1234567.89);   // "1.234.567,89"
```

## Locales

A **locale** is a BCP 47 tag: `en`, `en-US`, `en-GB`, `pt-BR`, `zh-Hans-CN`, `ar-EG`. Pass `undefined` for the user's default.

```js
navigator.language;                         // "en-US" (browser)
Intl.DateTimeFormat().resolvedOptions();    // effective locale, time zone, options
Intl.getCanonicalLocales("EN-us");          // ["en-US"]
Intl.supportedValuesOf("currency");         // supported currency codes
Intl.supportedValuesOf("timeZone");
```

## NumberFormat

```js
const usd = new Intl.NumberFormat("en-US", { style: "currency", currency: "USD" });
usd.format(1234.5);                                   // "$1,234.50"

new Intl.NumberFormat("en-IN", { style: "currency", currency: "INR" }).format(1234567.5);   // "₹12,34,567.50"
new Intl.NumberFormat("en", { style: "percent", maximumFractionDigits: 1 }).format(0.256);  // "25.6%"
new Intl.NumberFormat("en", { notation: "compact" }).format(1_250_000);                     // "1.3M"
new Intl.NumberFormat("en", { style: "unit", unit: "kilometer-per-hour" }).format(50);     // "50 km/h"
new Intl.NumberFormat("en", { minimumFractionDigits: 2 }).format(5);                        // "5.00"
```

Key options: `style` (`decimal`, `currency`, `percent`, `unit`), `currency`, `currencyDisplay`, `notation` (`standard`, `compact`, `scientific`, `engineering`), `minimumFractionDigits`, `maximumFractionDigits`, `signDisplay`, `useGrouping`.

Shortcut: `(1234.5).toLocaleString("en-US", { style: "currency", currency: "USD" })`. Reuse a formatter object in loops for speed.

## DateTimeFormat

```js
const d = new Date("2026-09-30T10:15:00Z");

new Intl.DateTimeFormat("en-US").format(d);                                        // "9/30/2026"
new Intl.DateTimeFormat("en-GB", { dateStyle: "full", timeStyle: "short", timeZone: "Asia/Kolkata" }).format(d);
new Intl.DateTimeFormat("ja-JP", { year: "numeric", month: "long", day: "numeric" }).format(d);   // "2026年9月30日"
new Intl.DateTimeFormat("en", { hour: "numeric", minute: "2-digit", hour12: false, timeZone: "UTC" }).format(d);

const parts = new Intl.DateTimeFormat("en", { dateStyle: "medium" }).formatToParts(d);
// [{ type: "month", value: "Sep" }, ...]  (build custom layouts)
new Intl.DateTimeFormat("en", { dateStyle: "short" }).formatRange(d1, d2);
```

Options: `dateStyle`, `timeStyle` (`full`, `long`, `medium`, `short`) or individual fields (`year`, `month`, `day`, `hour`, `minute`, `weekday`, `timeZoneName`), plus `timeZone`, `hour12`, `calendar`.

## RelativeTimeFormat

```js
const rtf = new Intl.RelativeTimeFormat("en", { numeric: "auto" });
rtf.format(-1, "day");     // "yesterday"
rtf.format(2, "week");     // "in 2 weeks"
rtf.format(-3, "hour");    // "3 hours ago"
```

## PluralRules

Plural categories differ by language (`zero`, `one`, `two`, `few`, `many`, `other`).

```js
const pr = new Intl.PluralRules("en");
pr.select(1);   // "one"
pr.select(5);   // "other"

const suffixes = { one: "st", two: "nd", few: "rd", other: "th" };
const ordinal = (n) => n + suffixes[new Intl.PluralRules("en", { type: "ordinal" }).select(n)];
ordinal(22);    // "22nd"
```

For real messages use message-format libraries (ICU MessageFormat, FormatJS, i18next).

## ListFormat

```js
new Intl.ListFormat("en", { style: "long", type: "conjunction" }).format(["A", "B", "C"]);   // "A, B, and C"
new Intl.ListFormat("es", { type: "disjunction" }).format(["A", "B"]);                        // "A o B"
```

## Collator (sorting and comparing)

```js
["ä", "a", "z"].sort(new Intl.Collator("de").compare);
["file10", "file2"].sort(new Intl.Collator(undefined, { numeric: true }).compare);   // natural sort
new Intl.Collator("en", { sensitivity: "base" }).compare("a", "Á");                 // 0 (ignore case/accents)
```

## Segmenter

Split text into graphemes, words or sentences (correct for emoji, CJK, scripts without spaces).

```js
[...new Intl.Segmenter("en", { granularity: "grapheme" }).segment("👨‍👩‍👧a")].length;   // 2
[...new Intl.Segmenter("ja", { granularity: "word" }).segment("今日は晴れ")].filter((s) => s.isWordLike);
```

## DisplayNames and Locale

```js
new Intl.DisplayNames("en", { type: "region" }).of("IN");      // "India"
new Intl.DisplayNames("en", { type: "language" }).of("fr-CA"); // "Canadian French"
new Intl.DisplayNames("en", { type: "currency" }).of("EUR");   // "Euro"

const loc = new Intl.Locale("en-Latn-US", { hourCycle: "h12" });
loc.language; loc.region; loc.maximize().toString();
```

## String methods that use Intl

```js
"a".localeCompare("b", "sv");
"İ".toLocaleLowerCase("tr");
(1234.5).toLocaleString("fr-FR");
date.toLocaleDateString("en-GB");
```

## Practical helpers

```js
const money = (n, currency = "USD", locale) => new Intl.NumberFormat(locale, { style: "currency", currency }).format(n);
const ago = (date, locale = "en") => {
  const rtf = new Intl.RelativeTimeFormat(locale, { numeric: "auto" });
  const diff = (date - Date.now()) / 1000;
  const units = [["year", 31536000], ["month", 2592000], ["day", 86400], ["hour", 3600], ["minute", 60]];
  for (const [unit, secs] of units) if (Math.abs(diff) >= secs) return rtf.format(Math.round(diff / secs), unit);
  return rtf.format(Math.round(diff), "second");
};
```

## Fallbacks and support

- Unsupported locales fall back to the default; check `resolvedOptions().locale`
- Node ships full ICU by default in current versions; older builds may have limited locales
- Output text can differ slightly between engines and versions: **never parse formatted output** and avoid exact-string assertions across environments

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| Hand-formatting numbers/dates | Wrong for other locales | `Intl` |
| Creating a formatter per call in hot loops | Slow | Reuse instances |
| Hard-coding currency symbol and separators | Wrong grouping/placement | `NumberFormat` with `currency` |
| Currency from locale instead of data | Wrong money | Store currency code with the amount |
| Parsing formatted strings | Locale-dependent | Keep raw numbers/ISO dates |
| Sorting with `<` | Wrong order for accents | `Collator` / `localeCompare` |
| Assuming English plurals (`s`) | Breaks other languages | `PluralRules` or ICU MessageFormat |
| Time zone assumptions | Off-by-hours | Pass `timeZone` explicitly |

## Key takeaways

- Use `Intl` for numbers, dates, relative time, lists, plurals and sorting
- Pass locales explicitly (or `undefined` for the user default) and time zones explicitly
- Reuse formatter instances and never parse formatted output
- Use `Segmenter` for real characters and words

**Next:** [Typed Arrays and ArrayBuffer](./10_typed-arrays-and-arraybuffer.md)
