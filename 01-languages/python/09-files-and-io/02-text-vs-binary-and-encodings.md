# Module 09: Files, Streams & OS (02-text-vs-binary-and-encodings)

> **Topic Overview**: In Python 3, the boundary between human-readable text (`str`) and raw machine bytes (`bytes`) is absolute and strictly enforced. Mastering character encodings (UTF-8, UTF-16, Latin-1, ASCII), byte-order marks (`utf-8-sig`), error handling strategies (`surrogateescape`, `replace`), and the transition towards UTF-8 as the universal default (PEP 540, PEP 597, PEP 686) is essential for writing bug-free software that operates reliably across all operating systems.

---

## Metadata Table

| Field | Value |
|---|---|
| **Category** | Core Language / Encodings & Unicode |
| **Target Level** | Intermediate to Advanced |
| **Python Version** | Python 3.10+ (tested on Python 3.12 - 3.14+) |
| **Tested With** | CPython 3.14.0, uv 0.13.9, ruff 0.17.6, mypy 2.4.1, pytest 9.1.1 |
| **Free-Threading / Platform** | Supported / Platform Independent |

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

- **Problem Statement**: Computers only store bits and bytes; they do not know what a "letter" or "symbol" is without a character encoding standard. In Python 2, implicit coercion between byte sequences and text strings led to ubiquitous `UnicodeDecodeError: 'ascii' codec can't decode byte...` crashes in production. Furthermore, operating systems differ: Linux defaults to UTF-8, while Windows historically defaulted to legacy code pages (e.g., `cp1252` in Western Europe/US), corrupting files transferred across platforms.
- **Mental Model**:
  - `bytes`: Concrete sequence of raw 8-bit unsigned integers ($0 \le b \le 255$).
  - `str`: Abstract sequence of Unicode code points ($U+0000$ to $U+10FFFF$).
  - **Encode**: `str` $\to$ `bytes` (`text.encode('utf-8')`).
  - **Decode**: `bytes` $\to$ `str` (`raw_bytes.decode('utf-8')`).
- **When to Use**:
  - Use text mode (`'r'`, `'w'`, `'a'`) with explicit `encoding="utf-8"` for all human-readable files (JSON, YAML, CSV, source code, text logs).
  - Use binary mode (`'rb'`, `'wb'`) for images, audio, network packets, compressed archives, and serialized binaries.
- **When NOT to Use**: Never rely on implicit system-default encodings; always specify `encoding="utf-8"`.

---

## 2. Core Concepts

### The Unicode Sandwich Architecture
Robust software follows the "Unicode Sandwich" architectural pattern:

```text
+---------------------------------------------------------------+
| The Unicode Sandwich                                          |
+---------------------------------------------------------------+
| Outer World: Bytes (Files, Sockets, HTTP, Databases)          |
|                           │                                   |
|                           v  DECODE (Edge input)              |
| [===========================================================] |
| | Inner Application Core: Pure Unicode (str)                | |
| | - All string slicing, concatenation, regex, comparisons   | |
| [===========================================================] |
|                           │                                   |
|                           v  ENCODE (Edge output)             |
| Outer World: Bytes (Files, Sockets, HTTP, Databases)          |
+---------------------------------------------------------------+
```

### Key Encoding Standards
1. **ASCII (7-bit)**: Maps integers $0-127$ to English characters and control codes.
2. **UTF-8 (Variable-length, 1-4 bytes)**: Backward-compatible with ASCII. ASCII characters consume 1 byte ($0x00-0x7F$); European/Accented characters consume 2 bytes; Asian scripts/symbols consume 3 bytes; Emojis and historic scripts consume 4 bytes. Universal standard of the web.
3. **UTF-16 (Variable-length, 2 or 4 bytes)**: Used internally by Windows APIs and Java.
4. **Latin-1 / ISO-8859-1 (1-byte)**: Maps 1:1 to bytes $0-255$. Can decode *any* arbitrary byte sequence without ever raising `UnicodeDecodeError`.

