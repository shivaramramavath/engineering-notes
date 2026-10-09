# JSON Basics

**JSON** (JavaScript Object Notation) is a small, text-based format for structured data: objects, arrays, strings, numbers, booleans, and `null`. It's the default language of web APIs, many configuration files, and plenty of message queues. It's easy to read, easy to produce, and supported everywhere, which is also why its limits (no dates, no comments, ambiguous numbers) cause so many bugs.

**Prerequisites:** [Records](../12-modern-java/01_records.md) and basic familiarity with Java collections.

---

## 1. The format

```json
{
  "id": 42,
  "email": "asha@example.com",
  "active": true,
  "balance": 1234.50,
  "tags": ["admin", "beta"],
  "address": { "city": "Berlin", "zip": null },
  "createdAt": "2026-03-10T04:00:00Z"
}
```

There are exactly **six kinds of value**:

| JSON type | Example | Notes |
|---|---|---|
| **object** | `{"a": 1, "b": 2}` | Unordered set of `"key": value` pairs. Keys are strings |
| **array** | `[1, "two", null]` | Ordered list. Elements can be any type, mixed |
| **string** | `"hello"` | Double quotes only. Unicode, with backslash escapes |
| **number** | `42`, `-1.5`, `1e3` | One numeric type. No distinction between integer and floating point |
| **boolean** | `true`, `false` | Lowercase |
| **null** | `null` | An explicit "no value" |

### Strict syntax rules (they trip people constantly)

- **Double quotes** around strings *and* keys. `{'a': 1}` and `{a: 1}` are not JSON.
- **No trailing commas**: `[1, 2,]` is invalid.
- **No comments** (`//` or `/* */`), which is a real limitation for config files.
- **No `undefined`, `NaN`, `Infinity`**, hexadecimal numbers, or leading `+`/leading zeros (`007`).
- Strings must escape `"`, `\`, and control characters (`\n`, `\t`, `\u0000`..). A raw newline inside a string is invalid.
- The text is **UTF-8** (RFC 8259). Use `application/json` as the media type. A `charset` parameter isn't needed.
- Whitespace between tokens is insignificant, so pretty-printed and minified JSON are equivalent.

Duplicate keys in one object (`{"a":1,"a":2}`) are legal syntax but **undefined behavior**: parsers may keep the first, the last, or fail. Treat them as an error.

---

## 2. What JSON can't express (and how people work around it)

JSON has no native type for many things you need constantly:

| Need | Convention | Pitfall |
|---|---|---|
| **Date/time** | ISO-8601 string: `"2026-03-10T04:00:00Z"` | Epoch numbers are ambiguous (seconds vs milliseconds). Zone-less strings are ambiguous ([Date-Time Best Practices](../10-date-and-time/05_date-time-best-practices.md)) |
| **Binary data** | Base64 string | ~33% bigger, so don't embed large files |
| **Money / exact decimals** | String (`"19.99"`) or integer minor units (`1999`) | Numbers parsed as `double` lose exactness ([Numeric Precision](../01-fundamentals/08_numeric-precision-and-math.md)) |
| **Large integers** | String for IDs beyond 2⁵³ | JavaScript numbers are doubles. `9007199254740993` silently becomes `9007199254740992` in a browser |
| **Enums** | String name | Unknown values from newer clients/servers must be handled |
| **Missing vs null** | Two different things (absent key vs `null` value) | Matters for partial updates ([Patterns](03_json-patterns-and-pitfalls.md#2-null-absent-and-empty)) |
| **Comments / metadata** | None | Use a different format for config (YAML/TOML/properties) |
| **Schema** | Separate **JSON Schema** documents | JSON itself is schemaless |
| **Ordering of keys** | Not significant | Don't rely on it, even though libraries often preserve it |

**Numbers deserve special care.** The spec doesn't limit size or precision, so what you get depends on the parser: `double` in JavaScript, `BigDecimal`/`long`/`double` in Java depending on library settings. `1.0` and `1` may deserialize to different Java types. Agree on representation (strings for money and big IDs) in your API contract.

---

## 3. Three ways to process JSON

Every Java JSON library offers some mix of these three models. Choosing the right one is a design decision:

```text
 1. Data binding     JSON ⇄ your Java classes        ObjectMapper.readValue(json, User.class)
 2. Tree model       JSON ⇄ generic node tree        JsonNode root = mapper.readTree(json)
 3. Streaming        a stream of tokens, one at a time   JsonParser / JsonGenerator
```

| Model | Memory | Convenience | Speed | Use when |
|---|---|---|---|---|
| **Data binding** | Whole object in memory | **Highest**: typed and validated by the compiler | Good | The structure is known and reasonably sized: **the default choice** |
| **Tree model** | Whole tree in memory | Medium: untyped navigation | Slower | Structure is dynamic or partially known, you need to inspect/modify fields without a class, or pass-through transformation |
| **Streaming** | **Constant**: one token at a time | Lowest: you manage state | **Fastest** | Huge documents/arrays, memory limits, or when you only need a few fields |

```java
// Data binding (Jackson): typed
User user = mapper.readValue(json, User.class);

// Tree model: dynamic
JsonNode root = mapper.readTree(json);
String city = root.path("address").path("city").asText("unknown");

