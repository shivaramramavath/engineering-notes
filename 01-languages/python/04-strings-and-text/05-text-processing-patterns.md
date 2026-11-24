---
title: "Text Processing Patterns in Python"
slug: "text-processing-patterns"
category: "strings-and-text"
difficulty: "intermediate"
tags: ["python", "text-processing", "string-template", "textwrap", "tokenization", "stream-processing", "patterns"]
prerequisites: ["00-string-methods", "04-regular-expressions"]
related_topics: ["09-files-and-io/00-reading-and-writing-files", "06-string-internals-and-interning"]
python_version: "3.8+"
tested_with: "CPython 3.14.0"
free_threading_support: "Yes"
---

# Text Processing Patterns in Python

## 1. Executive Summary & Core Value Proposition

Text processing represents one of the largest computational workloads in modern software engineering: ingesting multi-gigabyte server logs, transforming unstructured data into relational models, sanitizing user inputs, formatting reports, and tokenizing DSLs.

Writing clean, performant text processing pipelines requires mastering specialized design patterns:
1. **Streaming Iteration:** Processing text line-by-line or chunk-by-chunk with $O(1)$ memory consumption, preventing out-of-memory crashes on giant datasets.
2. **Safe Templating (`string.Template`):** Providing user-configurable string templates without exposing arbitrary code execution or template injection vulnerabilities.
3. **Multi-Pattern Dispatch:** Compiling multiple search patterns into unified single-pass state machines.
4. **Layout & Word Wrapping (`textwrap`):** Formatting terminal output, CLI documentation, and automated email bodies cleanly.

---

## 2. Syntax, Mechanics & Visual Architecture

### Pattern Architectural Spectrum

```
+---------------------------------------------------------------------------------------+
| Text Pattern           | Tool / Module              | Primary Value Proposition       |
+---------------------------------------------------------------------------------------+
| Safe Substitution      | string.Template            | Prevents code injection in templates |
| Text Wrapping / Fill   | textwrap                   | Automatic indentation & word wrapping|
| Streaming Pipelines    | Generators / iter()        | O(1) memory on multi-gigabyte logs  |
| Multi-Pattern Match    | re.compile("|".join(...))  | Single-pass regex automaton     |
| State Machine Tokenizer| Custom generator           | Fast lexical analysis without Lex/Yacc|
+---------------------------------------------------------------------------------------+
```

### Safe Templating (`string.Template`) vs F-Strings

```
F-String (Eager Code Execution):
f"{user_input}"  --> If user input contains code or arbitrary attributes,
                     evaluates inside application scope! (Injection hazard)

string.Template (Safe Variable Replacement):
Template("$user_name").substitute(user_name="Alice")
                 --> Only performs explicit mapping lookup;
                     NEVER evaluates code or accesses private globals.
```

---

## 3. Real-World Code Examples

### Example 1: Safe Configuration Templating with `string.Template`

`string.Template` provides secure, user-facing placeholder substitution using `$identifier` syntax, eliminating the security risks of f-strings or `.format()` when handling untrusted user templates.

```python
from string import Template

# User-provided template string
email_template = Template(
    "Hello $username,\n"
    "Your verification code for $service is: $code.\n"
    "This code will expire in $timeout minutes."
)

# Safe substitution using dictionary mapping
context = {
    "username": "Shiva",
    "service": "Antigravity Cloud",
    "code": "482-910",
    "timeout": 15
}

rendered = email_template.substitute(context)
print(rendered)

# safe_substitute() does not crash on missing keys; leaves placeholders intact
partial_context = {"username": "Alice"}
safe_rendered = email_template.safe_substitute(partial_context)
print("\nPartial rendered fallback:")
print(safe_rendered.splitlines()[0])
```

**Output:**
```
Hello Shiva,
Your verification code for Antigravity Cloud is: 482-910.
This code will expire in 15 minutes.

Partial rendered fallback:
Hello Alice,
```

---

### Example 2: Terminal Text Formatting with `textwrap`

