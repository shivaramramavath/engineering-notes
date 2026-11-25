---
title: "Advanced Regular Expressions: The re Module"
description: "Mastering pattern matching in Python: regex compilation, capturing vs non-capturing groups, named groups, lookahead/lookbehind assertions, catastrophic backtracking (ReDoS) prevention, and dynamic substitution."
module: "10-standard-library-tour"
order: 5
tags:
  - python
  - regex
  - re
  - pattern-matching
  - redos
  - lookaround
  - text-processing
difficulty: "Intermediate"
cpython_version: "3.12+"
last_verified: "2026-03"
---

# Advanced Regular Expressions: The re Module

[<- Back to Module Index](./README.md)

---

## Table of Contents
1. [What & Why](#what--why)
2. [Core Concepts & Mental Model](#core-concepts--mental-model)
3. [How It Works Internally](#how-it-works-internally)
4. [Syntax & API Reference](#syntax--api-reference)
5. [Code Examples](#code-examples)
6. [Edge Cases, Pitfalls & Anti-Patterns](#edge-cases-pitfalls--anti-patterns)
7. [Performance & Memory Characteristics](#performance--memory-characteristics)
8. [CPython Internals & Bytecode](#cpython-internals--bytecode)
9. [Best Practices & Idiomatic Patterns](#best-practices--idiomatic-patterns)
10. [Real-World Case Study: Production Log Ingestion & PII Masking Pipeline](#real-world-case-study-production-log-ingestion--pii-masking-pipeline)
11. [Interview Questions & Deep-Dive Scenarios](#interview-questions--deep-dive-scenarios)
12. [Practical Exercises & Code Katas](#practical-exercises--code-katas)

---

## What & Why

Python’s built-in `re` module provides a battle-tested regular expression engine supporting Perl-compatible regular expressions (PCRE-style). It enables complex pattern matching, text extraction, validation, lexical tokenization, and dynamic substitutions.

### Motivation
While simple string methods like `str.startswith()`, `str.find()`, and `str.replace()` handle basic static substrings, real-world data processing frequently requires matching dynamic formats:
1. Validating structured patterns: Email addresses, UUIDs, ISO timestamps, SemVer strings, IP addresses.
2. Contextual extraction: Extracting user IDs only when preceded by specific keywords without capturing the keywords themselves (zero-width lookarounds).
3. Dynamic text transformations: Rewriting markdown links, sanitizing PII (Personally Identifiable Information), or formatting dates using functional replacement callbacks.
4. Lexical analysis: Tokenizing domain-specific languages (DSLs) or programming languages into tokens.

### Real-World Analogy
Consider an automated package sorting facility:
- `str.find("94103")` is a sorter looking only for a single pre-printed postal code.
- A **regular expression** is an optical scanner programmed with a structural blueprint: `"Look for exactly 5 digits, optionally followed by a hyphen and 4 digits, located immediately after a two-letter state abbreviation, but only if preceded by the word 'SHIP TO:'"`. The scanner extracts the exact match regardless of the surrounding text layout.

### When to Use vs When Not to Use
- **Use `re` for:** Pattern validation, structured token extraction, multi-group parsing, complex text replacements, and custom DSL lexing.
- **Do Not Use `re` for:**
  - Simple static substring checks (use `needle in haystack` or `str.startswith()` which are 5x-10x faster).
  - Parsing recursive, nested grammars like full HTML/XML or JSON (use specialized parsers like `html.parser`, BeautifulSoup, or `json`).
  - Scenarios where catastrophic backtracking (ReDoS) could be triggered by untrusted user input without timeouts.

---

## Core Concepts & Mental Model

```
+-------------------------------------------------------------------------+
| Regular Expression Pattern Compilation                                  |
| Pattern: r'(?P<ip>\d{1,3}(?:\.\d{1,3}){3})\s+-\s+\[(?P<ts>[^\]]+)\]'   |
+-------------------------------------------------------------------------+
                                    |
                                    v
+-------------------------------------------------------------------------+
| Compiled Pattern Object (re.compile)                                    |
| Bytecode representation of NFA (Nondeterministic Finite Automaton)      |
+-------------------------------------------------------------------------+
                                    |
        +---------------------------+---------------------------+
        |                           |                           |
        v                           v                           v
+---------------+           +---------------+           +---------------+
| search()      |           | finditer()    |           | sub()         |
| First match   |           | Lazy iterator |           | Transformation|
| anywhere      |           | of Match objs |           | with callback |
+---------------+           +---------------+           +---------------+
        |                           |
        +---------------------------+
                    |
                    v
+-------------------------------------------------------------------------+
| Match Object                                                            |
| - m.group(0): Full matched string                                       |
| - m.group("ip"): Named group extraction                                 |
| - m.span(): (start_index, end_index) character offsets                  |
+-------------------------------------------------------------------------+
```

### Key Distinctions
1. **`re.match()` vs `re.search()` vs `re.fullmatch()`**:
   - `re.match()`: Matches **only at the beginning** of the string (implicit `^`).
   - `re.search()`: Scans through the entire string to find the **first match anywhere**.
   - `re.fullmatch()`: Matches only if the **entire string** matches the pattern (implicit `^...$`).
2. **Capturing vs Non-Capturing Groups**:
   - `(...)`: Captures matched text into numbered group registers.
   - `(?P<name>...)`: Captures text into named registers accessible via `match.group("name")`.
   - `(?:...)`: Non-capturing group. Groups tokens for operators (e.g. `(?:abc)+`) without incurring group allocation overhead.
3. **Zero-Width Lookaround Assertions**:
   Matches text based on surrounding context without including that context in the match:
   - `(?=...)`: Positive lookahead (must be followed by `...`).
   - `(?!...)`: Negative lookahead (must not be followed by `...`).
   - `(?<=...)`: Positive lookbehind (must be preceded by `...`).
   - `(?<!...)`: Negative lookbehind (must not be preceded by `...`).

---

## How It Works Internally

### 1. The SRE (Secret Labs Regular Expression) Engine
CPython's regex engine is implemented in C (`Modules/_sre/sre.c`).
- It is a **backtracking NFA (Nondeterministic Finite Automaton)**.
- When `re.compile(pattern)` is called:
  1. The pattern string is parsed into an Abstract Syntax Tree (AST) by `sre_parse.py`.
  2. The AST is compiled into internal SRE bytecode: an array of 32-bit integer instructions (`OPCODE_RANGE`, `OPCODE_REPEAT`, `OPCODE_ASSERT`, `OPCODE_SUCCESS`).
  3. The compiled bytecode is cached in an internal LRU cache (default size 512 patterns).
- During matching, the C runtime executes the bytecode using recursive backtracking.

### 2. Catastrophic Backtracking (ReDoS) Mechanics
Consider the pattern: `(a+)+$` evaluated against the string `"aaaaaaaaaaaaaaaaaaaaX"`.
- The engine matches all `a` characters using the inner `a+`.
- When it reaches `"X"`, the pattern fails to match `$`.
- The engine then **backtracks**: it tries splitting the 20 `a` characters between the inner and outer groups in every possible combination ($2^{20} = 1,048,576$ paths!).
- For 30 characters, this requires over 1 billion evaluations, freezing the CPU thread for minutes: a **Regular Expression Denial of Service (ReDoS)** vulnerability.
- In Python 3.11+, SRE optimizations mitigate some nested repeats, but nesting unbounded quantifiers (`(a+)+`, `(a|a)+`) remains dangerous.

---

## Syntax & API Reference

### Primary Module Functions
```python
import re

# Compilation
pattern = re.compile(pattern: str, flags: int = 0)

# Matching
m = pattern.search(string, pos=0, endpos=len(string))   # First match anywhere
m = pattern.match(string)                               # Match from start only
m = pattern.fullmatch(string)                           # Entire string must match

# Multi-Match Extraction
all_matches: list[str | tuple] = pattern.findall(string) # Returns list (eager)
iter_matches = pattern.finditer(string)                 # Returns iterator of Match objects (lazy)

# Splitting and Substitution
tokens: list[str] = pattern.split(string, maxsplit=0)
new_str: str = pattern.sub(repl: str | Callable, string, count=0)
new_str, num_subs = pattern.subn(repl, string, count=0)
```

### Essential Compilation Flags
- `re.IGNORECASE` / `re.I`: Case-insensitive matching.
- `re.MULTILINE` / `re.M`: `^` and `$` match beginning and end of each line (`\n`), not just entire string.
- `re.DOTALL` / `re.S`: Dot (`.`) matches **any** character, including newline (`\n`).
- `re.VERBOSE` / `re.X`: Allows whitespace and inline `# comments` inside the pattern for readable formatting.
- `re.ASCII` / `re.A`: Makes `\w`, `\b`, `\s`, and `\d` match ASCII only instead of full Unicode tables.

---

## Code Examples

### Example 1: Named Groups and Dictionary Extraction
```python
import re

def run_named_groups_demo():
    # Parse Apache common log format line
    log_line = '192.168.1.42 - - [25/Nov/2026:14:32:10 +0000] "GET /api/v1/users HTTP/1.1" 200 4520'

    log_pattern = re.compile(
        r'^(?P<ip>\S+)\s+'
        r'\S+\s+\S+\s+'
        r'\[(?P<timestamp>[^\]]+)\]\s+'
        r'"(?P<method>[A-Z]+)\s+(?P<path>\S+)\s+[A-Z0-9/.]+"\s+'
        r'(?P<status>\d{3})\s+'
        r'(?P<bytes>\d+)$'
    )

    match = log_pattern.search(log_line)
    assert match is not None

    # Extract all named groups as a clean dictionary
    data = match.groupdict()
    print("Extracted Log Dictionary:")
    for key, val in data.items():
        print(f"  {key}: {val}")

    print("\nDirect Named Access:")
    print("  IP Address: ", match.group("ip"))
    print("  HTTP Status:", match.group("status"))
    print("  Span offsets of path:", match.span("path"))

run_named_groups_demo()
# Output:
# Extracted Log Dictionary:
#   ip: 192.168.1.42
#   timestamp: 25/Nov/2026:14:32:10 +0000
#   method: GET
#   path: /api/v1/users
#   status: 200
#   bytes: 4520
# 
# Direct Named Access:
#   IP Address:  192.168.1.42
#   HTTP Status: 200
#   Span offsets of path: (48, 61)
```

### Example 2: Zero-Width Lookarounds (Lookahead & Lookbehind)
```python
import re

def run_lookaround_demo():
    text = "User purchased 15 USD and sold 25 EUR. Total balance is $100."

    # Lookahead: Match numbers that are immediately followed by ' USD'
    # (?=\s+USD) checks for ' USD' ahead, but does NOT include it in the match!
    usd_amounts = re.findall(r'\d+(?=\s+USD)', text)
    print("USD Amounts (Positive Lookahead):", usd_amounts)

    # Negative Lookahead: Match numbers that are NOT followed by ' EUR'
    non_eur_numbers = re.findall(r'\b\d+\b(?!\s+EUR)', text)
    print("Non-EUR Numbers (Negative Lookahead):", non_eur_numbers)

    # Positive Lookbehind: Match digits preceded by dollar sign '$'
    # (?<=\$) checks that '$' is behind the digits, without matching '$' itself
    dollar_amounts = re.findall(r'(?<=\$)\d+', text)
    print("Dollar Amounts (Positive Lookbehind):", dollar_amounts)

run_lookaround_demo()
# Output:
# USD Amounts (Positive Lookahead): ['15']
# Non-EUR Numbers (Negative Lookahead): ['15', '100']
# Dollar Amounts (Positive Lookbehind): ['100']
```

### Example 3: Dynamic Transformation with `re.sub` Callback Functions
```python
import re

def run_sub_callback_demo():
    # Convert kebab-case and snake_case strings to camelCase
    text = "calculate_tax_rate and format-output-string should be updated."

    def to_camel_case(match: re.Match) -> str:
        # Group 1 is the character following hyphen or underscore
        char = match.group(1)
        return char.upper()

    # Matches [_ -] followed by a lowercase letter
    camel_pattern = re.compile(r'[_-]([a-z])')
    result = camel_pattern.sub(to_camel_case, text)

    print("Original Text:", text)
    print("CamelCased:   ", result)

run_sub_callback_demo()
# Output:
# Original Text: calculate_tax_rate and format-output-string should be updated.
# CamelCased:    calculateTaxRate and formatOutputString should be updated.
```

### Example 4: Self-Documenting Regex with `re.VERBOSE`
```python
import re

def run_verbose_regex_demo():
    # re.VERBOSE ignores whitespace and allows inline comments
    semver_pattern = re.compile(r"""
        ^
        (?P<major>0|[1-9]\d*)          # Major version (no leading zero)
        \.
        (?P<minor>0|[1-9]\d*)          # Minor version
        \.
        (?P<patch>0|[1-9]\d*)          # Patch version
        (?:-(?P<prerelease>            # Optional prerelease tag
            [0-9A-Za-z.-]+
        ))?
        (?:(\+)(?P<build>              # Optional build metadata
            [0-9A-Za-z.-]+
        ))?
        $
    """, re.VERBOSE)

    version_str = "v2.10.4-rc.1+build.8492"
    clean_ver = version_str.lstrip("v")
    match = semver_pattern.match(clean_ver)

    assert match is not None
    print(f"SemVer Parsed for '{version_str}':")
    print("  Major:", match.group("major"))
    print("  Minor:", match.group("minor"))
    print("  Patch:", match.group("patch"))
    print("  Pre:  ", match.group("prerelease"))
    print("  Build:", match.group("build"))

run_verbose_regex_demo()
# Output:
# SemVer Parsed for 'v2.10.4-rc.1+build.8492':
#   Major: 2
#   Minor: 10
#   Patch: 4
#   Pre:   rc.1
#   Build: build.8492
```

### Example 5: High-Performance Tokenizer using `finditer()`
```python
import re
from typing import NamedTuple, Iterator

class Token(NamedTuple):
    type: str
    value: str
    position: int

def tokenize_dsl(code: str) -> Iterator[Token]:
    token_specification = [
        ("NUMBER",   r'\d+(?:\.\d+)?'),         # Integer or decimal
        ("ASSIGN",   r':='),                     # Assignment operator
        ("OP",       r'[+\-*/]'),                # Arithmetic operators
        ("IDENT",    r'[A-Za-z_][A-Za-z0-9_]*'), # Identifiers
        ("SKIP",     r'[ \t]+'),                 # Whitespace
        ("MISMATCH", r'.'),                      # Any other character
    ]
    # Join into master regex with named groups
    master_regex = "|".join(f"(?P<{pair[0]}>{pair[1]})" for pair in token_specification)
    pattern = re.compile(master_regex)

    for mo in pattern.finditer(code):
        kind = mo.lastgroup
        value = mo.group()
        if kind == "SKIP":
            continue
        elif kind == "MISMATCH":
            raise SyntaxError(f"Unexpected token {value!r} at pos {mo.start()}")
        yield Token(kind, value, mo.start())

def run_tokenizer_demo():
    code_snippet = "x := 42.5 + y * 10"
    tokens = list(tokenize_dsl(code_snippet))
    print(f"Tokenized '{code_snippet}':")
    for t in tokens:
        print(f"  {t.type:8} : {t.value:10} (pos {t.position})")

run_tokenizer_demo()
# Output:
# Tokenized 'x := 42.5 + y * 10':
#   IDENT    : x          (pos 0)
#   ASSIGN   : :=         (pos 2)
#   NUMBER   : 42.5       (pos 5)
#   OP       : +          (pos 10)
#   IDENT    : y          (pos 12)
#   OP       : *          (pos 14)
#   NUMBER   : 10         (pos 16)
```

---

## Edge Cases, Pitfalls & Anti-Patterns

### 1. `re.match()` vs `re.search()` Confusion
A widespread pitfall is expecting `re.match("pattern", string)` to locate `"pattern"` anywhere inside `string`. `re.match()` tests matches **only at the beginning of the string**. If the target substring occurs at index 1, `re.match()` returns `None`.
- **Rule**: Use `re.search()` when looking for substrings anywhere in text.

### 2. Forgetting Raw String Literals (`r"..."`)
In standard Python strings, backslashes are interpreted as escape characters (`\n` is newline, `\t` is tab, `\b` is backspace). In regex, `\b` signifies a word boundary. Writing `"\\bword\\b"` requires double-escaping, whereas raw string literals (`r"\bword\b"`) disable Python-level escapes, preserving the literal backslashes for the SRE engine.

### 3. ReDoS (Catastrophic Backtracking) on Nested Quantifiers
Patterns like `^([a-zA-Z0-9]+)+$` or `(a|aa)+$` cause exponential execution times when presented with non-matching strings.
- **Rule**: Avoid nesting quantifiers. Use non-overlapping character classes (e.g. `^[a-zA-Z0-9]+$`).

### Common Mistakes Table

| Bad Code | Good Code | Why | Severity |
| :--- | :--- | :--- | :--- |
| `re.match(r"hello", "say hello")` | `re.search(r"hello", "say hello")` | `match` checks only start of string; returns `None`. | High |
| `"\\bword\\b"` | `r"\bword\b"` | Without `r"..."`, `\b` is parsed as ASCII backspace (`0x08`). | High |
| `re.compile("(a+)+$")` | `re.compile("a+$")` | Nested quantifiers cause exponential ReDoS thread freezes. | Critical |
| `re.compile(pattern)` inside a 1M loop | Compile once outside loop | Incurs repeated compilation overhead (even with internal cache). | Medium |
| `re.sub(r"\d+", "X", text)` for simple replace | `text.replace("foo", "bar")` | `str.replace` is 10x faster for static substring replacement. | Medium |
| Using `.*` greedily across lines | Use `.*?` or `[^"]*` | Greedy `.*` spans across unintended boundaries. | Medium |

---

## Performance & Memory Characteristics

### Computational & Memory Complexity
- **Deterministic Tokenization**: $\mathcal{O}(N)$ linear scan where $N$ is input string length.
- **Backtracking Evaluation**: Worst-case $\mathcal{O}(2^N)$ or $\mathcal{O}(N^K)$ on pathological ReDoS patterns.
- **Memory Consumption**: `finditer()` yields `Match` objects lazily ($\mathcal{O}(1)$ memory), while `findall()` creates an eager list containing all matches simultaneously ($\mathcal{O}(M)$).

### Benchmark: `re.compile()` vs Direct `re.search()`
- Direct `re.search("pattern", text)` checks CPython's internal 512-entry LRU cache (`_cache` in `re.py`).
- Pre-compiling once via `pattern = re.compile("pattern")` bypasses the cache lookup dictionary, yielding an approximate **15%–25% throughput speedup** in high-frequency loops.

---

## CPython Internals & Bytecode

In CPython:
- The SRE engine bytecode is compiled in `Lib/sre_compile.py`.
- The pattern bytecode consists of integer opcodes:
  - `FAILURE`: Backtrack to last savepoint.
  - `SUCCESS`: Match succeeded.
  - `ANY`: Matches any character.
  - `RANGE`: Matches character in range `[min, max]`.
  - `AT`: Matches assertions (`AT_BEGINNING`, `AT_END`, `AT_BOUNDARY`).
- The C execution loop in `Modules/_sre/sre.c` executes an iterative state machine with an explicit heap-allocated backtracking stack, avoiding C-level stack overflow crashes.

---

## Best Practices & Idiomatic Patterns

1. **Always Use Raw String Literals (`r"..."`)**:
   Eliminates backslash escaping ambiguities.
2. **Compile Regex Once at Module Scope**:
   Store compiled patterns as module-level constants (`PATTERN = re.compile(r"...")`).
3. **Use `re.VERBOSE` for Complex Expressions**:
   Makes long, impenetrable regular expressions readable, commented, and maintainable.
4. **Prefer Character Classes Over Alternation**:
   `[abc]` compiles to a single range check instruction, whereas `(a|b|c)` creates three distinct NFA branch choices.

---

## Real-World Case Study: Production Log Ingestion & PII Masking Pipeline

The following high-throughput processor parses semi-structured log streams, extracts operational metrics, and dynamically redacts Personally Identifiable Information (Credit Cards, Social Security Numbers, API Keys) using functional substitution.

```python
import re
from typing import Dict, Any, List

class LogSanitizer:
    def __init__(self):
        # Credit Card: 13 to 16 digits, with optional hyphens or spaces
        self.cc_pattern = re.compile(r'\b(?:\d[ -]?){13,16}\b')
        
        # Social Security Number (US): XXX-XX-XXXX
        self.ssn_pattern = re.compile(r'\b\d{3}-\d{2}-\d{4}\b')
        
        # Bearer / API Tokens
        self.auth_pattern = re.compile(r'(?i)\b(bearer|token|apikey)\s+([A-Za-z0-9_\-\.]{16,})\b')

        # Structured Log Format Parser
        self.log_parser = re.compile(r"""
            ^\[(?P<level>INFO|WARN|ERROR|DEBUG)\]\s+
            (?P<timestamp>\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z)\s+
            (?P<service>[A-Za-z0-9_-]+):\s+
            (?P<message>.*)$
        """, re.VERBOSE)

    def mask_pii(self, text: str) -> str:
        """Sanitizes sensitive data while preserving format structure."""
        # Redact SSN
        text = self.ssn_pattern.sub("[REDACTED_SSN]", text)
        
        # Redact Credit Cards (preserve last 4 digits)
        def mask_cc(match: re.Match) -> str:
            raw = re.sub(r'[^0-9]', '', match.group(0))
            return f"****-****-****-{raw[-4:]}"
        text = self.cc_pattern.sub(mask_cc, text)

        # Redact Auth Tokens
        text = self.auth_pattern.sub(r'\1 [REDACTED_TOKEN]', text)
        return text

    def process_log_line(self, line: str) -> Dict[str, Any]:
        match = self.log_parser.match(line)
        if not match:
            # Fallback for unformatted lines
            return {"level": "UNKNOWN", "message": self.mask_pii(line)}

        data = match.groupdict()
        data["message"] = self.mask_pii(data["message"])
        return data

# Verification
if __name__ == "__main__":
    sanitizer = LogSanitizer()
    sample_log = (
        "[ERROR] 2026-11-25T14:30:00Z auth-service: "
        "User 842 failed login with SSN 123-45-6789 and card 4111 1111 1111 9999. "
        "Bearer sk_live_99847291823749182374 failed auth."
    )

    result = sanitizer.process_log_line(sample_log)
    print("Sanitized Log Output:")
    print("  Service:  ", result["service"])
    print("  Level:    ", result["level"])
    print("  Timestamp:", result["timestamp"])
    print("  Clean Msg:", result["message"])

    assert "[REDACTED_SSN]" in result["message"]
    assert "****-****-****-9999" in result["message"]
    assert "Bearer [REDACTED_TOKEN]" in result["message"]
```

---

## Interview Questions & Deep-Dive Scenarios

<details>
<summary><strong>1. Junior: What is the difference between <code>re.search()</code> and <code>re.match()</code>?</strong></summary>

`re.match()` checks for a match **only at the beginning** of the string. If the target pattern occurs anywhere after the first character, `re.match()` returns `None`. `re.search()` scans through the entire string and returns a `Match` object for the first occurrence found anywhere.
</details>

<details>
<summary><strong>2. Junior: Why should regular expression patterns always be defined using raw strings (<code>r"..."</code>)?</strong></summary>

In standard Python strings, the backslash `\` is an escape character (e.g. `\n` is newline, `\b` is ASCII backspace). In regular expressions, `\b` represents a word boundary and `\d` represents a digit. Using raw string literals (`r"..."`) disables Python-level escape interpretation, passing raw backslashes directly to the regex compiler without requiring confusing double-escaping like `"\\\\b"`.
</details>

<details>
<summary><strong>3. Mid: What is the difference between capturing groups <code>(...)</code> and non-capturing groups <code>(?:...)</code>?</strong></summary>

- Capturing groups `(...)` remember the matched text, allocating an internal register in the `Match` object accessible via `match.group(N)`. This incurs memory allocation and performance overhead.
- Non-capturing groups `(?:...)` group tokens together for quantifiers (e.g. `(?:abc)+`) without capturing the matched text into memory, improving execution speed and keeping group indexing clean.
</details>

<details>
<summary><strong>4. Mid: What is catastrophic backtracking (ReDoS), and what pattern structures cause it?</strong></summary>

Catastrophic backtracking occurs in NFA regex engines when nested unbounded quantifiers (e.g. `(a+)+$` or `(a|b)+$`) fail to match a string. The engine recursively tries every mathematical permutation of distributing characters between the groups, leading to exponential time complexity ($\mathcal{O}(2^N)$). An attacker can submit short strings (e.g. 30 characters) that freeze the CPU thread for minutes.
</details>

<details>
<summary><strong>5. Senior: How do lookahead assertions (<code>(?=...)</code>) differ from standard matching groups?</strong></summary>

Lookahead assertions are **zero-width assertions**: they inspect characters ahead of the current engine cursor to verify that a sub-pattern matches (positive) or does not match (negative), but they **do not consume characters**. After the lookahead condition succeeds, the regex cursor remains at the exact same position it was before the assertion, allowing subsequent tokens to match the inspected text.
</details>

<details>
<summary><strong>6. Senior: How does <code>re.sub()</code> handle replacement functions compared to replacement strings?</strong></summary>

When `re.sub(pattern, repl, string)` is passed a string for `repl`, it replaces matches with static text, supporting backreferences like `\1` or `\g<name>`. When `repl` is a callable (function), the regex engine calls the function for every match, passing the `re.Match` object as an argument. The function returns a string that replaces the matched text dynamically, enabling programmatic transformations (e.g. unit conversions, casing shifts).
</details>

<details>
<summary><strong>7. Staff: How does CPython's SRE engine differ from Google's RE2 or Rust's regex engine?</strong></summary>

CPython's SRE engine uses a **backtracking NFA**, which supports advanced features like backreferences (`\1`) and arbitrary zero-width lookaround assertions. However, backtracking NFAs are vulnerable to exponential ReDoS attacks. RE2 and Rust use **DFA / Thompson NFA** engines, which guarantee linear time complexity ($\mathcal{O}(N)$) on all inputs by matching all branches simultaneously. To guarantee linear safety, DFA engines deliberately omit support for backreferences and lookaround assertions.
</details>

<details>
<summary><strong>8. Staff: How would you architect a safe regex execution sandbox for evaluating user-submitted regular expressions without risking ReDoS?</strong></summary>

1. Static AST Analysis: Reject patterns with nested quantifiers (e.g. using `sre_parse` to detect nested `MAX_REPEAT` opcodes).
2. Process Isolation: Execute regex evaluation inside an isolated child worker process (`multiprocessing` or `subprocess`) with strict CPU timeouts (`signal.alarm` on POSIX). If the evaluation exceeds 100 ms, kill the process immediately.
3. Third-party DFA fallback: Use Google's `re2` engine via C-bindings (`google-re2` Python package) for user-provided patterns, guaranteeing linear execution time.
</details>

---

## Practical Exercises & Code Katas

### Exercise 1: IPv4 Address Validator
**Requirement:** Write a regex pattern that validates an IPv4 address string (`0.0.0.0` to `255.255.255.255`), ensuring numbers do not exceed 255.

<details>
<summary>Show Solution</summary>

```python
import re

# Octet regex: 0-9, 10-99, 100-199, 200-249, 250-255
OCTET = r'(?:25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)'
IPV4_PATTERN = re.compile(rf'^{OCTET}(?:\.{OCTET}){{3}}$')

def is_valid_ipv4(ip: str) -> bool:
    return bool(IPV4_PATTERN.match(ip))

# Verification
assert is_valid_ipv4("192.168.1.1") is True
assert is_valid_ipv4("255.255.255.255") is True
assert is_valid_ipv4("256.100.0.1") is False
assert is_valid_ipv4("192.168.1") is False
print("IPv4 validation verified.")
```
</details>

### Exercise 2: Markdown Link Extractor
**Requirement:** Use named groups to extract the anchor text and URL from markdown links of the format `[Anchor Text](https://example.com)`.

<details>
<summary>Show Solution</summary>

```python
import re

LINK_PATTERN = re.compile(r'\[(?P<text>[^\]]+)\]\((?P<url>https?://[^\s)]+)\)')

def extract_markdown_links(content: str) -> list[dict[str, str]]:
    return [m.groupdict() for m in LINK_PATTERN.finditer(content)]

# Test
doc = "Check [Python](https://python.org) and [FastAPI](https://fastapi.tiangolo.com) for details."
links = extract_markdown_links(doc)
print("Extracted Links:", links)
assert len(links) == 2
assert links[0]["text"] == "Python"
```
</details>

### Exercise 3: ISO 8601 Date Formatter
**Requirement:** Write a function using `re.sub` that transforms US formatted dates (`MM/DD/YYYY`) into ISO standard format (`YYYY-MM-DD`).

<details>
<summary>Show Solution</summary>

```python
import re

DATE_PATTERN = re.compile(r'\b(?P<m>\d{2})/(?P<d>\d{2})/(?P<y>\d{4})\b')

def us_to_iso_dates(text: str) -> str:
    # Use backreferences to named groups in replacement string
    return DATE_PATTERN.sub(r'\g<y>-\g<m>-\g<d>', text)

# Test
raw = "Meeting dates: 11/25/2026 and 12/01/2026."
formatted = us_to_iso_dates(raw)
print("Formatted Dates:", formatted)
assert formatted == "Meeting dates: 2026-11-25 and 2026-12-01."
```
</details>

### Exercise 4: Duplicate Word Detector
**Requirement:** Write a regex using backreferences to find repeated adjacent words in a text (e.g. `"the the"` or `"is is"`).

<details>
<summary>Show Solution</summary>

```python
import re

# \b(\w+)\s+\1\b matches any word followed by whitespace and the EXACT same word
DUP_PATTERN = re.compile(r'\b(?P<word>\w+)\s+(?P=word)\b', re.IGNORECASE)

def find_duplicate_words(text: str) -> list[str]:
    return [m.group("word") for m in DUP_PATTERN.finditer(text)]

# Test
sentence = "Paris in the the spring is is beautiful."
dups = find_duplicate_words(sentence)
print("Duplicate Words Found:", dups)
assert dups == ["the", "is"]
```
</details>

### Exercise 5: Password Complexity Validator
**Requirement:** Use lookahead assertions in a single regular expression to validate that a password has at least 8 characters, at least one uppercase letter, one lowercase letter, one digit, and one special symbol (`!@#$%^&*`).

<details>
<summary>Show Solution</summary>

```python
import re

# Lookahead assertions enforce conditions without consuming characters
PASSWORD_REGEX = re.compile(r"""
    ^
    (?=.*[A-Z])            # At least one uppercase letter
    (?=.*[a-z])            # At least one lowercase letter
    (?=.*\d)               # At least one digit
    (?=.*[!@#$%^&*])       # At least one special character
    [A-Za-z\d!@#$%^&*]{8,} # Minimum 8 characters total
    $
""", re.VERBOSE)

def is_strong_password(pwd: str) -> bool:
    return bool(PASSWORD_REGEX.match(pwd))

# Verification
assert is_strong_password("Secret1!") is True
assert is_strong_password("weakpass") is False
assert is_strong_password("NoSpecial123") is False
print("Password complexity regex verified.")
```
</details>

### Exercise 6: CamelCase to Snake_Case Converter
**Requirement:** Implement a function that converts `CamelCase` identifiers into `snake_case` using two lookahead/lookbehind regex substitution passes.

<details>
<summary>Show Solution</summary>

```python
import re

def camel_to_snake(name: str) -> str:
    # Insert underscore between lowercase/number and uppercase: (?<=[a-z0-9])[A-Z]
    s1 = re.sub(r'(?<=[a-z0-9])([A-Z])', r'_\1', name)
    # Insert underscore between multiple uppercase and lowercase: (?<=[A-Z])([A-Z][a-z])
    s2 = re.sub(r'(?<=[A-Z])([A-Z][a-z])', r'_\1', s1)
    return s2.lower()

# Verification
assert camel_to_snake("DeviceStatus") == "device_status"
assert camel_to_snake("HTTPServerConfig") == "http_server_config"
assert camel_to_snake("SimpleXMLParser") == "simple_xml_parser"
print("Camel to snake case verified.")
```
</details>
