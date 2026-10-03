# Regular Expressions

A **regular expression** (regex) is a compact pattern for matching text: validating input, searching, extracting parts, replacing and splitting. Java supports regex through `java.util.regex` (`Pattern`, `Matcher`) and through `String` methods (`matches`, `split`, `replaceAll`).

```java
Pattern p = Pattern.compile("(\\d{4})-(\\d{2})-(\\d{2})");
Matcher m = p.matcher("Date: 2026-10-03");
if (m.find()) {
    System.out.println(m.group(1));      // 2026
    System.out.println(m.group(2));      // 10
}
```

## The two-step model

```
pattern string ──compile──► Pattern (immutable, thread-safe, reusable)
                                │
                  matcher(text) ▼
                             Matcher (stateful, one per text, NOT thread-safe)
                                │
                  find / matches / group / replaceAll
```

## Quick start with `String` methods

```java
"2026-10-03".matches("\\d{4}-\\d{2}-\\d{2}");       // true: the WHOLE string must match
"a1b22c".replaceAll("\\d+", "#");                    // "a#b#c"
"one, two,three".split("\\s*,\\s*");                 // ["one", "two", "three"]
```

Each call compiles the pattern again. For repeated use, compile once into a `static final Pattern`.

## Escaping in Java strings

A regex backslash is a **Java string** backslash too, so it must be doubled.

| Regex | Java string literal |
|-------|---------------------|
| `\d+` | `"\\d+"` |
| `\.` | `"\\."` |
| `\\` (a literal backslash) | `"\\\\"` |
| `\b` (word boundary) | `"\\b"` |

Text blocks do not remove this: the regex still needs `\\d`... but they let you write multi-line patterns with the `COMMENTS` flag ([text blocks](./05_text-blocks.md)).

## Pattern syntax cheat sheet

### Characters and classes

| Pattern | Matches |
|---------|---------|
| `.` | Any character except a line break (with `DOTALL`: any) |
| `\d` / `\D` | Digit `[0-9]` / non-digit |
| `\w` / `\W` | Word character `[a-zA-Z0-9_]` / non-word |
| `\s` / `\S` | Whitespace / non-whitespace |
| `[abc]` | One of `a`, `b`, `c` |
| `[^abc]` | Anything **except** `a`, `b`, `c` |
| `[a-z0-9]` | A range |
| `[a-c&&[^b]]` | Intersection (`a` or `c`) |
| `\p{L}` | Any Unicode letter |
| `\p{Alpha}`, `\p{Punct}` | POSIX-style ASCII classes |
| `\.` `\[` `\$` | Escaped special characters |

### Anchors and boundaries

| Pattern | Matches |
|---------|---------|
| `^` / `$` | Start / end of input (of each line in `MULTILINE`) |
| `\b` / `\B` | Word boundary / not a word boundary |
| `\A` / `\z` | Absolute start / end of input |

### Quantifiers

| Greedy | Reluctant | Possessive | Meaning |
|--------|-----------|------------|---------|
| `*` | `*?` | `*+` | 0 or more |
| `+` | `+?` | `++` | 1 or more |
| `?` | `??` | `?+` | 0 or 1 |
| `{n}` | | | Exactly n |
| `{n,}` | `{n,}?` | `{n,}+` | n or more |
| `{n,m}` | `{n,m}?` | `{n,m}+` | Between n and m |

```java
"<a><b>".replaceAll("<.+>", "X");     // "X"      greedy: takes as much as possible
"<a><b>".replaceAll("<.+?>", "X");    // "XX"     reluctant: as little as possible
```

### Groups, alternation, lookaround

| Pattern | Meaning |
|---------|---------|
| `(abc)` | Capturing group (numbered from 1) |
| `(?:abc)` | Non-capturing group |
| `(?<name>abc)` | Named capturing group |
| `\1`, `\k<name>` | Backreference to an earlier group |
| `a\|b` | Alternation (a or b) |
| `(?=x)` / `(?!x)` | Lookahead: followed / not followed by `x` |
| `(?<=x)` / `(?<!x)` | Lookbehind: preceded / not preceded by `x` |
| `(?i)` | Case-insensitive from here on (also `(?m)`, `(?s)`, `(?x)`) |

## `Pattern` and `Matcher`

```java
private static final Pattern DATE = Pattern.compile(
    "(?<year>\\d{4})-(?<month>\\d{2})-(?<day>\\d{2})");

Matcher m = DATE.matcher("Due: 2026-10-03.");
if (m.find()) {
    String year = m.group("year");          // "2026"
    String whole = m.group();               // "2026-10-03"  (group 0)
    int start = m.start();                  // 5
    int end = m.end();                      // 15 (exclusive)
}
```

### `matches`, `find`, `lookingAt`

| Method | Succeeds when |
|--------|---------------|
| `matches()` | The **entire** input matches |
| `find()` | A match exists **anywhere**; call repeatedly to get successive matches |
| `lookingAt()` | The match starts at the **beginning** (but need not cover everything) |

```java
Pattern.compile("\\d+").matcher("abc123").matches();     // false
Pattern.compile("\\d+").matcher("abc123").find();        // true
```

### Finding all matches

```java
Matcher m = Pattern.compile("\\d+").matcher("a1 b22 c333");
while (m.find()) {
    System.out.println(m.group());          // 1, 22, 333
}

List<String> numbers = Pattern.compile("\\d+").matcher("a1 b22 c333")
        .results()                          // Java 9+: Stream<MatchResult>
        .map(MatchResult::group)
        .toList();
```

### Flags

```java
Pattern.compile("hello", Pattern.CASE_INSENSITIVE);
Pattern.compile("^\\w+$", Pattern.MULTILINE);       // ^ and $ match at each line
Pattern.compile("a.b", Pattern.DOTALL);             // . also matches line breaks
Pattern.compile("""
        \\d{3}     # area code
        -
        \\d{4}     # number
        """, Pattern.COMMENTS);                     // whitespace and # comments ignored
```