```python
import textwrap

sample_text = (
    "Python is an interpreted, high-level, general-purpose programming language. "
    "Its design philosophy emphasizes code readability with the use of significant indentation. "
    "Python is dynamically typed and garbage-collected."
)

# 1. Word wrap to 50 characters
wrapped = textwrap.fill(sample_text, width=50)
print("--- Wrapped Text (Width 50) ---")
print(wrapped)

# 2. Indent text for CLI sub-blocks
indented = textwrap.indent(wrapped, prefix="    | ")
print("\n--- Indented Block ---")
print(indented)

# 3. Shorten text with placeholder
shortened = textwrap.shorten(sample_text, width=60, placeholder="... [read more]")
print("\n--- Shortened Summary ---")
print(shortened)
```

**Output:**
```
--- Wrapped Text (Width 50) ---
Python is an interpreted, high-level, general-
purpose programming language. Its design
philosophy emphasizes code readability with the
use of significant indentation. Python is
dynamically typed and garbage-collected.

--- Indented Block ---
    | Python is an interpreted, high-level, general-
    | purpose programming language. Its design
    | philosophy emphasizes code readability with the
    | use of significant indentation. Python is
    | dynamically typed and garbage-collected.

--- Shortened Summary ---
Python is an interpreted, high-level,... [read more]
```

---

### Example 3: Streaming Log Filter with Zero-Allocation Pipeline

Processing a multi-gigabyte file by reading all lines into memory (`f.readlines()`) exhausts RAM. Combining generator functions creates a streaming pipeline operating in constant $O(1)$ memory.

```python
import io

raw_log_stream = io.StringIO(
    "2026-03-29 10:00:01 INFO [Auth] Login successful\n"
    "2026-03-29 10:00:02 ERROR [DB] Connection pool exhausted\n"
    "2026-03-29 10:00:03 DEBUG [Cache] Key hit: user_42\n"
    "2026-03-29 10:00:04 ERROR [Payment] Gateway timed out\n"
)

# Step 1: Generator yielding stripped lines
lines = (line.strip() for line in raw_log_stream)

# Step 2: Generator filtering for ERROR entries
errors = (line for line in lines if "ERROR" in line)

# Step 3: Generator parsing components
parsed_errors = (
    {"timestamp": parts[0], "service": parts[2], "message": " ".join(parts[3:])}
    for e in errors
    if (parts := e.split())
)

# Consume generator lazily
for err in parsed_errors:
    print(f"Captured Alert: [{err['service']}] at {err['timestamp']} -> {err['message']}")
```

**Output:**
```
Captured Alert: [[DB]] at 2026-03-29 -> Connection pool exhausted
Captured Alert: [[Payment]] at 2026-03-29 -> Gateway timed out
```

---

### Example 4: Lexical Analysis via State Machine Tokenizer

```python
import re
from typing import NamedTuple, Iterator

class Token(NamedTuple):
    type: str
    value: str
    line: int
    column: int

def tokenize(code: str) -> Iterator[Token]:
    """Tokenizes code using a unified single-pass regex state machine."""
    rules = [
        ("NUMBER",    r"\d+(\.\d*)?"),
        ("IDENT",     r"[a-zA-Z_]\w*"),
        ("OP",        r"[+\-*/=]"),
        ("NEWLINE",   r"\n"),
        ("SKIP",      r"[ \t]+"),
        ("MISMATCH",  r"."),
    ]
    # Build single master regex with named groups
    master_regex = "|".join(f"(?P<{name}>{pattern})" for name, pattern in rules)
    line_num = 1
    line_start = 0

    for match in re.finditer(master_regex, code):
        kind = match.lastgroup
        value = match.group()
        col = match.start() - line_start + 1
        
        if kind == "NEWLINE":
            line_start = match.end()
            line_num += 1
        elif kind == "SKIP":
            continue
        elif kind == "MISMATCH":
            raise SyntaxError(f"Unexpected character {value!r} at line {line_num}, col {col}")
        else:
            yield Token(kind, value, line_num, col)

sample_code = "total = price * 1.08\ncount = 42"
for tok in tokenize(sample_code):
    print(f"Token: {tok.type:<8} | Val: {tok.value:<10} | Pos: Line {tok.line}, Col {tok.column}")
```

