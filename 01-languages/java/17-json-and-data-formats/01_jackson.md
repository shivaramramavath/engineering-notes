# Jackson

**Jackson** is the standard JSON library for Java. It converts objects to JSON and back (*data binding*), offers a generic tree model and a low-level streaming API, and has a large ecosystem of modules for other formats (XML, YAML, CSV, Protobuf, CBOR) that reuse the same annotations. Spring Boot auto-configures it, so most Java developers use it even without choosing it.

Two major lines coexist in 2026: **Jackson 2.x** (`com.fasterxml.jackson`), found in most existing code, and **Jackson 3.x** (`tools.jackson`, released October 2025), a deliberate breaking upgrade. This note teaches the shared concepts with Jackson 2.x syntax and marks what changes in 3 ([section 10](#10-jackson-3-what-changed)).

**Prerequisites:** [JSON Basics](00_json-basics.md), [Records](../12-modern-java/01_records.md), [Annotations](../13-advanced-language-features/00_annotations.md), [Type Erasure](../07-generics/03_type-erasure.md).

---

## 1. Setup

```xml
<!-- Jackson 2.x: databind brings core + annotations -->
<dependency>
    <groupId>com.fasterxml.jackson.core</groupId>
    <artifactId>jackson-databind</artifactId>
    <version><!-- 2.x release; better: import the jackson-bom --></version>
</dependency>
<!-- java.time support is a separate module in 2.x -->
<dependency>
    <groupId>com.fasterxml.jackson.datatype</groupId>
    <artifactId>jackson-datatype-jsr310</artifactId>
</dependency>
```

Import the **Jackson BOM** so all Jackson artifacts share one version ([Dependency Management](../18-build-and-dependencies/02_dependency-management.md)). Mixing versions of core, databind, and annotations is a classic source of `NoSuchMethodError` ([Class Loading](../15-jvm-internals/01_class-loading.md#5-the-errors-decoded)).

---

## 2. `ObjectMapper` basics

```java
ObjectMapper mapper = JsonMapper.builder()
        .addModule(new JavaTimeModule())                                    // java.time support (2.x)
        .disable(SerializationFeature.WRITE_DATES_AS_TIMESTAMPS)           // dates as ISO-8601 strings
        .disable(DeserializationFeature.FAIL_ON_UNKNOWN_PROPERTIES)        // tolerate extra fields
        .build();

// Java → JSON
String json = mapper.writeValueAsString(user);
String pretty = mapper.writerWithDefaultPrettyPrinter().writeValueAsString(user);
mapper.writeValue(new File("user.json"), user);                  // also OutputStream, Writer

// JSON → Java
User u1 = mapper.readValue(json, User.class);
User u2 = mapper.readValue(inputStream, User.class);              // prefer streams: Jackson handles UTF-8 decoding
User u3 = mapper.readValue(new File("user.json"), User.class);
```

Key facts:

- **Create one mapper and reuse it.** Constructing an `ObjectMapper` is expensive (it caches serializers and deserializers per type). After configuration, it is **thread-safe**. Share it as a singleton or Spring bean.
- **Configure before first use.** Changing configuration after the mapper has been used is not safe in 2.x. In 3.x mappers are immutable.
- In 2.x, read/write methods throw the **checked** `JsonProcessingException` (a subtype of `IOException`).

---

## 3. What gets mapped

### Records

```java
public record User(long id, String email, boolean active, List<String> tags, Instant createdAt) {}

User u = mapper.readValue(json, User.class);        // works out of the box (Jackson 2.12+)
String out = mapper.writeValueAsString(u);          // {"id":42,"email":"...", ...}
```

Records are the cleanest binding target: immutable, constructor-based, and no annotations are needed for simple cases.

### Classes (POJOs)

By default Jackson finds:

- **Serialization:** public getters (`getEmail()` → `"email"`, `isActive()` → `"active"`) and public fields.
- **Deserialization:** a **no-arg constructor** plus setters or fields, or a constructor/factory marked **`@JsonCreator`**.

```java
public final class Money {
    private final BigDecimal amount;
    private final String currency;

    @JsonCreator                                              // immutable class: construct via this
    public Money(@JsonProperty("amount") BigDecimal amount,
                 @JsonProperty("currency") String currency) {
        this.amount = amount; this.currency = currency;
    }
    public BigDecimal getAmount() { return amount; }
    public String getCurrency()   { return currency; }
}
```

Without parameter names available, Jackson can't match JSON keys to constructor parameters, hence `@JsonProperty` (or compiling with `-parameters` plus the parameter-names module in 2.x). Private fields without getters or setters are **not** mapped unless you change visibility (`@JsonAutoDetect`) or annotate them.

Unmapped JSON properties: by default Jackson 2.x **fails** on unknown keys (`UnrecognizedPropertyException`). See [section 5](#5-configuration-features).

---

## 4. Generics, collections, and `TypeReference`

Type erasure removes `List<User>` at runtime, so `readValue(json, List.class)` gives a list of `LinkedHashMap`s, not `User`s. Pass the full type:

```java
List<User> users = mapper.readValue(json, new TypeReference<List<User>>() {});
Map<String, User> byId = mapper.readValue(json, new TypeReference<Map<String, User>>() {});

// or build the type programmatically
JavaType t = mapper.getTypeFactory().constructCollectionType(List.class, User.class);
List<User> users2 = mapper.readValue(json, t);
```

The anonymous subclass `new TypeReference<...>() {}` works because generic *superclass* information survives in the class file ([Reflection](../13-advanced-language-features/01_reflection.md#5-generics-annotations-parameters-records-enums)).

---

## 5. Configuration features

The settings you'll actually touch:

| Feature | Effect |
|---|---|
| `DeserializationFeature.FAIL_ON_UNKNOWN_PROPERTIES` | Default **true** in 2.x. Disable to ignore extra fields (a *tolerant reader*, usually right for clients, see [Patterns](03_json-patterns-and-pitfalls.md#1-dtos-and-compatibility)) |
| `DeserializationFeature.FAIL_ON_NULL_FOR_PRIMITIVES` | Fail when `null` is mapped to a primitive instead of silently using `0` |
| `DeserializationFeature.READ_UNKNOWN_ENUM_VALUES_AS_NULL` (or `@JsonEnumDefaultValue`) | Survive new enum values |
| `DeserializationFeature.USE_BIG_DECIMAL_FOR_FLOATS` | Parse non-integers as `BigDecimal` in untyped/tree reads (no precision loss) |
| `SerializationFeature.INDENT_OUTPUT` | Pretty printing |
| `SerializationFeature.WRITE_DATES_AS_TIMESTAMPS` | Default **true** in 2.x. Disable to get ISO-8601 strings |
| `SerializationFeature.FAIL_ON_EMPTY_BEANS` | Error for classes with no properties |
| `JsonInclude` via `setSerializationInclusion(NON_NULL)` | Omit nulls globally |
| `MapperFeature.*` | Property detection and ordering rules |

### `java.time`

Dates are the most common Jackson 2.x stumbling block:

```java
// Without JavaTimeModule: InvalidDefinitionException: Java 8 date/time type `java.time.Instant` not supported by default
// With the module but WITHOUT disabling timestamps, Instant is written as a number (1773115200.000000000)
```

Register `JavaTimeModule` **and** disable `WRITE_DATES_AS_TIMESTAMPS` to get ISO-8601 (`"2026-03-10T04:00:00Z"`). In **Jackson 3** `java.time` support is built in and ISO-8601 strings are the default. See [Date-Time Best Practices](../10-date-and-time/05_date-time-best-practices.md#5-interchange-apis-json-databases) for which types to expose (`Instant`/`OffsetDateTime` for moments, `LocalDate` for dates).

---

## 6. Annotations

All from `com.fasterxml.jackson.annotation` (and the same package in Jackson 3):

```java
@JsonInclude(JsonInclude.Include.NON_NULL)                  // omit null fields in output
@JsonPropertyOrder({"id", "email", "name"})
public class UserDto {
    @JsonProperty("user_id") private long id;                // rename
    @JsonAlias({"mail", "e-mail"}) private String email;     // accept alternative input names
    @JsonIgnore private String passwordHash;                 // never (de)serialize
    @JsonFormat(shape = JsonFormat.Shape.STRING) private long bigId;                 // number → string
    @JsonFormat(pattern = "yyyy-MM-dd") private LocalDate birthday;                  // custom date pattern
}
```

| Annotation | Purpose |
|---|---|
| `@JsonProperty` | Name a property, or bind a constructor parameter |
| `@JsonIgnore`, `@JsonIgnoreProperties({"a","b"})`, `@JsonIgnoreProperties(ignoreUnknown = true)` | Exclude properties / tolerate unknown input per class |
| `@JsonInclude` | Control null/empty/default inclusion |
| `@JsonAlias` | Accept multiple input names |
| `@JsonCreator` | Mark the constructor/factory for deserialization |
| `@JsonValue` | Use one method's result as the whole JSON value (for example an enum's code) |
| `@JsonFormat` | Shape and patterns for dates and numbers |
| `@JsonNaming` | Naming strategy: `@JsonNaming(PropertyNamingStrategies.SnakeCaseStrategy.class)` |
| `@JsonUnwrapped` | Flatten a nested object into its parent |
| `@JsonAnyGetter` / `@JsonAnySetter` | Capture arbitrary extra properties in a `Map` |
| `@JsonTypeInfo` + `@JsonSubTypes` | Polymorphism with a type discriminator ([Patterns](03_json-patterns-and-pitfalls.md#5-polymorphism)) |
| `@JsonView` | Different views of the same class (public vs admin) |

Global alternative to annotating every class: configure the mapper (for example a snake_case naming strategy). To annotate a class you don't own (a library type), use a **mix-in**:

```java
abstract class ThirdPartyMixin { @JsonIgnore abstract String getSecret(); }
ObjectMapper mapper = JsonMapper.builder().addMixIn(ThirdPartyType.class, ThirdPartyMixin.class).build();
```

Annotations are a Jackson-specific coupling. In a layered design, keep them on **API DTOs** and not on domain/persistence entities ([Patterns](03_json-patterns-and-pitfalls.md#1-dtos-and-compatibility)).

---

## 7. The tree model

For dynamic or partially known JSON, read into a `JsonNode` tree:

```java
JsonNode root = mapper.readTree(json);

String city = root.path("address").path("city").asText("unknown");   // path(): safe navigation, never null
String city2 = root.at("/address/city").asText("unknown");           // JSON Pointer
int count = root.get("items").size();
for (JsonNode item : root.withArray("items")) { ... }

if (root.has("error")) { ... }
root.path("x").isMissingNode();                                      // key absent
root.path("x").isNull();                                             // key present, value null
```

Building JSON dynamically:

```java
ObjectNode out = mapper.createObjectNode();
out.put("status", "ok");
out.putArray("ids").add(1).add(2).add(3);
out.set("user", mapper.valueToTree(user));          // embed a bound object
String json = mapper.writeValueAsString(out);
```

Convert between models: `mapper.treeToValue(node, User.class)`, `mapper.convertValue(map, User.class)`. Tree models are convenient for gateways, "pass through the fields I don't understand", and probing a payload before choosing a class. For fixed schemas, prefer typed binding. It's safer and less code.

---

## 8. Streaming

For documents too big for memory, or when speed matters, stream:

```java
// Read a huge top-level array one element at a time: constant memory
try (JsonParser p = mapper.createParser(inputStream)) {
    if (p.nextToken() != JsonToken.START_ARRAY) throw new IllegalStateException("Expected an array");
    while (p.nextToken() == JsonToken.START_OBJECT) {
        User u = mapper.readValue(p, User.class);     // binds ONE element, leaving the parser after it
        process(u);
    }
}

// Same idea with an iterator
try (MappingIterator<User> it = mapper.readerFor(User.class).readValues(inputStream)) {
    while (it.hasNext()) process(it.next());
}

// Write a large array without holding it in memory
try (SequenceWriter w = mapper.writer().writeValuesAsArray(outputStream)) {
    for (User u : users) w.write(u);                  // e.g., rows streamed from a database
}
```

Combine with a JDBC streaming read ([ResultSet](../16-jdbc-and-databases/03_resultset-and-data-mapping.md#5-large-results-and-fetch-size)) to export millions of rows as JSON in constant memory. For line-delimited data (NDJSON), read lines and call `readValue` per line.

---

## 9. Custom serialization

Use annotations first. When you need full control, write a serializer/deserializer and register it in a module:

```java
public class MoneySerializer extends StdSerializer<Money> {
    public MoneySerializer() { super(Money.class); }
    @Override
    public void serialize(Money m, JsonGenerator gen, SerializerProvider provider) throws IOException {
        gen.writeString(m.getAmount().toPlainString() + " " + m.getCurrency());     // "19.99 EUR"
    }
}

SimpleModule module = new SimpleModule().addSerializer(Money.class, new MoneySerializer());
ObjectMapper mapper = JsonMapper.builder().addModule(module).build();
```

Or attach per type/field with `@JsonSerialize(using = ...)` / `@JsonDeserialize(using = ...)`. Keep custom code rare: every custom serializer is a place where the wire format is hand-maintained.

---

## 10. Jackson 3: what changed

Jackson 3.0 (October 2025) is a **breaking** upgrade designed so that 2.x and 3.x can coexist on one classpath:

| Area | Jackson 2.x | Jackson 3.x |
|---|---|---|
| Maven group / package | `com.fasterxml.jackson.*` | **`tools.jackson.*`** (e.g. `tools.jackson.core:jackson-databind`, `tools.jackson.databind.json.JsonMapper`) |
| Annotations | `com.fasterxml.jackson.annotation` | **Unchanged**: still `com.fasterxml.jackson.annotation` (jackson-annotations 2.20). Your DTO annotations keep working. Annotations *inside databind* (such as `@JsonSerialize`) move to the new package |
| Java baseline | Java 8 | **Java 17** |
| Mapper creation | `new ObjectMapper()` or builder | **Immutable mappers via `JsonMapper.builder()...build()`** |
| `java.time`, `Optional`, parameter names | Separate modules (`jsr310`, `jdk8`, parameter-names) | **Built into** databind. No module registration |
| Exceptions | `JsonProcessingException` (checked) | **`JacksonException`** hierarchy (**unchecked**) |
| Custom (de)serializers | `JsonSerializer`, `JsonDeserializer` | `ValueSerializer`, `ValueDeserializer` |
| Modules | `Module` | `JacksonModule` |
| Date defaults | Timestamps | **ISO-8601 strings** |
| Other defaults | | Several defaults changed (see the Jackson project's migration guide and its list of default changes) |

```java
// Jackson 3
import tools.jackson.databind.json.JsonMapper;
import tools.jackson.databind.DeserializationFeature;

JsonMapper mapper = JsonMapper.builder()
        .disable(DeserializationFeature.FAIL_ON_UNKNOWN_PROPERTIES)
        .build();                                       // immutable, thread-safe, java.time already supported

User u = mapper.readValue(json, User.class);           // no checked exception to catch
```

Migration notes:

- Most changes don't produce compile errors in your *data* classes, but **they produce different JSON** (defaults). Run contract tests before and after.
- `JsonMapper.builderWithJackson2Defaults()` helps keep 2.x behavior during the transition. Spring Boot 4 exposes a property for the same purpose (`spring.jackson.use-jackson2-defaults`).
- Tools such as OpenRewrite have recipes for the package and API renames.
- Libraries built on Jackson 2 (including some Spring and cloud SDK versions) may still bring 2.x in transitively. Both can coexist because the packages differ, but you'll see two mappers.

---

## 11. Security and limits

- **Polymorphic deserialization is the dangerous feature.** Letting JSON name the Java class to instantiate (`@JsonTypeInfo(use = Id.CLASS)`, or enabling *default typing*) has been the root of many remote-code-execution vulnerabilities, the same class of problem as [Java deserialization](../11-io-and-networking/03_java-serialization.md#6-security-never-deserialize-untrusted-data). Use **named subtypes** (`Id.NAME` + `@JsonSubTypes`) or an explicit `PolymorphicTypeValidator` allow-list, and **never enable default typing for untrusted input**. See [Insecure Deserialization](../21-security/04_insecure-deserialization.md).
- **Bind input to dedicated request DTOs**, not to entities. Otherwise a client can set fields you never meant to expose (`isAdmin`, `role`), known as mass assignment ([Patterns](03_json-patterns-and-pitfalls.md#7-security)).
- Modern Jackson enforces **default processing limits** (nesting depth, number length, string length, since 2.15). Keep them, and tighten them for untrusted input, to resist deeply nested or enormous payloads.
- Cap **request body size** at the server/gateway as well.
- Keep Jackson **up to date**. It has had security advisories.

---

## 12. Performance

- **Reuse** `ObjectMapper`/`ObjectReader`/`ObjectWriter` (build readers/writers once per type for hot paths: `mapper.readerFor(User.class)`).
- Avoid the double-hop **bytes → String → object**: pass the `InputStream`/`byte[]` directly.
- Use **streaming** for huge documents. Avoid `readTree` on large inputs.
- Don't serialize large object graphs just to log them.
- Measure with [JMH](../20-performance/01_benchmarking-with-jmh.md) before switching libraries or enabling bytecode-generation modules.

---

## Common mistakes

| Mistake | Fix |
|---|---|
| `new ObjectMapper()` per call | A shared, configured instance |
| Using `List.class` and getting `LinkedHashMap`s | `TypeReference<List<User>>` |
| `java.time` types failing or becoming numbers (2.x) | `JavaTimeModule` + disable `WRITE_DATES_AS_TIMESTAMPS` |
| Unknown-property failures when the other side adds a field | Disable `FAIL_ON_UNKNOWN_PROPERTIES` / `@JsonIgnoreProperties(ignoreUnknown = true)` |
| Missing `@JsonCreator`/`@JsonProperty` on immutable classes | Use records, or annotate the constructor |
| Serializing JPA entities directly | DTOs ([Patterns](03_json-patterns-and-pitfalls.md#6-cycles-and-entities)) |
| Enabling default typing or `Id.CLASS` for external input | Named subtypes or an allow-list validator |
| Concatenating JSON strings by hand | `ObjectMapper`/`ObjectNode` |
| `readTree` on gigabyte payloads | Streaming / `MappingIterator` |
| Mixing Jackson versions (core vs databind vs annotations) | Import the BOM |
| Assuming Jackson 3 behaves like 2 after "just" changing imports | Test the JSON output and defaults |

### Debugging

| Exception (2.x names) | Meaning |
|---|---|
| `JsonParseException` | Malformed JSON (trailing comma, bad quote, HTML instead of JSON) |
| `MismatchedInputException` | The JSON token doesn't fit the Java type (an object where a list is expected, empty body) |
| `UnrecognizedPropertyException` | An unknown key while `FAIL_ON_UNKNOWN_PROPERTIES` is on |
| `InvalidFormatException` | A bad value: unknown enum constant, malformed date, text for a number |
| `InvalidDefinitionException` | The *class* can't be handled: no usable constructor/creator, no `java.time` module, conflicting getters |
| `JsonMappingException` | Parent of the mapping errors. Its path shows where (`User["address"]->Address["zip"]`) |

Read the **path** in the message. It names the failing property. In Jackson 3 these are `JacksonException` subclasses (unchecked).

---

## Quick Summary

- **`ObjectMapper`** (2.x) / **`JsonMapper`** (3.x): configure once, reuse, thread-safe. `writeValueAsString` and `readValue`.
- **Records** are the best binding targets. Classes need a no-arg constructor or `@JsonCreator`. Use **`TypeReference`** for generics.
- Disable `FAIL_ON_UNKNOWN_PROPERTIES` for tolerant readers. In 2.x register **`JavaTimeModule`** and disable timestamps for ISO-8601 dates.
- Use **annotations** (`@JsonProperty`, `@JsonIgnore`, `@JsonInclude`, `@JsonAlias`, `@JsonFormat`) on API DTOs, and **mix-ins** for classes you don't own.
- **Tree model** for dynamic JSON, **streaming** (`JsonParser`, `MappingIterator`, `SequenceWriter`) for huge data.
- **Jackson 3:** `tools.jackson` packages (annotations unchanged), Java 17, immutable builders, built-in `java.time`, unchecked exceptions, changed defaults.
- Never allow untrusted JSON to choose Java classes. Bind to DTOs, and keep size and depth limits.

**Next:** [Gson](02_gson.md)
