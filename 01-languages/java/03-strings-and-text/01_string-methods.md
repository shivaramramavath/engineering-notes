# String Methods

A working reference for the `String` API, grouped by purpose. Remember: strings are immutable, so every method that "changes" a string returns a **new** one ([string basics](./00_string-basics.md)).

```java
String s = "  Hello, World  ";
```

## Inspect

| Method | Result for `"Hello, World"` | Notes |
|--------|----------------------------|-------|
| `length()` | `12` | Number of UTF-16 code units |
| `charAt(i)` | `charAt(0)` → `'H'` | `StringIndexOutOfBoundsException` if out of range |
| `isEmpty()` | `false` | `length() == 0` |
| `isBlank()` | `false` | Empty or whitespace only (11+) |
| `indexOf("o")` | `4` | First occurrence, `-1` if absent |
| `indexOf("o", 5)` | `8` | Search from index 5 |
| `lastIndexOf("o")` | `8` | Last occurrence |
| `contains("World")` | `true` | Substring test |
| `startsWith("He")` | `true` | `startsWith(prefix, offset)` also exists |
| `endsWith("ld")` | `true` | |
| `matches("[A-Z].*")` | `true` | **Entire** string must match a regex ([regex](./06_regex.md)) |

```java
if (email.indexOf('@') < 0) { ... }        // -1 means "not found"
if (path.endsWith(".java")) { ... }
```

## Extract

```java
String s = "Hello, World";
s.substring(7);          // "World"        from index 7 to the end
s.substring(0, 5);       // "Hello"        end index is EXCLUSIVE
s.substring(3, 3);       // ""             empty
s.substring(0, 20);      // StringIndexOutOfBoundsException
```

Length of `substring(a, b)` is `b - a`.

### `split`

```java
"a,b,c".split(",");                 // ["a", "b", "c"]
"a, b ,c".split("\\s*,\\s*");       // ["a", "b", "c"]  (regex)
"a,b,,".split(",");                 // ["a", "b"]       trailing empty strings REMOVED
"a,b,,".split(",", -1);             // ["a", "b", "", ""]  keep them
"a,b,c".split(",", 2);              // ["a", "b,c"]     at most 2 parts
"a b   c".split("\\s+");            // ["a", "b", "c"]
" a b".split(" ");                  // ["", "a", "b"]   leading empty string stays
"abc".split("");                    // ["a", "b", "c"]
```

`split` takes a **regular expression**, so special characters must be escaped:

```java
"a.b.c".split(".");                 // []  (. matches any character, all pieces empty)
"a.b.c".split("\\.");               // ["a", "b", "c"]
"a|b|c".split("|");                 // ["a","|","b","|","c"]  (empty regex alternative)
"a|b|c".split("\\|");               // ["a", "b", "c"]
"a|b|c".split(Pattern.quote("|"));  // ["a", "b", "c"]  (literal)
```

### Lines and characters

```java
"line1\nline2\r\nline3".lines().toList();      // [line1, line2, line3]   (11+)
"hello".chars().filter(c -> c == 'l').count(); // 2
"hello".toCharArray();                         // char[]
```

## Transform

| Method | Example | Result |
|--------|---------|--------|
| `toUpperCase()` / `toLowerCase()` | `"Ab".toUpperCase()` | `"AB"` |
| `trim()` | `"  hi  ".trim()` | `"hi"` (removes chars ≤ `' '`) |
| `strip()` | `"\u2003hi\u2003".strip()` | `"hi"` (Unicode-aware, 11+) |
| `stripLeading()` / `stripTrailing()` | `"  hi ".stripLeading()` | `"hi "` |
| `replace(a, b)` | `"a-b-c".replace("-", "+")` | `"a+b+c"` (**literal**, all occurrences) |
| `replace(char, char)` | `"aXb".replace('X', '_')` | `"a_b"` |
| `replaceAll(regex, r)` | `"a1b22".replaceAll("\\d+", "#")` | `"a#b#"` |
| `replaceFirst(regex, r)` | `"a1b2".replaceFirst("\\d", "#")` | `"a#b2"` |
| `repeat(n)` | `"ab".repeat(3)` | `"ababab"` |
| `concat(s)` | `"a".concat("b")` | `"ab"` (same as `+`) |
| `indent(n)` | `"a\nb".indent(2)` | `"  a\n  b\n"` (12+) |
| `formatted(args...)` | `"%d items".formatted(3)` | `"3 items"` (15+) |

```java
String clean = input.strip().toLowerCase(Locale.ROOT);
String slug = title.toLowerCase(Locale.ROOT).replaceAll("[^a-z0-9]+", "-");
```

### `replace` vs `replaceAll`

```java
"a.b.c".replace(".", "-");          // "a-b-c"   (literal)
"a.b.c".replaceAll(".", "-");       // "-----"   (regex: . matches everything)
"a.b.c".replaceAll("\\.", "-");     // "a-b-c"

"cost: $5".replaceAll("\\$", "USD ");        // "cost: USD 5"
"x".replaceAll("x", "$");                    // IllegalArgumentException: $ is special in the replacement
"x".replaceAll("x", Matcher.quoteReplacement("$"));   // "$"
```

### `trim` vs `strip`

`trim()` removes only characters up to U+0020 (ASCII control and space). `strip()` removes all Unicode whitespace as defined by `Character.isWhitespace`. Prefer `strip()` in new code.