**Output:**
```
Token: IDENT    | Val: total      | Pos: Line 1, Col 1
Token: OP       | Val: =          | Pos: Line 1, Col 7
Token: IDENT    | Val: price      | Pos: Line 1, Col 9
Token: OP       | Val: *          | Pos: Line 1, Col 15
Token: NUMBER   | Val: 1.08       | Pos: Line 1, Col 17
Token: IDENT    | Val: count      | Pos: Line 2, Col 1
Token: OP       | Val: =          | Pos: Line 2, Col 7
Token: NUMBER   | Val: 42         | Pos: Line 2, Col 9
```

---

### Example 5: High-Speed Multi-Keyword Search (Aho-Corasick Emulation)

Searching for 1,000 keywords across documents by looping `for kw in keywords: if kw in text` runs in $O(M \cdot N)$ time. Compiling all keywords into a single regex alternation runs in a single pass.

```python
import re

banned_keywords = ["malware", "exploit", "trojan", "ransomware", "keylogger"]

# Compile alternation: (malware|exploit|trojan|ransomware|keylogger)
scanner = re.compile(r"\b(" + "|".join(map(re.escape, banned_keywords)) + r")\b", re.IGNORECASE)

payload = "Analysis: suspected keylogger activity detected along with ransomware signature."

matches = scanner.findall(payload)
print("Detected security violations in single pass:", set(matches))
```

**Output:**
```
Detected security violations in single pass: {'keylogger', 'ransomware'}
```

---

## 4. Common Anti-Patterns & Mistakes

| Anti-Pattern / Mistake | Broken / Unidiomatic Snippet | Idiomatic Correction | Why It Fails |
| :--- | :--- | :--- | :--- |
| **Reading whole file into memory** | `lines = f.readlines()` *(huge file)* | `for line in f:` | Allocates all file lines in memory simultaneously; triggers Out-Of-Memory (OOM). |
| **Evaluating user template via `format`** | `user_template.format(**data)` | `string.Template(user_template)` | Allows malicious users to traverse object graphs via `"{user.__class__}"`. |
| **Sequential `.replace()` calls** | 10x consecutive `text.replace()` | Regex callback or translation table | Scans the entire string 10 times consecutively, reallocating memory each time. |
| **Manual word wrapping** | Counting string length with `\n` | `textwrap.fill(text, width)` | Fails on variable word lengths and produces jagged margins. |
| **Unescaped regex alternation** | `"|".join(user_keywords)` | `"|".join(map(re.escape, user_kw))` | Special regex characters (like `.`, `+`, `*`) in user keywords break pattern parsing. |
| **Manual token parsing with `split`** | `tokens = code.split()` | Regex state machine tokenizer | Discards line/column coordinates and cannot separate symbols from alphanumeric tokens. |

---

## 5. Architectural & Design Decisions

### Security: Template Injection Mitigation
Using `str.format()` or f-strings for user-provided templates exposes **Server-Side Template Injection (SSTI)**. In Python, an expression like `"{user.__class__.__mro__[1].__subclasses__()}"` allows attackers to navigate object graphs and execute arbitrary system shell commands.
`string.Template` is architecturally safe: it only permits variable substitutions (`$identifier`), strictly forbidding attribute lookups, method execution, and indexing.

---

## 6. Verification & Practical Testing