Inline form: `(?i)hello`. Add `Pattern.UNICODE_CASE` for non-ASCII case-insensitivity.

## Replacing

```java
"John Smith".replaceAll("(\\w+) (\\w+)", "$2, $1");      // "Smith, John"  ($1, $2 are groups)
"a  b   c".replaceAll("\\s+", " ");                       // "a b c"
"x".replaceAll("x", Matcher.quoteReplacement("$5"));      // "$5": escape $ and \ in replacements

// Computed replacement
Pattern.compile("\\d+").matcher("a1 b22")
       .replaceAll(r -> String.valueOf(Integer.parseInt(r.group()) * 2));   // "a2 b44"
```

### Treating text literally

```java
Pattern.quote("1+1=2");                              // \Q1+1=2\E
Pattern.compile(Pattern.quote(userInput));           // user text, no regex meaning
text.replace("a.b", "x");                            // replace() is already literal
```

Always quote untrusted text that you embed into a pattern.

## Useful patterns

| Goal | Pattern |
|------|---------|
| Integer | `-?\d+` |
| Decimal | `-?\d+(\.\d+)?` |
| Whitespace-trim | `^\s+\|\s+$` |
| Simple identifier | `[A-Za-z_]\w*` |
| 24-hour time | `([01]\d\|2[0-3]):[0-5]\d` |
| Hex color | `#[0-9a-fA-F]{6}` |
| Basic email (lenient) | `^[\w.+-]+@[\w-]+(\.[\w-]+)+$` |
| Repeated word | `\b(\w+)\s+\1\b` |
| Digits with lookahead (password: at least one digit) | `(?=.*\d).{8,}` |

Regex validation of complex formats (email addresses, URLs, dates) is a trap: either too strict or too loose. Prefer a lenient shape check plus a real parser or confirmation (an email, `LocalDate.parse`, `URI`). See [10-date-and-time/04_date-time-formatting-and-parsing.md](../10-date-and-time/04_date-time-formatting-and-parsing.md).

## Performance and safety

### Compile once

```java
private static final Pattern WORDS = Pattern.compile("\\W+");   // reuse: Pattern is immutable and thread-safe
String[] parts = WORDS.split(text);
```

`Matcher` is not thread-safe: create one per use.

### Catastrophic backtracking (ReDoS)

Nested quantifiers on overlapping alternatives can take **exponential** time on certain inputs.

```java
Pattern.compile("(a+)+$").matcher("aaaaaaaaaaaaaaaaaaaaaaaaaaaa!").matches();   // practically hangs
```

An attacker who controls the input can freeze a thread. Defenses:

| Defense | How |
|---------|-----|
| Avoid nested or ambiguous quantifiers | `(a+)+`, `(.*)*`, `(a\|aa)+` |
| Use possessive quantifiers / atomic groups | `a++`, `(?>a+)` |
| Limit input length before matching | Reject huge strings early |
| Never compile user-supplied patterns without limits | Or use a timeout / non-backtracking engine |
| Test with long, near-matching inputs | Not just valid ones |

See [21-security/00_input-validation-and-injection.md](../21-security/00_input-validation-and-injection.md).

## When not to use regex

| Task | Better tool |
|------|-------------|
| Parse HTML, XML, JSON | A real parser ([17-json-and-data-formats](../17-json-and-data-formats/README.md)) |
| Nested or recursive structures | Parser or grammar |
| Simple prefix, suffix or contains checks | `startsWith`, `endsWith`, `contains` |
| Splitting on one literal character | `split(Pattern.quote(","))` or a plain-character split such as `split(",")` (fine, but remember it is still a regex) |
| Dates and numbers | `LocalDate.parse`, `Integer.parseInt` |
| CSV with quotes | A CSV library |

Test patterns interactively on [regex101.com](https://regex101.com) (choose the Java flavor) or in [JShell](../00-setup/03_jshell.md).

## Common mistakes

| Mistake | Symptom | Fix |
|---------|---------|-----|
| `"\d+"` in Java source | `illegal escape character` | `"\\d+"` |
| `"a.b".split(".")` | Empty result | `split("\\.")` |
| `matches()` expecting a partial match | `false` | `find()` or add `.*` |
| Calling `group()` before `find()`/`matches()` | `IllegalStateException: No match found` | Check the boolean result first |
| Using group numbers in complex patterns | Off-by-one after adding a group | Named groups |
| `replaceAll` with `$` or `\` in plain text | `IllegalArgumentException` | `Matcher.quoteReplacement` |
| Compiling in a loop | Slow | `static final Pattern` |
| Sharing a `Matcher` across threads | Corrupted results | One per thread |
| Greedy `.*` swallowing too much | Too-long matches | `.*?` or a specific class `[^>]*` |
| Validating emails or HTML with giant regexes | Wrong results, hard to maintain | Parser or simple check + verification |
| Nested quantifiers on user input | CPU exhaustion (ReDoS) | Simplify, possessive, length limits |
| Forgetting `CASE_INSENSITIVE` | Case-sensitive miss | Flag or `(?i)` |

## Key takeaways

- `Pattern.compile` once, `matcher(text)` per input; `Pattern` is thread-safe, `Matcher` is not
- Double every backslash in Java strings: `"\\d+"`
- `matches()` needs the whole string to match; `find()` searches
- Named groups, `results()` and `replaceAll(Function)` keep code readable
- Quote untrusted text, compile once, and watch for catastrophic backtracking
- Do not use regex to parse nested formats

**Next:** [Unicode and Encoding](./07_unicode-and-encoding.md)
