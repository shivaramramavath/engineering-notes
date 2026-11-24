# Module 04: Strings & Text Processing

## Overview

Module 04 provides comprehensive, engineering-grade coverage of Python's text processing and string manipulation systems. Rather than viewing strings as simple arrays of characters, this module explores the low-level architecture of Python's Unicode object model (`str`), binary buffer abstractions (`bytes`, `bytearray`, and `memoryview`), compiled regular expressions (`re`), and high-performance text processing patterns.

From PEP 393 flexible string representations and CPython's 1-byte, 2-byte, and 4-byte memory widening rules, to the Unicode Sandwich architectural pattern, ReDoS catastrophic backtracking prevention, and PEP 701 f-string parser integration, this module equips developers to handle high-throughput text and binary streams with precision and speed.

---

## Complete Module Syllabus

| # | Note Title | Slug | Key Topics Covered | Status |
| :-: | :--- | :--- | :--- | :-: |
| 00 | **String Methods** | `00-string-methods.md` | Search, casing, partition vs split, PEP 616 `removeprefix`/`removesuffix`, $O(n)$ `join()`, Unicode `casefold()` | Verified |
| 01 | **Formatting and F-Strings** | `01-formatting-f-strings.md` | Format specifier mini-language, alignment, digit grouping, `{expr=}`, PEP 701 syntax (nested quotes, backslashes) | Verified |
| 02 | **Unicode and Encodings** | `02-unicode-and-encodings.md` | The Unicode Sandwich pattern, UTF-8/16/32, error handlers (`strict`, `surrogateescape`), NFC/NFD/NFKC normalization | Verified |
| 03 | **bytes, bytearray, and memoryview** | `03-bytes-and-bytearray.md` | Immutable `bytes` vs mutable `bytearray`, zero-copy `memoryview`, PEP 3118 buffer protocol, in-place socket I/O | Verified |
| 04 | **Regular Expressions (re Module)** | `04-regular-expressions.md` | `match` vs `search` vs `fullmatch`, named capture groups `(?P<name>)`, lookaround assertions, callback substitutions, ReDoS | Verified |
| 05 | **Text Processing Patterns** | `05-text-processing-patterns.md` | Streaming pipelines ($O(1)$ memory), safe `string.Template`, `textwrap` formatting, state machine lexing | Verified |
| 06 | **String Internals & Interning** | `06-string-internals-and-interning.md` | PEP 393 (Latin-1, UCS-2, UCS-4), memory widening penalty, `sys.intern()`, $O(1)$ pointer comparison, Latin-1 singletons | Verified |

---

## Learning Outcomes

After completing Module 04, you will be able to:
1. Apply the Unicode Sandwich pattern to isolate encoding/decoding at system boundaries.
2. Exploit Python 3.12+ PEP 701 f-string capabilities and format specifiers for clean reports.
3. Normalize international text across NFC, NFD, and NFKC forms to guarantee deterministic string comparison.
4. Execute zero-copy slicing over large binary streams using `memoryview` and mutable `bytearray`.
5. Author high-speed, secure regular expressions with named capturing groups while avoiding catastrophic backtracking (ReDoS).
6. Explain PEP 393 flexible string representations and use `sys.intern()` to optimize memory and dictionary lookups.

---

## Prerequisites & Cross-Links

- **Prerequisites:** [Module 01: Fundamentals & Syntax](../01-fundamentals/README.md) and [Module 02: Data Structures](../02-data-structures/README.md).
- **Next Up:** [Module 05: Object-Oriented Programming](../05-oop/README.md) for classes, dunder protocols, inheritance, and dataclasses.
- **Downstream Foundations:**
  - File I/O Track (`09-files-and-io`): Builds on binary buffers, encodings, and stream processing.
  - Security Track (`18-security`): Builds on ReDoS prevention, template injection defense, and input sanitization.