```python
from string import Template
import textwrap

def test_string_template():
    t = Template("Hello $name, balance: $$100")
    assert t.substitute(name="Alice") == "Hello Alice, balance: $100"
    assert t.safe_substitute() == "Hello $name, balance: $100"

def test_textwrap_fill():
    raw = "Alpha beta gamma delta epsilon zeta eta theta"
    filled = textwrap.fill(raw, width=20)
    for line in filled.splitlines():
        assert len(line) <= 20

def test_multi_pattern():
    import re
    words = ["red", "blue", "green"]
    pat = re.compile("|".join(words))
    assert pat.findall("blue car and red truck") == ["blue", "red"]

if __name__ == "__main__":
    test_string_template()
    test_textwrap_fill()
    test_multi_pattern()
    print("All text processing pattern tests passed successfully.")
```

**Output:**
```
All text processing pattern tests passed successfully.
```

---

## 7. Deep Dive: `textwrap.dedent()` for Multi-Line Docstrings

When formatting multi-line docstrings or embedded SQL/HTML queries, indentation creates unwanted leading whitespace:

```python
import textwrap

def get_query():
    query = """
        SELECT user_id, email
        FROM users
        WHERE is_active = TRUE
    """
    return textwrap.dedent(query).strip()

print("Dedent Result:\n" + get_query())
```

**Output:**
```
Dedent Result:
SELECT user_id, email
FROM users
WHERE is_active = TRUE
```

`dedent()` automatically finds the common leading whitespace across all non-empty lines and strips it cleanly.

---

## 8. Summary & Key Takeaways

1. **Streaming Processing:** Use generators and direct file iteration (`for line in f:`) for $O(1)$ memory streaming.
2. **Safe Substitution:** Use `string.Template` to prevent template injection vulnerabilities when templates originate from users.
3. **Word Wrapping:** Use `textwrap.fill()` and `textwrap.dedent()` to format text output and CLI messages.
4. **Tokenization:** Construct a master regex with named groups (`(?P<NAME>...)`) for clean single-pass lexing.
5. **Escape User Patterns:** Always apply `re.escape()` before compiling dynamic alternations.

---

## 9. Frequently Asked Questions (Interview & Senior Roles)

<details>
<summary>1. Why is `string.Template` safer than `str.format()` for user-provided templates?</summary>

`str.format()` allows attribute access (`{user.password}`) and item lookups (`{config[SECRET_KEY]}`), which attackers can exploit to navigate the Python object model and extract sensitive data or execute arbitrary code. `string.Template` strictly allows identifier substitution (`$name`) without attribute navigation or expression evaluation.
</details>

<details>
<summary>2. How can you process a 50GB log file in Python on a machine with only 4GB of RAM?</summary>

Never load the file into memory using `f.read()` or `f.readlines()`. Instead, stream the file line by line using a generator pipeline:
```python
with open("giant.log", "r", encoding="utf-8") as f:
    for line in f:
        process(line)
```
Python reads lines lazily from the underlying OS file buffer, consuming only a few kilobytes of memory regardless of file size.
</details>

<details>
<summary>3. What does `textwrap.dedent()` do?</summary>

It removes any common leading whitespace from every line in the input text. This is commonly used to strip uniform indentation from triple-quoted multiline strings defined inside indented function bodies.
</details>

<details>
<summary>4. Why should you use `re.escape()` when constructing dynamic regex alternations?</summary>

If a user-supplied keyword contains regex metacharacters (such as `.` or `*` or `?`), compiling them directly alters the pattern logic or raises `re.error`. `re.escape()` prepends backslashes to all special characters, ensuring they are treated as exact literal strings.
</details>

<details>
<summary>5. How does `string.Template.safe_substitute()` differ from `substitute()`?</summary>

`substitute()` raises a `KeyError` if any placeholder in the template is missing from the provided dictionary. `safe_substitute()` never raises `KeyError`; it leaves unsupplied placeholders (e.g. `"$missing_var"`) in the output string unchanged.
</details>

<details>
<summary>6. How can you perform case-insensitive replacement across multiple terms in a single pass?</summary>

Compile an alternation of all terms with `re.IGNORECASE`. Pass a dictionary mapping lowercased keys to replacements, and use `re.sub()` with a callback function:
```python
pattern.sub(lambda m: mapping[m.group(0).lower()], text)
```
</details>