### Error Handling Handlers (`errors` Parameter)
- `strict` (Default): Raises `UnicodeDecodeError` or `UnicodeEncodeError`.
- `ignore`: Silently drops unencodable/undecodable bytes.
- `replace`: Replaces invalid bytes with the official Unicode replacement character `` (`U+FFFD`) or `?`.
- `surrogateescape` (PEP 383): Decodes invalid bytes into private surrogate code points ($U+DC80$ to $U+DCFF$) so they can be round-tripped back to the exact original bytes without data loss (standard for POSIX filenames and OS command lines).
- `backslashreplace`: Replaces unencodable characters with Python backslash escapes (`\x..`, `\u....`).

---

## 3. How It Works Internally

In CPython, the `str` type uses the PEP 393 flexible string representation to save RAM:

```text
+---------------------------------------------------------------+
| PEP 393 Flexible String Representation (Memory Layout)        |
+---------------------------------------------------------------+
| Kind 1: Compact ASCII (1 byte per char)                       |
|   - If all chars < 128 (e.g. "hello world")                   |
|   - Memory: 48 bytes header + 1 byte per char                 |
|                                                               |
| Kind 2: Compact UCS-1 (1 byte per char)                       |
|   - If all chars < 256 (e.g. Latin-1 "café")                  |
|   - Memory: 72 bytes header + 1 byte per char                 |
|                                                               |
| Kind 3: Compact UCS-2 (2 bytes per char)                      |
|   - If all chars < 65536 (e.g. Cyrillic, Greek, Basic Asian)  |
|   - Memory: 72 bytes header + 2 bytes per char                |
|                                                               |
| Kind 4: Compact UCS-4 (4 bytes per char)                      |
|   - If any char >= 65536 (e.g. Emojis "🚀", Math symbols)     |
|   - Memory: 72 bytes header + 4 bytes per char (all chars 4B!)|
+---------------------------------------------------------------+
```

---

## 4. Syntax & API Reference

### Encoding & Decoding Methods
```python
# String to Bytes
b = "Python 🐍".encode("utf-8")  # b'Python \xf0\x9f\x90\x8d'

# Bytes to String
s = b.decode("utf-8")           # "Python 🐍"

# Handling Errors
s_safe = b_corrupted.decode("utf-8", errors="replace")
s_escape = b_corrupted.decode("utf-8", errors="surrogateescape")
```

### Modern Python UTF-8 Standards
- **PEP 540 (Python 3.7+)**: UTF-8 Mode (`python -X utf8` or `PYTHONUTF8=1`) forces UTF-8 encoding regardless of system locale.
- **PEP 597 (Python 3.10+)**: Adds `EncodingWarning` when `open()` is called without explicit `encoding` (`python -X warn_default_encoding`).
- **PEP 686 (Python 3.15+)**: Makes UTF-8 the universal default encoding across all platforms, including Windows.

---

## 5. Code Examples

### 1. Minimal / Basic Example: Encode, Decode, and Byte Lengths
```python
# ASCII characters: 1 byte each
ascii_text = "Hello"
b_ascii = ascii_text.encode("utf-8")
print(f"'{ascii_text}' length in chars: {len(ascii_text)}, in bytes: {len(b_ascii)}")

# Accented characters: 2 bytes each in UTF-8
accent_text = "Café"
b_accent = accent_text.encode("utf-8")
print(f"'{accent_text}' length in chars: {len(accent_text)}, in bytes: {len(b_accent)} -> {b_accent}")

# Emojis: 4 bytes each in UTF-8
emoji_text = "Python 🚀"
b_emoji = emoji_text.encode("utf-8")
print(f"'{emoji_text}' length in chars: {len(emoji_text)}, in bytes: {len(b_emoji)} -> {b_emoji}")
```
**Output**:
```text
'Hello' length in chars: 5, in bytes: 5
'Café' length in chars: 4, in bytes: 5 -> b'Caf\xc3\xa9'
'Python 🚀' length in chars: 8, in bytes: 11 -> b'Python \xf0\x9f\x9a\x80'
```

