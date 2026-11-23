# JavaScript RegExp: Modern Flags, Lookaround Assertions, V8 Irregexp & ReDoS Defense

> Comprehensive technical reference on ECMAScript Regular Expressions, modern flags (`d`, `v`, `y`, `s`), named capture groups, lookaround assertions, the V8 Irregexp engine, and catastrophic backtracking defense.

| Metadata | Details |
| :--- | :--- |
| **Level** | Core / Engine Internals |
| **Prerequisites** | [09_built-in-objects/01_string.md](01_string.md) |
| **Time to Read** | 28 minutes |
| **Status** | Complete |
| **ECMAScript Baseline** | ES2018 (Lookbehind, named groups, s, \p{}) / ES2020 (matchAll) / ES2022 (/d indices) / ES2024 (/v unicodeSets) |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts: Flags, Assertions & Capture Groups](#2-core-concepts-flags-assertions--capture-groups)
3. [How It Works: Engine Architecture & State Machine Execution](#3-how-it-works-engine-architecture--state-machine-execution)
4. [Comprehensive Syntax & API Reference](#4-comprehensive-syntax--api-reference)
5. [Practical Examples](#5-practical-examples)
6. [Edge Cases, Pitfalls & Gotchas](#6-edge-cases-pitfalls--gotchas)
7. [Common Mistakes & Anti-Patterns](#7-common-mistakes--anti-patterns)
8. [Performance Characteristics & Catastrophic Backtracking (ReDoS)](#8-performance-characteristics--catastrophic-backtracking-redos)
9. [Engine Internals: V8 Irregexp Bytecode & Native Compilation](#9-engine-internals-v8-irregexp-bytecode--native-compilation)
10. [Best Practices & Design Guidelines](#10-best-practices--design-guidelines)
11. [Real-World Case Study: Production Markdown Lexer & High-Throughput Tokenizer](#11-real-world-case-study-production-markdown-lexer--high-throughput-tokenizer)
12. [Interview Questions & Deep Dives](#12-interview-questions--deep-dives)
13. [Exercises & Challenges](#13-exercises--challenges)

---

## 1. What & Why

In JavaScript, **Regular Expressions (`RegExp`)** provide a formal, highly optimized pattern-matching language for text search, validation, extraction, and lexical tokenization.

Historically, JavaScript regular expressions lacked critical features present in PCRE (Perl Compatible Regular Expressions), Python, and Java. Over recent ECMAScript editions (ES2018 to ES2024), the language underwent a massive upgrade:
- **Lookbehind Assertions (`(?<=...)`, `(?<!...)`):** Inspect preceding text without including it in the match.
- **Named Capture Groups (`(?<name>...)`):** Provide semantic names for captured segments instead of fragile numbered indices.
- **Indices Flag (`/d`):** Expose start and end slice offsets for matched sub-strings.
- **Set Operations (`/v`):** Perform intersection (`&&`) and subtraction (`--`) on Unicode character sets.

Modern engines execute regular expressions using **V8 Irregexp**, which compiles patterns directly into native machine code. Mastering RegExp in JavaScript requires understanding not only syntactic patterns, but also internal backtracking limits to prevent **Regular Expression Denial of Service (ReDoS)**.

---

## 2. Core Concepts: Flags, Assertions & Capture Groups

### 2.1 The 8 ECMAScript Regex Flags
Flags alter how the engine parses and matches text:

| Flag | Name | Purpose | Baseline |
| :--- | :--- | :--- | :--- |
| `g` | Global | Finds all matches rather than stopping at the first match. | ES3 |
| `i` | Ignore Case | Case-insensitive matching (`a` matches `A`). | ES3 |
| `m` | Multiline | `^` and `$` match beginning and end of lines, not just whole string. | ES3 |
| `s` | DotAll | Allows `.` to match newline characters (`\n`, `\r`, `\u2028`, `\u2029`). | ES2018 |
| `u` | Unicode | Enables 32-bit Unicode code point matching and UTF-16 surrogate pairs. | ES2015 |
| `v` | UnicodeSets | Supersedes `u`; enables Unicode character class set operations (`&&`, `--`). | ES2024 |
| `y` | Sticky | Matches strictly at `lastIndex` without scanning forward. | ES2015 |
| `d` | HasIndices | Populates `.indices` array containing start/end slice offsets. | ES2022 |

### 2.2 Lookaround Assertions
Assertions match a position without consuming characters:
- **Positive Lookahead `(?=pattern)`:** Matches if `pattern` succeeds to the right.
- **Negative Lookahead `(?!pattern)`:** Matches if `pattern` does NOT succeed to the right.
- **Positive Lookbehind `(?<=pattern)`:** Matches if `pattern` succeeds to the left.
- **Negative Lookbehind `(?<!pattern)`:** Matches if `pattern` does NOT succeed to the left.

```javascript
// Match currency amounts preceded by '$' without capturing '$':
const priceRegex = /(?<=\$)\d+(\.\d{2})?/;
console.log("Total: $49.99".match(priceRegex)[0]); // "49.99"
```

### 2.3 Named Capture Groups
Instead of tracking array indices `match[1]`, `match[2]`, ES2018 allows semantic grouping:
```javascript
const datePattern = /(?<year>\d{4})-(?<month>\d{2})-(?<day>\d{2})/;
const match = "2026-11-23".match(datePattern);

console.log(match.groups.year);  // "2026"
console.log(match.groups.month); // "11"
console.log(match.groups.day);   // "23"
```

---

## 3. How It Works: Engine Architecture & State Machine Execution

When a regex is evaluated, the engine constructs a **Non-Deterministic Finite Automaton (NFA)** with backtracking:

```
Pattern: /a(b|c)d/
Input:   "acd"

[Start] ---> (Match 'a') ---> (Branch: Try 'b') -x (Fail)
                                    |
                            [Backtrack to 'a']
                                    |
                              (Try 'c') ---> (Match 'd') ---> [Success!]
```

### Backtracking Mechanics:
1. When encountering a choice point (alternation `|` or quantifiers `*`, `+`, `?`), the engine records the current state and tries the first branch greedily.
2. If subsequent characters fail to match, the engine **backtracks**: it unwinds to the saved choice point and explores the alternate path.
3. If choices are nested (e.g., `(a+)+`), the number of paths grows exponentially ($O(2^n)$), causing execution to freeze the thread in a ReDoS loop.

---

## 4. Comprehensive Syntax & API Reference

### 4.1 RegExp Methods
- `regex.test(str)`: Returns boolean `true` if pattern matches; `false` otherwise.
- `regex.exec(str)`: Returns a detailed match array containing `index`, `groups`, `indices` (if `/d` enabled), or `null`. Updates `regex.lastIndex` if `/g` or `/y` is set.

### 4.2 String Methods Supporting RegExp
| Method | Description | Return Value |
| :--- | :--- | :--- |
| `str.search(regex)` | Finds position of first match | Index number or `-1` |
| `str.match(regex)` | Returns first match or all matches (if `/g`) | Array or `null` |
| `str.matchAll(regex)` | Returns iterator of full matches with capture groups | Iterator of Match Arrays (Requires `/g`) |
| `str.replace(regex, rep)` | Replaces matched patterns | New string |
| `str.split(regex, limit)` | Splits string at pattern boundaries | Array of strings |

### 4.3 Match Indices Flag (`/d`) Reference
Standardized in ES2022, the `/d` flag populates the `.indices` property on match objects:
```javascript
const regex = /(?<word>\w+)/d;
const match = regex.exec("Hello world");

console.log(match.indices[0]);           // [0, 5] (Start and end offsets for whole match)
console.log(match.indices.groups.word);  // [0, 5] (Offsets for named group)
```

---

## 5. Practical Examples

### Example 1: Semantic Versioning (SemVer) Parser with Named Groups
```javascript
const semverRegex = /^v?(?<major>\d+)\.(?<minor>\d+)\.(?<patch>\d+)(?:-(?<prerelease>[0-9A-Za-z.-]+))?(?:\+(?<build>[0-9A-Za-z.-]+))?$/;

function parseSemVer(version) {
  const match = semverRegex.exec(version);
  if (!match) throw new Error(`Invalid SemVer format: ${version}`);
  
  return {
    major: Number(match.groups.major),
    minor: Number(match.groups.minor),
    patch: Number(match.groups.patch),
    prerelease: match.groups.prerelease || null,
    build: match.groups.build || null
  };
}

console.log(parseSemVer("v2.4.1-rc.1+build.120"));
// Output: { major: 2, minor: 4, patch: 1, prerelease: 'rc.1', build: 'build.120' }
```

### Example 2: High-Performance Sticky Tokenizer (Lexer)
Using the sticky flag `/y` allows a lexer to parse tokens without allocating intermediate substrings via `slice()`:
```javascript
const lexerRegex = /(?<whitespace>\s+)|(?<number>\d+)|(?<ident>[a-zA-Z_]\w*)|(?<op>[+\-*/=])/y;

function* tokenize(code) {
  lexerRegex.lastIndex = 0; // Reset sticky pointer
  
  while (lexerRegex.lastIndex < code.length) {
    const match = lexerRegex.exec(code);
    if (!match) {
      throw new Error(`Unexpected token at index ${lexerRegex.lastIndex}: "${code[lexerRegex.lastIndex]}"`);
    }

    const [type] = Object.entries(match.groups).find(([_, val]) => val !== undefined);
    if (type !== "whitespace") {
      yield { type, value: match[0], offset: match.index };
    }
  }
}

const tokens = [...tokenize("let x = 42 + 8")];
console.log(tokens);
// [
//   { type: 'ident', value: 'let', offset: 0 },
//   { type: 'ident', value: 'x', offset: 4 },
//   { type: 'op', value: '=', offset: 6 },
//   { type: 'number', value: '42', offset: 8 },
//   { type: 'op', value: '+', offset: 11 },
//   { type: 'number', value: '8', offset: 13 }
// ]
```

### Example 3: Sanitizing Sensitive Data via Lookbehind & Lookahead
```javascript
// Redact credit card numbers (preserve first 4 and last 4 digits)
function maskCreditCard(text) {
  // Matches middle 8 digits of a 16-digit card
  return text.replace(/(?<=\b\d{4})[-\s]?\d{4}[-\s]?\d{4}(?=[-\s]?\d{4}\b)/g, "-XXXX-XXXX");
}

const rawLog = "Customer paid using card: 4111 2222 3333 4444 successfully.";
console.log(maskCreditCard(rawLog));
// "Customer paid using card: 4111-XXXX-XXXX-4444 successfully."
```

### Example 4: Unicode Set Operations with the `/v` Flag (ES2024)
```javascript
// Match Greek characters excluding vowels using set subtraction (--):
// \p{Script=Greek} minus [aeiou vowels]:
const greekConsonants = /^[\p{Script=Greek}--[αεηιουω]]+$/v;

console.log(greekConsonants.test("βγδ")); // true
console.log(greekConsonants.test("βα"));  // false ('α' is excluded)
```

### Example 5: Complete Query String Parameter Extraction via `matchAll`
```javascript
function parseQueryParams(queryString) {
  const paramRegex = /(?:[?&])(?<key>[^=&]+)=(?<value>[^&#]*)/g;
  const params = {};

  for (const match of queryString.matchAll(paramRegex)) {
    const key = decodeURIComponent(match.groups.key);
    const value = decodeURIComponent(match.groups.value);
    params[key] = value;
  }
  return params;
}

const url = "https://api.domain.com/v1/search?query=javascript&sort=asc&page=2";
console.log(parseQueryParams(url));
// { query: 'javascript', sort: 'asc', page: '2' }
```

---

## 6. Edge Cases, Pitfalls & Gotchas

### 6.1 The State Mutation Bug of `lastIndex` with `/g`
When a RegExp has the global (`g`) or sticky (`y`) flag, it is **stateful**. Every call to `.test()` or `.exec()` updates the `lastIndex` property. If you reuse the same regex instance across calls, `.test()` alternates between returning `true` and `false`!

```javascript
const isEmail = /^[a-z]+@[a-z]+\.[a-z]+$/g; // Unnecessary 'g' flag!

console.log(isEmail.test("user@domain.com")); // true  (lastIndex set to 15)
console.log(isEmail.test("user@domain.com")); // false (Searches starting from index 15!)
console.log(isEmail.test("user@domain.com")); // true  (lastIndex resets to 0)
```
**Fix:** Never use the `/g` flag when validating single inputs with `.test()`.

### 6.2 Escaping Dynamic User Input for `new RegExp()`
Passing unescaped user input into `new RegExp(userInput)` allows special characters (`.`, `*`, `?`, `(`) to alter the expression grammar or crash the app with `SyntaxError`:
```javascript
function escapeRegExp(string) {
  return string.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
}

const searchKeyword = "user.name[0]";
const safeRegex = new RegExp(escapeRegExp(searchKeyword), "i");
console.log(safeRegex.test("Filter: user.name[0] matched")); // true
```

### 6.3 Empty String Matches and Infinite Loops
A regex that can match zero characters (e.g., `/\d*/g`) matches empty strings. If manually iterating with `lastIndex`, an empty match at the same index will cause an infinite loop unless `lastIndex` is manually advanced.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Bad Practice | Idiomatic Solution | Reason |
| :--- | :--- | :--- | :--- |
| **Using `/g` flag in validation** | `/^\d+$/g.test(str)` | `/^\d+$/.test(str)` | `/g` mutates `lastIndex`, causing alternating `true`/`false` results. |
| **Unescaped `new RegExp()`** | `new RegExp(userInput)` | `new RegExp(escapeRegExp(userInput))` | Unescaped user strings cause syntax errors and ReDoS vulnerabilities. |
| **Parsing HTML with RegExp** | `/<div id="(.+)">/` | Use DOMParser, Cheerio, or AST parsers | HTML is not a regular language; nesting and attributes defeat regex. |
| **Greedy Quantifiers in Delimiters** | `/<tag>.*<\/tag>/` | `/<tag>.*?<\/tag>/` | Greedy `.*` overshoots past closing tags to the very end of the file. |
| **Recompiling in Loops** | `arr.filter(s => new RegExp("pattern").test(s))` | Compile once outside: `const re = /pattern/;` | Compiling regex inside loops wastes CPU recompiling state machines. |
| **Numbered Groups for Complex Regex** | `match[1]`, `match[2]` | Named groups: `(?<name>...)` | Changing pattern order breaks numbered indices silently. |

---

## 8. Performance Characteristics & Catastrophic Backtracking (ReDoS)

### 8.1 Catastrophic Backtracking Explained
A **Regular Expression Denial of Service (ReDoS)** occurs when a pattern with nested, overlapping quantifiers evaluates against a non-matching input:

```javascript
// Vulnerable ReDoS Pattern:
const evilRegex = /(a+)+$/;

// Matching input: Fast
evilRegex.test("aaaaaaaaaaaaaaaaaaaa!"); // Evaluates 2^N branch possibilities!
```
For an input of 30 characters, the engine evaluates over $1,000,000,000$ backtracking paths, freezing the single-threaded Node.js event loop for seconds or minutes.

### 8.2 Defending Against ReDoS
1. **Avoid nested quantifiers:** Never write `(a+)*`, `(a|b+)+`, or `(.*a)+`.
2. **Use possessive or atomic grouping emulations:** Ensure branches are mutually exclusive.
3. **Enforce input length limits:** Validate string length (`str.length < 1000`) before running complex regular expressions.

---

## 9. Engine Internals: V8 Irregexp Bytecode & Native Compilation

In V8, regular expressions are handled by a dedicated engine called **Irregexp**:

```
 RegExp Pattern: /ab*c/
          |
          v
 Irregexp Parser & AST Generator
          |
          v
 Irregexp Bytecode (Bytecode Engine)
          |
          v (After warm execution / JIT optimization)
 Native Machine Code (x86_64 / ARM64 assembly)
```

### Irregexp Optimization Passes:
1. **Boyer-Moore String Search:** If the pattern contains a fixed literal prefix, Irregexp uses Boyer-Moore or Horspool algorithms to skip characters rapidly without checking character-by-character.
2. **Native Assembly JIT:** For high-throughput expressions, Irregexp compiles the state machine directly into native assembly code, bypassing the V8 JavaScript interpreter.
3. **Dispatch Tables:** Alternations like `[a-z]` are compiled into bitmasks and branch tables for single-cycle character classification.

---

## 10. Best Practices & Design Guidelines

1. **Pre-Compile Static Expressions:** Declare RegExp literals at the module top level so they are compiled once at initialization rather than on every function invocation.
2. **Use Named Capture Groups:** Self-documenting regex patterns make code resilient to refactoring.
3. **Use `/d` for Code Editors and Syntax Highlighters:** The ES2022 `/d` flag eliminates manual `indexOf` searches by providing exact start and end offsets.
4. **Prefer String Methods for Simple Lookups:** For basic substring checks, `str.includes()` and `str.startsWith()` are $5\times$ faster than instantiating a RegExp.

---

## 11. Real-World Case Study: Production Markdown Lexer & High-Throughput Tokenizer

### Problem
A documentation generator parsed thousands of markdown files on every Git commit. The initial regex parser used monolithic expressions with `.*` and global replacement, causing high memory churn and hanging indefinitely on corrupted codeblocks due to ReDoS.

### Solution
Implement an encapsulated, non-backtracking block tokenizer using atomic character classes and sticky `/y` iteration:

```javascript
class MarkdownTokenizer {
  // Safe, non-backtracking patterns:
  static HEADER = /^(?<hashes>#{1,6})\s+(?<text>[^\r\n]+)/;
  static CODE_BLOCK = /^```(?<lang>[a-zA-Z0-9_-]*)\r?\n(?<code>[\s\S]*?)\r?\n```/;
  static LIST_ITEM = /^[-*+]\s+(?<content>[^\r\n]+)/;
  static PARAGRAPH = /^[^\r\n]+/;

  static parse(markdown) {
    const lines = markdown.split(/\r?\n/);
    const tokens = [];
    let i = 0;

    while (i < lines.length) {
      const line = lines[i];

      // Empty line
      if (line.trim() === "") {
        i++;
        continue;
      }

      // 1. Headers:
      const headerMatch = MarkdownTokenizer.HEADER.exec(line);
      if (headerMatch) {
        tokens.push({
          type: "header",
          level: headerMatch.groups.hashes.length,
          text: headerMatch.groups.text.trim()
        });
        i++;
        continue;
      }

      // 2. Fenced Code Block:
      if (line.startsWith("```")) {
        const lang = line.slice(3).trim();
        const codeLines = [];
        i++;
        while (i < lines.length && !lines[i].startsWith("```")) {
          codeLines.push(lines[i]);
          i++;
        }
        tokens.push({
          type: "codeblock",
          language: lang || "text",
          code: codeLines.join("\n")
        });
        i++; // Skip closing ```
        continue;
      }

      // 3. Fallback to Paragraph:
      tokens.push({ type: "paragraph", text: line });
      i++;
    }

    return tokens;
  }
}

const doc = `
# JavaScript Architecture

\`\`\`javascript
const x = 42;
\`\`\`

This is high performance.
`;

console.log(MarkdownTokenizer.parse(doc));
// Outputs:
// [
//   { type: 'header', level: 1, text: 'JavaScript Architecture' },
//   { type: 'codeblock', language: 'javascript', code: 'const x = 42;' },
//   { type: 'paragraph', text: 'This is high performance.' }
// ]
```

---

## 12. Interview Questions & Deep Dives

<details>
<summary><strong>Q1: Why does <code>regex.test()</code> alternate between returning <code>true</code> and <code>false</code> when using the <code>/g</code> flag?</strong></summary>

The `/g` (global) flag makes the `RegExp` instance stateful by tracking the character position of the last successful match in its `lastIndex` property. When `.test()` matches, it sets `lastIndex` to the end of the match. The next call starts searching from `lastIndex`. When it reaches the end of the string and fails to find another match, it returns `false` and resets `lastIndex` to `0`, causing the cycle to repeat.
</details>

<details>
<summary><strong>Q2: What is ReDoS (Regular Expression Denial of Service), and what causes it?</strong></summary>

ReDoS is an algorithmic complexity vulnerability that occurs when a regex engine uses backtracking to evaluate ambiguous patterns with nested quantifiers (e.g., `(a+)+$`). When evaluated against non-matching input strings, the number of backtracking branches increases exponentially ($O(2^n)$), causing the CPU to spike to 100% and freezing the JavaScript main thread.
</details>

<details>
<summary><strong>Q3: What is the purpose of the <code>/d</code> flag introduced in ECMAScript 2022?</strong></summary>

The `/d` (hasIndices) flag instructs the engine to record the start and end character indices for the full match and for every individual capture group. It exposes these slice bounds under the `.indices` property on the resulting match array (e.g., `match.indices[0] === [start, end]`), eliminating manual substring coordinate calculations.
</details>

<details>
<summary><strong>Q4: How does the sticky flag <code>/y</code> differ from the global flag <code>/g</code>?</strong></summary>

Both flags track `lastIndex`. However, when `exec()` is called on a `/g` regex, the engine scans *forward* through the string until it finds the next match. With `/y` (sticky), the engine attempts to match *strictly and exclusively* at the exact character index specified by `lastIndex`. If no match begins at `lastIndex`, it fails immediately without scanning ahead.
</details>

<details>
<summary><strong>Q5: What is the difference between positive lookahead <code>(?=...)</code> and positive lookbehind <code>(?<=...)</code>?</strong></summary>

A **positive lookahead** tests if a pattern appears immediately *after* the current position. A **positive lookbehind** tests if a pattern appears immediately *before* the current position. Both are zero-width assertions, meaning they verify conditions without including the matched characters in the final match result.
</details>

<details>
<summary><strong>Q6: How does the new <code>/v</code> flag (ES2024) improve upon the <code>/u</code> flag?</strong></summary>

The `/v` (unicodeSets) flag supersedes `/u`. In addition to full UTF-16 surrogate pair and code point support, it adds set operations inside character classes: set subtraction (`--`), set intersection (`&&`), and support for matching multi-character Unicode grapheme clusters (such as emoji flags).
</details>

<details>
<summary><strong>Q7: What is the V8 Irregexp engine?</strong></summary>

Irregexp is V8's specialized regular expression compiler and runtime. It compiles regular expressions into specialized bytecode instructions and, for hot expressions, directly into optimized native machine code (x86_64/ARM). It implements Boyer-Moore fast-scanning optimizations for fixed string prefixes.
</details>

<details>
<summary><strong>Q8: Why does <code>/./.test("\n")</code> return false, and how do you make it return true?</strong></summary>

By specification, the dot `.` metacharacter matches any character *except* newline terminators (`\n`, `\r`, `\u2028`, `\u2029`). To make the dot match newlines, enable the `s` (`dotAll`) flag introduced in ES2018: `/./s.test("\n")` returns `true`.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Robust URL Parser using Named Capture Groups
Write a function `parseUrl(url)` that extracts the `protocol`, `hostname`, `port` (optional), and `pathname` from a URL string using a single regular expression with named groups.

<details>
<summary><strong>Solution</strong></summary>

```javascript
function parseUrl(urlString) {
  const urlRegex = /^(?<protocol>https?):\/\/(?<hostname>[^:/\s]+)(?::(?<port>\d+))?(?<pathname>\/[^?\s]*)?$/;
  const match = urlRegex.exec(urlString);
  if (!match) return null;

  return {
    protocol: match.groups.protocol,
    hostname: match.groups.hostname,
    port: match.groups.port ? Number(match.groups.port) : (match.groups.protocol === "https" ? 443 : 80),
    pathname: match.groups.pathname || "/"
  };
}

console.log(parseUrl("https://api.github.com:8443/repos"));
// Output: { protocol: 'https', hostname: 'api.github.com', port: 8443, pathname: '/repos' }
```
</details>

### Exercise 2: CamelCase to Kebab-Case Converter via Lookarounds
Convert a camelCase string into kebab-case without using capturing group indices or string slicing.

<details>
<summary><strong>Solution</strong></summary>

```javascript
function camelToKebab(str) {
  // Matches position between lowercase letter and uppercase letter:
  return str
    .replace(/(?<=[a-z0-9])(?=[A-Z])/g, "-")
    .toLowerCase();
}

console.log(camelToKebab("backgroundColor")); // "background-color"
console.log(camelToKebab("parsedJsonPayload")); // "parsed-json-payload"
```
</details>

### Exercise 3: Validating Passwords with Strict Lookaheads
Write a regex validation function `validatePassword(pw)` that verifies all of the following rules without multiple loops:
1. At least 8 characters long
2. Contains at least one uppercase letter
3. Contains at least one lowercase letter
4. Contains at least one number
5. Contains at least one special symbol (`!@#$%^&*`)

<details>
<summary><strong>Solution</strong></summary>

```javascript
function validatePassword(password) {
  const pwRegex = /^(?=.*[a-z])(?=.*[A-Z])(?=.*\d)(?=.*[!@#$%^&*])[A-Za-z\d!@#$%^&*]{8,}$/;
  return pwRegex.test(password);
}

console.log(validatePassword("Pass123!")); // true
console.log(validatePassword("password")); // false (no uppercase, number, symbol)
console.log(validatePassword("P1!"));     // false (too short)
```
</details>
