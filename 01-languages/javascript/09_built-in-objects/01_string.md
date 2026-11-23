# JavaScript String: Unicode, Encodings, Internal Representations & High-Performance Operations

> Deep technical guide to ECMAScript String primitives, UTF-16 code units vs Unicode code points, normalization forms (NFC/NFD), V8 string memory representations (ConsString, SlicedString, ThinString), and high-throughput text processing.

| Metadata | Details |
| :--- | :--- |
| **Level** | Core / Engine Internals |
| **Prerequisites** | [01_fundamentals/02_data-types.md](../01_fundamentals/02_data-types.md), [01_fundamentals/05_operators.md](../01_fundamentals/05_operators.md) |
| **Time to Read** | 26 minutes |
| **Status** | Complete |
| **ECMAScript Baseline** | ES2015 (codePointAt, fromCodePoint, normalize) / ES2020 (matchAll) / ES2021 (replaceAll) / ES2022 (at) |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts: Immutability, UTF-16 & Unicode Code Points](#2-core-concepts-immutability-utf-16--unicode-code-points)
3. [How It Works: Engine String Architecture & Memory Models](#3-how-it-works-engine-string-architecture--memory-models)
4. [Comprehensive Syntax & API Reference](#4-comprehensive-syntax--api-reference)
5. [Practical Examples](#5-practical-examples)
6. [Edge Cases, Pitfalls & Gotchas](#6-edge-cases-pitfalls--gotchas)
7. [Common Mistakes & Anti-Patterns](#7-common-mistakes--anti-patterns)
8. [Performance Characteristics](#8-performance-characteristics)
9. [Engine Internals: V8 ConsString, SlicedString & String Flattening](#9-engine-internals-v8-consstring-slicedstring--string-flattening)
10. [Best Practices & Design Guidelines](#10-best-practices--design-guidelines)
11. [Real-World Case Study: Grapheme-Aware Truncation & Token Sanitization Engine](#11-real-world-case-study-grapheme-aware-truncation--token-sanitization-engine)
12. [Interview Questions & Deep Dives](#12-interview-questions--deep-dives)
13. [Exercises & Challenges](#13-exercises--challenges)

---

## 1. What & Why

In JavaScript, textual data is represented by the primitive **`String`**. Unlike C (where strings are null-terminated byte arrays) or Rust (where strings are strictly validated UTF-8 sequences), ECMAScript specifies that strings are **immutable sequences of 16-bit unsigned integer values (UTF-16 code units)**.

Because JavaScript was designed in 1995 when Unicode only defined 65,536 characters (the Basic Multilingual Plane, BMP), JavaScript chose UCS-2/UTF-16. When Unicode expanded to over 1.1 million code points (spanning planes 1 through 16 for emojis, historical scripts, and symbols), 16 bits became insufficient.

Understanding JavaScript strings requires mastering:
1. **The UTF-16 surrogate pair mechanism:** Characters above `U+FFFF` require two 16-bit code units.
2. **String immutability:** Strings can never be modified in-place; all transformations return new strings.
3. **Engine-level string representation:** Modern engines (like V8) do not store strings as plain flat character arrays; they employ ropes (`ConsString`), slices (`SlicedString`), and internalized strings (`ThinString`) to make operations near-instantaneous.

---

## 2. Core Concepts: Immutability, UTF-16 & Unicode Code Points

### 2.1 Primitive String vs String Object
In JavaScript, string literals create primitive values. Primitive strings have no methods of their own; when a method like `.toUpperCase()` is called, JavaScript automatically **boxes** the primitive into a temporary wrapper object `new String(str)`, executes the prototype method, and discards the wrapper:
```javascript
const str = "hello";
console.log(typeof str); // "string" (primitive)

const obj = new String("hello");
console.log(typeof obj); // "object" (avoid in production!)

console.log(str == obj);  // true (loose coercion)
console.log(str === obj); // false (strict type check fails)
```

### 2.2 UTF-16 Code Units vs Code Points
- **Code Unit:** A 16-bit storage unit (values $0$ to $65535$ / `0x0000` to `0xFFFF`).
- **Code Point:** An actual abstract character in the Unicode codespace (values `U+0000` to `U+10FFFF`).
- **Surrogate Pair:** Characters with code points $> \text{U+FFFF}$ (such as emojis `😀` `U+1F600`) are encoded as two 16-bit code units: a **High Surrogate** (`0xD800`–`0xDBFF`) and a **Low Surrogate** (`0xDC00`–`0xDFFF`).

```
Unicode Code Point: U+1F600 (😀)
----------------------------------------
UTF-16 High Surrogate: \uD83D (0xD83D)
UTF-16 Low Surrogate:  \uDE00 (0xDE00)
----------------------------------------
str.length:           2 (counts 16-bit code units)
[...str].length:      1 (iterates Unicode code points)
```

### 2.3 Unicode Normalization Forms
A single visual glyph can often be represented by multiple distinct code point sequences (e.g., `é` can be a single code point `\u00E9` or the base letter `e` followed by a combining acute accent `\u0065\u0301`):
```javascript
const s1 = "\u00E9";       // "é" (Single code point: NFC form)
const s2 = "e\u0301";      // "é" (Two code points: 'e' + combining accent)

console.log(s1 === s2);    // false!
console.log(s1.length);    // 1
console.log(s2.length);    // 2

// Normalize to canonical composition (NFC):
console.log(s1.normalize("NFC") === s2.normalize("NFC")); // true
```

The four Unicode normalization forms are:
- `NFC` (Canonical Decomposition followed by Canonical Composition - default web standard)
- `NFD` (Canonical Decomposition)
- `NFKC` (Compatibility Decomposition followed by Canonical Composition)
- `NFKD` (Compatibility Decomposition)

---

## 3. How It Works: Engine String Architecture & Memory Models

In V8, strings are managed on the managed heap. Rather than copying arrays of bytes on every concatenation or substring operation, V8 uses a rich polymorphism of internal string structures:

```
                               +-----------------------------+
                               |    v8::internal::String     |
                               +-----------------------------+
                               | Shape Map / Base Header     |
                               +--------------+--------------+
                                              |
     +-------------------+--------------------+--------------------+--------------------+
     |                   |                    |                    |                    |
+----+------------+ +----+------------+ +-----+------------+ +-----+------------+ +-----+------------+
|   SeqString     | |   ConsString    | |   SlicedString   | |   ThinString     | |  ExternalString  |
+-----------------+ +-----------------+ +------------------+ +------------------+ +------------------+
| Direct flat buf | | Tree rope:      | | Window slice:    | | Forward pointer  | | Points to memory |
| - OneByte (ASCII| | - left String*  | | - parent String* | | to internalized  | | outside V8 heap  |
| - TwoByte (UTF16| | - right String* | | - offset, length | | string table     | | (e.g. Node buffer|
+-----------------+ +-----------------+ +------------------+ +------------------+ +------------------+
```

### Key V8 Invariants:
1. **SeqOneByteString:** If all characters in a string fit in Latin-1 (`U+0000` to `U+00FF`), V8 allocates only 1 byte per character. If a single character exceeds `U+00FF`, V8 transitions to `SeqTwoByteString` (2 bytes per character).
2. **ConsString (Rope):** When concatenating strings (`a + b`), V8 does not allocate a new buffer and copy bytes. Instead, it creates a small `ConsString` object with two pointers pointing to `a` and `b`.
3. **SlicedString:** Substring operations (`str.slice(start, end)`) do not copy characters; they create a `SlicedString` referencing the original string with an offset and length.

---

## 4. Comprehensive Syntax & API Reference

### 4.1 Inspection & Code Points
```javascript
const text = "A😀B";

// Length (counts 16-bit code units, NOT characters):
console.log(text.length); // 4

// Legacy code unit access:
console.log(text.charAt(1));     // "\uD83D" (Broken high surrogate)
console.log(text.charCodeAt(1));  // 55357 (0xD83D)

// Modern code point access:
console.log(text.codePointAt(1)); // 128512 (0x1F600 - full emoji code point)
console.log(String.fromCodePoint(128512)); // "😀"

// Relative indexing (ES2022):
console.log(text.at(-1)); // "B"
```

### 4.2 Searching & Matching
| Method | Signature | Description |
| :--- | :--- | :--- |
| `indexOf` | `(str, fromIndex?)` | Returns first index or `-1` |
| `lastIndexOf` | `(str, fromIndex?)` | Returns last index or `-1` |
| `includes` | `(str, fromIndex?)` | Returns boolean (ES2015) |
| `startsWith` | `(str, fromIndex?)` | Returns boolean (ES2015) |
| `endsWith` | `(str, fromIndex?)` | Returns boolean (ES2015) |
| `search` | `(regex)` | Returns index matching regex |
| `match` | `(regex)` | Returns match array or `null` |
| `matchAll` | `(regex)` | Returns iterator of all regex matches with groups (ES2020) |

### 4.3 Transformation & Formatting
```javascript
const msg = "   JavaScript Core   ";

// Trimming:
console.log(msg.trim());      // "JavaScript Core"
console.log(msg.trimStart()); // "JavaScript Core   "
console.log(msg.trimEnd());   // "   JavaScript Core"

// Padding:
console.log("42".padStart(5, "0")); // "00042"
console.log("42".padEnd(5, "."));   // "42..."

// Replacement:
const s = "cat and cat";
console.log(s.replace("cat", "dog"));    // "dog and cat" (replaces first only!)
console.log(s.replaceAll("cat", "dog")); // "dog and dog" (ES2021)

// Splitting & Repeating:
console.log("a,b,c".split(",")); // ["a", "b", "c"]
console.log("ha".repeat(3));     // "hahaha"
```

---

## 5. Practical Examples

### Example 1: Unicode-Safe String Length & Character Reversal
```javascript
function getUnicodeLength(str) {
  // Spreading respects Unicode code points via Symbol.iterator:
  return [...str].length;
}

function reverseUnicodeString(str) {
  return [...str].reverse().join("");
}

const emojiText = "Hello 🚀 World";
console.log(emojiText.length);             // 14 code units
console.log(getUnicodeLength(emojiText));   // 13 code points
console.log(reverseUnicodeString("Rocket 🚀")); // "🚀 tekcoR"
```

### Example 2: Parsing Key-Value Headers with `matchAll`
```javascript
const rawHeaders = `
Content-Type: application/json; charset=utf-8
Content-Length: 1024
X-Request-Id: req_77fa8901
`;

const headerRegex = /^(?<key>[a-zA-Z0-9_-]+):\s*(?<value>.+)$/gm;
const headers = {};

for (const match of rawHeaders.matchAll(headerRegex)) {
  const { key, value } = match.groups;
  headers[key.toLowerCase()] = value.trim();
}

console.log(headers);
// Output:
// {
//   'content-type': 'application/json; charset=utf-8',
//   'content-length': '1024',
//   'x-request-id': 'req_77fa8901'
// }
```

### Example 3: Safe Slug Generator with Unicode Normalization
```javascript
function createSlug(title) {
  return title
    .normalize("NFD")                 // Decompose accented letters: é -> e + ´
    .replace(/[\u0300-\u036f]/g, "") // Strip combining diacritical marks
    .toLowerCase()
    .trim()
    .replace(/[^a-z0-9\s-]/g, "")    // Remove invalid characters
    .replace(/[\s_-]+/g, "-")        // Collapse whitespace and underscores to dashes
    .replace(/^-+|-+$/g, "");        // Remove leading/trailing dashes
}

console.log(createSlug("Café & Crème Brûlée Vol. 1!")); 
// Output: "cafe-creme-brulee-vol-1"
```

### Example 4: CSV Field Parsing with Escaped Quotes
```javascript
function parseCsvRow(row) {
  const fields = [];
  let current = "";
  let insideQuotes = false;

  for (let i = 0; i < row.length; i++) {
    const char = row[i];
    const nextChar = row[i + 1];

    if (char === '"') {
      if (insideQuotes && nextChar === '"') {
        current += '"';
        i++; // Skip escaped double quote
      } else {
        insideQuotes = !insideQuotes;
      }
    } else if (char === ',' && !insideQuotes) {
      fields.push(current);
      current = "";
    } else {
      current += char;
    }
  }
  fields.push(current);
  return fields;
}

const sample = '101,"Doe, Jane",95000,"Software ""Principal"" Engineer"';
console.log(parseCsvRow(sample));
// Output: [ '101', 'Doe, Jane', '95000', 'Software "Principal" Engineer' ]
```

### Example 5: High-Performance Multi-Replace Pattern
```javascript
function multiReplace(str, map) {
  // Construct regex from escaped keys:
  const pattern = new RegExp(
    Object.keys(map).map(k => k.replace(/[.*+?^${}()|[\]\\]/g, '\\$&')).join("|"),
    "g"
  );
  return str.replace(pattern, matched => map[matched]);
}

const template = "Hello <name>, your order <orderId> is <status>.";
const sanitized = multiReplace(template, {
  "<name>": "Ada",
  "<orderId>": "#9042",
  "<status>": "SHIPPED"
});
console.log(sanitized); // "Hello Ada, your order #9042 is SHIPPED."
```

---

## 6. Edge Cases, Pitfalls & Gotchas

### 6.1 Slicing Through Surrogate Pairs
Using `slice()`, `substring()`, or index lookups on strings containing 4-byte characters can slice directly between the high and low surrogates, leaving an orphaned surrogate code point:
```javascript
const emoji = "😀"; // Code units: \uD83D \uDE00
const broken = emoji.slice(0, 1); // "\uD83D"

console.log(broken); // Outputs Unicode replacement character "" or broken glyph
console.log(broken.charCodeAt(0).toString(16)); // "d83d" (orphaned high surrogate)
```

### 6.2 Regex Special Replacements in `replace()`
When passing a replacement string to `str.replace()`, special character patterns have magical meanings:
- `$$` inserts a literal `$`
- `$&` inserts the matched substring
- `$\`` inserts the portion of the string before the match
- `$'` inserts the portion of the string after the match

```javascript
const input = "Price is 100";
// Intended: replace 100 with $50
console.log(input.replace("100", "$50")); // "Price is $50"
// BUT if replacing with $`:
console.log(input.replace("100", "$`"));  // "Price is Price is " (Copies prefix!)
```
**Fix:** Always use a replacement function `() => replacementValue` when dealing with untrusted dynamic replacement strings.

### 6.3 Memory Retention via `SlicedString`
In V8, `largeString.slice(0, 10)` creates a `SlicedString` pointing back to `largeString`. If the small slice is retained in a long-lived closure or global cache, the **entire large original string is kept in memory** and cannot be garbage collected.

```javascript
let retained;
function parseResponse() {
  const hugeData = "X".repeat(100_000_000); // 100 MB string
  // Retains entire 100MB buffer in V8:
  retained = hugeData.slice(0, 5);
}
```
**Fix:** Force deep string flattening / copying:
```javascript
retained = String(hugeData.slice(0, 5)); // or (...[hugeData.slice(0, 5)]).join("")
```

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Bad Practice | Idiomatic Solution | Reason |
| :--- | :--- | :--- | :--- |
| **Using `new String()`** | `const s = new String("x");` | `const s = "x";` or `String(val)` | Instantiates a wrapper object; breaks `===` strict equality. |
| **Calculating length via `.length` for emojis** | `if (username.length > 10)` | `if ([...username].length > 10)` | Emojis count as 2+ code units, causing premature truncation. |
| **Using `replace` without global flag** | `str.replace("a", "b")` | `str.replaceAll("a", "b")` | String `replace` only replaces the first match occurrence. |
| **Comparing Unicode without normalization** | `user1 === user2` | `user1.normalize("NFC") === user2.normalize("NFC")` | Accented characters can have identical glyphs but different code points. |
| **Repeated `+` in massive loops** | `for (...) str += chunk;` | Use an array `chunks.push(chunk)` and `join("")` | Prevents excessive ConsString rope tree nesting and reallocation. |
| **Using indexer on out-of-bound negative indices** | `str[str.length - 1]` | `str.at(-1)` | ES2022 `.at(-1)` is cleaner, less verbose, and idiomatic. |

---

## 8. Performance Characteristics

1. **Concatenation Complexity:**
   - Small concatenations (`a + b`) allocate a `ConsString` in $O(1)$ time without copying memory.
   - However, deeply nested `ConsString` trees (thousands of levels deep) cause stack overflows or high latency during subsequent indexing or regex operations because V8 is forced to **flatten** the rope into a flat `SeqString`.
2. **Substrings (`slice` vs `substring`):**
   - Both are $O(1)$ allocations producing a `SlicedString`.
3. **Internalized Strings (Symbols / Property Keys):**
   - V8 maintains an internal string table (Atom table). When strings are used as object property keys, V8 internalizes them so comparisons can be performed by simple pointer equality ($O(1)$) rather than character-by-character comparisons ($O(n)$).

---

## 9. Engine Internals: V8 ConsString, SlicedString & String Flattening

Inside V8, when you repeatedly concatenate strings:
```javascript
let s = "a";
for (let i = 0; i < 1000; i++) {
  s = s + "b";
}
```
V8 generates a binary tree of `ConsString` nodes:

```
               ConsString (Root)
              /                \
      ConsString                "b"
     /          \
 ConsString     "b"
  /      \
"a"      "b"
```

### String Flattening:
When an operation requires direct linear memory access (such as passing the string to a Regular Expression or accessing `s.charCodeAt(500)`):
1. V8 notices the string is a `ConsString`.
2. It allocates a single, contiguous `SeqString` of the total combined length.
3. It performs an in-order traversal of the tree, copying characters into the new flat buffer.
4. It converts the root `ConsString` into a `ThinString` whose pointer points directly to the newly allocated flat `SeqString`.

---

## 10. Best Practices & Design Guidelines

1. **Always Normalize User Input Strings:** When comparing usernames, email addresses, search queries, or passwords containing international characters, always run `.normalize("NFC")`.
2. **Use `Intl.Segmenter` for Grapheme Clusters:** If counting characters in user-facing UI text (e.g., character counters for Twitter/X), use `Intl.Segmenter` with `granularity: "grapheme"` instead of `[...str].length` to correctly handle flags and complex multi-code-point emojis.
3. **Use `replaceAll()` for Literal Replacements:** Avoid the error-prone regex `new RegExp("term", "g")` for simple literal replacements. Use `str.replaceAll("term", "replacement")`.
4. **Beware of Large Memory Leaks via Slices:** When reading megabyte/gigabyte payloads and storing small tokens, copy the token or use template string concatenation to detach it from the parent `SlicedString`.

---

## 11. Real-World Case Study: Grapheme-Aware Truncation & Token Sanitization Engine

### Problem
A content management system truncates user comments to 20 characters with an ellipsis (`...`). Using `str.slice(0, 20)` frequently sliced through Unicode surrogate pairs and multi-codepoint emojis (like `👨‍👩‍👧‍👦` or flags `🇺🇸`), causing broken glyphs `` to appear in production.

### Solution
Implement a grapheme-cluster aware truncator using modern `Intl.Segmenter`:

```javascript
class UnicodeTextSanitizer {
  constructor(locale = "en") {
    this.segmenter = new Intl.Segmenter(locale, { granularity: "grapheme" });
  }

  // Accurately counts true visual glyphs:
  countGraphemes(text) {
    let count = 0;
    for (const _ of this.segmenter.segment(text)) {
      count++;
    }
    return count;
  }

  // Safely truncates without splitting complex emojis or diacritics:
  truncate(text, maxGraphemes, suffix = "...") {
    const segments = [...this.segmenter.segment(text)];
    if (segments.length <= maxGraphemes) {
      return text;
    }
    
    const sliceEnd = segments[maxGraphemes - 1].index + segments[maxGraphemes - 1].segment.length;
    return text.slice(0, sliceEnd) + suffix;
  }
}

const sanitizer = new UnicodeTextSanitizer();

// Complex multi-code-point emoji: Family (4 emojis + 3 zero-width joiners = 8 UTF-16 code units)
const sampleText = "Family: 👨‍👩‍👧‍👦 is traveling to 🇯🇵 today!";

console.log("Raw UTF-16 length:", sampleText.length); // 38
console.log("True grapheme count:", sanitizer.countGraphemes(sampleText)); // 27

// Truncate at 10 graphemes:
const truncated = sanitizer.truncate(sampleText, 10);
console.log("Safely truncated:", truncated);
// Output: "Family: 👨‍👩‍👧‍👦 is..." (Emoji completely preserved intact!)
```

---

## 12. Interview Questions & Deep Dives

<details>
<summary><strong>Q1: Why does <code>"😀".length</code> evaluate to 2 in JavaScript?</strong></summary>

JavaScript's `String.prototype.length` property counts 16-bit **UTF-16 code units**, not human-perceived characters or Unicode code points. The emoji `😀` has a code point value of `U+1F600`, which is outside the 16-bit Basic Multilingual Plane (BMP). In UTF-16, it is encoded as a surrogate pair of two 16-bit code units (`\uD83D\uDE00`), hence `length` returns 2.
</details>

<details>
<summary><strong>Q2: What is the difference between <code>String.prototype.codePointAt()</code> and <code>String.prototype.charCodeAt()</code>?</strong></summary>

`charCodeAt(index)` returns the 16-bit numeric value of the single UTF-16 code unit at `index` (between $0$ and $65,535$). If `index` points to the start of a surrogate pair, it only returns the high surrogate. `codePointAt(index)` checks if the character at `index` is the start of a surrogate pair; if so, it decodes the full 32-bit Unicode code point (up to `0x10FFFF`).
</details>

<details>
<summary><strong>Q3: How does V8 optimize string concatenation using <code>ConsString</code>?</strong></summary>

When concatenating two strings (`a + b`), V8 avoids the $O(n)$ overhead of allocating a new character buffer and copying characters. Instead, it allocates a lightweight `ConsString` object with two pointers referencing `a` and `b` in $O(1)$ time. The tree is only flattened into a contiguous string buffer when an operation requires linear access.
</details>

<details>
<summary><strong>Q4: Why does <code>"café".normalize("NFC") === "cafe\u0301".normalize("NFC")</code> return true, while raw comparison returns false?</strong></summary>

In Unicode, accented characters can be represented either as a single pre-composed code point (`\u00E9` for `é`) or as a decomposed sequence of base letter plus combining mark (`e` + `\u0301`). Raw string comparison checks exact UTF-16 code unit sequences. Calling `.normalize("NFC")` transforms all decomposed characters into their canonical composed forms, producing identical byte sequences.
</details>

<details>
<summary><strong>Q5: What is a <code>SlicedString</code> in V8, and how can it cause a memory leak?</strong></summary>

A `SlicedString` is a V8 internal structure created during `str.slice()` or `str.substring()`. Rather than copying characters, it holds a reference pointer to the original parent string along with an offset and length. If a large string (e.g., 50 MB) is sliced to extract a small 5-character token, keeping that 5-character token prevents the entire 50 MB parent string from being garbage collected.
</details>

<details>
<summary><strong>Q6: How does <code>String.prototype.replaceAll()</code> differ from <code>String.prototype.replace()</code> when supplied with a string argument?</strong></summary>

When given a string as the search pattern, `replace(target, replacement)` only replaces the very first occurrence of `target`. In contrast, `replaceAll(target, replacement)` replaces all occurrences of `target` without needing a global regular expression.
</details>

<details>
<summary><strong>Q7: What is string interning (or atomization), and why does JavaScript use it?</strong></summary>

String interning is an engine optimization where unique string values are stored in a centralized lookup table (Atom table). When property keys or small identifiers are interned, all identical strings point to the same memory address. This allows the engine to compare strings using fast pointer equality ($O(1)$) rather than character-by-character checks ($O(n)$).
</details>

<details>
<summary><strong>Q8: What is the difference between Unicode Code Points and Grapheme Clusters?</strong></summary>

A **Code Point** is a single numerical value assigned by the Unicode Consortium. A **Grapheme Cluster** is a user-perceived character that may be composed of multiple code points. For example, the flag of Japan `🇯🇵` consists of two regional indicator code points (`U+1F1EF` and `U+1F1F5`). A code point iterator counts 2, but `Intl.Segmenter` (grapheme) correctly identifies it as 1 visual character.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Unicode-Aware Character Frequency Map
Write a function `characterFrequency(str)` that counts the frequency of each unique Unicode character, ensuring surrogate pairs are not split.

<details>
<summary><strong>Solution</strong></summary>

```javascript
function characterFrequency(str) {
  const freq = new Map();
  // for...of iterates by Unicode code points:
  for (const char of str.normalize("NFC")) {
    freq.set(char, (freq.get(char) || 0) + 1);
  }
  return freq;
}

const text = "A😀B😀A";
console.log(characterFrequency(text));
// Map(3) { 'A' => 2, '😀' => 2, 'B' => 1 }
```
</details>

### Exercise 2: Detaching Sliced Strings from Large Parent Memory
Write a utility function `detachString(slice)` that guarantees a substring is detached from any underlying `SlicedString` reference to allow the parent buffer to be collected.

<details>
<summary><strong>Solution</strong></summary>

```javascript
function detachString(str) {
  // Method 1: Slicing into array of chars and joining forces flat SeqString allocation
  return [...str].join("");
}

// Verification:
let huge = "X".repeat(10_000_000);
let token = detachString(huge.slice(0, 10));
huge = null; // huge buffer is now eligible for immediate garbage collection
console.log(token); // "XXXXXXXXXX"
```
</details>

### Exercise 3: Case-Insensitive, Accent-Insensitive Substring Matcher
Implement a function `fuzzyContains(haystack, needle)` that returns `true` if `haystack` contains `needle`, ignoring both case differences and diacritical accents.

<details>
<summary><strong>Solution</strong></summary>

```javascript
function stripAccents(str) {
  return str.normalize("NFD").replace(/[\u0300-\u036f]/g, "").toLowerCase();
}

function fuzzyContains(haystack, needle) {
  const cleanHaystack = stripAccents(haystack);
  const cleanNeedle = stripAccents(needle);
  return cleanHaystack.includes(cleanNeedle);
}

console.log(fuzzyContains("Crème Brûlée", "brulee")); // true
console.log(fuzzyContains("München", "munchen"));     // true
console.log(fuzzyContains("Kraków", "krakow"));       // true
console.log(fuzzyContains("Warsaw", "berlin"));       // false
```
</details>