### 2. Real-World Production Example: Handling Windows BOM (`utf-8-sig`)
```python
import tempfile
from pathlib import Path

# Windows tools (Excel, Notepad) often prepend a 3-byte Byte Order Mark (BOM: \xef\xbb\xbf)
raw_bom_bytes = b'\xef\xbb\xbfid,name,role\n1,Alice,Admin\n'

with tempfile.NamedTemporaryFile("wb", delete=False) as tmp:
    tmp.write(raw_bom_bytes)
    tmp_path = Path(tmp.name)

try:
    # BAD: Reading with standard 'utf-8' leaves BOM as zero-width non-breaking space '\ufeff'
    with open(tmp_path, "r", encoding="utf-8") as f:
        header_bad = f.readline()
        print("Raw first column with 'utf-8':    ", repr(header_bad.split(",")[0]))

    # GOOD: Reading with 'utf-8-sig' automatically strips the BOM if present!
    with open(tmp_path, "r", encoding="utf-8-sig") as f:
        header_good = f.readline()
        print("Clean first column with 'utf-8-sig':", repr(header_good.split(",")[0]))
finally:
    tmp_path.unlink()
```
**Output**:
```text
Raw first column with 'utf-8':     '\ufeffid'
Clean first column with 'utf-8-sig': 'id'
```

### 3. Edge Case: The Latin-1 "Never-Fail" Decoder
```python
# Latin-1 (ISO-8859-1) maps bytes 0..255 directly to Unicode U+0000..U+00FF
# It CANNOT fail with UnicodeDecodeError, making it useful for raw binary dumps
arbitrary_corrupt_bytes = b"\xff\xfe\x80\x90\x00\xab"

decoded = arbitrary_corrupt_bytes.decode("latin-1")
print("Decoded length:", len(decoded))
print("Code points:", [ord(c) for c in decoded])

# Re-encodes back to the exact same bytes identically:
re_encoded = decoded.encode("latin-1")
assert re_encoded == arbitrary_corrupt_bytes
print("Lossless roundtrip confirmed.")
```
**Output**:
```text
Decoded length: 6
Code points: [255, 254, 128, 144, 0, 171]
Lossless roundtrip confirmed.
```

### 4. Failure Case & Exception Handling: Error Recovery Strategies
```python
corrupt_utf8 = b"Standard text \xff\xfe corrupt bytes \x80 ending"

# 1. Strict (Default) raises UnicodeDecodeError
try:
    corrupt_utf8.decode("utf-8", errors="strict")
except UnicodeDecodeError as err:
    print(f"[STRICT] Caught error: {err}")

# 2. Replace: substitutes replacement character 
res_replace = corrupt_utf8.decode("utf-8", errors="replace")
print("[REPLACE]:", res_replace)

# 3. Ignore: discards invalid bytes
res_ignore = corrupt_utf8.decode("utf-8", errors="ignore")
print("[IGNORE]: ", res_ignore)

# 4. Backslashreplace: prints escape sequences
res_backslash = corrupt_utf8.decode("utf-8", errors="backslashreplace")
print("[BACKSLASH]:", res_backslash)
```
**Output**:
```text
[STRICT] Caught error: 'utf-8' codec can't decode byte 0xff in position 14: invalid start byte
[REPLACE]: Standard text  corrupt bytes  ending
[IGNORE]:  Standard text  corrupt bytes  ending
[BACKSLASH]: Standard text \xff\xfe corrupt bytes \x80 ending
```

### 5. Idiomatic Pythonic Pattern: Round-Tripping OS Artifacts via `surrogateescape`
```python
# Simulating an OS filename that contains raw non-UTF-8 bytes
raw_filename_bytes = b"project_\x80_report.pdf"

# Decode using surrogateescape (used internally by sys.argv and os.listdir on POSIX)
safe_filename_str = raw_filename_bytes.decode("utf-8", errors="surrogateescape")
print("Surrogate string representation:", repr(safe_filename_str))

# Pass string through application layers...
# Re-encode back to exact bytes before calling OS system calls:
roundtripped_bytes = safe_filename_str.encode("utf-8", errors="surrogateescape")
assert roundtripped_bytes == raw_filename_bytes
print("Roundtripped bytes match original exactly:", roundtripped_bytes)
```
**Output**:
```text
Surrogate string representation: 'project_\udc80_report.pdf'
Roundtripped bytes match original exactly: b'project_\x80_report.pdf'
```

