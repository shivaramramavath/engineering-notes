# Strings & Regular Expressions Cheatsheet

> Quick-reference guide covering modern ECMAScript string manipulation methods, template literals, RegExp flags, regex syntax tokens, and grapheme segmentation.

| Metadata | Details |
| :--- | :--- |
| **Track** | Reference Cheatsheets (`27_cheatsheets`) |
| **Baseline** | ECMAScript 2024 / Node.js 22 LTS |
| **Level** | Reference / Quick Lookup |

---

## 1. Modern String Methods

| Method | Syntax | Description |
| :--- | :--- | :--- |
| `replaceAll()` | `str.replaceAll(search, replacement)` | Replaces all matches (no `/g` flag required for strings) |
| `at()` | `str.at(-1)` | Negative index support for strings |
| `padStart()` | `str.padStart(len, fill)` | Pads beginning of string |
| `padEnd()` | `str.padEnd(len, fill)` | Pads end of string |
| `trimStart()` / `trimEnd()` | `str.trimStart()` | Strips leading/trailing whitespace |
| `isWellFormed()` | `str.isWellFormed()` | Returns `true` if string contains no lone UTF-16 surrogates |
| `toWellFormed()` | `str.toWellFormed()` | Replaces lone UTF-16 surrogates with `\uFFFD` |

```javascript
"test-file-final.js".replaceAll("-", "_"); // "test_file_final.js"
"5".padStart(3, "0");                      // "005"
"hello".at(-1);                            // "o"
```

---

## 2. RegExp Flags Reference

| Flag | Name | Functionality |
| :--- | :--- | :--- |
| `g` | Global | Retains `lastIndex`; matches all occurrences |
| `i` | Ignore Case | Case-insensitive matching |
| `m` | Multi-line | `^` and `$` match start/end of line instead of whole input |
| `s` | DotAll | `.` matches any character including newline `\n` |
| `u` | Unicode | Handles full 21-bit Unicode code points and surrogate pairs |
| `v` | Unicode Sets | Advanced set operations (`[a-z--[aeiou]]`) and string properties |
| `y` | Sticky | Matches only starting at current `lastIndex` position |
| `d` | Indices | Generates start/end character match indices (`match.indices`) |

---

## 3. Regular Expression Token Quick Reference

| Token | Meaning | Example |
| :--- | :--- | :--- |
| `\d` / `\D` | Digit / Non-digit | `\d+` $\to$ `"123"` |
| `\w` / `\W` | Word character (`[a-zA-Z0-9_]`) / Non-word | `\w+` $\to$ `"user_name"` |
| `\s` / `\S` | Whitespace (`[\r\n\t\f\v ]`) / Non-whitespace | `\s*` $\to$ `"   "` |
| `\b` / `\B` | Word boundary / Non-word boundary | `\bcat\b` $\to$ `"cat"` |
| `(?<name>...)` | Named Capture Group | `/(?<year>\d{4})-(?<month>\d{2})/` |
| `(?:...)` | Non-Capturing Group | `/(?:https?:\/\/)/` |
| `(?=...)` | Positive Lookahead | `/\d+(?=px)/` (Matches digits followed by "px") |
| `(?!...)` | Negative Lookahead | `/\d+(?!px)/` (Matches digits not followed by "px") |
| `(?<=...)` | Positive Lookbehind | `/(?<=\$)\d+/` (Matches digits preceded by "$") |
| `(?<!...)` | Negative Lookbehind | `/(?<!\$)\d+/` (Matches digits not preceded by "$") |

```javascript
// Named Capture Groups and Indices:
const re = /(?<year>\d{4})-(?<month>\d{2})/d;
const match = re.exec("Date: 2026-11");
console.log(match.groups.year);       // "2026"
console.log(match.indices.groups.year); // [6, 10]
```

---

## 4. Internationalization & Unicode Segmentation

```javascript
// Accurate Word and Grapheme Boundary Splitting:
const segmenter = new Intl.Segmenter('en', { granularity: 'grapheme' });
const text = '👨‍👩‍👧‍👦 family';

// Accurate character count for multi-codepoint emojis:
const segments = Array.from(segmenter.segment(text));
console.log(segments.length); // 8 graphemes (text.length would report 18!)
```
