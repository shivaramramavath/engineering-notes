# Unicode and Encoding

Text has three layers that are easy to confuse:

```
 Characters (what humans see)         "é"   "€"   "😀"
        │  Unicode assigns each one a number: the CODE POINT
        ▼
 Code points                          U+00E9   U+20AC   U+1F600
        │  an ENCODING turns code points into bytes (UTF-8, UTF-16, ...)
        ▼
 Bytes (what files and networks carry)
```

In memory, Java strings are sequences of **UTF-16 code units** (`char`). On disk and on the wire, text is **bytes** in some charset. Bugs appear when these layers are mixed up.

## Unicode, code points, `char`

- **Unicode** assigns every character a **code point** from `U+0000` to `U+10FFFF`
- A Java `char` is 16 bits, so it can hold code points up to `U+FFFF` (the Basic Multilingual Plane)
- Code points above `U+FFFF` (emoji, many historic scripts, some CJK) are stored as a **surrogate pair**: two `char`s

```java
String s = "😀";                          // U+1F600
s.length();                               // 2     two UTF-16 code units
s.codePointCount(0, s.length());          // 1     one code point
s.charAt(0);                              // '\uD83D' (high surrogate: not a real character)
s.codePointAt(0);                         // 128512 (0x1F600)

String t = "A€";
t.length();                               // 2
t.codePointCount(0, t.length());          // 2
```

| API | Counts / yields |
|-----|------------------|
| `length()`, `charAt`, `chars()` | UTF-16 code units |
| `codePointCount`, `codePointAt`, `codePoints()` | Code points |

```java
"héllo 😀".chars().count();               // 8  (code units)
"héllo 😀".codePoints().count();          // 7  (code points)

// Safe iteration over real characters
"héllo 😀".codePoints().forEach(cp ->
    System.out.println(new String(Character.toChars(cp))));
```

### Working with code points

```java
Character.isLetter('é');                  // true
Character.isLetter(0x1F600);              // false: the int overload takes code points
Character.charCount(0x1F600);             // 2: number of chars needed
Character.toChars(0x1F600);               // char[] of length 2
new StringBuilder().appendCodePoint(0x1F600);
Character.getName(0x1F600);               // "GRINNING FACE"
```

`StringBuilder.reverse()` treats surrogate pairs as one character, but reversing with a manual `char` loop breaks them.

### Grapheme clusters

What a user sees as **one character** may be several code points: `é` written as `e` + a combining accent, a flag (two regional indicators), a family emoji (several emoji joined by zero-width joiners).

```java
String family = "👨‍👩‍👧";
family.codePointCount(0, family.length());   // 5 code points, one visible symbol
```

For user-perceived characters use `BreakIterator.getCharacterInstance()` (or a library such as ICU4J). Truncating a string at `n` chars can cut an emoji or accent in half.

## Unicode escapes in source code

```java
char c = '\u0041';                  // 'A'
String s = "caf\u00e9";             // "café"
```

`\uXXXX` escapes are translated **before** the compiler parses the source, even inside comments. A comment such as `// path C:\users` is a compile error (`\u` followed by non-hex digits), and `// \u000a` ends the comment line. Avoid `\u` outside literals.

## Normalization

The same visible text can have different code point sequences:

```java
String composed   = "\u00e9";           // é as one code point
String decomposed = "e\u0301";          // e + combining acute accent
composed.equals(decomposed);            // false
composed.length();                      // 1
decomposed.length();                    // 2

Normalizer.normalize(decomposed, Normalizer.Form.NFC).equals(composed);   // true
```

| Form | Meaning | Use for |
|------|---------|---------|
| `NFC` | Composed | Storing and comparing text (the usual default) |
| `NFD` | Decomposed | Stripping accents |
| `NFKC` / `NFKD` | Compatibility forms (fold ligatures, full-width letters, etc.) | Search keys, identifiers, usernames |

```java
// Remove accents
String plain = Normalizer.normalize("café", Normalizer.Form.NFD)
        .replaceAll("\\p{M}", "");      // "cafe"
```

Normalize **before** comparing, hashing or storing user-supplied text such as usernames, to avoid duplicate-looking identities.

## Encodings (charsets)

| Charset | Bytes per code point | Notes |
|---------|----------------------|-------|
| ASCII | 1 | 128 characters, English only |
| ISO-8859-1 (Latin-1) | 1 | Western European, first 256 code points |
| **UTF-8** | 1-4 | The web and modern default; ASCII-compatible |
| UTF-16 | 2 or 4 | Java's in-memory form; has byte-order variants (BE/LE) and a BOM |
| Windows-1252 | 1 | Windows legacy, similar to Latin-1 |

UTF-8 sizes:

```java
"A".getBytes(StandardCharsets.UTF_8).length;     // 1
"é".getBytes(StandardCharsets.UTF_8).length;     // 2
"€".getBytes(StandardCharsets.UTF_8).length;     // 3
"😀".getBytes(StandardCharsets.UTF_8).length;    // 4
```

Three different "lengths" of the same text are common: `length()` (UTF-16 units), `codePointCount` (code points) and `getBytes(...).length` (bytes). Database column limits and protocol headers usually count **bytes**.

## Converting between `String` and bytes

```java
byte[] bytes = "café".getBytes(StandardCharsets.UTF_8);
String back  = new String(bytes, StandardCharsets.UTF_8);       // "café"
```

**Always pass the charset explicitly**, using `StandardCharsets` constants (no checked exception, no typos).

```java
"café".getBytes();                                  // uses the default charset
new String(bytes, "UTF-8");                         // string name: throws a checked UnsupportedEncodingException
```

### Mojibake: decoding with the wrong charset

