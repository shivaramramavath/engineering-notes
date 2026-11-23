# JavaScript Template Literals & Tagged Templates: DSLs, Sanitization & Raw Strings

> Comprehensive guide to ECMAScript template literals, string interpolation, multiline formatting, tagged template functions, raw strings (`String.raw`), and building safe DSLs (SQL/HTML).

| Metadata | Details |
| :--- | :--- |
| **Level** | Core / Intermediate |
| **Prerequisites** | [01_fundamentals/02_data-types.md](../01_fundamentals/02_data-types.md), [02_functions/01_declarations-and-expressions.md](../02_functions/01_declarations-and-expressions.md) |
| **Time to Read** | 21 minutes |
| **Status** | Complete |
| **ECMAScript Baseline** | ES2015 (Template Literals & Tags) / ES2018 (Template Literal Revision) |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts: Interpolation & Tagged Templates](#2-core-concepts-interpolation--tagged-templates)
3. [How It Works: Tagged Template Execution Model](#3-how-it-works-tagged-template-execution-model)
4. [Syntax & API Reference](#4-syntax--api-reference)
5. [Practical Examples](#5-practical-examples)
6. [Edge Cases, Pitfalls & Gotchas](#6-edge-cases-pitfalls--gotchas)
7. [Common Mistakes & Anti-Patterns](#7-common-mistakes--anti-patterns)
8. [Performance Characteristics](#8-performance-characteristics)
9. [Engine Internals: V8 Template Object Caching](#9-engine-internals-v8-template-object-caching)
10. [Best Practices & Design Guidelines](#10-best-practices--design-guidelines)
11. [Real-World Case Study: SQL Injection Defense via Tagged Query Builder](#11-real-world-case-study-sql-injection-defense-via-tagged-query-builder)
12. [Interview Questions & Deep Dives](#12-interview-questions--deep-dives)
13. [Exercises & Challenges](#13-exercises--challenges)

---

## 1. What & Why

Before ECMAScript 2015 (ES6), constructing dynamic strings in JavaScript was tedious, visually noisy, and prone to formatting errors:
```javascript
// Pre-ES6 clumsy concatenation:
var html = '<div class="' + className + '">' +
           '  <h1>' + title + '</h1>' +
           '</div>';
```
Developers had to manually escape quotes, append newline escapes (`\n`), and concatenate strings with the `+` operator.

**Template Literals** (delimited by backticks `` ` ``) revolutionized string manipulation by introducing native **multiline strings**, **embedded expressions** (`${expr}`), and **Tagged Templates**. Tagged templates allow developers to intercept the parsing of a template string, enabling Domain-Specific Languages (DSLs) such as styled-components in React, internationalization formatters, and parameter-sanitizing SQL query builders.

---

## 2. Core Concepts: Interpolation & Tagged Templates

```
                              Template Literals
                                      │
         ┌────────────────────────────┴────────────────────────────┐
         ▼                                                         ▼
   Standard Interpolation                                   Tagged Templates
   `Hello, ${name}!`                                        sql`SELECT * FROM users WHERE id = ${id}`
   • Evaluates expressions to strings                       • Function tag precedes template string
   • Concatenates static and dynamic parts                  • Receives static string chunks & dynamic values
   • Preserves whitespace and newlines                      • Full control over rendering and escaping
```

### The Structure of a Tagged Template
When a tag function is applied to a template literal:
```javascript
tag`Hello ${firstName} ${lastName}!`;
```
The JavaScript engine translates this call into:
```javascript
tag(stringsArray, firstName, lastName);
```
Where `stringsArray` is a frozen array of static string pieces: `["Hello ", " ", "!"]`.

---

## 3. How It Works: Tagged Template Execution Model

```
Tagged Template Anatomy: sql`SELECT * FROM users WHERE id = ${userId} AND active = ${true}`
┌────────────────────────────────────────────────────────────────────────────────────────┐
│ Parameter 1: strings Array (Frozen, Cached by Engine)                                  │
│   strings[0] = "SELECT * FROM users WHERE id = "                                       │
│   strings[1] = " AND active = "                                                        │
│   strings[2] = ""                                                                      │
│   strings.raw = [ "SELECT *...", " AND...", "" ] (Contains raw unescaped escapes)      │
├────────────────────────────────────────────────────────────────────────────────────────┤
│ Parameter 2: value 1 = userId (e.g. 101)                                               │
│ Parameter 3: value 2 = true                                                            │
└────────────────────────────────────────────────────────────────────────────────────────┘
```

The count of static string segments is **always exactly equal to the count of dynamic values plus one**:
$$\text{strings.length} = \text{values.length} + 1$$

---

## 4. Syntax & API Reference

### 1. Basic Literals
- `` `Hello ${name}` `` : Basic interpolation. Expressions can be any valid JavaScript expression (variables, function calls, arithmetic).
- `` `Line 1\nLine 2` `` : Preserves literal newlines without explicit `\n`.

### 2. Built-in `String.raw`
- `String.raw` is a native tag function that returns strings with escape sequences (like `\n`, `\t`, `\u0020`) preserved as raw text:
  ```javascript
  console.log(String.raw`C:\Development\new_project`);
  // Output: "C:\Development\new_project" (does not convert \n to newline)
  ```

### 3. Tag Function Signature
```javascript
function tag(strings, ...values) {
  // strings is an Array of static segments with a .raw property
  // values is an Array of evaluated dynamic interpolation expressions
}
```

---

## 5. Practical Examples

### Example 1: Basic Multiline Strings & Expressions
```javascript
const user = { name: "Ada", role: "Admin", loginCount: 42 };

// Multiline formatting with embedded ternary logic
const userSummary = `
User Profile:
- Name: ${user.name}
- Role: ${user.role}
- Status: ${user.loginCount > 0 ? "Active" : "New Account"}
- Next Level In: ${100 - user.loginCount} logins
`.trim();

console.log(userSummary);
```

### Example 2: Real-World HTML Escaping Tagged Template (XSS Defense)
```javascript
function html(strings, ...values) {
  const sanitize = str =>
    String(str)
      .replace(/&/g, "&amp;")
      .replace(/</g, "&lt;")
      .replace(/>/g, "&gt;")
      .replace(/"/g, "&quot;")
      .replace(/'/g, "&#39;");

  return strings.reduce((acc, str, i) => {
    const val = i < values.length ? sanitize(values[i]) : "";
    return acc + str + val;
  }, "");
}

const userInput = "<script>alert('Malicious XSS!')</script>";
const renderedHtml = html`<div>Welcome, ${userInput}!</div>`;

console.log(renderedHtml);
// Output: <div>Welcome, &lt;script&gt;alert(&#39;Malicious XSS!&#39;)&lt;/script&gt;!</div>
```

### Example 3: Edge Case with Raw Escapes via `String.raw`
```javascript
// Useful for Regular Expressions and Windows File Paths:
const regexPattern = String.raw`\d+\.\d+\.\d+`;
const semverRegex = new RegExp(regexPattern);

console.log(regexPattern); // "\d+\.\d+\.\d+" (backslashes preserved!)
console.log(semverRegex.test("1.24.5")); // true
```

### Example 4: Failure Mode with Untrusted String Concatenation in SQL
```javascript
// DANGER: Naive interpolation creates SQL Injection vulnerabilities!
function insecureQuery(userId) {
  // An attacker passing "1 OR 1=1" bypasses authentication!
  return `SELECT * FROM users WHERE id = ${userId}`;
}

console.log(insecureQuery("1 OR 1=1"));
// "SELECT * FROM users WHERE id = 1 OR 1=1" (Vulnerable!)
```

### Example 5: Idiomatic Styled Component Tag Simulation
```javascript
function css(strings, ...values) {
  return strings.reduce((acc, str, i) => {
    return acc + str + (values[i] !== undefined ? values[i] : "");
  }, "").trim();
}

const primaryColor = "#0070f3";
const padding = 16;

const buttonStyles = css`
  background-color: ${primaryColor};
  color: #ffffff;
  padding: ${padding}px;
  border-radius: 4px;
`;

console.log("Generated CSS:\n" + buttonStyles);
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **Expressions Coerce to String via `ToString()`**: Objects placed in `${obj}` are converted via `String(obj)`. An object without a custom `toString()` or `Symbol.toPrimitive` produces `"[object Object]"`.
2. **ES2018 Revision for Illegal Escape Sequences**: Prior to ES2018, writing non-standard escapes like `\unicode` or `\xg` inside a template literal threw an early `SyntaxError`. In ES2018, this restriction was lifted for tagged templates: the invalid escape is set to `undefined` in the cooked array, but preserved verbatim in the `strings.raw` array.
3. **Leading / Trailing Whitespace**: Multiline template literals include all indentation whitespace literally. Use `.trim()` or a dedicated dedent tag to strip common leading indentation.
4. **`strings` Object is Frozen and Cached**: The first argument passed to a tag function (`strings`) is `Object.freeze()`-d by the engine. You cannot mutate `strings[0] = "new"`.

---

## 7. Common Mistakes & Anti-Patterns

| Mistake | Bad Code | Good Code | Why It Matters |
| :--- | :--- | :--- | :--- |
| Object stringification trap | `` `User: ${user}` `` | `` `User: ${user.name}` `` | Objects render as `"[object Object]"` by default. |
| SQL injection via raw template | `` db.query(`WHERE id=${id}`) `` | Use tagged parameterized template | Untrusted strings bypass database escaping. |
| Indentation cluttering output | Multiline with literal indentation | Use `.trim()` or dedent tag | Extra tabs/spaces leak into generated output. |
| Over-using template tags | Tagging simple string operations | Standard template literal | Tagged templates incur function call overhead. |
| Mutating `strings` array | `strings.push(...)` in tag | Create a new output string | The `strings` array is deeply frozen by the engine. |
| Forgetting `String.raw` for paths | `` `C:\new\test` `` | `` String.raw`C:\new\test` `` | `\n` is interpreted as newline, `\t` as tab. |

---

## 8. Performance Characteristics

- **Standard Template Literals**: Fast $O(n)$ string concatenation. Modern V8 optimizes simple template literals (`Hello ${name}`) directly to bytecode instructions identical to `+` concatenation.
- **Tagged Templates**: Function call overhead. However, V8 **caches the `strings` array** across identical call-sites in memory. Calling `tag` 10,000 times reuses the exact same frozen `strings` object reference, avoiding repeated array allocations.

---

## 9. Engine Internals: V8 Template Object Caching

Inside Google V8:
1. When V8 parses a tagged template literal, it creates a `TemplateObject` on the heap containing the parsed string segments and the `.raw` property.
2. V8 caches this `TemplateObject` in the active function's **Feedback Vector**.
3. On subsequent executions of that exact code line, V8 reuses the cached `TemplateObject` pointer directly, passing it as the first argument without allocating a new array.

---

## 10. Best Practices & Design Guidelines

1. **Use Tagged Templates for Security Boundaries**: Always use tagged template builders for SQL queries, HTML rendering, and shell commands to enforce automatic parameter escaping.
2. **Use `String.raw` for Regular Expressions & File Paths**: Avoid double-escaping backslashes (`\\\\`) by authoring regex strings with `String.raw`.
3. **Prefer Template Literals Over `+` Concatenation**: Template literals are more readable, less error-prone, and provide equal or superior performance in modern V8.

---

## 11. Real-World Case Study: SQL Injection Defense via Tagged Query Builder

An enterprise database query builder that transparently separates SQL syntax from user-supplied values, producing parameterized queries compatible with PostgreSQL/MySQL drivers.

```javascript
class SafeDatabaseQuery {
  // Tagged template function separating SQL text from dynamic parameters
  static sql(strings, ...values) {
    const text = strings.reduce((query, str, index) => {
      // In PostgreSQL syntax, parameters use $1, $2, ...
      const placeholder = index < values.length ? `$${index + 1}` : "";
      return query + str + placeholder;
    }, "").trim();

    return Object.freeze({
      text,
      values, // Parameter values passed safely out-of-band!
      toQueryObject() {
        return { text: this.text, values: this.values };
      }
    });
  }
}

// Verification
const requestedUserId = "usr_42";
const userStatus = "active";
const attackerPayload = "1; DROP TABLE users; --";

// 1. Safe query construction
const safeQuery = SafeDatabaseQuery.sql`
  SELECT id, username, email 
  FROM users 
  WHERE id = ${requestedUserId} AND status = ${userStatus}
`;

console.log("SQL Text:", safeQuery.text);
// "SELECT id, username, email \n  FROM users \n  WHERE id = $1 AND status = $2"
console.log("Parameters:", safeQuery.values);
// ['usr_42', 'active']

// 2. Attack attempt neutralized:
const attackedQuery = SafeDatabaseQuery.sql`
  SELECT * FROM accounts WHERE id = ${attackerPayload}
`;
console.log("Attacked Query Text:", attackedQuery.text);
// "SELECT * FROM accounts WHERE id = $1"
console.log("Attacked Query Params:", attackedQuery.values);
// ['1; DROP TABLE users; --'] (Safely isolated from SQL grammar!)
```

---

## 12. Interview Questions & Deep Dives

<details>
<summary><strong>1. What is the fundamental difference between a standard template literal and a tagged template?</strong></summary>

A standard template literal evaluates its embedded expressions, coerces them to strings, concatenates them with the static string chunks, and returns a single primitive string. A tagged template prefixes the literal with a tag function (`tag`...``), allowing the function to receive the static string segments as a frozen array and the evaluated expressions as discrete arguments, enabling custom compilation, DSL construction, or escaping.
</details>

<details>
<summary><strong>2. What is the relationship between the number of string segments and dynamic values in a tagged template?</strong></summary>

In any tagged template, the number of static string segments is always exactly one greater than the number of interpolated values:
$$\text{strings.length} === \text{values.length} + 1$$
Even if the template starts or ends with an expression (e.g. `${a}${b}`), the engine inserts empty strings `""` at the boundaries to preserve this invariant.
</details>

<details>
<summary><strong>3. What does <code>String.raw</code> do, and what is the <code>strings.raw</code> property?</strong></summary>

`strings.raw` is a property attached to the first argument of a tagged template function containing the exact string segments as written in source code, with escape sequences uninterpreted (e.g. `\n` remains two characters `\` and `n`). `String.raw` is a built-in tag function that uses this property to assemble raw strings without processing escape sequences.
</details>

<details>
<summary><strong>4. How does V8 optimize tagged template string array allocations?</strong></summary>

V8 caches the `strings` array in the call-site's Feedback Vector. Because the static string portions of a template literal are immutable at compile time, the engine reuses the exact same frozen array instance on every invocation of the tagged template, reducing garbage collection overhead.
</details>

<details>
<summary><strong>5. What was the ES2018 Template Literal Revision?</strong></summary>

Prior to ES2018, template literals adhered strictly to ECMAScript escape sequence rules; writing invalid escapes (like `\unicode` or `\0123`) threw a compile-time `SyntaxError`. In ES2018, this syntactic restriction was removed for tagged templates to support DSLs (such as LaTeX or custom domain parsers): illegal escape sequences are populated as `undefined` in the cooked array, but preserved verbatim in `strings.raw`.
</details>

<details>
<summary><strong>6. Can a tagged template return a value other than a string?</strong></summary>

Yes. A tagged template is simply a function call. It can return any arbitrary JavaScript value, including a DOM node, an AST object, an array, a Number, or an executive Promise.
</details>

<details>
<summary><strong>7. How do nested expressions evaluate inside template literals?</strong></summary>

Expressions inside `${...}` are standard JavaScript expressions evaluated in the current lexical scope. Template literals can be nested arbitrarily inside expressions:
```javascript
`Outer ${`Inner ${val}`}`
```
</details>

<details>
<summary><strong>8. Is the <code>strings</code> array passed to a tag function frozen?</strong></summary>

Yes. Under the ECMAScript specification, the `strings` array (and its `.raw` property) are deeply frozen via `Object.freeze()`. Any attempt to add, delete, or modify elements on `strings` throws a `TypeError` in strict mode.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Basic Tagged Template Logger (Easy)
Write a tag function `logTag` that logs the static strings and values separately and returns the concatenated string.

<details>
<summary>View Solution</summary>

```javascript
function logTag(strings, ...values) {
  console.log("Static strings:", strings);
  console.log("Dynamic values:", values);
  return strings.reduce((acc, str, i) => acc + str + (values[i] ?? ""), "");
}

const res = logTag`User: ${"Grace"} Age: ${42}`;
console.log("Result:", res);
```
</details>

### Exercise 2: Raw Path Formatter (Easy)
Use `String.raw` to construct a Windows file path without escaped backslashes.

<details>
<summary>View Solution</summary>

```javascript
const filename = "app.log";
const fullPath = String.raw`C:\Users\Admin\AppData\Local\Temp\${filename}`;
console.log(fullPath);
// Output: "C:\Users\Admin\AppData\Local\Temp\${filename}"
```
</details>

### Exercise 3: Multiline Indentation Stripper (Dedent) (Medium)
Implement a tagged template function `dedent` that strips common leading indentation from multiline template strings.

<details>
<summary>View Solution</summary>

```javascript
function dedent(strings, ...values) {
  const raw = strings.reduce((acc, str, i) => acc + str + (values[i] ?? ""), "");
  const lines = raw.split("\n");

  // Filter out leading/trailing empty lines
  while (lines.length > 0 && lines[0].trim() === "") lines.shift();
  while (lines.length > 0 && lines[lines.length - 1].trim() === "") lines.pop();

  // Find minimum leading whitespace
  const minIndent = lines.reduce((min, line) => {
    if (line.trim().length === 0) return min;
    const match = line.match(/^(\s+)/);
    const indent = match ? match[1].length : 0;
    return Math.min(min, indent);
  }, Infinity);

  return lines.map(line => line.slice(minIndent === Infinity ? 0 : minIndent)).join("\n");
}

const code = dedent`
    function test() {
      const a = 1;
      return a;
    }
`;

console.log(code);
/*
function test() {
  const a = 1;
  return a;
}
*/
```
</details>

### Exercise 4: Currency Formatter Tag (Medium)
Create a tagged template `money` that automatically formats numbers to currency strings (e.g. `$25.00`).

<details>
<summary>View Solution</summary>

```javascript
function money(strings, ...values) {
  const format = val => (typeof val === "number" ? `$${val.toFixed(2)}` : val);
  return strings.reduce((acc, str, i) => acc + str + (i < values.length ? format(values[i]) : ""), "");
}

const item = "Espresso";
const price = 4.5;
console.log(money`The ${item} costs ${price} with tax.`);
// Output: "The Espresso costs $4.50 with tax."
```
</details>

### Exercise 5: URL Parameter Encoding Tag (Hard)
Write a tagged template `url` that safely encodes all interpolated values using `encodeURIComponent` while leaving static path segments intact.

<details>
<summary>View Solution</summary>

```javascript
function url(strings, ...values) {
  return strings.reduce((acc, str, i) => {
    const val = i < values.length ? encodeURIComponent(String(values[i])) : "";
    return acc + str + val;
  }, "");
}

const search = "Node & Express";
const page = 1;
const endpoint = url`https://api.domain.com/search?q=${search}&page=${page}`;

console.log(endpoint);
// Output: "https://api.domain.com/search?q=Node%20%26%20Express&page=1"
```
</details>

### Exercise 6: Shell Command Quoting Tag (Hard)
Build a tagged template `sh` that validates and quotes arguments to defend against shell injection when constructing CLI commands.

<details>
<summary>View Solution</summary>

```javascript
function sh(strings, ...values) {
  const escapeShellArg = arg => {
    // Wrap in single quotes and escape existing single quotes
    return `'${String(arg).replace(/'/g, "'\\''")}'`;
  };

  return strings.reduce((acc, str, i) => {
    const val = i < values.length ? escapeShellArg(values[i]) : "";
    return acc + str + val;
  }, "").trim();
}

const userFilename = "my file; rm -rf /";
const command = sh`cat ${userFilename} | grep 'error'`;

console.log("Safe Shell Command:", command);
// Output: cat 'my file; rm -rf /' | grep 'error'
```
</details>
