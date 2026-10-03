# 03 - Strings and Text

Almost every program handles text: names, messages, files, JSON, SQL, logs. This folder covers `String` and everything around it: building, comparing, formatting, searching with regular expressions, and the character-encoding issues that cause real production bugs.

```
string basics ─► string methods ─► pool & comparison ─► StringBuilder
                                                            │
            unicode & encoding ◄─ regex ◄─ text blocks ◄─ formatting
```

## Prerequisites

[02-methods](../02-methods/README.md). You should be comfortable with loops, arrays, methods, and the idea that variables hold references to objects ([pass-by-value](../02-methods/01_pass-by-value.md)).

## Reading order

| # | File | You will learn |
|---|------|----------------|
| 0 | [00_string-basics.md](./00_string-basics.md) | What a `String` is, immutability, creation, concatenation, `null` vs empty |
| 1 | [01_string-methods.md](./01_string-methods.md) | The everyday API: search, extract, transform, split, join |
| 2 | [02_string-pool-and-comparison.md](./02_string-pool-and-comparison.md) | The string pool, `==` vs `equals`, `intern`, ordering |
| 3 | [03_stringbuilder-and-stringbuffer.md](./03_stringbuilder-and-stringbuffer.md) | Building strings efficiently, thread-safety, `StringJoiner` |
| 4 | [04_string-formatting.md](./04_string-formatting.md) | `printf`/`format`/`formatted`, locales, number and currency formatting |
| 5 | [05_text-blocks.md](./05_text-blocks.md) | Multi-line strings for JSON, SQL and HTML |
| 6 | [06_regex.md](./06_regex.md) | `Pattern`, `Matcher`, groups, replacement, backtracking pitfalls |
| 7 | [07_unicode-and-encoding.md](./07_unicode-and-encoding.md) | `char` vs code point, UTF-8, charsets, normalization |

## Practice

| After file | Try |
|------------|-----|
| 00-01 | Reverse a string, count vowels, check palindrome, capitalize each word, remove duplicate characters |
| 02 | Predict the result of 8 `==` comparisons, then verify in [JShell](../00-setup/03_jshell.md) |
| 03 | Build a CSV line from an array without a trailing comma; benchmark `+=` vs `StringBuilder` for 100,000 appends |
| 04 | Print a right-aligned receipt table with prices to two decimals and thousands separators |
| 05 | Embed a JSON sample and a SQL query as text blocks |
| 06 | Extract all numbers from a sentence; parse `2026-10-03` into named groups; mask digits in a card number |
| 07 | Print the length, code point count and UTF-8 byte count of `"héllo 😀"` |

**Project:** [26-projects/01-cli-application](../26-projects/01-cli-application/) uses everything in this folder (parsing commands, formatting output).

## You are done when you can

- [ ] Explain why `s.toUpperCase();` alone changes nothing
- [ ] Predict `==` vs `equals` results for literals, `new String`, `intern` and runtime concatenation
- [ ] Choose between `+`, `StringBuilder`, `String.join` and `String.format` for a task
- [ ] Explain why `"a.b".split(".")` returns an empty array and fix it
- [ ] Write a regex with named groups and know why `"\\d"` needs two backslashes
- [ ] Explain why `"😀".length()` is `2` and why you should always pass `StandardCharsets.UTF_8`

## Key takeaways

- `String` is immutable; every "change" returns a new string
- Compare content with `equals`, never `==`
- Build strings in loops with `StringBuilder`
- Text is UTF-16 in memory and bytes on disk and the wire: always name the charset

**Next:** [04-oop](../04-oop/README.md)
