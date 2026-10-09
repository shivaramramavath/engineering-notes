# Gson

**Gson** is Google's JSON library for Java. It has a small, approachable API and works by reflecting over an object's **fields**, so a plain class often needs no annotations at all. You'll meet it in Android code, older server code, and many small tools. It describes itself as being in maintenance mode: stable and widely used, but gaining few new features, which is one reason many projects choose Jackson for new work.

This note covers Gson's model and, importantly, where it behaves *differently* from Jackson in ways that bite.

**Prerequisites:** [JSON Basics](00_json-basics.md), [Reflection](../13-advanced-language-features/01_reflection.md), [Type Erasure](../07-generics/03_type-erasure.md).

---

## 1. Setup and the basics

```xml
<dependency>
    <groupId>com.google.code.gson</groupId>
    <artifactId>gson</artifactId>
    <version><!-- current 2.x release --></version>
</dependency>
```

```java
Gson gson = new Gson();                                   // thread-safe: create once, reuse

public class User {
    private long id;
    private String email;
    private List<String> tags;
}

String json = gson.toJson(user);                          // {"id":42,"email":"asha@example.com","tags":["admin"]}
User back   = gson.fromJson(json, User.class);
```

`Gson` instances are **thread-safe** and cache type adapters, so share one.

---

## 2. Configuration with `GsonBuilder`

```java
Gson gson = new GsonBuilder()
        .setPrettyPrinting()
        .serializeNulls()                                              // include null fields (omitted by default)
        .setFieldNamingPolicy(FieldNamingPolicy.LOWER_CASE_WITH_UNDERSCORES)   // userId → user_id
        .disableHtmlEscaping()                                         // see below
        .create();
```