<details>
<summary>7. What is the difference between `splitlines()` and `split('\n')`?</summary>

`split('\n')` only splits on the Unix newline character `\n` and leaves trailing `\r` carriage returns on Windows files. `splitlines()` transparently splits across all standard line break boundaries (`\r`, `\n`, `\r\n`, and Unicode line separators) and discards trailing empty lines.
</details>

<details>
<summary>8. How do text processing patterns behave in Free-threaded Python (PEP 703)?</summary>

Streaming pipelines and regex matchers allocate local string tokens within the executing worker thread. Because strings are immutable, multiple threads can process distinct chunks or lines concurrently without global lock contention.
</details>

---

## 10. Practical Exercises & Self-Assessment

<details>
<summary>Exercise 1: Safe SQL Identifier Formatter</summary>

**Task:** Use `string.Template` to format a table and column name into a query template without risk of format injection.

**Solution:**
```python
from string import Template

sql_tpl = Template("SELECT $column FROM $table WHERE active = TRUE;")
query = sql_tpl.substitute(column="email", table="users")
print(query)  # SELECT email FROM users WHERE active = TRUE;
```
</details>

<details>
<summary>Exercise 2: CLI Description Formatter</summary>

**Task:** Format a long command description with `textwrap` so that it has an initial indentation of `  ` and subsequent lines indented by `    ` with width 60.

**Solution:**
```python
import textwrap

desc = "This command audits all microservices across the staging cluster and outputs a consolidated security report."
formatted = textwrap.fill(desc, width=60, initial_indent="  * ", subsequent_indent="    ")
print(formatted)
```
</details>

<details>
<summary>Exercise 3: Markdown Header Extractor</summary>

**Task:** Stream lines from a markdown document and extract all Level 1 and Level 2 headers (`#` and `##`) using a generator.

**Solution:**
```python
def extract_headers(md_lines):
    for line in md_lines:
        s = line.strip()
        if s.startswith("# ") or s.startswith("## "):
            yield s

sample = ["# Introduction", "Text here...", "## Setup", "More text..."]
print(list(extract_headers(sample)))  # ['# Introduction', '## Setup']
```
</details>

<details>
<summary>Exercise 4: Redact Sensitive Keys in Text</summary>

**Task:** Replace words `["secret", "password", "token"]` with `"[REDACTED]"` in a single regex pass regardless of casing.

**Solution:**
```python
import re

terms = ["secret", "password", "token"]
regex = re.compile(r"\b(" + "|".join(terms) + r")\b", re.IGNORECASE)
redacted = regex.sub("[REDACTED]", "The password was leaked and the Secret token exposed.")
print(redacted)
# The [REDACTED] was leaked and the [REDACTED] [REDACTED] exposed.
```
</details>

<details>
<summary>Exercise 5: State Machine CSV Line Parser</summary>

**Task:** Write a simple tokenizer that parses comma-separated values where values can be optionally wrapped in double quotes.

**Solution:**
```python
import re

def parse_csv_line(line: str) -> list[str]:
    # Matches either quoted string or non-comma chars
    pattern = re.compile(r'"([^"]*)"|([^,]+)')
    fields = []
    for m in pattern.finditer(line):
        fields.append(m.group(1) if m.group(1) is not None else m.group(2).strip())
    return fields

print(parse_csv_line('101, "Shiva Ram", 95.5, "Active, Verified"'))
# ['101', 'Shiva Ram', '95.5', 'Active, Verified']
```
</details>

<details>
<summary>Exercise 6: Truncate Text at Word Boundary</summary>

**Task:** Truncate a sentence to at most 30 characters without cutting words in half using `textwrap.shorten()`.

**Solution:**
```python
import textwrap

sentence = "The deployment to European cluster completed without incident."
short = textwrap.shorten(sentence, width=30, placeholder="...")
print(short)  # The deployment to European...
assert len(short) <= 30
```
</details>