### Case conversion and locales

```java
"TITLE".toLowerCase(Locale.forLanguageTag("tr-TR"));    // "tıtle" (dotless i!)
"TITLE".toLowerCase(Locale.ROOT);                       // "title"
```

The no-argument `toLowerCase()`/`toUpperCase()` use the **default locale**. For identifiers, protocol keywords and comparisons, pass `Locale.ROOT`. See [07_unicode-and-encoding.md](./07_unicode-and-encoding.md).

## Join

```java
String.join(", ", "a", "b", "c");                   // "a, b, c"
String.join("-", List.of("x", "y", "z"));           // "x-y-z"
List.of(1, 2, 3).stream()
    .map(String::valueOf)
    .collect(Collectors.joining(", ", "[", "]"));   // "[1, 2, 3]"
```

For building incrementally see [StringBuilder and StringJoiner](./03_stringbuilder-and-stringbuffer.md).

## Compare

```java
"abc".equals("abc");                    // true
"abc".equalsIgnoreCase("ABC");          // true
"apple".compareTo("banana");            // negative (a < b)
"b".compareTo("a");                     // positive
"abc".compareToIgnoreCase("ABD");       // negative
"abc".contentEquals(new StringBuilder("abc"));   // true
"Hello".regionMatches(true, 1, "ELL", 0, 3);     // true (ignore case)
```

Details and the `==` trap: [02_string-pool-and-comparison.md](./02_string-pool-and-comparison.md).

## Convert

| Method | Purpose |
|--------|---------|
| `toCharArray()` | `char[]` copy |
| `getBytes(StandardCharsets.UTF_8)` | `byte[]` in an explicit charset |
| `String.valueOf(x)` | Any value to string |
| `String.copyValueOf(chars)` | From a `char[]` |
| `chars()` / `codePoints()` | `IntStream` of characters / code points |
| `intern()` | Canonical pooled instance |

## Quick recipes

```java
// Reverse
new StringBuilder(s).reverse().toString();

// Palindrome (ignoring case, non-letters)
String t = s.toLowerCase(Locale.ROOT).replaceAll("[^a-z0-9]", "");
boolean pal = new StringBuilder(t).reverse().toString().equals(t);

// Count a character
long count = s.chars().filter(c -> c == 'a').count();

// Capitalize the first letter
String cap = s.isEmpty() ? s : Character.toUpperCase(s.charAt(0)) + s.substring(1);

// Word frequency
Map<String, Long> freq = Arrays.stream(text.toLowerCase(Locale.ROOT).split("\\W+"))
    .filter(w -> !w.isEmpty())
    .collect(Collectors.groupingBy(w -> w, Collectors.counting()));

// Check all digits
boolean digits = !s.isEmpty() && s.chars().allMatch(Character::isDigit);

// Pad / truncate: see formatting
String padded = String.format("%-10s|", s);
```

## Which method do I need?

| I want to... | Use |
|--------------|-----|
| Check for a substring | `contains`, `indexOf` |
| Check start or end | `startsWith`, `endsWith` |
| Get part of a string | `substring` |
| Break into pieces | `split`, `lines`, `Scanner` |
| Glue pieces | `String.join`, `StringBuilder`, `Collectors.joining` |
| Replace literal text | `replace` |
| Replace by pattern | `replaceAll`, `Pattern`/`Matcher` |
| Remove surrounding whitespace | `strip` |
| Normalize case for comparison | `equalsIgnoreCase`, or `toLowerCase(Locale.ROOT)` |
| Check "empty or whitespace" | `isBlank` |
| Insert values into a template | `formatted`, `String.format` |
| Process characters | `toCharArray`, `chars()` |

## Common mistakes

| Mistake | Symptom | Fix |
|---------|---------|-----|
| Ignoring the return value | No change | Assign the result |
| `substring(a, b)` with `b` as a length | Wrong or exception | `b` is an end **index**, exclusive |
| `split(".")` or `split("|")` | Empty or odd arrays | Escape or `Pattern.quote` |
| Expecting trailing empty strings from `split` | They vanish | `split(regex, -1)` |
| `replaceAll` with plain text containing `.`, `$`, `\` | Wrong result or exception | `replace` or `quoteReplacement` |
| `trim()` for Unicode whitespace | Spaces remain | `strip()` |
| `toLowerCase()` without a locale on keywords | Fails on Turkish systems | `Locale.ROOT` |
| `indexOf` result used as a boolean | `-1` is truthy-looking | Compare with `< 0` / `>= 0` |
| `matches` expecting partial match | Always `false` for partial | Use `find()` or `.*` ([regex](./06_regex.md)) |
| Calling methods on `null` | `NullPointerException` | Null-check first |
| Compiling a regex on every `split`/`replaceAll` call in hot loops | Slow | Precompile a `Pattern` |

## Key takeaways

- Strings are immutable: always use the returned value
- `substring` end is exclusive; `indexOf` returns `-1` when absent
- `split`, `replaceAll` and `matches` take **regexes**; `replace` is literal
- Use `strip`, `isBlank`, `repeat`, `lines`, `formatted` from modern Java
- Pass `Locale.ROOT` when changing case for non-display purposes

**Next:** [String Pool and Comparison](./02_string-pool-and-comparison.md)
