# Data Formats Comparison

JSON is the default, but it isn't always the right choice. Configuration files want comments. Analytics wants columnar storage. Internal high-throughput services want compact, schema-checked binary messages. Legacy partners send XML or CSV. Choosing a format deliberately, based on **who reads it, how big and fast it must be, and how it will change**, saves a lot of pain later.

**Prerequisites:** [JSON Basics](00_json-basics.md), [Jackson](01_jackson.md).

---

## 1. The questions that decide

| Question | Why it matters |
|---|---|
| Will **humans** read or edit it? | Text formats (JSON, YAML, XML, CSV) vs binary |
| Is there a **schema**, and who enforces it? | Safety and evolution: schema-first (Protobuf, Avro) vs schemaless (JSON) |
| How will it **evolve**? | Old and new readers and writers must interoperate for years |
| **Size and speed** | Binary formats are typically smaller and faster to parse. Measure your case |
| **Who is the consumer**? | Browsers want JSON. Data platforms want Parquet/Avro. Partners dictate theirs |
| **Streaming / append-only**? | JSON Lines, CSV, Avro container files, and Protobuf with framing all stream well |
| **Tooling and language support** | Debuggability, code generation, ecosystem |

---

## 2. The candidates at a glance

| Format | Text/Binary | Schema | Human-friendly | Typical use |
|---|---|---|---|---|
| **JSON** | Text | Optional (JSON Schema) | Good | Web/REST APIs, general interchange, document stores |
| **XML** | Text | XSD, DTD | Verbose | Enterprise/legacy integration, SOAP, documents, config in older stacks |
| **YAML** | Text | Optional | Very good (comments, less noise) | Configuration (Kubernetes, CI), not for untrusted data exchange |
| **TOML / `.properties`** | Text | None | Very good | Simple configuration |
| **CSV / TSV** | Text | None (header row only) | Good | Spreadsheets, bulk import/export, simple tabular data |
| **Protocol Buffers (Protobuf)** | Binary | **Required** (`.proto`) | No | gRPC, internal service-to-service, compact messages |
| **Apache Avro** | Binary | **Required** (schema travels with or alongside data) | No | Kafka and big-data pipelines, schema registries |
| **MessagePack / CBOR / BSON** | Binary | Optional | No | "Binary JSON": same data model, smaller and faster |
| **Parquet / ORC** | Binary, **columnar** | Embedded | No | Analytics, data lakes (read a few columns of huge tables fast) |
| **Java serialization** | Binary | The class itself | No | Legacy only. Avoid ([Java Serialization](../11-io-and-networking/03_java-serialization.md)) |

---

## 3. Each format in practice

### JSON

Strengths: universal, readable, every language and browser, schemaless flexibility. Weaknesses: no comments, no date/binary/decimal types, verbose, text parsing overhead, easy to drift without a schema ([Patterns](03_json-patterns-and-pitfalls.md)). **Default for public APIs**, and often fine internally until measurements say otherwise.

**JSON Lines / NDJSON** (one JSON value per line) turns JSON into a streamable, appendable, line-splittable format. It's excellent for logs and large exports.

### XML

```xml
<user id="42"><email>asha@example.com</email><tags><tag>admin</tag></tags></user>
```