---

## 6. Edge Cases & Pitfalls

1. **The Hidden BOM in CSV Headers**:
   - If a UTF-8 file contains a Byte Order Mark and is read with `encoding="utf-8"`, the first column dictionary key in `csv.DictReader` becomes `'\ufeffid'` instead of `'id'`, causing silent `KeyError` failures. Always use `encoding="utf-8-sig"` if files originate from Windows.
2. **`surrogateescape` and ASCII Encoding**:
   - If a string contains surrogate escapes (`\udc80`), attempting to encode it with normal `"ascii"` or `"utf-8"` with `errors="strict"` will raise `UnicodeEncodeError: 'utf-8' codec can't encode characters in position...`. You must re-encode with `errors="surrogateescape"`.
3. **PEP 393 Memory Spikes**:
   - If a 10-million character ASCII string has a single emoji (`🚀`) inserted into it, CPython promotes the *entire* string from 1-byte per character (UCS-1) to 4-bytes per character (UCS-4), instantly quadrupling its memory footprint from 10 MB to 40 MB!

---

## 7. Common Mistakes

| Mistake | Why It Fails | Exception / Diagnostic | Correct Pattern |
|---|---|---|---|
| Omitting `encoding="utf-8"` in `open()` | Uses system default; works on macOS/Linux, crashes on Windows | `UnicodeDecodeError` in prod | Always `encoding="utf-8"` |
| Reading binary files in text mode | Modifies `\r\n` line endings, corrupts raw binary payloads | Corrupted image/zip archive | Use mode `'rb'` |
| Writing `str` to binary file | Binary streams strictly require `bytes` | `TypeError: a bytes-like object is required, not 'str'` | `f.write(data.encode('utf-8'))` |
| Using `errors="ignore"` on financial/audit data | Silently deletes corrupted bytes, potentially shifting numbers | Data corruption / lost data | Use `errors="strict"` or validate |
| Slicing raw bytes instead of text for Unicode | Slicing multi-byte UTF-8 sequences in the middle creates invalid bytes | Broken character decoding | Decode to `str` before slicing |
| Assuming 1 character equals 1 byte | In UTF-8, characters span 1 to 4 bytes | Truncated strings / buffer underflow | Distinguish `len(str)` from `len(bytes)` |

---

## 8. Performance & Complexity

- **Encoding / Decoding Complexity**: $O(N)$ where $N$ is the byte length, running via vectorized SIMD C routines inside CPython.
- **Memory Footprint**:
  - `bytes`: 33 bytes header + $N$ bytes.
  - `str` (PEP 393):
    - ASCII: $48\text{ B} + 1\times N$.
    - BMP (UCS-2): $72\text{ B} + 2\times N$.
    - Non-BMP (UCS-4, emojis): $72\text{ B} + 4\times N$.
- **UTF-8 Decode Speed**: CPython decodes pure ASCII in UTF-8 mode at $\approx 2-4\ \text{GB/s}$ using 64-bit word scanning.

---

## 9. CPython Internals & Bytecode

Let's inspect how PEP 393 compact string representations differ:

```python
import sys

s_ascii = "hello"          # ASCII: 1 byte per char
s_accent = "café"          # Latin-1: 1 byte per char (UCS-1)
s_cyrillic = "привет"      # BMP: 2 bytes per char (UCS-2)
s_emoji = "hi 🚀"          # Non-BMP: 4 bytes per char (UCS-4)

for s in [s_ascii, s_accent, s_cyrillic, s_emoji]:
    size = sys.getsizeof(s)
    print(f"'{s:<8}' | chars: {len(s)} | bytes on heap: {size:>2} | ratio: {size / len(s):.1f} B/char")
```

