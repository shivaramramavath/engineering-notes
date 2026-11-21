# 03. Strings and Text

> Comprehensive reference and architecture guide to strings, text blocks, character encoding, regular expressions, and mutable sequences in modern Java.

---

## Module Overview

Text is the universal interface of computing. In Java, strings enjoy deep compiler and JVM runtime optimizations, from the String Constant Pool and Compact Strings memory reduction to `invokedynamic` string concatenation and multi-line Text Blocks.

This module provides exhaustive coverage of string mechanics, memory representations, performance trade-offs, and modern Java standard APIs.

---

## Document Index

| # | Note | Level | Description | Key Topics |
| :- | :--- | :--- | :--- | :--- |
| 00 | [String Basics & Immutability](./00_string-basics.md) | Beginner | Immutability mechanics and Compact Strings | `java.lang.String`, Compact Strings (JEP 254), memory layout |
| 01 | [String Methods](./01_string-methods.md) | Beg-Int | Inspection, search, and functional transform | `length()`, `isBlank()`, `strip()`, `lines()`, `transform()` |
| 02 | [String Pool & Equality](./02_string-pool-and-comparison.md) | Intermediate | String constant pool and deduplication | `StringTable`, `.intern()`, `==` vs `.equals()`, `-XX:+UseStringDeduplication` |
| 03 | [StringBuilder & StringBuffer](./03_stringbuilder-and-stringbuffer.md) | Intermediate | Mutable character sequences | `AbstractStringBuilder`, capacity growth, concurrency trade-offs |
| 04 | [String Formatting](./04_string-formatting.md) | Intermediate | Formatter specifiers and templating | `String.format()`, `.formatted()`, `MessageFormat`, String Templates status |
| 05 | [Text Blocks](./05_text-blocks.md) | Intermediate | Multi-line string literals (JEP 378) | Delimiters, whitespace stripping, `\s` and `\`, SQL/JSON templates |
| 06 | [Regular Expressions](./06_regex.md) | Int-Adv | Pattern matching and ReDoS defense | `Pattern`, `Matcher`, possessive quantifiers, named groups, Stream API |
| 07 | [Unicode & Encoding](./07_unicode-and-encoding.md) | Int-Adv | Character sets and UTF-8 by Default | Unicode code points, surrogate pairs, UTF-8 (JEP 400), BOM handling |

---

## Conceptual Architecture Map

```
                          ┌──────────────────────────┐
                          │   java.lang.CharSequence │
                          └────────────┬─────────────┘
                                       │
            ┌──────────────────────────┴──────────────────────────┐
            ▼                                                     ▼
┌──────────────────────────┐                           ┌──────────────────────────┐
│     java.lang.String     │                           │  AbstractStringBuilder   │
├──────────────────────────┤                           ├──────────────────────────┤
│ - Immutable              │                           │ - Mutable                │
│ - Compact Strings (JEP254│                           │ - Dynamic capacity resize│
│ - byte[] value           │                           │ - byte[] value           │
│ - byte coder (LATIN1/16) │                           │ - byte coder             │
└───────────┬──────────────┘                           └────────────┬─────────────┘
            │                                                       │
            ├────────────────────────┐              ┌───────────────┴───────────────┐
            ▼                        ▼              ▼                               ▼
┌──────────────────────┐  ┌────────────────────┐ ┌────────────────┐ ┌────────────────┐
│ String Constant Pool │  │ Text Blocks(JEP378)│ │ StringBuilder  │ │ StringBuffer   │
│ - JVM StringTable    │  │ - Multi-line """   │ │ - Unsynchronized│ │ - Synchronized │
│ - Reference sharing  │  │ - Whitespace strip │ │ - High throughput│ │ - Legacy       │
└──────────────────────┘  └────────────────────┘ └────────────────┘ └────────────────┘
```

---

## Learning Objectives Checklist

- [x] Explain why `String` is immutable and how it enables security, thread safety, and hash caching.
- [x] Describe Java 9+ Compact Strings (`byte[]` + `coder`) and measure its memory savings.
- [x] Differentiate reference equality (`==`) from value equality (`.equals()`).
- [x] Use `StringBuilder` with appropriate pre-sized capacity to prevent quadratic concatenation overhead in loops.
- [x] Format text cleanly using Java 15 `.formatted()` and Text Blocks (JEP 378).
- [x] Protect against Catastrophic Backtracking (ReDoS) in regular expressions using possessive quantifiers.
- [x] Traverse Unicode text safely using code points instead of naive `char` indexing.
- [x] Understand Java 18 UTF-8 by Default (JEP 400) and configure robust NIO `CharsetDecoder` error actions.
