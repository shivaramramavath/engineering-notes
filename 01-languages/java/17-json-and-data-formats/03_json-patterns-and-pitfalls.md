# JSON Patterns and Pitfalls

Parsing JSON is easy. Keeping a JSON **API** working for years, across versions of clients and servers, without leaking data or corrupting numbers, is where the real bugs live. This note collects the patterns and traps that matter in production. Examples use Jackson; the ideas apply to any library.

**Prerequisites:** [JSON Basics](00_json-basics.md), [Jackson](01_jackson.md), [Records](../12-modern-java/01_records.md).

---

## 1. DTOs and compatibility

### Separate wire models from internal models

```text
 HTTP JSON  ⇄  API DTO (record)  ⇄  mapping code  ⇄  domain model / JPA entity
```

- **Never serialize your persistence entities or domain objects directly.** It couples the public contract to your database schema, leaks fields (password hashes, internal IDs), triggers lazy loading ([Cycles and entities](#6-cycles-and-entities)), and makes every refactor a breaking API change.
- Use **records as DTOs**, with Jackson annotations *on the DTOs only*. Map to and from domain objects explicitly (by hand, or with MapStruct-style generators).
- Use **separate request and response DTOs**. A request to create a user shouldn't contain `id` or `createdAt`, and a response shouldn't contain `password`.

### Be a tolerant reader, a conservative writer

The most important compatibility rule (Postel's law, applied to APIs):

- **Readers ignore fields they don't know.** Configure `FAIL_ON_UNKNOWN_PROPERTIES = false` on *clients* so a server that adds a field doesn't break them. Servers validating *inbound* requests may reasonably be stricter, but decide deliberately.
- **Writers add, never change.** Safe evolutions: add an optional field, add a new endpoint, add a new enum value (if clients tolerate it, see [section 4](#4-enums-and-unknown-values)).
- **Breaking changes:** removing or renaming a field, changing a type (number → string), changing the meaning of a value, making an optional field required, tightening validation. Do those only behind a **new version** (`/v2/...`, a media-type version, or a new field name with the old one deprecated for a period).
- Give new fields **defaults** so old clients' requests still deserialize. `@JsonAlias` lets you accept the old name while moving to a new one.

```java
public record CreateUser(
        String email,
        String name,
        @JsonAlias("nick") String nickname) {}      // accept the old field name during the transition
```

Add **contract tests** with saved sample payloads from older versions ([section 9](#9-testing)).

---

## 2. Null, absent, and empty

JSON distinguishes three states that Java usually collapses:

```json
{ "nickname": "Ash" }     // present, with a value
{ "nickname": null }      // present, explicitly null  → "clear it"
{ }                       // absent                    → "I'm not saying anything about it"
```

For plain reads these all become `null` in the Java field, and you can't tell them apart. That matters for **partial updates (PATCH)**: "set nickname to null" and "leave nickname alone" are different instructions.

### Handling PATCH correctly

Options:

```java
// 1. Read the raw tree and check presence
JsonNode patch = mapper.readTree(body);
if (patch.has("nickname")) {                                 // present…
    user.setNickname(patch.get("nickname").isNull() ? null : patch.get("nickname").asText());   // …null or value
}                                                             // absent → untouched

// 2. Use a tri-state type: libraries such as OpenAPI's jackson-databind-nullable provide JsonNullable<T>
//    (undefined / null / value). Check compatibility with your Jackson major version.

// 3. Standardize on JSON Merge Patch (RFC 7396): present+null = delete, absent = keep
```

### Empty vs null collections and strings

Decide a convention and document it. A common, safe one: **return `[]` for empty collections, never `null`**; accept both on input; and treat `""` vs `null` for strings consistently (normalize at the boundary). `@JsonInclude(NON_NULL)` (omit nulls) is fine for responses *if* clients handle absent fields. `NON_EMPTY` also drops empty lists and strings, which can surprise clients that expect `[]`.

### Primitives silently hide missing values

```java
public record Order(long id, int quantity) {}
// {"id": 1}  →  quantity == 0, and nobody knows it was missing
```

Use wrapper types (`Integer`) where absence matters and validate (`@NotNull`), or enable `FAIL_ON_NULL_FOR_PRIMITIVES`/`FAIL_ON_MISSING_CREATOR_PROPERTIES` in Jackson.

---

## 3. Dates, numbers, and money

### Dates and times

- Send **ISO-8601** strings, with a zone for moments: `"2026-03-10T04:00:00Z"` (`Instant`/`OffsetDateTime`), and `"2026-03-10"` for calendar dates (`LocalDate`) ([Date-Time Best Practices](../10-date-and-time/05_date-time-best-practices.md#5-interchange-apis-json-databases)).
- Avoid epoch numbers in public APIs (seconds? milliseconds?), and avoid zone-less local timestamps for events.
- Durations: ISO-8601 (`"PT30M"`) or a number with the unit in the field name (`timeoutSeconds`). Never an unlabelled `30`.
- Jackson 2.x needs `JavaTimeModule` and `WRITE_DATES_AS_TIMESTAMPS` disabled. Jackson 3 does this by default. If a service upgrades Jackson, **date output can change**, so test it.

### Numbers

- **Money:** use a **string** (`"19.99"`) or **integer minor units** (`1999` with a `currency`), plus `BigDecimal` in Java. Never `double` ([Numeric Precision](../01-fundamentals/08_numeric-precision-and-math.md)).
- **Large integers:** IDs above 2⁵³ (9,007,199,254,740,992), like Snowflake-style 64-bit IDs, **must be strings** for JavaScript clients, which parse numbers as doubles and silently corrupt the low digits.

```java
public record Order(@JsonFormat(shape = JsonFormat.Shape.STRING) long id, ...) {}   // "id": "9007199254740993"
```

- **`BigDecimal` output:** Jackson may write large or scaled values in scientific notation (`1E+3`). Enable `JsonGenerator.Feature.WRITE_BIGDECIMAL_AS_PLAIN` when clients expect plain digits. When *reading* untyped data, `USE_BIG_DECIMAL_FOR_FLOATS` avoids `double` rounding.
- `NaN` and `Infinity` aren't valid JSON, and libraries handle them inconsistently (strings, errors, or invalid output). Don't emit them: filter or map them to `null`.
- Integer vs decimal: `1` and `1.0` are the same JSON number but may bind differently. Fix your DTO field types and don't depend on the lexical form.

---

## 4. Enums and unknown values

Map enums by **name** (the default), never ordinal. The difficult part is evolution: a server adds `status: "REFUNDED"`, and an older client's enum doesn't have it.

```java
public enum OrderStatus {
    NEW, PAID, SHIPPED,
    @JsonEnumDefaultValue UNKNOWN          // requires READ_UNKNOWN_ENUM_VALUES_USING_DEFAULT_VALUE
}
```

Choices for the *reader*:

- map unknown values to a designated `UNKNOWN` (`@JsonEnumDefaultValue` with `DeserializationFeature.READ_UNKNOWN_ENUM_VALUES_USING_DEFAULT_VALUE`), or to `null` (`READ_UNKNOWN_ENUM_VALUES_AS_NULL`), and handle that explicitly in code,
- or keep the raw `String` in the DTO and convert in the mapping layer, where you decide what an unknown status means.

For the *server*, treat adding enum values as a potentially breaking change for strict clients, and document that clients must tolerate unknown values. If the enum's JSON representation should differ from its constant name, use `@JsonValue` on a method or field returning the stable external code, so refactoring Java names doesn't change the contract.

---

## 5. Polymorphism

When a field can hold different shapes, add a **discriminator** property naming the variant:

```json
{ "type": "card", "last4": "4242" }
{ "type": "bank", "iban": "DE89370400440532013000" }
```

```java
@JsonTypeInfo(use = JsonTypeInfo.Id.NAME, include = JsonTypeInfo.As.PROPERTY, property = "type")
@JsonSubTypes({
        @JsonSubTypes.Type(value = Payment.Card.class,         name = "card"),
        @JsonSubTypes.Type(value = Payment.BankTransfer.class, name = "bank")
})
public sealed interface Payment {
    record Card(String last4)         implements Payment {}
    record BankTransfer(String iban)  implements Payment {}
}
```

This pairs naturally with **sealed interfaces and records** ([Sealed Classes](../12-modern-java/02_sealed-classes.md)). After deserialization, branch with an exhaustive `switch` ([Pattern Matching](../12-modern-java/04_pattern-matching.md)):

```java
String label = switch (payment) {
    case Payment.Card c         -> "Card ending " + c.last4();
    case Payment.BankTransfer b -> "Transfer to " + b.iban();
};
```

**Security:** always use **`Id.NAME`** with an explicit list of subtypes. Never use `Id.CLASS`/`Id.MINIMAL_CLASS` or "default typing" with untrusted input. That lets the payload choose arbitrary Java classes, a known remote-code-execution vector ([Jackson: Security](01_jackson.md#11-security-and-limits), [Insecure Deserialization](../21-security/04_insecure-deserialization.md)).

Also decide what an **unknown `type`** does: fail (strict) or map to a fallback (`defaultImpl`) when new variants may appear.

---

## 6. Cycles and entities

Serializing JPA entities directly is a reliable way to hit two classic failures:

```java
class User  { @OneToMany(mappedBy = "user") List<Order> orders; }
class Order { @ManyToOne User user; }
// Serializing a User → its orders → each order's user → its orders → ... forever
```

```text
JsonMappingException: Infinite recursion (StackOverflowError)
No serializer found for class org.hibernate.proxy.pojo.bytebuddy.ByteBuddyInterceptor
LazyInitializationException: could not initialize proxy - no Session
```

The cycle and the lazy-loading proxy problems both come from exposing a persistence model ([From JDBC to ORM](../16-jdbc-and-databases/07_from-jdbc-to-orm.md#4-the-classic-traps)). The fix is **DTOs**: build exactly the shape you want to send, with no back-references. Annotations such as `@JsonManagedReference`/`@JsonBackReference`, `@JsonIgnore` on the back-link, or `@JsonIdentityInfo` break cycles, but keep the coupling problem, so treat them as stopgaps.

---

## 7. Security

| Risk | Mitigation |
|---|---|
| **Mass assignment / over-posting**: binding request JSON directly onto an entity lets a client set `isAdmin`, `role`, `balance` | Bind to **request DTOs** containing only allowed fields (an **allow-list**, not a blacklist of `@JsonIgnore`s) |
| **Unsafe polymorphic deserialization** | `Id.NAME` + explicit subtypes, no default typing ([section 5](#5-polymorphism)) |
| **Sensitive data exposure** in responses and logs | Response DTOs expose only intended fields. Never log full payloads with PII, tokens, or secrets |
| **Resource exhaustion**: huge bodies, deeply nested arrays, enormous numbers | Body size limits at the gateway/server, keep the parser's nesting/length limits (Jackson 2.15+ defaults), timeouts |
| **Injection into other systems**: JSON values reaching SQL, shell, or HTML | Parameterized queries ([Prepared Statements](../16-jdbc-and-databases/02_statements-and-prepared-statements.md)), output encoding, validation |
| **XSS via JSON embedded in HTML** (`<script>var data = {...}</script>`) | Escape `<`, `>`, `&`, and U+2028/2029 when inlining, or serve JSON separately with `Content-Type: application/json` and `X-Content-Type-Options: nosniff` |
| **Unvalidated input** | Validate after parsing ([next section](#8-errors-and-validation)) |
| Outdated parser libraries | Keep Jackson/Gson current ([Dependency Security](../21-security/05_dependency-security.md)) |

---

## 8. Errors and validation

### Treat malformed input as a client error

Parsing failures are the caller's fault. Return **400**, not 500, with a safe, structured body. **RFC 9457 Problem Details** is the standard shape:

```json
{
  "type": "https://api.example.com/problems/invalid-request",
  "title": "Invalid request",
  "status": 400,
  "detail": "Field 'email' must be a valid email address",
  "errors": [ { "field": "email", "message": "must be a valid email address" } ]
}
```

Don't pass raw library messages to clients. Jackson messages can reveal **internal class names** and structure (`Cannot construct instance of com.acme.internal.User...`). Log the detail, and return a sanitized message.

### Validate after parsing

Parsing only checks *syntax and types*, not business rules:

```java
public record CreateUser(
        @NotBlank @Email String email,
        @NotBlank @Size(max = 100) String name,
        @Min(0) @Max(150) Integer age) {}
// controller: @Valid @RequestBody CreateUser body  → Jakarta Bean Validation rejects bad values with a 400
```

- Jakarta Bean Validation covers field rules. Cross-field and business rules belong in the service layer.
- For **external or evolving contracts**, validate against a **JSON Schema** (libraries exist for Java), and publish the schema (or an OpenAPI document) so clients can generate code.
- With Gson (which never reports missing fields), post-parse validation is mandatory ([Gson](02_gson.md#6-pitfalls-that-differ-from-jackson)).

---

## 9. Testing

- **Round-trip tests:** `object → JSON → object` equals the original, catching missing annotations and creators.
- **Golden-file / contract tests:** keep sample payloads (current *and* older versions) in `src/test/resources` and assert that they still deserialize, and that serialization output matches the expected document. Libraries such as JSONAssert and JsonUnit compare JSON semantically (ignoring key order and whitespace, and with strict/lenient modes).
- **Test unknown and missing fields:** extra properties, missing optionals, nulls, empty arrays, unknown enum values, and bad types.
- **Use the production mapper configuration** in tests (build it from the same factory/bean). A test using `new ObjectMapper()` with different defaults proves nothing.
- **Upgrades:** when bumping Jackson (especially 2 → 3), run the contract tests. Many changes alter output *without compile errors* ([Jackson 3 changes](01_jackson.md#10-jackson-3-what-changed)).
- Use Java **text blocks** for readable inline fixtures ([Text Blocks](../03-strings-and-text/05_text-blocks.md)).

---

## 10. Performance and payload size

- **Reuse** the configured mapper and per-type readers/writers ([Jackson: Performance](01_jackson.md#12-performance)).
- **Stream** large collections instead of building the whole object graph in memory ([Jackson: Streaming](01_jackson.md#8-streaming)).
- **Paginate** list endpoints, and offer field selection for heavy resources, instead of returning everything.
- **Compress** responses (gzip/Brotli at the server/gateway), which usually dwarfs micro-optimizations of the JSON itself.
- Don't shorten field names to save bytes at the expense of readability. Compression handles repetition.
- Avoid parsing the same payload twice (for example `readTree` then `treeToValue`) in hot paths.
- If JSON parsing/size is measurably a bottleneck, consider a binary format for internal traffic ([Data Formats Comparison](04_data-formats-comparison.md)).

---

## Common mistakes

| Mistake | Fix |
|---|---|
| Exposing entities as the API model | DTOs (records) at the boundary |
| Binding request bodies straight onto entities | Request DTOs with an allow-list of fields (prevent mass assignment) |
| Failing on unknown fields in clients | Tolerant reader |
| Renaming/removing a field "because it's cleaner" | Add new, deprecate old, version the API |
| Treating `null`, absent, and `[]` as the same | Decide and document each, handle PATCH explicitly |
| Money as `double`; big IDs as JSON numbers | String or minor units; IDs as strings |
| Epoch numbers or zone-less timestamps | ISO-8601 with `Z`/offset |
| Enum ordinals; strict enums with no unknown-value plan | Names, `@JsonValue`, and an `UNKNOWN`/tolerant strategy |
| `Id.CLASS` / default typing for external input | `Id.NAME` with explicit subtypes |
| Returning library exception messages | Sanitized Problem Details |
| Logging full request/response bodies | Log IDs and shapes. Redact PII and secrets |
| Testing with a differently configured mapper | Share the production mapper config |
| No contract tests before a Jackson upgrade | Golden-file tests |

### Debugging checklist

- A field is `null`/`0`: check name case and naming strategy, `@JsonProperty`/`@SerializedName`, whether the key was *absent* vs `null`, and unknown-property settings.
- Dates wrong: zone handling, `Instant` vs local types, timestamps vs strings, Jackson 2 vs 3 defaults.
- Numbers off by small amounts in a browser: big integers as JSON numbers.
- `InvalidDefinitionException`/`MismatchedInputException`: the class shape doesn't match, there is no creator, or an empty body was sent.
- Infinite recursion or lazy-loading errors: you're serializing entities.
- Works in tests but fails in production: different mapper/config, library versions, or data shapes (older payloads).

---

## Quick Summary

- **DTOs, not entities**, on the wire. Separate request and response models.
- **Tolerant reader, conservative writer**: add optional fields, never break existing ones, and version breaking changes.
- **Null ≠ absent ≠ empty.** Handle PATCH deliberately (tree presence checks, `JsonNullable`, or JSON Merge Patch).
- Dates: **ISO-8601 with zone**. Money: **string/minor units**. Big IDs: **strings**. Enums: **by name**, with an unknown-value strategy.
- Polymorphism: a **discriminator** with **named subtypes**, never class-name typing for untrusted input.
- Avoid cycles and lazy-loading problems by not serializing entities.
- Security: allow-listed request DTOs, no unsafe typing, size/depth limits, no secrets in responses or logs.
- Return **400 with Problem Details** for bad input, validate after parsing, and protect compatibility with **contract tests**.

**Next:** [Data Formats Comparison](04_data-formats-comparison.md)