**Output**:
```text
'hello   ' | chars: 5 | bytes on heap: 54 | ratio: 10.8 B/char
'café    ' | chars: 4 | bytes on heap: 78 | ratio: 19.5 B/char
'привет  ' | chars: 6 | bytes on heap: 86 | ratio: 14.3 B/char
'hi 🚀   ' | chars: 4 | bytes on heap: 96 | ratio: 24.0 B/char
```
Notice how `hi 🚀` (4 characters) consumes 96 bytes on the heap because the presence of the 4-byte rocket emoji forces CPython to allocate 4 bytes for *every* character in the string.

---

## 10. Best Practices

1. **Always Specify Encoding**: Enforce `encoding="utf-8"` with Ruff (`UP015` / `PLW1514`) or `python -X warn_default_encoding`.
2. **Use `utf-8-sig` for User Uploads**: When parsing user-submitted CSVs or text files from unknown sources, use `utf-8-sig` to gracefully strip Windows BOM headers.
3. **Use Binary Mode for Raw Data**: When handling encryption, hashing, network protocols, or binary formats, always open with `'rb'` / `'wb'`.

---

## 11. Real-World Case Study: Universal File Sanitizer and Transcoder

```python
from pathlib import Path
import tempfile

def transcode_to_clean_utf8(input_path: Path, output_path: Path) -> str:
    """Reads a file of uncertain encoding, normalizes to clean UTF-8 without BOM."""
    raw_data = input_path.read_bytes()

    # Strategy 1: Check for UTF-8 with BOM
    if raw_data.startswith(b"\xef\xbb\xbf"):
        text = raw_data[3:].decode("utf-8")
        detected = "UTF-8-SIG"
    else:
        # Strategy 2: Attempt standard UTF-8
        try:
            text = raw_data.decode("utf-8")
            detected = "UTF-8"
        except UnicodeDecodeError:
            # Strategy 3: Fall back to Latin-1
            text = raw_data.decode("latin-1")
            detected = "LATIN-1"

    # Write clean normalized UTF-8
    output_path.write_text(text, encoding="utf-8")
    return detected

# Test with various inputs
with tempfile.TemporaryDirectory() as tmp:
    t = Path(tmp)
    f_bom = t / "bom.txt"
    f_bom.write_bytes(b"\xef\xbb\xbfUser,Score\nAlice,100\n")

    f_latin = t / "latin.txt"
    f_latin.write_bytes("Café crème".encode("latin-1"))

    clean_1 = t / "clean_1.txt"
    det_1 = transcode_to_clean_utf8(f_bom, clean_1)
    print("Detected BOM file as:", det_1)
    print("Clean contents:      ", repr(clean_1.read_text(encoding="utf-8")))

    clean_2 = t / "clean_2.txt"
    det_2 = transcode_to_clean_utf8(f_latin, clean_2)
    print("Detected Latin file as:", det_2)
    print("Clean contents:        ", repr(clean_2.read_text(encoding="utf-8")))
```
**Output**:
```text
Detected BOM file as: UTF-8-SIG
Clean contents:       'User,Score\nAlice,100\n'
Detected Latin file as: LATIN-1
Clean contents:         'Café crème'
```

---

## 12. Interview Questions

### Q1: What is the "Unicode Sandwich" principle?
<details>
<summary><strong>Answer</strong></summary>
The Unicode Sandwich principle states that bytes should be decoded into pure Unicode strings (<code>str</code>) as early as possible at the boundaries of your application (file reads, network sockets, HTTP requests). Inside your application's business logic, all manipulation is performed strictly on <code>str</code>. Output bytes are encoded at the outer edge right before writing or sending.
</details>

### Q2: Why does `len("Python 🚀")` return 8, but `len("Python 🚀".encode("utf-8"))` return 11?
<details>
<summary><strong>Answer</strong></summary>
<code>len(str)</code> returns the number of Unicode code points (characters). "Python " is 7 characters, plus 1 rocket emoji = 8 characters. <code>len(bytes)</code> returns the number of physical 8-bit bytes in the UTF-8 encoded sequence. In UTF-8, ASCII characters consume 1 byte each (7 bytes), while the rocket emoji <code>U+1F680</code> is a non-BMP character requiring 4 bytes: $7 + 4 = 11$ bytes.
</details>