```java
byte[] utf8 = "café".getBytes(StandardCharsets.UTF_8);
new String(utf8, StandardCharsets.ISO_8859_1);      // "cafÃ©"   garbled
new String(utf8, StandardCharsets.US_ASCII);        // "caf??"   undecodable bytes replaced by U+FFFD
```

The `\uFFFD` replacement character (�) in output is a sign that something decoded bytes with the wrong charset. The information is often already lost: fix the **decoding step**, not the string.

### Strict decoding

`String` constructors silently replace bad bytes. To fail instead:

```java
CharsetDecoder decoder = StandardCharsets.UTF_8.newDecoder()
        .onMalformedInput(CodingErrorAction.REPORT)
        .onUnmappableCharacter(CodingErrorAction.REPORT);
String text = decoder.decode(ByteBuffer.wrap(bytes)).toString();   // CharacterCodingException on bad input
```

## The default charset

| Java version | Default charset for APIs like `new FileReader`, `getBytes()` |
|--------------|---------------------------------------------------------------|
| 17 and earlier | Platform-dependent (often Windows-1252 on Windows, UTF-8 on Linux/macOS) |
| **18 and later** | **UTF-8** on every platform (JEP 400) |

Even with Java 18+, **still state the charset explicitly**: it documents intent, and code may run on older runtimes. Check the current default with `Charset.defaultCharset()`.

Note that the console and some OS interfaces may use a different charset (`stdout.encoding`, `native.encoding`), so non-ASCII output can still look wrong in some Windows terminals even when your program is correct.

## Where encodings must be specified

| Place | How |
|-------|-----|
| Reading and writing text files | `Files.readString(path, UTF_8)` (UTF-8 is the default for the `Files` text methods), `new InputStreamReader(in, UTF_8)`, `new FileReader(file, UTF_8)` (11+) |
| `String` ↔ `byte[]` | `getBytes(UTF_8)`, `new String(bytes, UTF_8)` |
| Source files | `javac -encoding UTF-8`; Maven `<project.build.sourceEncoding>UTF-8</project.build.sourceEncoding>` |
| HTTP | `Content-Type: text/plain; charset=UTF-8`; use `HttpResponse.BodyHandlers.ofString(UTF_8)` ([11-io-and-networking](../11-io-and-networking/04_http-client-and-networking.md)) |
| JSON | UTF-8 by specification |
| Databases | Database, table and connection charset (for example MySQL `utf8mb4`, not `utf8`) |
| Properties and resource bundles | UTF-8 supported since Java 9 |

See [11-io-and-networking/00_io-streams-readers-writers.md](../11-io-and-networking/00_io-streams-readers-writers.md).

## Case, locales and comparisons

```java
"İ".toLowerCase(Locale.ROOT);                       // "i̇" (i + combining dot): not a 1:1 mapping
"straße".toUpperCase(Locale.ROOT);                  // "STRASSE": length changes
"TITLE".toLowerCase(Locale.forLanguageTag("tr"));   // "tıtle"
```

- Case conversion can change a string's length and depends on the locale
- Use `Locale.ROOT` for non-display purposes (keys, identifiers, protocol tokens)
- Use `Collator` for sorting text shown to users ([string pool and comparison](./02_string-pool-and-comparison.md))
- `equalsIgnoreCase` is fine for simple cases, but not a full Unicode comparison; normalize first if it matters

## Practical rules

| Rule | Why |
|------|-----|
| Use UTF-8 everywhere (files, DB, network, source) | One encoding to reason about |
| Always pass `StandardCharsets.UTF_8` explicitly | Independent of platform defaults |
| Never convert between `String` and `byte[]` without a charset | Hidden platform dependence |
| Do not treat `char` as a character | Emoji and many scripts need two `char`s |
| Count and truncate by code points (or graphemes), not `char`s | Avoid cutting characters in half |
| Normalize (NFC) text before comparing or storing | `é` has two representations |
| Measure limits in the unit the limit uses | Bytes for column and header sizes |
| Test with non-ASCII data (`é`, `ß`, `İ`, `日本語`, `😀`) | Many bugs hide in ASCII-only tests |

## Common mistakes

| Mistake | Symptom | Fix |
|---------|---------|-----|
| `getBytes()` / `new String(bytes)` without a charset | Different output per machine | Pass `StandardCharsets.UTF_8` |
| Using `length()` as the character count | Wrong for emoji and rare scripts | `codePointCount` (or graphemes) |
| Truncating with `substring` at a surrogate pair | Broken character (`?` or `�`) | Cut at code point boundaries |
| Output like `cafÃ©` | UTF-8 decoded as Latin-1 | Decode with UTF-8 |
| `?` or `�` for non-ASCII output | Wrong or missing encoding on output | Check the writer, terminal and database charset |
| Source file saved in a legacy encoding | Garbled string literals | Save as UTF-8 and set `-encoding UTF-8` |
| MySQL `utf8` instead of `utf8mb4` | Emoji fail to store | `utf8mb4` |
| Comparing accented text without normalization | `equals` says different | `Normalizer` NFC |
| `toUpperCase()` / `toLowerCase()` on identifiers with the default locale | Breaks under the Turkish locale | `Locale.ROOT` |
| Looping `char` by `char` to reverse text | Corrupts surrogate pairs | `StringBuilder.reverse()` or code points |
| Writing `\u` in comments | Compile error | Avoid it |

## Key takeaways

- Java strings are UTF-16: `char` is a code unit, not necessarily a whole character
- Use code point APIs when text may contain emoji or rare scripts; use `BreakIterator` for what users see as one character
- Bytes need a charset: always pass `StandardCharsets.UTF_8`
- Java 18+ defaults to UTF-8, but explicit is still better
- Normalize before comparing, and use `Locale.ROOT` for non-display case changes
- Test with non-ASCII data

**Next:** [04-oop](../04-oop/README.md)
