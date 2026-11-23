# Module 09: JavaScript Built-in Objects & Standard Library

> Authoritative curriculum and deep-dive technical notes on ECMAScript standard library built-ins, text encodings, IEEE-754 numbers, binary buffers, collections, serialization, internationalization, and metaprogramming.

## Module Overview

JavaScript ships with an extensive, highly optimized standard library built directly into the engine runtime. Understanding the exact mechanical behavior, time complexity, and memory characteristics of these built-ins prevents subtle production bugs and eliminates redundant third-party dependencies:

1. **Text & Numbers:** UTF-16 surrogate pairs, Unicode normalization forms (NFC), IEEE-754 double precision limits ($2^{53}-1$), BigInt arbitrary precision, and V8 Smi tagging.
2. **Math & Geometry:** Rounding semantics (`floor`, `ceil`, `trunc`, `round`), PRNG mechanics (`xorshift128+`), and hardware CPU SIMD intrinsics.
3. **Temporal Computing:** Legacy `Date` epoch representations, UTC vs local timezone resolution, ISO-8601 parsing, and the modern TC39 Stage 3 `Temporal` API.
4. **Pattern Matching & Regular Expressions:** V8 Irregexp engine, modern flags (`d`, `v`, `y`, `s`), named capture groups, lookaround assertions, and ReDoS defense.
5. **Keyed Collections:** `Map` and `Set` via V8 `OrderedHashTable`, `SameValueZero` equality, and ES2024 mathematical set algebra (`union`, `intersection`, `difference`).
6. **Memory & Weak References:** Ephemeron tables in `WeakMap`/`WeakSet`, memory leak prevention, and ES2021 `WeakRef` / `FinalizationRegistry`.
7. **Serialization & Internationalization:** `JSON` SIMD parsing, circular safe replacers/revivers, ECMA-402 `Intl` ICU integration, and localized formatting.
8. **Binary Data & Systems Programming:** `ArrayBuffer`, TypedArray views, `DataView` endianness handling, ES2024 buffer transfer, and `SharedArrayBuffer` Atomics.
9. **Metaprogramming & Environment Globals:** The 13 Proxy traps, `Reflect` parity, invariant rules, `globalThis`, and `structuredClone`.

---

## Curriculum & Reading Order

| Note | Title | Level | Reading Time | Topics Covered |
| :--- | :--- | :--- | :--- | :--- |
| **[01](01_string.md)** | [String & Unicode Architecture](01_string.md) | Core | 26 min | UTF-16, code units vs code points, normalization, V8 ConsString/SlicedString |
| **[02](02_number-and-bigint.md)** | [Number & BigInt](02_number-and-bigint.md) | Core | 25 min | IEEE-754 binary64, Number.EPSILON, safe integer limits, BigInt, V8 Smi |
| **[03](03_math.md)** | [Math & PRNG Mechanics](03_math.md) | Core | 24 min | Rounding algorithms, trigonometry, xorshift128+ PRNG, TurboFan math intrinsics |
| **[04](04_date-and-temporal.md)** | [Date & Temporal API](04_date-and-temporal.md) | Core | 27 min | Legacy Date pitfalls, DST transitions, ISO-8601 UTC, Temporal API |
| **[05](05_regexp.md)** | [RegExp & Irregexp Engine](05_regexp.md) | Core | 28 min | Flags (d, v, y, s), lookarounds, named groups, V8 Irregexp, ReDoS defense |
| **[06](06_map-and-set.md)** | [Map, Set & ES2024 Set Methods](06_map-and-set.md) | Core | 25 min | SameValueZero, ES2024 Set algebra, V8 OrderedHashTable, LRU cache |
| **[07](07_weakmap-weakset-weakref.md)** | [Weak Collections & Memory](07_weakmap-weakset-weakref.md) | Advanced | 26 min | Ephemerons, WeakMap, WeakSet, WeakRef, FinalizationRegistry, GC tracing |
| **[08](08_json.md)** | [JSON Serialization & Protocols](08_json.md) | Core | 25 min | Replacers, revivers, toJSON(), V8 SIMD parser, structuredClone comparison |
| **[09](09_intl.md)** | [Intl & ECMA-402 Localization](09_intl.md) | Core | 25 min | NumberFormat, DateTimeFormat, Collator, ListFormat, Segmenter, ICU |
| **[10](10_typed-arrays-and-arraybuffer.md)** | [TypedArrays & Binary Data](10_typed-arrays-and-arraybuffer.md) | Advanced | 27 min | ArrayBuffer, DataView, Endianness, ES2024 transfer/resize, Atomics |
| **[11](11_proxy-and-reflect.md)** | [Proxy & Reflect Metaprogramming](11_proxy-and-reflect.md) | Advanced | 28 min | 13 language traps, Reflect parity, invariants, receiver, reactivity engine |
| **[12](12_global-objects.md)** | [Global Objects & Runtimes](12_global-objects.md) | Core | 25 min | globalThis, encodeURIComponent, atob/btoa, structuredClone, eval |

---

## Cross-Module References

- **Object Prototypes & Inheritance:** Covered in [05_this-and-oop/03_prototypes-and-prototype-chain.md](../05_this-and-oop/03_prototypes-and-prototype-chain.md).
- **Iteration Protocol Consumption:** Covered in [08_modern-javascript/04_iterators-and-iterables.md](../08_modern-javascript/04_iterators-and-iterables.md).
- **Asynchronous Execution & Event Loop:** Covered in [11_asynchronous-javascript/](../11_asynchronous-javascript/) & [12_event-loop/](../12_event-loop/).
- **Memory Management & Garbage Collection:** Covered in [18_memory-and-garbage-collection/](../18_memory-and-garbage-collection/).
