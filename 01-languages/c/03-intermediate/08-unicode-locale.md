# Unicode and Locale

C's `char` is a byte, and its string functions know nothing about human languages. Modern text is Unicode, usually encoded as UTF-8. This note explains what that means in practice, which C features help, and where the pitfalls are.

**Prerequisites:** [Strings](../02-core-concepts/02-strings.md), [integer representation](../01-fundamentals/03-integer-representation.md).

## Three different "lengths"

| Unit | Example: `"é"` | Notes |
|---|---|---|
| **Bytes** (what C counts) | 2 in UTF-8 | `strlen` returns this |
| **Code points** | 1 (U+00E9) | what Unicode assigns numbers to |
| **Grapheme clusters** (what users see as one character) | 1 | `"e"` + combining accent is 2 code points but 1 visible character |

Most C code only needs bytes. When you need characters (truncating for display, cursor movement, text editors), you need to understand UTF-8.

## UTF-8 in one table

UTF-8 encodes each code point in 1-4 bytes:

| Code point range | Bytes | Bit pattern |
|---|---|---|
| U+0000 - U+007F | 1 | `0xxxxxxx` |
| U+0080 - U+07FF | 2 | `110xxxxx 10xxxxxx` |
| U+0800 - U+FFFF | 3 | `1110xxxx 10xxxxxx 10xxxxxx` |
| U+10000 - U+10FFFF | 4 | `11110xxx 10xxxxxx 10xxxxxx 10xxxxxx` |

Useful properties: ASCII is unchanged; `'\0'` never appears inside a multi-byte sequence, so `strlen`, `strcpy`, `strcmp` (byte order) and `printf("%s")` work on UTF-8 strings; continuation bytes always look like `10xxxxxx`, so you can find character boundaries.

```c
/* Count code points in a (valid) UTF-8 string: skip continuation bytes */
size_t utf8_length(const char *s) {
    size_t n = 0;
    for (; *s; s++) {
        if (((unsigned char)*s & 0xC0) != 0x80) n++;
    }
    return n;
}
```

### Decoding one code point, with validation

```c
#include <stdint.h>
#include <stddef.h>

/* Returns bytes consumed (1-4), or 0 if the sequence is invalid. */
int utf8_decode(const unsigned char *s, size_t n, uint32_t *cp) {
    if (n == 0) return 0;
    unsigned char c = s[0];
    if (c < 0x80) { *cp = c; return 1; }

    int len; uint32_t v, min;
    if      ((c & 0xE0) == 0xC0) { len = 2; v = c & 0x1F; min = 0x80;    }
    else if ((c & 0xF0) == 0xE0) { len = 3; v = c & 0x0F; min = 0x800;   }
    else if ((c & 0xF8) == 0xF0) { len = 4; v = c & 0x07; min = 0x10000; }
    else return 0;

    if ((size_t)len > n) return 0;
    for (int i = 1; i < len; i++) {
        if ((s[i] & 0xC0) != 0x80) return 0;
        v = (v << 6) | (s[i] & 0x3F);
    }
    if (v < min || v > 0x10FFFF || (v >= 0xD800 && v <= 0xDFFF)) return 0;   /* overlong, range, surrogate */
    *cp = v;
    return len;
}
```

Never trust external bytes to be valid UTF-8; invalid or overlong sequences have been used to bypass security filters.

### Pitfalls with UTF-8 and `char`

- **Truncating** with `strncpy`/`snprintf` can cut a sequence in half. Back up to a boundary (skip bytes of the form `10xxxxxx`) before terminating.
- `s[i]` is a byte, not a character; indexing and `reverse` loops break multi-byte characters.
- `toupper`, `isalpha`, `tolower` work on single bytes (and the current locale), **not** Unicode. Case-insensitive comparison, collation, and normalization (`é` as one code point vs two) need a library such as **ICU** or **utf8proc**.
- `strcmp` orders by bytes, which is not alphabetical order for most languages.

## Wide and Unicode character types

| Type | Header | Meaning |
|---|---|---|
| `wchar_t` | `<wchar.h>` | wide char; **16-bit on Windows, 32-bit on Linux/macOS**, so it is not portable for Unicode |
| `char16_t`, `char32_t` | `<uchar.h>` (C11) | UTF-16 and UTF-32 code units, with `u"..."` and `U"..."` literals |
| `char8_t` | `<uchar.h>` (C23) | UTF-8 code unit; `u8"..."` literals (type `char8_t[]` in C23, `char[]` earlier) |

Most modern C programs store text as **UTF-8 in `char` arrays** and convert at the edges (for example to UTF-16 for Windows APIs). `wchar_t`-based code mostly exists for legacy reasons.

`mbstowcs`, `wcstombs`, `mbrtowc` convert between the multibyte encoding of the current locale and `wchar_t`; they depend on the locale being set.

## Locale

A locale is a set of cultural conventions: decimal separator, date format, character classification, and sort order. A C program starts in the minimal `"C"` locale; `setlocale` changes it:

```c
#include <locale.h>

setlocale(LC_ALL, "");      /* adopt the user's environment settings (LANG, LC_*) */
```

| Category | Affects |
|---|---|
| `LC_CTYPE` | character classification, multibyte conversion |
| `LC_NUMERIC` | decimal point in `printf`/`strtod` |
| `LC_COLLATE` | `strcoll` ordering |
| `LC_TIME` | `strftime` |
| `LC_MONETARY` | `localeconv` currency data |

### Locale pitfalls

- With a German locale, `printf("%f", 1.5)` prints `1,5` and `strtod("1.5", ...)` stops at the dot. **Data files, protocols, and JSON must not depend on locale.** Keep the default `"C"` locale for `LC_NUMERIC`, or call `setlocale(LC_NUMERIC, "C")`.
- `setlocale` changes global, process-wide state and isn't thread-safe. **Libraries should not call it**; leave that to the application.
- A program that sets `setlocale(LC_ALL, "")` and then parses ASCII data can behave differently on users' machines than on yours.
- Sorting with `strcoll` instead of `strcmp` gives human order but is slower and locale-dependent.

## Output and terminals

- Source files should be saved as UTF-8; string literals then contain UTF-8 bytes and print correctly on UTF-8 terminals.
- Windows console encoding differs; use `SetConsoleOutputCP(CP_UTF8)` or Windows' UTF-8 support options.
- Column alignment with `%-10s` counts **bytes**, so accented or CJK text misaligns. Terminal width is a separate problem (`wcwidth`).

## Other tools

- **`iconv`** (POSIX) converts between encodings.
- **ICU**, **utf8proc**, and **libunistring** handle normalization, case mapping, collation, and segmentation.

## Common mistakes

| Mistake | Effect |
|---|---|
| Treating `strlen` as character count | Wrong for non-ASCII |
| Cutting UTF-8 mid-sequence | Invalid output, mojibake |
| Using `wchar_t` as portable Unicode | 16 vs 32 bit differences |
| `toupper` on UTF-8 bytes | Corrupts multi-byte characters |
| Calling `setlocale(LC_ALL, "")` and then parsing numeric data | Decimal-comma bugs |
| Assuming valid UTF-8 from input | Security and crash risk |

## Summary

- C counts bytes; UTF-8 text works with standard string functions as long as you don't split sequences.
- Validate input and use a library for case, collation, and normalization.
- `wchar_t` isn't portable; prefer UTF-8 in `char` and convert at boundaries.
- Locale changes number formatting and classification; keep data formats locale-independent.

**Next:** [Time and random numbers](./09-time-and-random.md)