| Option | Effect |
|---|---|
| `setPrettyPrinting()` | Indented output |
| `serializeNulls()` | Write `"field": null`. **By default null fields are omitted** |
| `setFieldNamingPolicy(...)` | Map Java field names to JSON names (snake_case, etc.) |
| `excludeFieldsWithoutExposeAnnotation()` | Only fields annotated `@Expose` are used |
| `setExclusionStrategies(...)` | Programmatic field exclusion |
| `registerTypeAdapter(...)` | Custom (de)serialization for a type ([section 5](#5-custom-type-adapters)) |
| `disableHtmlEscaping()` | By default Gson escapes `<`, `>`, `&`, `=`, `'` as `\u003c` etc. (safe for embedding in HTML, but surprising in API output) |
| `setObjectToNumberStrategy(...)` | How numbers become Java types when the target is `Object` ([section 6](#6-pitfalls-that-differ-from-jackson)) |
| Strictness settings | Gson historically parses leniently (accepts some malformed JSON). Recent versions let you set strictness explicitly. Check the version you use |

---

## 3. Fields, names, and annotations

Gson maps **fields**, including `private` ones, so getters and setters are irrelevant. It skips `transient` and `static` fields.

```java
public class UserDto {
    @SerializedName("user_id")                       // JSON name
    private long id;

    @SerializedName(value = "email", alternate = {"mail", "e-mail"})   // accept alternatives when reading
    private String email;

    private transient String passwordHash;           // never serialized

    @Expose private String name;                     // used with excludeFieldsWithoutExposeAnnotation()
}
```

Other annotations: `@Since`/`@Until` (version-based inclusion), `@JsonAdapter` (attach a custom adapter to a class or field).

**Records** are supported in recent Gson versions (2.10+). They're deserialized through their canonical constructor.

---

## 4. Generics, the tree model, and streaming

### Generics need `TypeToken`

Same erasure problem as Jackson:

```java
Type listType = new TypeToken<List<User>>() {}.getType();
List<User> users = gson.fromJson(json, listType);

// programmatic form
Type t = TypeToken.getParameterized(List.class, User.class).getType();
```

`gson.fromJson(json, List.class)` yields a list of `LinkedTreeMap`s, not `User`s.

### Tree model

```java
JsonElement root = JsonParser.parseString(json);                 // JsonObject / JsonArray / JsonPrimitive / JsonNull
JsonObject obj = root.getAsJsonObject();

String city = obj.getAsJsonObject("address").get("city").getAsString();
if (obj.has("error") && !obj.get("error").isJsonNull()) { ... }

JsonObject out = new JsonObject();
out.addProperty("status", "ok");
JsonArray ids = new JsonArray(); ids.add(1); ids.add(2);
out.add("ids", ids);
String s = gson.toJson(out);
```

Tree navigation is **stricter than Jackson's `path()`**: `get("x")` returns `null` for a missing key, so check with `has` first.

### Streaming

`JsonReader` and `JsonWriter` provide token-level access for large documents:

```java
try (JsonReader reader = new JsonReader(new InputStreamReader(in, StandardCharsets.UTF_8))) {
    reader.beginArray();
    while (reader.hasNext()) {
        User u = gson.fromJson(reader, User.class);               // one element at a time
        process(u);
    }
    reader.endArray();
}
```

---

## 5. Custom type adapters

For types Gson can't handle by reflection, write a `TypeAdapter`:

```java
class InstantAdapter extends TypeAdapter<Instant> {
    @Override public void write(JsonWriter out, Instant value) throws IOException {
        out.value(value.toString());                              // ISO-8601
    }
    @Override public Instant read(JsonReader in) throws IOException {
        return Instant.parse(in.nextString());
    }
}

Gson gson = new GsonBuilder()
        .registerTypeAdapter(Instant.class, new InstantAdapter().nullSafe())   // nullSafe() handles JSON null
        .create();
```

`JsonSerializer<T>` / `JsonDeserializer<T>` are an alternative tree-based API (simpler, slower). `TypeAdapterFactory` handles families of types. **Register an adapter for every `java.time` type you use** (see the next section).

---

## 6. Pitfalls that differ from Jackson

### Constructors are bypassed

Gson uses a **no-arg constructor if one exists**. Otherwise it creates the object **without calling any constructor** (using JDK internals). So field initializers, constructor validation, and class invariants may be skipped, and fields can end up `null`/`0` even if your constructor would have set them:

```java
class Config {
    List<String> hosts = new ArrayList<>();      // initializer runs only if a no-arg constructor exists
    Config(String name) { ... }                   // no no-arg constructor → hosts is null after fromJson!
}
```

Give mapped classes a no-arg constructor (it can be `private`), or use records. This is the same hazard as Java deserialization ([Serialization](../11-io-and-networking/03_java-serialization.md#3-constructors-are-bypassed)).

### Missing and unknown fields are silent

- JSON fields with no matching Java field are **ignored without any error** (Jackson 2.x fails by default).
- Java fields with no JSON value keep their default (`null`, `0`, `false`). Gson has **no "required" concept**, so you must validate after parsing ([Patterns](03_json-patterns-and-pitfalls.md#8-errors-and-validation)).

### `java.time` is not supported out of the box

On modern JDKs, reflecting into JDK classes is blocked ([Reflection](../13-advanced-language-features/01_reflection.md#4-access-control-and-modules)), so serializing an `Instant` or `LocalDate` fails with an error like:

```text
JsonIOException: Failed making field 'java.time.Instant#seconds' accessible;
either increase its visibility or write a custom TypeAdapter for its declaring type.
```

Write and register adapters (section 5) or switch to Jackson, which supports `java.time`.

### Numbers into `Object`/`Map` become `Double`

```java
Map<String, Object> m = gson.fromJson("{\"count\": 3}", new TypeToken<Map<String, Object>>() {}.getType());
m.get("count");        // 3.0 (a Double!)
```

Large `long`s lose precision this way. Configure `new GsonBuilder().setObjectToNumberStrategy(ToNumberPolicy.LONG_OR_DOUBLE)` (Gson 2.8.9+), or bind to typed classes.

### Polymorphism isn't built in

There's no built-in `@JsonTypeInfo` equivalent. The popular `RuntimeTypeAdapterFactory` lives in Gson's *extras* source (not the published artifact), so teams copy it or write their own adapter. For heavy polymorphism, Jackson is easier ([Patterns](03_json-patterns-and-pitfalls.md#5-polymorphism)).

### Obfuscation and field renames (Android)

Gson maps by **field name**. Code shrinkers (R8/ProGuard) rename fields, silently breaking JSON mapping. Annotate with `@SerializedName` or add keep rules.

### HTML escaping and nulls

`<`, `>`, `&` are escaped as `\u003c`.. by default (valid JSON, but surprising). Nulls are omitted unless `serializeNulls()`.

---

## 7. Gson vs Jackson

| | Gson | Jackson |
|---|---|---|
| API size | Small, simple | Large, many features |
| Mapping by | **Fields** (reflection) | Getters/setters, fields, constructors, records |
| `java.time` | Needs custom adapters | Module (2.x) / built in (3.x) |
| Unknown properties | Ignored silently | Fail by default (2.x), configurable |
| Constructors | Bypassed if no no-arg constructor | Uses constructors/`@JsonCreator` |
| Polymorphism | Manual | `@JsonTypeInfo`/`@JsonSubTypes` (and a security surface to manage) |
| Other formats (XML/YAML/CSV/Protobuf) | No | Yes, via modules and the same annotations |
| Streaming/perf | Good | Excellent, with many optimizations |
| Ecosystem/framework support | Android, libraries | Spring and most server frameworks default |
| Project status | Maintenance mode | Very active (2.x and 3.x) |

Choose **Jackson** for server-side work and new projects, **Gson** when you're already on it (Android, existing codebase) or need something tiny. Mixing both in one application is possible but confusing. Pick one per service.

---

## Common mistakes

| Mistake | Fix |
|---|---|
| `fromJson(json, List.class)` | `TypeToken<List<T>>` |
| Expecting constructors/initializers to run | Provide a no-arg constructor, or use records |
| Assuming missing fields cause errors | Validate after parsing |
| Serializing `Instant`/`LocalDate` with no adapter | Register `TypeAdapter`s |
| Reading numbers into `Map<String, Object>` and getting doubles | `ToNumberPolicy.LONG_OR_DOUBLE`, or typed classes |
| Wondering why `null` fields vanish from output | `serializeNulls()` |
| Surprise `\u003c` in output | `disableHtmlEscaping()` (when not embedding in HTML) |
| New `Gson` per call | Share one instance |
| Field names broken by code shrinking | `@SerializedName` / keep rules |
| Using `transient` and expecting it to be ignored by Jackson too | Different libraries, different rules (`@JsonIgnore`) |

### Debugging

- `JsonSyntaxException: java.lang.IllegalStateException: Expected BEGIN_OBJECT but was STRING at line 1 column 1 path $` → the body isn't the object you expected (often an error message or HTML). The **path** (`$.items[2].price`) shows where parsing failed.
- `JsonSyntaxException: ... Expected a long but was STRING` → type mismatch at that path.
- `JsonIOException` → reflection/access problems (JDK types) or an I/O error. Write an adapter.
- Object fields unexpectedly `null` → no matching JSON key, wrong name or naming policy, or no constructor ran.
- Different output between environments → check `GsonBuilder` settings (pretty printing, nulls, naming policy) and Gson versions.

---

## Quick Summary

- Gson: `new Gson().toJson(obj)` / `fromJson(json, Type.class)`. Thread-safe. Maps **fields**, skipping `transient`/`static`.
- Customize with `GsonBuilder`, `@SerializedName`, `@Expose`, and **`TypeAdapter`s**. Use **`TypeToken`** for generics.
- **Differences from Jackson that bite:** constructors may be bypassed, unknown/missing fields are silent, `java.time` needs adapters, numbers become `Double` in untyped maps, polymorphism is manual, nulls are omitted by default.
- Tree model: `JsonParser.parseString` + `JsonObject`/`JsonArray`. Streaming: `JsonReader`/`JsonWriter`.
- Prefer **Jackson** for new server-side code. Keep Gson where it's already established, with explicit adapters and post-parse validation.

**Next:** [JSON Patterns and Pitfalls](03_json-patterns-and-pitfalls.md)
