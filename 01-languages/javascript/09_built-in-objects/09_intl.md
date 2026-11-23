# JavaScript Intl: ECMA-402, Formatting, Localization & ICU Engine Architecture

> Complete technical guide to the ECMAScript Internationalization API (`Intl`), `DateTimeFormat`, `NumberFormat`, `Collator`, `ListFormat`, `Segmenter`, BCP 47 locale tags, and V8 ICU integration.

| Metadata | Details |
| :--- | :--- |
| **Level** | Core / Production Architecture |
| **Prerequisites** | [09_built-in-objects/01_string.md](01_string.md), [09_built-in-objects/04_date-and-temporal.md](04_date-and-temporal.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **ECMAScript Baseline** | ECMA-402 (Intl) / ES2020 (RelativeTimeFormat, ListFormat) / ES2021 (DisplayNames) / ES2022 (Segmenter) |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts: BCP 47 & ECMA-402 Formatters](#2-core-concepts-bcp-47--ecma-402-formatters)
3. [How It Works: Engine ICU Integration & Locale Resolution](#3-how-it-works-engine-icu-integration--locale-resolution)
4. [Comprehensive Syntax & API Reference](#4-comprehensive-syntax--api-reference)
5. [Practical Examples](#5-practical-examples)
6. [Edge Cases, Pitfalls & Gotchas](#6-edge-cases-pitfalls--gotchas)
7. [Common Mistakes & Anti-Patterns](#7-common-mistakes--anti-patterns)
8. [Performance Characteristics: Constructor Caching](#8-performance-characteristics-constructor-caching)
9. [Engine Internals: V8 ICU C++ Wrappers & Memory Layout](#9-engine-internals-v8-icu-c-wrappers--memory-layout)
10. [Best Practices & Design Guidelines](#10-best-practices--design-guidelines)
11. [Real-World Case Study: Global E-Commerce Localization & Currency Engine](#11-real-world-case-study-global-e-commerce-localization--currency-engine)
12. [Interview Questions & Deep Dives](#12-interview-questions--deep-dives)
13. [Exercises & Challenges](#13-exercises--challenges)

---

## 1. What & Why

Historically, web applications that required internationalization (i18n) and localization (l10n) were forced to download massive third-party JavaScript libraries (e.g., Moment.js, Numeral.js, FormatJS), adding hundreds of kilobytes of bundle bloat.

Furthermore, naive client-side formatting frequently failed on basic cultural rules:
- In Germany, `$1,234.56` is written as `1.234,56 $` (commas and periods swapped).
- In Arabic or Hindi, digits are often displayed in non-Latin scripts (`١٢٣`).
- Alphabetical string sorting in German must treat `ä` either alongside `a` or as `ae` (phonebook collation), which standard `str.localeCompare` or `sort()` failed to handle consistently.

The **ECMA-402 Internationalization Standard** introduced the native **`Intl`** namespace. Built directly into browser engines and the Node.js runtime, `Intl` provides zero-bundle-size, blazing-fast localized formatting, sorting, pluralization, and text segmentation backed by the industry-standard **ICU (International Components for Unicode)** library.

---

## 2. Core Concepts: BCP 47 & ECMA-402 Formatters

### 2.1 BCP 47 Language Tags
Locales are specified using IETF BCP 47 tags:
`language-script-region-variants-extensions`
- `en-US`: English (United States)
- `zh-Hans-CN`: Chinese (Simplified script, China)
- `de-DE-u-co-phonebk`: German (Germany, with Unicode extension specifying phonebook collation)
- `ja-JP-u-ca-japanese`: Japanese (Japan, with Japanese imperial calendar)

### 2.2 The Intl Suite of Constructors
| API | Purpose | Baseline |
| :--- | :--- | :--- |
| **`Intl.DateTimeFormat`** | Localized dates, times, calendars, and timezones | ES2015 |
| **`Intl.NumberFormat`** | Currencies, percentages, units (km, bytes), compact notation | ES2015 / ES2020 |
| **`Intl.Collator`** | Linguistic string comparison and sorting | ES2015 |
| **`Intl.RelativeTimeFormat`** | Humanized relative time ("3 days ago", "in 2 hours") | ES2020 |
| **`Intl.ListFormat`** | Grammatical list joining ("A, B, and C") | ES2020 |
| **`Intl.PluralRules`** | Plural category resolution (`zero`, `one`, `few`, `many`, `other`)| ES2018 |
| **`Intl.Segmenter`** | Grapheme cluster, word, and sentence boundary detection | ES2022 |
| **`Intl.DisplayNames`** | Names of languages, regions, scripts, and currencies | ES2021 |

---

## 3. How It Works: Engine ICU Integration & Locale Resolution

All major JavaScript engines (V8 in Chrome/Node, JavaScriptCore in Safari, SpiderMonkey in Firefox) link directly to **ICU (International Components for Unicode)** and the **CLDR (Common Locale Data Repository)** database maintained by the Unicode Consortium:

```
 JavaScript API Call: new Intl.NumberFormat("de-DE", { style: "currency", currency: "EUR" })
                                     |
                                     v
 V8 Engine: v8::internal::Intl::CreateNumberFormat()
                                     |
                                     v
 Native ICU Library (C++): ucurr_format(), ures_open()
                                     |
                                     v
 CLDR Data Tables: Resolves locale fallback, currency symbols, and grouping rules
```

### Locale Resolution Algorithms:
When requesting a locale, the engine resolves it against available system data using one of two strategies:
1. `"best fit"` (Default): The engine uses heuristics to find the closest matching locale (e.g., fallback from `fr-CA` to `fr`).
2. `"lookup"`: RFC 4647 strict prefix-matching fallback algorithm.

---

## 4. Comprehensive Syntax & API Reference

### 4.1 `Intl.NumberFormat`
```javascript
// Localized currency:
const euro = new Intl.NumberFormat("de-DE", { style: "currency", currency: "EUR" });
console.log(euro.format(123456.78)); // "123.456,78 €"

// Compact unit formatting (ES2020):
const compact = new Intl.NumberFormat("en-US", { notation: "compact", compactDisplay: "short" });
console.log(compact.format(2_500_000)); // "2.5M"

// Measurement units (ES2020):
const speed = new Intl.NumberFormat("en-US", { style: "unit", unit: "kilometer-per-hour" });
console.log(speed.format(110)); // "110 km/h"
```

### 4.2 `Intl.DateTimeFormat`
```javascript
const d = new Date("2026-11-23T14:30:00Z");

const fmt = new Intl.DateTimeFormat("en-US", {
  dateStyle: "full",
  timeStyle: "short",
  timeZone: "America/New_York"
});
console.log(fmt.format(d)); 
// "Monday, November 23, 2026 at 9:30 AM"
```

### 4.3 `Intl.RelativeTimeFormat`
```javascript
const rtf = new Intl.RelativeTimeFormat("es", { numeric: "auto" });
console.log(rtf.format(-1, "day"));  // "ayer" (yesterday)
console.log(rtf.format(3, "month")); // "dentro de 3 meses" (in 3 months)
```

### 4.4 `Intl.ListFormat`
```javascript
const listFmt = new Intl.ListFormat("en-US", { style: "long", type: "conjunction" });
console.log(listFmt.format(["Alice", "Bob", "Charlie"])); // "Alice, Bob, and Charlie"

const disjunction = new Intl.ListFormat("de-DE", { style: "long", type: "disjunction" });
console.log(disjunction.format(["Rot", "Grün", "Blau"])); // "Rot, Grün oder Blau"
```

### 4.5 `Intl.Segmenter` (ES2022)
```javascript
const text = "Hello world! 🚀";
const segmenter = new Intl.Segmenter("en", { granularity: "word" });

for (const { segment, isWordLike } of segmenter.segment(text)) {
  if (isWordLike) console.log("Word:", segment);
}
// Outputs: Word: Hello, Word: world
```

---

## 5. Practical Examples

### Example 1: High-Performance Linguistic Sorting via `Intl.Collator`
Standard array sorting `arr.sort()` compares UTF-16 code unit values, causing accented letters to sort after the entire Latin alphabet:
```javascript
const names = ["Zoe", "Åke", "Adam", "Örjan"];

// Standard sort (Broken for Swedish):
console.log([...names].sort()); // [ 'Adam', 'Zoe', 'Åke', 'Örjan' ] (Z before Å!)

// Correct Swedish Collation:
const swedishCollator = new Intl.Collator("sv");
console.log([...names].sort(swedishCollator.compare));
// [ 'Adam', 'Zoe', 'Åke', 'Örjan' ] (In Swedish: Å, Ä, Ö are distinct letters at the end)

// German Phonebook Collation:
const germanPhonebook = new Intl.Collator("de-u-co-phonebk");
const germanWords = ["Müller", "Mustermann", "Mueller"];
console.log(germanWords.sort(germanPhonebook.compare));
// [ 'Mueller', 'Müller', 'Mustermann' ] (Müller treated as Mueller!)
```

### Example 2: Grammatical Pluralization Engine via `Intl.PluralRules`
Different languages have completely different plural categories (English has `one` and `other`; Arabic has `zero`, `one`, `two`, `few`, `many`, `other`):
```javascript
function getLocalizedNotification(locale, count) {
  const pr = new Intl.PluralRules(locale);
  const rule = pr.select(count);

  const messages = {
    en: {
      one: `${count} new notification`,
      other: `${count} new notifications`
    },
    ar: {
      zero: "لا توجد إشعارات جديدة",
      one: "إشعار واحد جديد",
      two: "إشعاران جديدان",
      few: `${count} إشعارات جديدة`,
      many: `${count} إشعارًا جديدًا`,
      other: `${count} إشعار جديد`
    }
  };

  return messages[locale]?.[rule] || `${count} notifications`;
}

console.log(getLocalizedNotification("en", 1)); // "1 new notification"
console.log(getLocalizedNotification("en", 5)); // "5 new notifications"
```

### Example 3: Parsing Localized Number Inputs
```javascript
class LocalizedNumberParser {
  constructor(locale) {
    const parts = new Intl.NumberFormat(locale).formatToParts(12345.6);
    this.group = parts.find(d => d.type === "group").value;
    this.decimal = parts.find(d => d.type === "decimal").value;
  }

  parse(stringNumber) {
    // Strip thousands separators and normalize decimal to standard '.'
    const normalized = stringNumber
      .replaceAll(this.group, "")
      .replace(this.decimal, ".");
    return parseFloat(normalized);
  }
}

const parserDe = new LocalizedNumberParser("de-DE");
console.log(parserDe.parse("1.234.567,89")); // 1234567.89
```

### Example 4: Formatting Date Ranges with `formatRange`
```javascript
const d1 = new Date("2026-07-10T10:00:00Z");
const d2 = new Date("2026-07-15T18:00:00Z");

const rangeFmt = new Intl.DateTimeFormat("en-US", { month: "short", day: "numeric" });
console.log(rangeFmt.formatRange(d1, d2)); // "Jul 10 – 15"

const rangeDe = new Intl.DateTimeFormat("de-DE", { month: "short", day: "numeric" });
console.log(rangeDe.formatRange(d1, d2)); // "10. – 15. Juli"
```

### Example 5: Non-Breaking Spaces in Financial Reports
```javascript
const formatter = new Intl.NumberFormat("fr-FR", { style: "currency", currency: "EUR" });
const result = formatter.format(100);

// In French, currency uses a non-breaking space (U+202F or U+00A0) before €:
console.log(result); // "100,00 €"
console.log(result.charCodeAt(6).toString(16)); // "202f" (Narrow No-Break Space)
```

---

## 6. Edge Cases, Pitfalls & Gotchas

### 6.1 Creating `Intl` Instances in Hot Loops
Instantiating `new Intl.DateTimeFormat()` or `new Intl.NumberFormat()` is **100x slower** than calling `.format()`. Instantiation loads ICU locale tables, parses BCP-47 options, and creates C++ bindings:
```javascript
// DISASTROUS: Instantiating inside a loop
for (let i = 0; i < 100_000; i++) {
  new Intl.NumberFormat("en-US").format(i); // Allocates 100k C++ objects!
}

// OPTIMAL: Instantiate once, reuse format method
const fmt = new Intl.NumberFormat("en-US");
for (let i = 0; i < 100_000; i++) {
  fmt.format(i);
}
```

### 6.2 Narrow Non-Breaking Spaces (`\u202F`) in Equality Checks
Many European currency formats (French, Polish, German) use non-breaking spaces between numbers and currency symbols. Comparing against standard space `" "` will fail:
```javascript
const frenchFormatted = new Intl.NumberFormat("fr-FR", { style: "currency", currency: "EUR" }).format(50);
console.log(frenchFormatted === "50,00 €"); // false! (Uses U+202F, not U+0020)
```

### 6.3 Node.js Small-ICU vs Full-ICU Builds
In older Node.js versions, default builds only included English locale data (small-ICU). Since Node.js v13+, **full-ICU** is bundled by default, supporting all world languages. Always verify full-ICU presence in Docker production containers.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Bad Practice | Idiomatic Solution | Reason |
| :--- | :--- | :--- | :--- |
| **Instantiating Formatters inside Loops** | `items.map(x => new Intl.NumberFormat().format(x))` | Instantiate formatter once outside the loop | Constructor involves heavy C++ ICU table initialization. |
| **Hardcoding Currency Symbols** | `"$ " + price.toFixed(2)` | `new Intl.NumberFormat(locale, { style: "currency", currency }).format(price)` | Cultural placement and symbols vary dramatically across locales. |
| **Manual String Sorting** | `strings.sort((a, b) => a.localeCompare(b))` | Cache an `Intl.Collator` instance: `strings.sort(collator.compare)` | Direct `collator.compare` is up to $10\times$ faster than repeated `localeCompare`. |
| **Joining Lists with Commas** | `users.join(", ") + " and " + last` | `new Intl.ListFormat(locale).format(users)` | Manual joining fails Oxford comma rules and foreign grammar. |
| **Ignoring Graphemes in Counting** | `str.length` for UI counters | `new Intl.Segmenter(locale, { granularity: "grapheme" })` | Emojis and combined diacritics count incorrectly without segmentation. |
| **Replacing Non-Breaking Spaces Unsafely** | Regex `str.replace(" ", "")` | Normalize spaces or use `formatToParts()` | Misses narrow no-break spaces (`\u202F`), causing parsing errors. |

---

## 8. Performance Characteristics: Constructor Caching

| Operation | Typical Execution Latency | Heap Allocation |
| :--- | :--- | :--- |
| `new Intl.NumberFormat()` | $\approx 25,000\text{ ns}$ ($25\ \mu\text{s}$) | $\approx 2\text{ KB}$ (C++ ICU wrapper + V8 object) |
| `formatter.format(100)` | $\approx 250\text{ ns}$ ($0.25\ \mu\text{s}$) | $\approx 40\text{ bytes}$ (String result) |
| `Intl.Collator.prototype.compare` | $\approx 180\text{ ns}$ | $0\text{ bytes}$ (Direct integer return) |

**Conclusion:** Caching `Intl` instances provides a **$100\times$ speedup** in high-throughput data processing pipelines.

---

## 9. Engine Internals: V8 ICU C++ Wrappers & Memory Layout

Inside V8, every `Intl` instance wraps a native ICU C++ pointer:

```
                  +-----------------------------------+
                  |   v8::internal::JSNumberFormat    |
                  +-----------------------------------+
                  | - Shape / Map pointer             |
                  | - icu_number_formatter: void*     | ---> Pointer to native C++
                  +-----------------------------------+      icu::number::LocalizedNumberFormatter
```

### Key V8 Mechanics:
1. **ICU Memory Allocation:** ICU objects are allocated in native C++ heap memory outside the managed V8 garbage-collected heap.
2. **Finalizer Cleanup:** When a JavaScript `Intl` instance is garbage-collected, a V8 weak callback frees the underlying C++ ICU formatter pointer to prevent native memory leaks.
3. **FormatToParts:** Methods like `.formatToParts()` invoke C++ field position iterators, constructing a structured array of tokens (`{ type: "integer", value: "10" }`) directly in V8 heap memory.

---

## 10. Best Practices & Design Guidelines

1. **Always Cache Intl Instances:** Create a dedicated formatters cache or registry keyed by locale and options string.
2. **Use `formatToParts()` for Custom UI Highlighting:** If you need to colorize currency symbols separately from numbers in React/HTML, use `formatToParts()` rather than string parsing.
3. **Specify Explicit Timezones:** In `Intl.DateTimeFormat`, always pass `{ timeZone: "UTC" }` or explicit IANA zones to prevent discrepancies between server and client.
4. **Use `Intl.DisplayNames` for Localization Dropdowns:** Avoid hardcoding country/language dropdowns; let `Intl.DisplayNames` generate localized labels dynamically.

---

## 11. Real-World Case Study: Global E-Commerce Localization & Currency Engine

### Problem
An e-commerce storefront expanded across 40 countries. Developers used string concatenation for prices (`€ + amount`), resulting in malformed currency symbols in Japan, Switzerland, and the UK. Product checkout pages frequently froze during product grid rendering because formatters were being reinstantiated on every card render.

### Solution
Implement an encapsulated, high-performance Localization Service with instance caching:

```javascript
class LocalizationService {
  static #numberFormatters = new Map();
  static #dateTimeFormatters = new Map();

  static getNumberFormatter(locale, options = {}) {
    const cacheKey = `${locale}:${JSON.stringify(options)}`;
    let fmt = this.#numberFormatters.get(cacheKey);
    if (!fmt) {
      fmt = new Intl.NumberFormat(locale, options);
      this.#numberFormatters.set(cacheKey, fmt);
    }
    return fmt;
  }

  static formatPrice(amount, currency, locale) {
    const formatter = this.getNumberFormatter(locale, {
      style: "currency",
      currency: currency,
      currencyDisplay: "symbol"
    });
    return formatter.format(amount);
  }

  static formatRelativeActivity(timestamp, locale) {
    const elapsedSeconds = Math.round((timestamp - Date.now()) / 1000);
    const rtf = new Intl.RelativeTimeFormat(locale, { numeric: "auto" });

    if (Math.abs(elapsedSeconds) < 60) return rtf.format(elapsedSeconds, "second");
    const minutes = Math.round(elapsedSeconds / 60);
    if (Math.abs(minutes) < 60) return rtf.format(minutes, "minute");
    const hours = Math.round(minutes / 60);
    return rtf.format(hours, "hour");
  }
}

// Fast cached pricing across multiple markets:
console.log(LocalizationService.formatPrice(49.99, "USD", "en-US")); // "$49.99"
console.log(LocalizationService.formatPrice(49.99, "EUR", "de-DE")); // "49,99 €"
console.log(LocalizationService.formatPrice(5400, "JPY", "ja-JP"));  // "￥5,400"
console.log(LocalizationService.formatPrice(120.50, "CHF", "de-CH"));// "CHF 120.50"

// Relative activity:
const twoMinutesAgo = Date.now() - 120_000;
console.log(LocalizationService.formatRelativeActivity(twoMinutesAgo, "en-US")); // "2 minutes ago"
```

---

## 12. Interview Questions & Deep Dives

<details>
<summary><strong>Q1: Why is <code>Intl.Collator.prototype.compare</code> preferred over <code>String.prototype.localeCompare</code> in array sorting?</strong></summary>

`str.localeCompare(other)` instantiates an internal `Intl.Collator` instance on every single call unless optimized by the engine. In an array sort of $N$ items, `localeCompare` performs $O(N \log N)$ collator initializations. By creating a single `Intl.Collator` once and passing `collator.compare` to `arr.sort()`, sorting runs up to $10\times$ faster.
</details>

<details>
<summary><strong>Q2: What is the purpose of the <code>Intl.Segmenter</code> API introduced in ECMAScript 2022?</strong></summary>

`Intl.Segmenter` enables locale-sensitive text segmentation at three granularity levels: `grapheme` (visual characters/emojis), `word` (words and punctuation), and `sentence`. It replaces fragile regex hacks that split characters across surrogate pairs or fail to detect word boundaries in non-space-delimited languages like Chinese or Japanese.
</details>

<details>
<summary><strong>Q3: What are BCP 47 Unicode extensions, and how are they used in Intl?</strong></summary>

BCP 47 Unicode extensions begin with `-u-` and specify cultural or regional overrides within a language tag. Examples:
- `-u-nu-latn`: Force Latin numerals in Arabic or Devanagari locales.
- `-u-ca-buddhist`: Use Buddhist calendar in Thai locales.
- `-u-co-phonebk`: Use phonebook collation order in German.
</details>

<details>
<summary><strong>Q4: Why does currency formatting with <code>Intl.NumberFormat</code> produce non-breaking spaces?</strong></summary>

Typography standards in countries like France, Germany, and Russia mandate that currency symbols (like `€` or `руб`) must never wrap onto a new line separate from the numbers. The CLDR database specifies non-breaking spaces (`\u00A0` or narrow `\u202F`) to prevent typographic orphans.
</details>

<details>
<summary><strong>Q5: What is the difference between <code>format()</code> and <code>formatToParts()</code>?</strong></summary>

`format(value)` returns a single concatenated string. `formatToParts(value)` returns an array of token objects (e.g., `[{ type: "integer", value: "10" }, { type: "decimal", value: "." }, { type: "fraction", value: "50" }]`), allowing developers to style or extract individual components without regex slicing.
</details>

<details>
<summary><strong>Q6: How does <code>Intl.PluralRules</code> differ between English and languages like Arabic or Russian?</strong></summary>

English only defines two grammatical plural forms: `one` (1 item) and `other` (0, 2+ items). Russian has `one` (1, 21, 31), `few` (2-4, 22-24), and `many` (5-20, 25-30). Arabic defines six distinct forms (`zero`, `one`, `two`, `few`, `many`, `other`). `Intl.PluralRules.prototype.select(n)` resolves numbers to these exact CLDR categories.
</details>

<details>
<summary><strong>Q7: What is the underlying engine library that powers the JavaScript <code>Intl</code> API?</strong></summary>

Modern JavaScript engines rely on **ICU (International Components for Unicode)**, an open-source C/C++ library maintained by the Unicode Consortium. ICU provides internationalization data and formatting logic based on the **CLDR (Common Locale Data Repository)**.
</details>

<details>
<summary><strong>Q8: What performance penalty occurs if you don't cache <code>Intl</code> constructors?</strong></summary>

Instantiating an `Intl` constructor requires ICU locale resolution, BCP-47 tag parsing, and C++ memory allocations, which takes approximately $25\ \mu\text{s}$ ($100\times$ slower than formatting). Repeated instantiations inside render loops cause severe GC pressure and frame drops.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Natural Language Array Joiner with Custom Oxford Comma
Write a helper function `formatAuthors(authorsList, locale = "en-US")` that grammatically formats author names for research citations using `Intl.ListFormat`.

<details>
<summary><strong>Solution</strong></summary>

```javascript
function formatAuthors(authors, locale = "en-US") {
  if (authors.length === 0) return "";
  const formatter = new Intl.ListFormat(locale, {
    style: "long",
    type: "conjunction"
  });
  return formatter.format(authors);
}

console.log(formatAuthors(["Knuth", "Dijkstra"], "en-US")); 
// "Knuth and Dijkstra"

console.log(formatAuthors(["Knuth", "Dijkstra", "Hoare"], "en-US")); 
// "Knuth, Dijkstra, and Hoare"

console.log(formatAuthors(["Knuth", "Dijkstra", "Hoare"], "fr-FR")); 
// "Knuth, Dijkstra et Hoare"
```
</details>

### Exercise 2: Precise Word Counter for Multi-Script Documents
Implement a word counter using `Intl.Segmenter` that accurately counts words in English, emoji strings, and Chinese text without relying on whitespace splitting.

<details>
<summary><strong>Solution</strong></summary>

```javascript
function countWords(text, locale = "en") {
  const segmenter = new Intl.Segmenter(locale, { granularity: "word" });
  let count = 0;

  for (const { isWordLike } of segmenter.segment(text)) {
    if (isWordLike) {
      count++;
    }
  }
  return count;
}

console.log(countWords("Hello, world! 🚀")); // 2
console.log(countWords("JavaScript is awesome.")); // 3
console.log(countWords("我喜欢编程")); // 3 (Chinese words segmented without spaces!)
```
</details>

### Exercise 3: Currency Custom Part Highlighting Formatter
Create a function `formatCurrencyWithBadges(amount, currency, locale)` that uses `formatToParts()` to return an HTML string wrapping the currency symbol in a `<span class="badge">` element.

<details>
<summary><strong>Solution</strong></summary>

```javascript
function formatCurrencyWithBadges(amount, currency, locale = "en-US") {
  const formatter = new Intl.NumberFormat(locale, { style: "currency", currency });
  const parts = formatter.formatToParts(amount);

  return parts
    .map(({ type, value }) => {
      if (type === "currency") {
        return `<span class="badge">${value}</span>`;
      }
      return value;
    })
    .join("");
}

console.log(formatCurrencyWithBadges(99.99, "USD", "en-US"));
// '<span class="badge">$</span>99.99'

console.log(formatCurrencyWithBadges(99.99, "EUR", "de-DE"));
// '99,99 <span class="badge">€</span>'
```
</details>