// Streaming: token by token (sketch)
try (JsonParser p = factory.createParser(inputStream)) {
    while (p.nextToken() != null) {
        if (p.currentToken() == JsonToken.FIELD_NAME && "email".equals(p.currentName())) { ... }
    }
}
```

Details for Jackson are in [Jackson](01_jackson.md). Gson offers the same three in its own API ([Gson](02_gson.md)).

---

## 4. Mapping JSON to Java

How the pieces correspond (by default, with Jackson-style binding):

| JSON | Java |
|---|---|
| object | a class or record (fields/properties), or `Map<String, Object>` |
| array | `List<T>`, `T[]`, `Set<T>` |
| string | `String`, enum, `UUID`, `java.time` types (with support) |
| number | `int`/`long`/`double`/`BigDecimal`/`BigInteger` (depends on the target type and settings) |
| boolean | `boolean` / `Boolean` |
| null | `null` (or an absent `Optional`) |

A record maps cleanly onto an object:

```java
public record User(long id, String email, boolean active, List<String> tags, Address address, Instant createdAt) {}
public record Address(String city, String zip) {}
```

Without a target type, a parser produces **generic** structures: `Map<String, Object>`, `List<Object>`, `Integer`/`Long`/`Double`. These lose type information and encourage casting, so prefer typed classes whenever the shape is known.

---

## 5. The Java landscape

The JDK itself doesn't include a standard JSON API in the Java releases these notes cover (check your JDK's release notes). You use a library:

| Library | Style | Notes |
|---|---|---|
| **Jackson** | Binding + tree + streaming; huge module ecosystem | The de-facto standard. Default in Spring. [Note 01](01_jackson.md) |
| **Gson** | Binding + tree + streaming | Simple API from Google. Common in Android and older code. [Note 02](02_gson.md) |
| **Jakarta JSON Binding (JSON-B)** / **JSON-P** | Standard APIs (Jakarta EE) with implementations like Yasson/Parsson | Used in Jakarta EE servers. JSON-B for binding, JSON-P for processing/streaming |
| **Moshi** | Binding, Kotlin-friendly | Popular on Android/Kotlin |
| **org.json**, **JSON-java** | Tiny tree-style API | Simple scripts; limited binding |
| **Jackson/JSON-B alternatives** (DSL-JSON, Jsoniter, others) | Performance-oriented | Consider when benchmarks show Jackson is a bottleneck |

Related standards worth knowing: **JSON Schema** (validation), **JSON Pointer** (RFC 6901, addressing: `/address/city`), **JSON Patch** (RFC 6902) and **JSON Merge Patch** (RFC 7396) for partial updates, **JSON Lines / NDJSON** (one JSON value per line, great for logs and streaming big datasets), and **RFC 9457 Problem Details** for HTTP error bodies.

---

## 6. Reading and writing correctly

- **Always use UTF-8** when converting between bytes and text, and prefer APIs that take `InputStream`/`OutputStream` (the parser handles the encoding) over decoding to a `String` yourself ([I/O Streams](../11-io-and-networking/00_io-streams-readers-writers.md#6-charsets-the-usual-source-of-garbled-text)).
- **Never build JSON by string concatenation.** Quotes, backslashes, and control characters in values will break the output or enable injection:

```java
// WRONG: a name containing a quote produces invalid JSON (or lets an attacker add fields)
String json = "{\"name\": \"" + name + "\"}";

// RIGHT: let the library escape
String json = mapper.writeValueAsString(Map.of("name", name));
```

  (Text blocks are fine for *test fixtures* with fixed content: [Text Blocks](../03-strings-and-text/05_text-blocks.md).)
- **Stream big payloads.** Parsing a 2 GB array into a `List` means `OutOfMemoryError` ([File Processing Patterns](../11-io-and-networking/02_file-processing-patterns.md)).
- **Pretty-print only for humans.** Minified JSON is smaller on the wire.
- For HTTP: `Content-Type: application/json`, and an `Accept` header ([HTTP Client](../11-io-and-networking/04_http-client-and-networking.md)).

---

## Common mistakes

| Mistake | Fix |
|---|---|
| Treating JSON like a JavaScript object literal (single quotes, trailing commas, comments) | Follow the strict grammar, and validate with a real parser |
| Using `double` for money | `BigDecimal` in Java, string or integer cents in JSON |
| Sending 64-bit IDs as numbers to browsers | Send as strings |
| Epoch timestamps without a unit | ISO-8601 strings with a zone (`Z`) |
| Building JSON with `+` and `String.format` | Use a library or builder |
| Parsing everything into `Map<String, Object>` | Typed records, or `JsonNode` where truly dynamic |
| Loading huge arrays into memory | Streaming |
| Ignoring the difference between a missing field and `null` | Decide and document the contract |
| Relying on key order or duplicate keys | Don't |
| Comments in "JSON" config | Use YAML/TOML/properties, or a JSON dialect your parser explicitly supports |

### Debugging

- **Invalid JSON?** Paste it into a validator or `jq .`. Typical culprits: trailing comma, single quotes, an unescaped newline in a string, a BOM at the start, or an HTML error page returned where JSON was expected (check the HTTP status and `Content-Type` first).
- Numbers off by a few units in the browser → big integers lost precision as doubles.
- `null` where you expected a value → the field is missing, misspelled (case-sensitive), or named differently (`snake_case` vs `camelCase`).
- Garbled non-ASCII characters → wrong charset somewhere between bytes and text.

---

## Quick Summary

- JSON = objects, arrays, strings, numbers, booleans, `null`: strict syntax (double quotes, no trailing commas, no comments), UTF-8.
- It has **no types for dates, binary, money, or big integers**. Use conventions: ISO-8601 strings, Base64, strings for money and IDs above 2⁵³.
- Three processing models: **data binding** (default), **tree** (dynamic), **streaming** (huge/low-memory).
- Java has no standard built-in JSON API in these releases: use **Jackson** (dominant), Gson, or JSON-B/JSON-P.
- Never concatenate JSON by hand, always use UTF-8, and distinguish *absent* from *null*.

**Next:** [Jackson](01_jackson.md)