### Q3: What is the difference between `encoding="utf-8"` and `encoding="utf-8-sig"`?
<details>
<summary><strong>Answer</strong></summary>
<code>utf-8</code> treats the Byte Order Mark (<code>\xef\xbb\xbf</code>) as a regular character (<code>\ufeff</code> zero-width no-break space), leaving it in the string. <code>utf-8-sig</code> automatically detects and strips the BOM on reading, and prepends a BOM when writing, providing seamless compatibility with Windows tools like Excel and Notepad.
</details>

### Q4: When should you use the `surrogateescape` error handler?
<details>
<summary><strong>Answer</strong></summary>
Use <code>surrogateescape</code> (PEP 383) when dealing with operating system interfaces (POSIX file paths, environment variables, command-line arguments) that may contain arbitrary bytes with unknown encodings. It decodes unencodable bytes into a private surrogate range and guarantees lossless round-trip re-encoding back to the exact original bytes.
</details>

### Q5: What is PEP 393 flexible string representation in CPython?
<details>
<summary><strong>Answer</strong></summary>
PEP 393 redesigned CPython's <code>str</code> implementation so that memory allocation per character adapts to the largest code point in the string: 1 byte per character for ASCII/Latin-1 (UCS-1), 2 bytes per character for the Basic Multilingual Plane (UCS-2), and 4 bytes per character only if non-BMP characters like emojis are present (UCS-4).
</details>

### Q6: What does the command-line flag `-X warn_default_encoding` (PEP 597) do?
<details>
<summary><strong>Answer</strong></summary>
Introduced in Python 3.10, it emits an <code>EncodingWarning</code> whenever <code>open()</code> or <code>TextIOWrapper()</code> is invoked without an explicit <code>encoding</code> parameter, helping developers identify platform-dependent encoding bugs before deploying across Linux and Windows.
</details>

### Q7: Why is it impossible for `bytes.decode("latin-1")` to raise a `UnicodeDecodeError`?
<details>
<summary><strong>Answer</strong></summary>
Latin-1 (ISO-8859-1) is an 8-bit encoding that maps all 256 possible byte values ($0x00$ through $0xFF$) directly to the first 256 Unicode code points ($U+0000$ through $U+00FF$). Because every possible byte is a valid character, there are no invalid byte sequences.
</details>

### Q8: What does Python's UTF-8 Mode (PEP 540) do?
<details>
<summary><strong>Answer</strong></summary>
When enabled via <code>python -X utf8</code> or <code>PYTHONUTF8=1</code>, Python ignores the operating system's configured locale encoding and forces UTF-8 as the default encoding for standard streams (<code>sys.stdin</code>, <code>sys.stdout</code>, <code>sys.stderr</code>) and filesystem operations.
</details>

---

## 13. Exercises & Challenges

### Challenge 1 (Easy): Byte Counter vs Character Counter
Write a function `inspect_string_metrics(s: str) -> dict[str, int]` that returns a dictionary containing character count, UTF-8 byte count, and UTF-16 byte count.
<details>
<summary><strong>Solution</strong></summary>

```python
def inspect_string_metrics(s: str) -> dict[str, int]:
    return {
        "char_count": len(s),
        "utf8_bytes": len(s.encode("utf-8")),
        "utf16_bytes": len(s.encode("utf-16-le"))
    }

metrics = inspect_string_metrics("Python 🐍")
assert metrics["char_count"] == 8
assert metrics["utf8_bytes"] == 11
assert metrics["utf16_bytes"] == 16  # 6*2 + 1*2 + 2*2 (surrogate pair) = 16 bytes
```
</details>

### Challenge 2 (Easy): Safe Replacement Decoder
Write a function `safe_decode_bytes(b: bytes) -> str` that decodes an arbitrary byte sequence using UTF-8, replacing invalid bytes with the Unicode replacement character `` without raising an exception.
<details>
<summary><strong>Solution</strong></summary>

