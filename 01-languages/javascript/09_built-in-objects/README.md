# 09 · Built-in Objects

JavaScript ships with a standard library of objects available in every runtime: text, numbers, dates, patterns, collections, serialization, internationalization, binary data and meta-programming tools. Knowing them saves you from reinventing (and mis-implementing) common utilities.

## Reading order

| # | File | You will learn |
|---|------|----------------|
| 1 | [String](./01_string.md) | Searching, slicing, Unicode, normalization |
| 2 | [Number and BigInt](./02_number-and-bigint.md) | Floats, precision, parsing, big integers |
| 3 | [Math](./03_math.md) | Rounding, random, trigonometry, helpers |
| 4 | [Date and Temporal](./04_date-and-temporal.md) | `Date` quirks, time zones, the Temporal API |
| 5 | [RegExp](./05_regexp.md) | Flags, groups, lookarounds, safe patterns |
| 6 | [Map and Set](./06_map-and-set.md) | Keyed and unique collections, set algebra |
| 7 | [WeakMap, WeakSet, WeakRef](./07_weakmap-weakset-weakref.md) | Weak references and garbage collection |
| 8 | [JSON](./08_json.md) | `parse`, `stringify`, replacers, revivers |
| 9 | [Intl](./09_intl.md) | Locale-aware formatting and sorting |
| 10 | [Typed Arrays and ArrayBuffer](./10_typed-arrays-and-arraybuffer.md) | Binary data, `DataView`, text encoding |
| 11 | [Proxy and Reflect](./11_proxy-and-reflect.md) | Intercepting object operations |
| 12 | [Global Objects](./12_global-objects.md) | `globalThis`, `structuredClone`, timers, web-platform globals |

## Where to look things up

| Question | Best source |
|----------|-------------|
| Exact method behavior | MDN Web Docs |
| Is it supported here? | MDN compatibility tables, `node.green` |
| Language rules | ECMAScript specification |

## Goal

By the end you can pick the right built-in for text, numbers, time, patterns, collections and binary data, and avoid the classic traps of each.

**Next:** [String](./01_string.md)
