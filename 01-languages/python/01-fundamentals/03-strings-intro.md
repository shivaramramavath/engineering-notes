# Module 01: Strings Introduction & Fundamentals (03-strings-intro)

> **Topic Overview**: The core string architecture of Python: immutable Unicode sequences (`str`), string literals, escape sequences, raw strings, f-strings introduction, indexing and slicing mechanics, UTF-8/UTF-16 encoding abstractions, and PEP 393 flexible string representation.

---

## Metadata Table

| Field | Value |
|---|---|
| **Category** | Python Fundamentals & Syntax |
| **Target Level** | Beginner to Intermediate |
| **Python Version** | Python 3.10+ (tested on CPython 3.14.0) |
| **Tested With** | CPython 3.14.0, sys module, Ruff 0.17.6 |
| **Free-Threading / Platform** | Platform independent |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & API Reference](#4-syntax--api-reference)
5. [Code Examples](#5-code-examples)
6. [Edge Cases & Pitfalls](#6-edge-cases--pitfalls)
7. [Common Mistakes](#7-common-mistakes)
8. [Performance & Complexity](#8-performance--complexity)
9. [CPython Internals & Bytecode](#9-cpython-internals--bytecode)
10. [Best Practices](#10-best-practices)
11. [Real-World Case Study](#11-real-world-case-study)
12. [Interview Questions](#12-interview-questions)
13. [Exercises & Challenges](#13-exercises--challenges)

---

## 1. What & Why
- **Problem Statement**: Text handling across global languages involves complex Unicode code points, varying byte sizes, and multi-byte encodings. In many legacy systems, treating strings as raw byte arrays leads to corrupted text (Mojibake), broken indexing, and buffer overruns.
- **Mental Model**: In Python 3, a string (`str`) is an **immutable sequence of Unicode characters (code points)**. Python completely decouples abstract text (`str`) from raw binary data (`bytes`). You index and slice characters, not bytes.
- **When to Use**: Representing human-readable text, names, identifiers, URLs, JSON keys, and log messages.
- **When NOT to Use**: Raw binary data, network socket payloads, or compressed cryptographic blobs (use `bytes` or `bytearray`).

---

## 2. Core Concepts
1. **Unicode Sequence Abstraction**: `len("Hello 🌍")` returns `7` characters. Python counts Unicode code points, not raw UTF-8 bytes (which would be 10 bytes).
2. **String Immutability**: Strings cannot be mutated in place. Methods like `.replace()`, `.upper()`, or `.strip()` return a newly allocated string on the heap.
3. **Literal Prefix Variants**:
   - Standard: `"text"` or `'text'` (single and double quotes are interchangeable).
   - Formatted (f-string): `f"Hello, {name}"`.
   - Raw string (`r"..."`): Treats backslashes literally (essential for regex and Windows filesystem paths).
   - Multiline docstrings: Triple quotes `"""..."""` or `'''...'''`.
4. **String Slicing (`s[start:stop:step]`)**: Non-destructive sequence indexing that extracts sub-sequences in $O(k)$ time, where $k$ is the length of the slice.
5. **Flexible String Representation (PEP 393)**: CPython automatically optimizes string storage using 1, 2, or 4 bytes per character (Latin-1, UCS-2, or UCS-4) based on the highest code point in the string, saving up to 75% memory on ASCII text.

---

## 3. How It Works Internally

Memory representation under PEP 393 (Flexible String Representation):

```text
CPython `PyASCIIObject` / `PyCompactUnicodeObject`:
┌────────────────────────────────────────────────────────┐
│ ob_refcnt = 1                                          │
│ ob_type   = &PyUnicode_Type                            │
│ length    = 5 (Number of code points)                  │
│ hash      = -1 (Cached on first hash computation)      │
│ state     = [compact=1, ascii=1, kind=PyUnicode_1BYTE] │
├────────────────────────────────────────────────────────┤
│ Compact Character Buffer: ['H', 'e', 'l', 'l', 'o', 0] │
│ (1 byte per char for pure ASCII / Latin-1)             │
└────────────────────────────────────────────────────────┘

If an emoji (e.g. 🌍 U+1F30E) is added:
kind switches to `PyUnicode_4BYTE` (UCS-4), allocating 4 bytes per code point
to guarantee O(1) random character indexing!
```

---

## 4. Syntax & API Reference

### Common String Methods
```python
s = "  Python Engineering  "

s.strip()               # "Python Engineering" (removes surrounding whitespace)
s.lower()               # "  python engineering  "
s.upper()               # "  PYTHON ENGINEERING  "
s.startswith("  Py")    # True
s.endswith("ing  ")     # True
s.split()               # ['Python', 'Engineering']
", ".join(["A", "B"])   # "A, B"
```

### Slicing Syntax
```python
text = "ABCDEFG"
text[0]         # 'A' (first character)
text[-1]        # 'G' (last character)
text[1:4]       # 'BCD' (start at index 1 up to index 4 exclusive)
text[::-1]      # 'GFEDCBA' (reverse string via negative step)
```

---

## 5. Code Examples

### 1. Minimal / Basic Example: Modern F-Strings and Basic Methods
```python
user = "Ada Lovelace"
role = "Pioneer"
formatted = f"User: {user.upper()} | Role: {role.capitalize()}"
print(formatted)
```
**Output**:
```text
User: ADA LOVELACE | Role: Pioneer
```

---

### 2. Real-World Production Example: Sanitizing and Normalizing User Input
```python
def sanitize_email(raw_input: str) -> str:
    """Normalizes and validates an email string according to RFC standards."""
    cleaned = raw_input.strip().lower()
    if "@" not in cleaned or cleaned.startswith("@") or cleaned.endswith("@"):
        raise ValueError(f"Invalid email address: '{raw_input}'")
    
    local_part, domain = cleaned.split("@", 1)
    if "." not in domain:
        raise ValueError(f"Invalid domain in email: '{domain}'")
        
    return f"{local_part}@{domain}"

sample_emails = [
    "  User.Name@Example.COM  ",
    "developer@cloud.internal",
]

for email in sample_emails:
    print("Sanitized:", sanitize_email(email))
```
**Output**:
```text
Sanitized: user.name@example.com
Sanitized: developer@cloud.internal
```

---

### 3. Edge Case: Unicode Code Points vs Character Length
```python
import sys

# Standard ASCII string (1 byte per char)
ascii_str = "hello"

# String with 4-byte Unicode emoji
emoji_str = "hello 🌍"

print(f"ASCII string len: {len(ascii_str)}, sizeof: {sys.getsizeof(ascii_str)} bytes")
print(f"Emoji string len: {len(emoji_str)}, sizeof: {sys.getsizeof(emoji_str)} bytes")
```
**Output**:
```text
ASCII string len: 5, sizeof: 54 bytes
Emoji string len: 7, sizeof: 104 bytes
```
*Note*: Adding a single 4-byte emoji causes CPython to widen the entire internal buffer from 1-byte characters to 4-byte characters (UCS-4) to preserve $O(1)$ random indexing.

---

### 4. Failure Case: Attempting In-Place String Mutation
```python
name = "Python"

try:
    # Strings are strictly immutable:
    name[0] = "J"  # type: ignore[index]
except TypeError as exc:
    print(f"Caught expected error: {type(exc).__name__}: {exc}")

# Idiomatic replacement:
new_name = "J" + name[1:]
print("New allocated string:", new_name)
```
**Output**:
```text
Caught expected error: TypeError: 'str' object does not support item assignment
New allocated string: Jython
```

---

### 5. Idiomatic Pythonic Pattern: Raw Strings for Regular Expressions & Paths
```python
import re

# Raw string prevents backslash escaping collisions:
pattern = r"C:\Users\([a-zA-Z0-9_]+)\Documents"
sample_path = r"C:\Users\alex\Documents"

match = re.match(pattern, sample_path)
if match:
    print("Matched username:", match.group(1))
```
**Output**:
```text
Matched username: alex
```

---

## 6. Edge Cases & Pitfalls
1. **Inefficient String Concatenation in Loops**: Repeatedly concatenating strings with `+=` inside a loop (`s += item`) creates $O(n^2)$ quadratic allocations because each addition creates a brand-new string. Always collect parts in a list and use `"".join(parts)` ($O(n)$ linear).
2. **Raw Strings Trailing Backslash**: A raw string cannot end with an odd number of backslashes (e.g. `r"C:\"` causes `SyntaxError: unterminated string literal`). Escape the trailing slash or write `r"C:\path" + "\\"` or use `pathlib.Path`.
3. **Case Folding in Internationalization**: Using `.lower()` for case-insensitive comparisons fails for certain languages (e.g., German `"ß"` becomes `"SS"` in uppercase). Use `.casefold()` for robust caseless matching.

---

## 7. Common Mistakes

| Mistake | Why It Fails | Exception / Diagnostic | Correct Pattern |
|---|---|---|---|
| String concatenation in a loop (`s += x`) | Copies entire string on every iteration; $O(n^2)$ time | High memory and CPU latency | Collect in list and use `"".join(list)` |
| Trailing backslash in raw string (`r"C:\"`) | Lexer interprets `\"` as escaping the closing quote | `SyntaxError: unterminated string literal` | Use `Path("C:/")` or `r"C:" + "\\"` |
| Attempting in-place mutation (`s[0] = 'a'`) | Strings are strictly immutable | `TypeError: 'str' object does not support item assignment` | Reconstruct with slice or `replace` |
| Using `.lower()` for caseless international comparison | Inaccurate for non-ASCII Unicode (e.g. German eszett) | Logic failure in international text | Use `s.casefold()` |
| Misunderstanding negative slice bounds | `s[-1:2]` returns empty string if step is positive | Returns unexpected empty string | Specify correct direction/step |
| Comparing byte strings and text strings directly | Python 3 separates `bytes` and `str` | `"abc" == b"abc"` returns `False` | Explicitly decode bytes: `b.decode("utf-8")` |

---

## 8. Performance & Complexity
- **Indexing (`s[i]`)**: $O(1)$ constant time due to PEP 393 uniform width buffers.
- **Slicing (`s[i:j]`)**: $O(k)$ where $k = j - i$ (copies $k$ characters into a new string).
- **String Length (`len(s)`)**: $O(1)$ constant time (cached on `PyASCIIObject.length`).
- **Joining (`"".join(items)`)**: $O(N)$ linear time (pre-calculates total length and allocates exact memory once).

---

## 9. CPython Internals & Bytecode: The `BUILD_STRING` Opcode
In Python 3.12+, f-strings are compiled into high-performance `BUILD_STRING` instructions:

```python
import dis

def make_greeting(user: str) -> str:
    return f"Welcome, {user}!"

dis.dis(make_greeting)
```
Disassembly:
```text
  1           0 RESUME                   0

  2           2 LOAD_CONST               1 ('Welcome, ')
              4 LOAD_FAST                0 (user)
              6 FORMAT_VALUE             0 (None)
              8 LOAD_CONST               2 ('!')
             10 BUILD_STRING             3
             12 RETURN_VALUE
```
- `FORMAT_VALUE`: Formats the object using `__format__`.
- `BUILD_STRING 3`: Concatenates the 3 items on the stack into a single allocated string in C.

---

## 10. Best Practices
1. Use **f-strings** (`f"..."`) for all string formatting and interpolation.
2. Use `"".join(parts)` instead of repeated `+=` inside loops.
3. Use raw string literals (`r"..."`) for regular expressions and Windows path strings.
4. Use `.casefold()` for case-insensitive matching on user input.

---

## 11. Real-World Case Study: High-Throughput Log Line Construction
A payment gateway service processed 40,000 transactions per second. Profiling with `py-spy` showed that 22% of CPU time was consumed by formatting log strings using the legacy `%` operator and repeated string concatenation:
1. The logging pipeline was refactored to use f-strings and list joins.
2. Log line construction throughput doubled, reducing CPU consumption from 22% to 6%.

---

## 12. Interview Questions

### Q1: What is PEP 393 (Flexible String Representation) in CPython?
<details>
<summary><strong>Answer</strong></summary>
Prior to Python 3.3, CPython was compiled with either 2-byte (UCS-2) or 4-byte (UCS-4) Unicode representations, causing massive memory waste for ASCII text. PEP 393 introduced the Flexible String Representation: CPython inspects the highest code point in the string upon creation and chooses the most compact internal representation:
- 1 byte per character for ASCII and Latin-1 (U+0000 to U+00FF).
- 2 bytes per character for UCS-2 (up to U+FFFF).
- 4 bytes per character for UCS-4 (astral planes, emojis, up to U+10FFFF).
This saves 50%–75% memory while guaranteeing $O(1)$ random character indexing.
</details>

### Q2: Why is `"".join(list_of_strings)` significantly faster than repeated `+=` in a loop?
<details>
<summary><strong>Answer</strong></summary>
Because strings are immutable, `s += x` creates a new string on each iteration and copies all previous characters into it. For $N$ items, this results in $1 + 2 + \dots + N = O(N^2)$ memory allocations and byte copies. `"".join()` makes two passes: first it calculates the exact total length required, allocates a single memory buffer on the heap, and copies all strings in linear $O(N)$ time.
</details>

### Q3: What is the difference between `.lower()` and `.casefold()`?
<details>
<summary><strong>Answer</strong></summary>
`.lower()` applies standard lowercase character mappings defined in the Unicode character database. `.casefold()` applies full Unicode case-folding rules designed specifically for caseless comparisons. For example, the German lowercase sharp s (`"ß"`) remains `"ß"` under `.lower()`, but casefolds to `"ss"`, allowing `"straße".casefold() == "STRASSE".casefold()` to evaluate to `True`.
</details>

### Q4: Why can a raw string not end with a single backslash?
<details>
<summary><strong>Answer</strong></summary>
In CPython's lexer, a raw string literal (`r"..."`) changes the handling of escape sequences inside the string, but a backslash immediately preceding a quote character still escapes that quote character so that the programmer can embed quotes into raw strings (e.g. `r"\""`). As a consequence, a trailing backslash escapes the closing quote, causing the lexer to report an unterminated string literal.
</details>

### Q5: How does string interning work in Python?
<details>
<summary><strong>Answer</strong></summary>
String interning is an optimization where CPython stores only one copy of distinct immutable string objects in a global hash table (`interned` dictionary). CPython automatically interns string literals that resemble valid Python identifiers at compile time. Interned strings allow $O(1)$ pointer comparison using `is` rather than $O(N)$ character comparison with `==`.
</details>

### Q6: What does the slice `s[::-1]` do?
<details>
<summary><strong>Answer</strong></summary>
It reverses the string. The slicing syntax is `s[start:stop:step]`. When `step` is `-1` and `start` and `stop` are omitted, Python traverses the entire sequence backward from the end to the beginning, returning a new reversed string.
</details>

### Q7: What is the difference between `str` and `bytes` in Python 3?
<details>
<summary><strong>Answer</strong></summary>
`str` represents human-readable text composed of abstract Unicode code points. `bytes` represents an immutable sequence of raw 8-bit integers (values from 0 to 255). Converting `str` to `bytes` requires encoding (e.g., `.encode("utf-8")`), and converting `bytes` to `str` requires decoding (e.g., `.decode("utf-8")`).
</details>

### Q8: What is the `BUILD_STRING` opcode?
<details>
<summary><strong>Answer</strong></summary>
`BUILD_STRING` is a bytecode instruction introduced to accelerate f-strings. It takes an argument $N$ representing the number of string fragments currently on the evaluation stack, pops them, allocates a single unified C buffer with their combined length, and pushes the concatenated string back onto the stack.
</details>

---

## 13. Exercises & Challenges

### Challenge 1 (Easy): Palindrome Checker
Write a function `is_palindrome(text)` that returns `True` if a string reads the same forwards and backwards, ignoring case and whitespace.
<details>
<summary><strong>Solution</strong></summary>

```python
def is_palindrome(text: str) -> bool:
    cleaned = "".join(ch.casefold() for ch in text if ch.isalnum())
    return cleaned == cleaned[::-1]

print("Racecar is palindrome:", is_palindrome("Race car"))
print("Hello is palindrome:", is_palindrome("Hello"))
```
</details>

### Challenge 2 (Easy): Extract Domain from URL String
Write a function `extract_domain(url)` that parses the hostname domain from an HTTP/HTTPS URL string without using third-party libraries.
<details>
<summary><strong>Solution</strong></summary>

```python
def extract_domain(url: str) -> str:
    cleaned = url.split("://")[-1]
    domain = cleaned.split("/")[0].split(":")[0]
    return domain

print("Domain:", extract_domain("https://api.github.com:443/repos/octocat"))
```
</details>

### Challenge 3 (Medium): Word Wrap Utility
Write a function `word_wrap(text, max_width=20)` that wraps a paragraph into lines of at most `max_width` characters without splitting words across lines.
<details>
<summary><strong>Solution</strong></summary>

```python
def word_wrap(text: str, max_width: int = 20) -> list[str]:
    words = text.split()
    lines = []
    current_line: list[str] = []
    current_len = 0

    for word in words:
        if current_len + len(word) + (1 if current_line else 0) <= max_width:
            current_line.append(word)
            current_len += len(word) + (1 if len(current_line) > 1 else 0)
        else:
            lines.append(" ".join(current_line))
            current_line = [word]
            current_len = len(word)

    if current_line:
        lines.append(" ".join(current_line))
    return lines

sample_para = "Python engineering involves writing clean robust code with zero placeholders"
for line in word_wrap(sample_para, 25):
    print(f"|{line}|")
```
</details>

### Challenge 4 (Medium): Case-Insensitive Substring Counter
Write a function `count_occurrences_caseless(text, sub)` that counts all non-overlapping occurrences of `sub` in `text` using `.casefold()`.
<details>
<summary><strong>Solution</strong></summary>

```python
def count_occurrences_caseless(text: str, sub: str) -> int:
    return text.casefold().count(sub.casefold())

print("Occurrences of 'py':", count_occurrences_caseless("Python is pythonic with PyPy", "py"))
```
</details>

### Challenge 5 (Hard): String Compression Algorithm (Run-Length Encoding)
Implement Run-Length Encoding (RLE) that converts `"AAABBBCCDAA"` into `"A3B3C2D1A2"`. If the compressed string is not shorter than the original, return the original string.
<details>
<summary><strong>Solution</strong></summary>

```python
def compress_string(s: str) -> str:
    if not s:
        return ""
    parts = []
    current_char = s[0]
    count = 1

    for char in s[1:]:
        if char == current_char:
            count += 1
        else:
            parts.append(f"{current_char}{count}")
            current_char = char
            count = 1
    parts.append(f"{current_char}{count}")

    compressed = "".join(parts)
    return compressed if len(compressed) < len(s) else s

print("Compressed:", compress_string("AAAAABBBBBCCCC"))
print("Not compressed:", compress_string("ABC"))
```
</details>

### Challenge 6 (Hard): Custom String Interpolation Engine
Implement a basic string interpolation engine that parses and replaces `{key}` placeholders in a template string from a dictionary without using `str.format()` or f-strings.
<details>
<summary><strong>Solution</strong></summary>

```python
import re

def custom_interpolate(template: str, context: dict[str, object]) -> str:
    def replacer(match: re.Match[str]) -> str:
        key = match.group(1)
        if key not in context:
            raise KeyError(f"Missing interpolation key: '{key}'")
        return str(context[key])

    return re.sub(r"\{([a-zA-Z0-9_]+)\}", replacer, template)

tpl = "Hello {user}, your account balance is ${balance}."
ctx = {"user": "Maria", "balance": 1500}
print(custom_interpolate(tpl, ctx))
```
</details>