```python
def safe_decode_bytes(b: bytes) -> str:
    return b.decode("utf-8", errors="replace")

res = safe_decode_bytes(b"Good \xff\xfe Data")
assert "" in res
assert res.startswith("Good ")
assert res.endswith(" Data")
```
</details>

### Challenge 3 (Medium): Strip BOM Header Utility
Write a function `strip_utf8_bom(data: bytes) -> bytes` that inspects the first 3 bytes of a byte sequence. If it matches the UTF-8 BOM (`b"\xef\xbb\xbf"`), return the byte sequence with the BOM stripped; otherwise, return the original bytes.
<details>
<summary><strong>Solution</strong></summary>

```python
BOM = b"\xef\xbb\xbf"

def strip_utf8_bom(data: bytes) -> bytes:
    if data.startswith(BOM):
        return data[len(BOM):]
    return data

assert strip_utf8_bom(b"\xef\xbb\xbfheader,val") == b"header,val"
assert strip_utf8_bom(b"clean,data") == b"clean,data"
```
</details>

### Challenge 4 (Medium): Detect Non-ASCII Characters in Source
Write a function `find_non_ascii_locations(text: str) -> list[tuple[int, str, int]]` that scans a string and returns a list of `(character_index, character, unicode_code_point)` for every character with an ordinal value $\ge 128$.
<details>
<summary><strong>Solution</strong></summary>

```python
def find_non_ascii_locations(text: str) -> list[tuple[int, str, int]]:
    return [
        (idx, char, ord(char))
        for idx, char in enumerate(text)
        if ord(char) >= 128
    ]

results = find_non_ascii_locations("Café 🚀")
assert len(results) == 2
assert results[0] == (3, "é", 233)
assert results[1] == (5, "🚀", 128640)
```
</details>

### Challenge 5 (Hard): Round-Trip Binary Codec with Surrogateescape
Simulate an operating system filename containing arbitrary bytes. Write two functions: `filename_to_str(raw: bytes) -> str` and `str_to_filename(name: str) -> bytes` using `surrogateescape`, proving that any arbitrary invalid byte sequence round-trips with 100% byte-for-byte fidelity.
<details>
<summary><strong>Solution</strong></summary>

```python
def filename_to_str(raw: bytes) -> str:
    return raw.decode("utf-8", errors="surrogateescape")

def str_to_filename(name: str) -> bytes:
    return name.encode("utf-8", errors="surrogateescape")

# Raw non-UTF8 bytes
arbitrary_bytes = b"my_\x80\x95\xfe_file.dat"
text_repr = filename_to_str(arbitrary_bytes)

# Verify it is a valid Python str
assert isinstance(text_repr, str)
# Verify lossless round-trip
assert str_to_filename(text_repr) == arbitrary_bytes
```
</details>

### Challenge 6 (Hard): Multi-Encoding Sniffer and Converter
Implement a function `auto_decode_stream(raw_bytes: bytes) -> tuple[str, str]` that attempts decoding against a prioritized list of encodings: `["utf-8", "utf-8-sig", "utf-16", "latin-1"]`. Return a tuple `(decoded_text, encoding_used)`.
<details>
<summary><strong>Solution</strong></summary>

```python
def auto_decode_stream(raw_bytes: bytes) -> tuple[str, str]:
    if raw_bytes.startswith(b"\xef\xbb\xbf"):
        return raw_bytes[3:].decode("utf-8"), "utf-8-sig"
    if raw_bytes.startswith((b"\xff\xfe", b"\xfe\xff")):
        return raw_bytes.decode("utf-16"), "utf-16"
    
    try:
        return raw_bytes.decode("utf-8"), "utf-8"
    except UnicodeDecodeError:
        return raw_bytes.decode("latin-1"), "latin-1"

t1, e1 = auto_decode_stream(b"\xef\xbb\xbfHello")
assert e1 == "utf-8-sig" and t1 == "Hello"

t2, e2 = auto_decode_stream(b"Standard UTF8")
assert e2 == "utf-8" and t2 == "Standard UTF8"

t3, e3 = auto_decode_stream(b"\xff\xfe\x00\x00")
assert e3 == "latin-1"
```
</details>