Strengths: mature schema tooling (XSD), namespaces, attributes vs elements, XPath/XSLT, and a long enterprise history. Weaknesses: verbose, awkward for simple data, more complex APIs (DOM, SAX, StAX, JAXB/Jakarta XML Binding, or Jackson's `XmlMapper`).

**Security warning:** XML parsers can resolve **external entities (XXE)** and expand huge entity definitions ("billion laughs"). Always **disable DTDs/external entities** on the parser factory when handling untrusted input ([Input Validation](../21-security/00_input-validation-and-injection.md)).

### YAML

```yaml
server:
  port: 8080          # comments are allowed
  hosts: [a, b]
```

Strengths: readable, comments, minimal punctuation. Weaknesses: **indentation-sensitive**, many implicit typing surprises (for YAML 1.1 parsers, unquoted `no`/`off` can become booleans and `010` an octal number, and a version like `1.10` may become a float), multiple ways to write the same thing. **Never load untrusted YAML with a parser configured to instantiate arbitrary Java classes** (this has been a real source of remote-code-execution bugs, same family as [insecure deserialization](../21-security/04_insecure-deserialization.md)). Use safe/restricted loading. Fine for config you control, a poor choice for data exchange.

### CSV

```csv
id,email,name
42,asha@example.com,"Asha, K."
```

Strengths: universal for spreadsheets and bulk data, streams line by line. Weaknesses: **no types** (everything is text), no nesting, and the format is under-specified (delimiters, quoting, embedded newlines, encodings, header presence). Don't `split(",")`. Use a CSV library ([File Processing Patterns](../11-io-and-networking/02_file-processing-patterns.md#csv-caution)). Beware spreadsheet formula injection when exporting user-controlled cells that begin with `=`, `+`, `-`, or `@`.

### Protocol Buffers

You define messages in a `.proto` schema, generate Java classes, and exchange compact binary:

```proto
syntax = "proto3";
option java_package = "com.acme.api";

message User {
  int64  id    = 1;
  string email = 2;
  repeated string tags = 3;
  reserved 4;            // a removed field: never reuse its number
}
```

```java
User user = User.newBuilder().setId(42).setEmail("asha@example.com").addTags("admin").build();
byte[] bytes = user.toByteArray();
User back = User.parseFrom(bytes);
```

Strengths: small and fast, strongly typed generated code, excellent cross-language support, and **well-defined evolution rules**. It's the standard payload for **gRPC**. Weaknesses: not human-readable (needs tooling to inspect), generated-code build step, no `null` (fields have defaults), and clients need the schema.

**Evolution rules (the heart of Protobuf):** fields are identified by **number**, not name. You can add fields (old readers ignore unknown ones), but you must **never reuse or renumber** a field, and should `reserve` removed numbers. Don't change a field's type incompatibly.

### Avro

Avro stores data with a **schema** (JSON-defined). Container files embed the schema, and streaming systems (Kafka) usually reference it from a **schema registry**. It has strong support for **schema evolution with reader/writer schema resolution** (add fields with defaults, remove fields with defaults), and is a staple of data pipelines.

### MessagePack, CBOR, BSON

Same data model as JSON (maps, arrays, strings, numbers) in a compact binary encoding, plus extra types (binary, timestamps in some). Jackson reads and writes them through modules, with the **same annotations**, which makes them an easy size/speed upgrade without a schema compiler. BSON is MongoDB's format. They keep JSON's schemaless drawbacks.

### Parquet and ORC (columnar)

Store data **by column**, so analytic queries read only the columns they need, with strong compression. They're for data lakes and warehouses (Spark, Trino, etc.), not for messages or APIs.

---

## 4. One model, many formats (with Jackson)

Jackson's data-format modules let the **same annotated classes** be read and written as different formats:

```java
User user = new User(42, "asha@example.com", List.of("admin"));

String json = new ObjectMapper().writeValueAsString(user);            // jackson-databind
String yaml = new YAMLMapper().writeValueAsString(user);              // jackson-dataformat-yaml
String xml  = new XmlMapper().writeValueAsString(user);               // jackson-dataformat-xml
String csv  = new CsvMapper().writer(csvSchema).writeValueAsString(user);   // jackson-dataformat-csv (needs a schema)
byte[] cbor = new CBORMapper().writeValueAsBytes(user);               // jackson-dataformat-cbor
```

(Class names shown for Jackson 2.x. Jackson 3 provides format-specific mapper subclasses under `tools.jackson`.) This is handy for config in YAML, exports in CSV, and internal binary in CBOR/MessagePack, all from one set of DTOs.

---

## 5. Comparison on the dimensions that matter

| | JSON | XML | YAML | CSV | Protobuf | Avro | Parquet |
|---|---|---|---|---|---|---|---|
| Human readable | ✔ | ✔ (verbose) | ✔✔ | ✔ | ✘ | ✘ | ✘ |
| Comments | ✘ | ✔ | ✔ | ✘ | n/a | n/a | n/a |
| Schema enforced | optional | optional (XSD) | optional | ✘ | **✔** | **✔** | **✔** |
| Nested data | ✔ | ✔ | ✔ | ✘ | ✔ | ✔ | ✔ |
| Typed values | basic | text (typed via XSD) | implicit (risky) | ✘ | **✔** | **✔** | **✔** |
| Compact | medium | large | medium | medium | **small** | **small** | **small** (columnar) |
| Parse speed | medium | slow | slow | fast | **fast** | **fast** | fast for column scans |
| Streamable | ✔ (NDJSON) | ✔ (SAX/StAX) | limited | ✔ | with framing | ✔ | by row group |
| Schema evolution story | convention | XSD versioning | convention | fragile | **strong** | **strong** | **good** |
| Browser-native | ✔ | partial | ✘ | ✘ | needs library | ✘ | ✘ |
| Safe for untrusted input | ✔ (with care) | needs XXE hardening | **needs safe loader** | ✔ | ✔ | ✔ | ✔ |

Rule of thumb on size and speed: binary schema formats are usually noticeably smaller and faster to encode/decode than JSON, but **how much matters depends on your payloads**. Measure with realistic data before deciding that JSON is "too slow" ([JMH](../20-performance/01_benchmarking-with-jmh.md)). Compression (gzip/Zstandard) narrows the *size* gap considerably for text formats.

---

## 6. Choosing

```text
 Public REST API / browser clients / third parties?               → JSON  (+ OpenAPI / JSON Schema)
 Internal service-to-service RPC, high volume, strict contracts?  → Protobuf (gRPC)
 Event streaming (Kafka) with evolving schemas?                   → Avro or Protobuf, with a schema registry
 Application configuration humans edit?                           → YAML, TOML, or .properties (not JSON: no comments)
 Bulk import/export for spreadsheets / business users?            → CSV (with a real library and a documented schema)
 Integration with legacy / enterprise partners?                   → XML (hardened parser)
 Logs or huge exports you stream and grep?                        → JSON Lines
 Analytics / data lake?                                           → Parquet (or ORC)
 "JSON but smaller/faster", no schema compiler wanted?            → MessagePack / CBOR
 Persisting Java objects long-term?                               → Not Java serialization. Use one of the above
```

Guidelines:

- **Start with JSON** unless you have a concrete reason not to. It's debuggable and universal. Move to binary where measurements and contracts justify it.
- **Make the schema explicit** whatever the format: OpenAPI/JSON Schema for JSON, `.proto` for Protobuf, Avro schemas. Unwritten schemas drift.
- **Plan evolution first.** Know your compatibility rule (backward, forward, full) and test it ([Patterns: Compatibility](03_json-patterns-and-pitfalls.md#1-dtos-and-compatibility)).
- **Don't mix formats casually** inside one service. Each adds tooling and failure modes.
- **Security is format-specific:** XXE for XML, unsafe object loading for YAML and Java serialization, size/depth limits for all, type-by-name polymorphism for JSON libraries.

---

## Common mistakes

| Mistake | Fix |
|---|---|
| Choosing a binary format "for performance" without measuring | Benchmark with real payloads and compression |
| Using JSON for config that needs comments | YAML/TOML/properties |
| Loading untrusted YAML with a full-object-construction loader | Safe loader, or a different format |
| Parsing XML from outside with default settings | Disable DTDs/external entities |
| Reusing or renumbering Protobuf field numbers | Add new numbers, `reserved` removed ones |
| `split(",")` for CSV | A CSV library |
| CSV with no documented schema, encoding, or quoting rules | Specify them, and validate on import |
| Inventing a custom text/binary format | Use an existing one with tooling |
| Different services using different undeclared schemas for "the same" data | One shared, versioned schema |
| Persisting data with Java serialization | A schema'd format |

### Debugging

- **Binary payload won't decode:** wrong schema version or message type, or the bytes were compressed/encoded (Base64, gzip) on the way. Log length and the first bytes, and check both ends' schema versions.
- **CSV import mangled data:** encoding (UTF-8 vs Windows-1252), delimiter (`,` vs `;` in some locales), embedded quotes/newlines, BOM.
- **YAML value has the wrong type:** quote it, or use a stricter parser mode.
- **XML parser slow or exhausting memory on tiny input:** entity expansion. Harden the parser.
- **Protobuf field "disappeared":** a field number changed or was reused, or the reader's schema is older. Compare `.proto` versions.

---

## Quick Summary

- Pick the format by **audience, schema needs, evolution, size/speed, and tooling**, not by habit.
- **JSON** is the default for APIs. **Protobuf/Avro** for strict, compact, evolving internal contracts. **YAML/TOML/properties** for config. **CSV** for tabular exchange. **XML** for legacy integration. **Parquet** for analytics. **JSON Lines** for streaming logs and exports.
- Binary schema formats win on size/speed and evolution rules, but lose readability. **Measure** before switching, and remember compression.
- Whatever you choose, write the **schema**, plan **evolution**, and mind the **format-specific security issues** (XXE, unsafe YAML/serialization, polymorphic typing, size limits).
- Jackson's data-format modules let one set of annotated classes serve JSON, YAML, XML, CSV, and binary formats.

**Next module:** [Build and Dependencies](../18-build-and-dependencies/README.md)
