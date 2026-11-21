# 08 Unicode & Locale

> Master internationalization, character encodings, UTF-8 byte decoding, C11 `<uchar.h>`, and locale management in standard C.

| Metadata | Details |
| :--- | :--- |
| **Level** | Intermediate |
| **Prerequisites** | [02-core-concepts/02-strings.md](../02-core-concepts/02-strings.md), [03-intermediate/05-bit-manipulation.md](05-bit-manipulation.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **Standard** | C11 / C17 / C23 |

---

## Table of Contents
1. [1. What & Why](#1-what--why)
2. [2. Core Concepts](#2-core-concepts)
3. [3. How It Works Internally](#3-how-it-works-internally)
4. [4. Syntax & Usage](#4-syntax--usage)
5. [5. Practical Examples](#5-practical-examples)
6. [6. Best Practices](#6-best-practices)
7. [7. Common Mistakes & Pitfalls](#7-common-mistakes--pitfalls)
8. [8. Performance & Security Notes](#8-performance--security-notes)
9. [9. Comparison / When to Use What](#9-comparison--when-to-use-what)
10. [10. Interview Questions](#10-interview-questions)
11. [11. Practice Exercises](#11-practice-exercises)
12. [12. Summary / Cheat Sheet](#12-summary--cheat-sheet)
13. [13. Further Reading](#13-further-reading)

---

## 1. What & Why

The C language was conceived in an era when 7-bit ASCII reigned supreme. A single `char` was assumed to represent exactly one human-readable character. In modern globalized computing, this assumption is completely broken:
- **Global Text**: Modern applications process multilingual text (Arabic, Chinese, Japanese, Cyrillic, Indic scripts, emoji, mathematical symbols).
- **Variable-Length Encoding**: Encodings like UTF-8 represent a single character using anywhere from 1 to 4 distinct bytes.
- **Locale Quirks**: Different geographical locales alter how numbers are formatted (e.g. `1,000.50` in the US vs `1.000,50` in Germany), how dates are ordered, and how collating sequences sort strings.
- **Portability Traps**: The size of `wchar_t` is not standardized: it is 16 bits (UTF-16) on Windows and 32 bits (UTF-32) on Linux and macOS.

Understanding how Unicode code points map to UTF-8 byte sequences, how to parse multi-byte streams defensively, and how to configure system locales without breaking numeric serialization is mandatory for systems developers.

---

## 2. Core Concepts

### 2.1 Unicode Terminology
- **Code Point**: An abstract integer value in the range `U+0000` to `U+10FFFF` assigned by the Unicode Consortium representing a discrete character or formatting control.
- **Code Unit**: The basic physical storage unit used by an encoding: 8 bits for UTF-8, 16 bits for UTF-16, 32 bits for UTF-32.
- **Grapheme Cluster**: What a human perceives as a single visual character, which may consist of multiple code points combined (e.g. `e` + combining acute accent `´` = `é`, or family emoji composed of multiple person code points connected with Zero-Width Joiners).
- **Byte Length vs Display Width**: `strlen()` counts physical bytes. The number of code points is different. The number of terminal display columns occupied by full-width characters (CJK) or zero-width modifiers is also different!

### 2.2 UTF-8 Encoding Scheme
UTF-8 is backward-compatible with 7-bit ASCII:
- Any byte with MSB `0` is a 1-byte ASCII character (`0x00`–`0x7F`).
- Multibyte characters begin with a leading byte (`110xxxxx`, `1110xxxx`, `11110xxx`) that explicitly indicates how many trailing bytes follow.
- All continuation bytes strictly start with binary `10xxxxxx` (`0x80`–`0xBF`).

```text
Scalar Range         Byte 1    Byte 2    Byte 3    Byte 4
---------------------------------------------------------
U+0000   .. U+007F   0xxxxxxx
U+0080   .. U+07FF   110xxxxx  10xxxxxx
U+0800   .. U+FFFF   1110xxxx  10xxxxxx  10xxxxxx
U+10000  .. U+10FFFF 11110xxx  10xxxxxx  10xxxxxx  10xxxxxx
```

### 2.3 C Standard Headers for Internationalization
1. `<locale.h>`: Controls formatting categories via `setlocale(category, locale_str)`:
   - `LC_ALL`: Affects all locale settings.
   - `LC_NUMERIC`: Decimal radix point and thousands separator.
   - `LC_CTYPE`: Character classification and multibyte conversion rules.
   - `LC_TIME`: Date and time formatting (`strftime`).
2. `<wchar.h>` & `<wctype.h>`: Wide character routines (`wchar_t`, `wprintf`, `iswalpha`).
3. `<uchar.h>` (Introduced in C11): Fixed-width character types:
   - `char16_t` (UTF-16 code units) and `char32_t` (UTF-32 code units).
   - `mbrtoc16()`, `c16rtomb()`, `mbrtoc32()`, `c32rtomb()`.

---

## 3. How It Works Internally

### UTF-8 Bit Extraction Pipeline
When decoding a 3-byte sequence (such as Japanese kanji `世` = `U+4E16`):
```
Raw UTF-8 Bytes in Memory:
Byte 0: 0xE4 -> 1110 0100 (Leading byte, 3 bytes total)
Byte 1: 0xB8 -> 1011 1000 (Continuation byte 1)
Byte 2: 0x96 -> 1001 0110 (Continuation byte 2)

Bit Extraction:
Byte 0 & 0x0F:      0100
Byte 1 & 0x3F:            111000
Byte 2 & 0x3F:                  010110
Reassembled:        0100  111000  010110
Hexadecimal:        0x4     E       1   6  -> U+4E16
```

### Overlong Encoding Security Hazard
A critical vulnerability in naive UTF-8 decoders is failing to reject **overlong sequences**. For instance, ASCII `/` (`0x2F`) can be illegally encoded in 2 bytes as `0xC0 0xAF`. If an application checks for `"../"` using byte matching but passes the string to a parser that decodes overlong sequences, directory traversal security checks are bypassed. Conforming decoders must reject any sequence that encodes a value smaller than the valid minimum for that byte length.

---

## 4. Syntax & Usage

### 4.1 Configuring the Host Locale
By default, every C program starts in the minimal `"C"` (POSIX ASCII) locale. To enable the user's system locale:
```c
#include <locale.h>
#include <stdio.h>

void init_locale(void) {
    /* Query and adopt the environment's locale settings */
    char *loc = setlocale(LC_ALL, "");
    if (loc) {
        printf("Active locale: %s\n", loc);
    } else {
        fprintf(stderr, "Failed to apply system locale\n");
    }
}
```

### 4.2 C23 Modernization: `char8_t` and `u8` Literals
In C11/C17, UTF-8 string literals `u8"hello"` had type `char[]`.
In ISO C23:
- Type of `u8"hello"` is `const unsigned char[]` (or `char8_t[]` if defined).
- Introduced UTF-8 character literals: `u8'A'`, representing an 8-bit unsigned integer value.

---

## 5. Practical Examples

### Comprehensive Example: Pure C UTF-8 Decoder & Code Point Counter
The following self-contained program implements a strict, secure UTF-8 decoder that validates byte boundaries, rejects overlong encodings and surrogate halves, and extracts Unicode scalar values.

```c
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* UTF-8 decoder and length counter */
typedef struct {
    uint32_t codepoint;
    int bytes_read;
    bool is_valid;
} Utf8Char;

static Utf8Char utf8_decode_next(const uint8_t *s, size_t max_bytes) {
    Utf8Char result = {0, 0, false};
    if (max_bytes == 0) return result;

    uint8_t b0 = s[0];
    if (b0 <= 0x7F) {
        /* 1-byte ASCII (0xxxxxxx) */
        result.codepoint = b0;
        result.bytes_read = 1;
        result.is_valid = true;
    } else if ((b0 & 0xE0) == 0xC0) {
        /* 2-byte sequence (110xxxxx 10xxxxxx) */
        if (max_bytes < 2 || (s[1] & 0xC0) != 0x80) return result;
        uint32_t cp = (uint32_t)((b0 & 0x1F) << 6) | (s[1] & 0x3F);
        if (cp < 0x80) return result; /* Overlong */
        result.codepoint = cp;
        result.bytes_read = 2;
        result.is_valid = true;
    } else if ((b0 & 0xF0) == 0xE0) {
        /* 3-byte sequence (1110xxxx 10xxxxxx 10xxxxxx) */
        if (max_bytes < 3 || (s[1] & 0xC0) != 0x80 || (s[2] & 0xC0) != 0x80) return result;
        uint32_t cp = (uint32_t)((b0 & 0x0F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F);
        if (cp < 0x800 || (cp >= 0xD800 && cp <= 0xDFFF)) return result; /* Overlong or surrogate */
        result.codepoint = cp;
        result.bytes_read = 3;
        result.is_valid = true;
    } else if ((b0 & 0xF8) == 0xF0) {
        /* 4-byte sequence (11110xxx 10xxxxxx 10xxxxxx 10xxxxxx) */
        if (max_bytes < 4 || (s[1] & 0xC0) != 0x80 || (s[2] & 0xC0) != 0x80 || (s[3] & 0xC0) != 0x80) return result;
        uint32_t cp = (uint32_t)((b0 & 0x07) << 18) | ((s[1] & 0x3F) << 12) | ((s[2] & 0x3F) << 6) | (s[3] & 0x3F);
        if (cp < 0x10000 || cp > 0x10FFFF) return result; /* Overlong or out-of-range */
        result.codepoint = cp;
        result.bytes_read = 4;
        result.is_valid = true;
    }
    return result;
}

int main(void) {
    printf("=== Unicode and UTF-8 Decoding in C ===\n");

    /* "Hello, \u4e16\u754c!" ("Hello, 世界!") in UTF-8 hex */
    const uint8_t sample[] = "Hello, \xE4\xB8\x96\xE7\x95\x8C!";
    size_t byte_len = strlen((const char *)sample);

    printf("Input byte length: %u bytes\n", (unsigned int)byte_len);
    printf("Decoded Unicode Code Points:\n");

    const uint8_t *ptr = sample;
    size_t remaining = byte_len;
    size_t codepoint_count = 0;

    while (remaining > 0) {
        Utf8Char ch = utf8_decode_next(ptr, remaining);
        if (!ch.is_valid) {
            printf("  [Malformed UTF-8 sequence at byte offset %u]\n", (unsigned int)(ptr - sample));
            break;
        }
        printf("  U+%04X (takes %d bytes)\n", (unsigned int)ch.codepoint, ch.bytes_read);
        ptr += ch.bytes_read;
        remaining -= (size_t)ch.bytes_read;
        codepoint_count++;
    }

    printf("Total valid Unicode code points: %u\n", (unsigned int)codepoint_count);
    return 0;
}
```

#### Compilation & Verified Execution Output
```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -Werror 08-unicode-locale.c -o 08-unicode-locale.exe
./08-unicode-locale.exe
```

```text
=== Unicode and UTF-8 Decoding in C ===
Input byte length: 14 bytes
Decoded Unicode Code Points:
  U+0048 (takes 1 bytes)
  U+0065 (takes 1 bytes)
  U+006C (takes 1 bytes)
  U+006C (takes 1 bytes)
  U+006F (takes 1 bytes)
  U+002C (takes 1 bytes)
  U+0020 (takes 1 bytes)
  U+4E16 (takes 3 bytes)
  U+754C (takes 3 bytes)
  U+0021 (takes 1 bytes)
Total valid Unicode code points: 10
```

---

## 6. Best Practices

1. **Adopt "UTF-8 Everywhere"**: Store, process, and transmit all internal strings as UTF-8 in standard `char *` arrays. Avoid `wchar_t` for internal representation due to cross-platform size mismatches.
2. **Never Truncate UTF-8 Arbitrarily**: If cutting a string to fit a buffer limit, never slice across a multibyte sequence. Rewind past continuation bytes (`(b & 0xC0) == 0x80`) to find the leading byte.
3. **Keep `LC_NUMERIC` in `"C"` for Data Serialization**: Calling `setlocale(LC_ALL, "")` in libraries breaks JSON, XML, and SQL generators because floating point numbers may output commas instead of decimal dots (`3,14` instead of `3.14`).
4. **Reject Overlong and Surrogate Sequences**: Security-critical decoders must explicitly check that codepoints do not fall into surrogate half ranges (`0xD800`–`0xDFFF`) or represent overlong encodings.
5. **Use C11 `<uchar.h>` for Explicit Fixed-Width Needs**: Prefer `char32_t` over `wchar_t` when true 32-bit Unicode scalar values are needed.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It's Wrong | The Fix |
| :--- | :--- | :--- |
| `size_t chars = strlen(s);` | `strlen()` measures byte count up to ` `, not the count of visible characters or code points. | Use a UTF-8 decoding loop to count code points or graphemes. |
| Assuming `sizeof(wchar_t) == 4` | On Windows, `wchar_t` is 16 bits (UTF-16); on Linux, it is 32 bits (UTF-32). | Use `char32_t` from `<uchar.h>` for portable 32-bit codepoints. |
| Calling `strtod()` after `setlocale(LC_ALL, "")` | In European locales, `strtod` expects `,` as decimal point, causing `"3.14"` to parse as `3.0`. | Use `uselocale()` (POSIX) or keep `LC_NUMERIC` set to `"C"`. |
| Slicing a string with `s[maxlen] = ' '` | May truncate in the middle of a 2-4 byte UTF-8 sequence, creating corrupt invalid text. | Backtrack until `(s[i] & 0xC0) != 0x80`. |
| Trusting unvalidated UTF-8 in path resolution | Overlong encodings like `0xC0 0xAF` for `/` can bypass security validation checks. | Validate UTF-8 strictly before path canonicalization. |
| Indexing by character with `str[i]` | Assumes 1 byte per character. Accessing byte index `i` yields incomplete fragments of multibyte glyphs. | Iterate through codepoints using a decoding parser. |

---

## 8. Performance & Security Notes

- **SIMD Validation**: Modern high-performance servers (e.g. Node.js, Envoy proxy) validate gigabytes of UTF-8 text per second using AVX-512 / ARM NEON vector instructions, checking 64 bytes per instruction cycle.
- **Normalization Hazards**: Unicode allows identical characters to be represented in multiple forms (e.g. precomposed `é` `U+00E9` vs decomposed `e` + combining acute accent `U+0065 U+0301`). Direct string equality checks (`strcmp`) will return non-zero even though the characters look identical. Security applications must perform Unicode normalization (NFC/NFKC).
- **Homograph Spoofing**: Cyrillic `а` (`U+0430`) looks identical to Latin `a` (`U+0061`). Without visual collision detection, attackers can spoof domain names or user identities.

---

## 9. Comparison / When to Use What

| Encoding | Code Unit | Endian Sensitive? | RAM Overhead (ASCII) | Portability | Recommended Use |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **UTF-8** | 8 bits | No | Zero (1 byte/char) | Universal | Industry standard; all wire & storage text |
| **UTF-16** | 16 bits | Yes (LE/BE BOM) | 100% (2 bytes/char) | Windows / Java | Only for Win32 API (`wchar_t`) interop |
| **UTF-32** | 32 bits | Yes (LE/BE BOM) | 300% (4 bytes/char) | Linux `wchar_t` | Constant-time random access by code point |
| **ASCII** | 7 bits | No | None | Universal | Legacy legacy protocols only |

---

## 10. Interview Questions

<details>
<summary><strong>1. Why is `strlen()` insufficient for calculating user-visible string length in UTF-8?</strong></summary>

`strlen()` counts the number of physical `char` bytes preceding the null terminator. In UTF-8, ASCII characters take 1 byte, but accented letters take 2 bytes, East Asian scripts take 3 bytes, and emojis take 4 bytes. Moreover, combining diacritics and zero-width joiners consume bytes without adding visible width, while East Asian full-width characters occupy 2 terminal columns. Thus, byte length (`strlen`), code point count, and display column width (`wcwidth`) are three completely distinct metrics.
</details>

<details>
<summary><strong>2. How does UTF-8 guarantee self-synchronization?</strong></summary>

In UTF-8, the leading byte of a multibyte sequence and all subsequent continuation bytes have distinct, mutually exclusive bit patterns: leading bytes start with `0xxxxxxx` or `11xxxxxx`, while all continuation bytes start with `10xxxxxx`. If a decoder begins reading from an arbitrary random byte offset in a corrupted or truncated stream, it can easily locate the start of the next valid character simply by skipping forward until it encounters a byte that does not start with binary `10`.
</details>

<details>
<summary><strong>3. What is an overlong UTF-8 encoding and why is it a security vulnerability?</strong></summary>

An overlong encoding occurs when a character is represented using more bytes than necessary (for example, encoding ASCII `NUL` `0x00` as the 2-byte sequence `0xC0 0x80`). If a web server or firewall inspects a URL looking for malicious characters (like `../` or SQL delimiters) using ASCII matching, the check passes. If an internal component later decodes the overlong sequence, the malicious character appears, allowing path traversal or injection attacks. Conforming decoders must reject overlong encodings.
</details>

<details>
<summary><strong>4. Why is `wchar_t` non-portable between Windows and Linux?</strong></summary>

The C standard does not specify the width of `wchar_t`. On Windows, Microsoft chose a 16-bit `wchar_t` to align with UCS-2/UTF-16. On Linux and macOS, `wchar_t` is 32 bits, representing UTF-32. Consequently, code assuming `wchar_t` can hold any Unicode code point directly works on Linux, but truncates supplementary plane characters (like emojis `U+10000`+) on Windows. C11 introduced `char16_t` and `char32_t` to resolve this ambiguity.
</details>

<details>
<summary><strong>5. How does calling `setlocale(LC_ALL, "")` break standard JSON or CSV serialization?</strong></summary>

In many European locales (such as German or French), the decimal separator is a comma (`,`) rather than a period (`.`). Once `setlocale(LC_ALL, "")` is active, standard functions like `sprintf(buf, "%f", 3.14)` produce `"3,14"`. When written to a JSON file or CSV document, the comma breaks the document structure, causing external parsers to fail. Robust libraries should isolate `LC_NUMERIC` or use thread-safe locale functions like `newlocale()` and `uselocale()`.
</details>

<details>
<summary><strong>6. What are Unicode surrogate code points, and why must they never appear in valid UTF-8?</strong></summary>

Code points in the range `U+D800` through `U+DFFF` are reserved exclusively for UTF-16 to encode characters in supplementary planes as surrogate pairs. They do not represent characters in isolation. The Unicode standard explicitly forbids encoding surrogate code points in UTF-8. A compliant decoder must flag any 3-byte sequence attempting to decode into `0xD800`–`0xDFFF` as invalid.
</details>

<details>
<summary><strong>7. How does C23 treat UTF-8 string literals compared to C11/C17?</strong></summary>

In C11/C17, a UTF-8 literal `u8"hello"` had type `char[]`, which could cause signedness warnings when passing bytes to functions expecting `unsigned char *`. In ISO C23, the type of a UTF-8 string literal is `const unsigned char[]` (or `char8_t[]`), enforcing type distinction. In addition, C23 added UTF-8 character literals `u8'x'`, which have type `unsigned char`.
</details>

<details>
<summary><strong>8. What is the difference between Unicode NFC and NFD normalization?</strong></summary>

NFC (Normalization Form C) applies canonical decomposition followed by canonical composition, producing precomposed characters (e.g. single code point `é` `U+00E9`). NFD (Normalization Form D) decomposes characters into base characters and combining marks (e.g. two code points: base `e` `U+0065` followed by combining acute `U+0301`). Two strings in different normalization forms sound and look identical to humans, but a byte-for-byte `strcmp()` reports them as unequal.
</details>

---

## 11. Practice Exercises

### Easy: Check if a Byte is a UTF-8 Continuation Byte
Write an inline function `bool is_utf8_continuation(uint8_t byte)` that returns true if the byte is a continuation byte (`10xxxxxx`).

<details>
<summary>Solution</summary>

```c
#include <stdint.h>
#include <stdbool.h>

static inline bool is_utf8_continuation(uint8_t byte) {
    return (byte & 0xC0) == 0x80;
}
```
</details>

### Medium: Count Valid Code Points in a UTF-8 String
Write a function `size_t utf8_strlen(const char *s)` that returns the count of Unicode code points in a valid UTF-8 string without using external libraries.

<details>
<summary>Solution</summary>

```c
#include <stddef.h>
#include <stdint.h>

size_t utf8_strlen(const char *s) {
    if (!s) return 0;
    size_t count = 0;
    const uint8_t *p = (const uint8_t *)s;

    while (*p != ' ') {
        /* Continuation bytes do not represent the start of a code point */
        if ((*p & 0xC0) != 0x80) {
            count++;
        }
        p++;
    }

    return count;
}
```
</details>

### Hard: Safe In-Place UTF-8 Truncator
Write a function `void utf8_truncate_safe(char *s, size_t max_bytes)` that truncates a UTF-8 string so that it occupies at most `max_bytes` without slicing through the middle of any multibyte character sequence.

<details>
<summary>Solution</summary>

```c
#include <string.h>
#include <stdint.h>

void utf8_truncate_safe(char *s, size_t max_bytes) {
    if (!s) return;
    size_t len = strlen(s);
    if (len <= max_bytes) return;

    /* Start at desired cut point */
    size_t cut = max_bytes;

    /* If cut lands on a continuation byte, rewind to leading byte */
    while (cut > 0 && (((uint8_t)s[cut]) & 0xC0) == 0x80) {
        cut--;
    }

    /* Now determine length of sequence at cut */
    uint8_t leading = (uint8_t)s[cut];
    size_t seq_len = 1;
    if ((leading & 0xE0) == 0xC0) seq_len = 2;
    else if ((leading & 0xF0) == 0xE0) seq_len = 3;
    else if ((leading & 0xF8) == 0xF0) seq_len = 4;

    /* If the entire multibyte sequence cannot fit in max_bytes, cut before it */
    if (cut + seq_len > max_bytes) {
        s[cut] = ' ';
    } else {
        s[cut + seq_len] = ' ';
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

| Category | Primitive / Concept | Rule |
| :--- | :--- | :--- |
| ASCII Range | `0x00`–`0x7F` | 1 byte, identical to 7-bit ASCII |
| Continuation Byte | `0x80`–`0xBF` (`10xxxxxx`) | Strictly follows a leading byte |
| 2-Byte Lead | `0xC2`–`0xDF` (`110xxxxx`) | `0xC0`–`0xC1` are illegal (overlong) |
| 3-Byte Lead | `0xE0`–`0xEF` (`1110xxxx`) | `0xD800`–`0xDFFF` surrogates illegal |
| 4-Byte Lead | `0xF0`–`0xF4` (`11110xxx`) | Max valid codepoint is `U+10FFFF` |
| Locale Reset | `setlocale(LC_ALL, "C")` | Deterministic ASCII / dot decimal |

---

## 13. Further Reading

- The Unicode Standard, Version 15.0 — Chapter 3: Conformance (UTF-8 definition & constraints).
- UTF-8 Everywhere Manifesto (https://utf8everywhere.org/).
- ISO/IEC 9899:2018 (C17) §7.11 (`<locale.h>`), §7.28 (`<uchar.h>`).
- SEI CERT C Coding Standard: STR38-C (Do not confuse narrow and wide character strings and functions).

---

[← Previous](07-text-processing-parsing.md) | [Section Index](README.md) | [Next →](09-time-and-random.md)
