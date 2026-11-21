# 17 · JSON and Data Formats

Nearly every Java service exchanges data with something else: a browser, another service, a queue, a config file, a file on disk. This module is about **turning Java objects into text or bytes and back**: what JSON is, how Jackson and Gson do the conversion, the pitfalls that cause real production bugs, and how JSON compares with the alternatives (XML, YAML, CSV, Protocol Buffers, Avro).

## Contents

| # | Note | Focus |
|---|------|-------|
| 00 | [JSON Basics](00_json-basics.md) | The format, its limits, the three processing models, the Java landscape |
| 01 | [Jackson](01_jackson.md) | `ObjectMapper`/`JsonMapper`, annotations, `java.time`, tree and streaming APIs, security, Jackson 2 vs 3 |
| 02 | [Gson](02_gson.md) | Google's library: `Gson`, `TypeToken`, adapters, differences from Jackson |
| 03 | [JSON Patterns and Pitfalls](03_json-patterns-and-pitfalls.md) | DTOs, null vs absent, dates, big numbers, polymorphism, cycles, compatibility, security |
| 04 | [Data Formats Comparison](04_data-formats-comparison.md) | JSON vs XML vs YAML vs CSV vs Protobuf vs Avro, and how to choose |

## The layers

```text
   Java objects (records / classes)
        │   data binding         Jackson ObjectMapper, Gson          ← notes 01, 02
        ▼
   tree model (JsonNode / JsonElement)   — optional, for dynamic data
        │
   streaming (parser / generator tokens)                              ← fast, low memory
        │
   characters / bytes  (UTF-8 text)                                   ← note 00
        │
   HTTP body, file, queue message, database column
```

## Suggested path

Read **00 → 01** first (Jackson is the dominant library, and Spring uses it by default). **02** is short and useful if you meet Gson in Android or older code. **03** is the one to reread before shipping an API. **04** helps you choose a format deliberately instead of by habit.

## Versions and status (October 2026)

- **Jackson 2.x** (`com.fasterxml.jackson`) is the long-lived line found in most existing code (Spring Boot 3.x uses it).
- **Jackson 3.x** (3.0 released October 2025) is a breaking upgrade: new `tools.jackson` packages and Maven group, Java 17 baseline, immutable builder-based mappers, unchecked exceptions, `java.time`/`Optional` support built in, and some changed defaults (for example ISO-8601 date strings). Spring Boot 4 builds on it. Notes 01 and 03 show both, and call out the differences.
- **Gson** is stable and widely deployed, and describes itself as being in maintenance mode.
- Library versions and defaults move, so verify details against the version you use.

## Prerequisites

[Records](../12-modern-java/01_records.md), [Generics](../07-generics/README.md) (especially [type erasure](../07-generics/03_type-erasure.md), which explains `TypeReference`), [java.time](../10-date-and-time/00_java-time-overview.md), [Annotations](../13-advanced-language-features/00_annotations.md), and [Reflection](../13-advanced-language-features/01_reflection.md) for how libraries find your fields.

## Related

[Java Serialization](../11-io-and-networking/03_java-serialization.md) · [HTTP Client](../11-io-and-networking/04_http-client-and-networking.md) · [Insecure Deserialization](../21-security/04_insecure-deserialization.md) · [Input Validation](../21-security/00_input-validation-and-injection.md)

**Next module:** [Build and Dependencies](../18-build-and-dependencies/README.md)
