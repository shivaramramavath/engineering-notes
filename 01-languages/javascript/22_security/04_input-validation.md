# Input Validation and Injection Defense in JavaScript

> Schema validation (Zod, Valibot), Parse Don't Validate principle, SQL/NoSQL injection defense, command injection, and input sanitization.

| Metadata | Details |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | [Error Handling](file:///e:/Learning/engineering-notes/01-languages/javascript/10_error-handling/01_errors.md), [Child Processes](file:///e:/Learning/engineering-notes/01-languages/javascript/16_nodejs/09_child-process-and-cluster.md) |
| **Time to read** | 25 minutes |
| **Status** | Complete |
| **Standard / Runtime** | ECMAScript 2015+, Node.js & Evergreen Browsers |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & API Reference](#4-syntax--api-reference)
5. [Practical Examples](#5-practical-examples)
6. [Edge Cases, Pitfalls & Gotchas](#6-edge-cases-pitfalls--gotchas)
7. [Common Mistakes & Anti-Patterns](#7-common-mistakes--anti-patterns)
8. [Performance Characteristics](#8-performance-characteristics)
9. [Engine Internals](#9-engine-internals)
10. [Best Practices & Design Guidelines](#10-best-practices--design-guidelines)
11. [Real-World Case Study](#11-real-world-case-study)
12. [Interview Questions](#12-interview-questions)
13. [Exercises & Challenges](#13-exercises--challenges)

---

## 1. What & Why

Virtually all application-level security vulnerabilities—SQL Injection, NoSQL Injection, Command Injection, Prototype Pollution, Path Traversal, and Cross-Site Scripting—share a single root cause: **untrusted input was accepted without rigorous validation and passed directly into an interpreter or execution engine**.

In dynamically typed JavaScript, trusting incoming data shapes (`req.body`, `req.query`, or WebSocket packets) is catastrophic. Attackers can supply unexpected types (submitting objects where strings are expected) to bypass authentication, or interpolate special syntax characters (`'`, `;`, `$gt`) to alter query logic.

Input validation is the first line of perimeter defense. Embracing the **Parse, Don't Validate** philosophy using modern schema validation libraries (like **Zod**, **Valibot**, and **Ajv**) ensures that untrusted data is strictly parsed into validated domain models before business logic executes.

---

## 2. Core Concepts

### Validation vs Sanitization
- **Validation**: Enforcing that input conforms strictly to expected structural rules (types, lengths, ranges, regex patterns, allowed enum values). If input fails, it is **rejected immediately with an error**.
- **Sanitization**: Modifying input to strip or transform potentially dangerous characters (e.g. trimming whitespace, lowercasing emails, or escaping HTML entities). Sanitization should always be paired with validation.

### "Parse, Don't Validate"
Traditional validation runs ad-hoc `if` checks across raw data, leaving the data typed as an ambiguous generic structure. The **Parse, Don't Validate** principle (from functional type systems) transforms raw untrusted inputs into strongly typed domain objects at the system boundary. Once parsed, downstream business logic can rely on guaranteed invariants without redundant defensive checks.

### Classic Injection Attacks in JavaScript
1. **SQL Injection (SQLi)**: Interpolating strings into SQL queries (`SELECT * FROM users WHERE id = '${id}'`). Solved via **Parameterized Prepared Statements**.
2. **NoSQL Injection**: Supplying MongoDB query operator objects (`{"password": {"$gt": ""}}`) instead of strings. Solved via schema type validation and disabling operator casting.
3. **Command Injection**: Interpolating user input into shell execution (`child_process.exec("ping " + ip)`). Solved via `child_process.execFile` with argument arrays.
4. **Path Traversal**: Supplying `../../etc/passwd` to file system operations. Solved via path normalization and directory jailing.

---

## 3. How It Works Internally

```
PERIMETER BOUNDARY SCHEMA VALIDATION (Parse, Don't Validate):

Incoming HTTP Request Body (Untrusted JSON):
{
  "email": "ADA@EXAMPLE.COM  ",
  "age": "32",
  "role": "admin",             <--- Attack: Unauthorized field
  "password": { "$gt": "" }    <--- Attack: NoSQL Injection object
}
                                |
                                v
+--------------------------------------------------------------------------+
| ZOD / SCHEMA VALIDATOR PIPELINE                                          |
| 1. Checks types: 'password' MUST be string -> REJECTED!                  |
| 2. Rejects unexpected properties: 'role' stripped (.strict())            |
| 3. Normalizes: email trimmed and lowercased                              |
| 4. Coerces: 'age' converted to integer and checked (18-120)              |
+--------------------------------------------------------------------------+
                                |
    +---------------------------+---------------------------+
    | Validation Fails                                      | Validation Succeeds
    v                                                       v
HTTP 400 Bad Request                        Domain Validated User Model:
{ "errors": ["Invalid password"] }          { email: "ada@example.com", age: 32 }
                                            (100% Safe for Business Logic)
```

---

## 4. Syntax & API Reference

### Schema Validation with Zod
```javascript
import { z } from 'zod';

// Define strict domain schema
export const UserRegistrationSchema = z.object({
  username: z.string().min(3).max(30).regex(/^[a-zA-Z0-9_]+$/),
  email: z.string().email().toLowerCase().trim(),
  age: z.coerce.number().int().min(18).max(120),
  newsletter: z.boolean().default(false),
}).strict(); // Disallow unknown/injected properties!

// Parsing function:
export function parseRegistration(rawInput) {
  // .parse() throws a ZodError if invalid
  // .safeParse() returns { success, data, error }
  const result = UserRegistrationSchema.safeParse(rawInput);
  if (!result.success) {
    throw new Error(result.error.issues.map(i => i.message).join(', '));
  }
  return result.data; // Fully typed, normalized, and validated!
}
```

---

## 5. Practical Examples

### Example 1: Preventing SQL Injection via Parameterized Queries
```javascript
// DANGEROUS: String interpolation creates direct SQL Injection vulnerability!
async function badFindUser(db, userInput) {
  // If userInput is: " ' OR '1'='1 " -> Dumps entire user table!
  const query = `SELECT * FROM users WHERE username = '${userInput}'`;
  return db.query(query);
}

// SECURE: Parameterized Prepared Statements
async function safeFindUser(db, userInput) {
  // Database driver sends query text and parameters in separate protocol packets.
  // The database engine compiles query structure BEFORE binding variables!
  const query = 'SELECT id, username, email FROM users WHERE username = $1';
  const result = await db.query(query, [userInput]);
  return result.rows[0] || null;
}
```

### Example 2: Preventing NoSQL Injection in MongoDB / Mongoose
```javascript
// VULNERABLE CODE:
// If attacker sends JSON: { "username": "admin", "password": { "$gt": "" } }
// MongoDB matches the first user whose password is greater than empty string!
async function badLogin(req, res) {
  const { username, password } = req.body;
  const user = await db.collection('users').findOne({ username, password });
  if (user) res.send('Logged in as Admin!');
}

// SECURE: Validate types explicitly before querying
import { z } from 'zod';

const LoginSchema = z.object({
  username: z.string().min(1),
  password: z.string().min(1), // Enforces string; rejects object operator attacks!
});

async function safeLogin(req, res) {
  const parsed = LoginSchema.safeParse(req.body);
  if (!parsed.success) {
    res.writeHead(400);
    return res.end('Invalid credentials format');
  }

  // Guaranteed that password is a primitive string, not a MongoDB operator object
  const user = await db.collection('users').findOne({
    username: parsed.data.username,
    password: parsed.data.password,
  });
}
```

### Example 3: Preventing Command Injection in Node.js
```javascript
import { exec, execFile } from 'node:child_process';

// DANGEROUS: exec() invokes a system shell (/bin/sh or cmd.exe)
function badPingHost(host) {
  // If host is: "8.8.8.8; rm -rf /" -> Shell executes both commands!
  exec(`ping -c 1 ${host}`, (err, stdout) => {
    console.log(stdout);
  });
}

// SECURE: execFile() invokes the binary directly without spawning a shell
function safePingHost(host) {
  // Step 1: Validate input against strict allowlist (valid IP or hostname)
  if (!/^[a-zA-Z0-9.-]+$/.test(host)) {
    throw new Error('Invalid host format');
  }

  // Step 2: Pass arguments as an array to prevent argument injection
  execFile('ping', ['-c', '1', host], (err, stdout) => {
    console.log(stdout);
  });
}
```

### Example 4: Preventing Path Traversal in File Serving
```javascript
import fs from 'node:fs/promises';
import path from 'node:path';

const PUBLIC_ROOT = path.resolve('./public');

export async function safeServeStaticFile(requestedPath) {
  // Step 1: Normalize and resolve full absolute path
  const safePath = path.resolve(PUBLIC_ROOT, `.${path.normalize(requestedPath)}`);

  // Step 2: Enforce Directory Jailing
  // Verify resolved path strictly starts with the authorized public directory
  if (!safePath.startsWith(PUBLIC_ROOT + path.sep) && safePath !== PUBLIC_ROOT) {
    throw new Error('Access Denied: Path Traversal Detected');
  }

  return fs.readFile(safePath);
}
```

### Example 5: High-Performance Schema Validation with Valibot
```javascript
// Valibot is an ultra-lightweight alternative to Zod with a modular, tree-shakeable API (< 1KB)
import * as v from 'valibot';

const OrderItemSchema = v.object({
  sku: v.pipe(v.string(), v.regex(/^[A-Z]{3}-\d{4}$/, 'SKU must match format ABC-1234')),
  quantity: v.pipe(v.number(), v.integer(), v.minValue(1), v.maxValue(100)),
  priceCents: v.pipe(v.number(), v.integer(), v.minValue(0)),
});

export function validateOrderItem(data) {
  return v.parse(OrderItemSchema, data);
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **Mass Assignment / Parameter Tampering**: Accepting raw request bodies into database updates (`User.update(req.body)`) allows users to inject fields like `isAdmin: true` or `balance: 100000`. Always use `.strict()` in Zod or explicitly destructure whitelisted fields.
2. **Regex Denial of Service (ReDoS)**: Complex regular expressions with nested quantifiers (e.g. `/(a+)+$/`) take exponential $O(2^N)$ time on malicious inputs, freezing the event loop. Always test regex with ReDoS linters or enforce input length limits before regex matching.
3. **Type Coercion Surprises in Zod**: Using `z.coerce.number()` will coerce `false` to `0` and `true` to `1`. If Boolean inputs should be rejected, use strict `z.number()`.
4. **Unicode Normalization Mismatches**: Two visually identical Unicode strings (e.g. `é` represented as a single code point `\u00E9` vs composed `e` + accent `\u0065\u0301`) can bypass string comparisons. Always normalize with `str.normalize('NFC')` before validation.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Why It Breaks | Correct Pattern |
| :--- | :--- | :--- |
| String concatenation in SQL queries | Allows complete SQL injection | Use parameterized prepared statements (`$1`, `?`) |
| Passing user strings directly to `child_process.exec()` | Allows command injection via `;`, `\|`, backticks | Use `child_process.execFile()` with argument arrays |
| Trusting `typeof` alone for input validation | `typeof null === 'object'`, objects bypass checks | Use schema validation libraries (Zod, Valibot) |
| Using `path.join(root, userPath)` without verification | Vulnerable to `../../etc/passwd` directory traversal | Verify with `path.resolve` and `startsWith(root)` |
| Allowing unrestricted object properties into DB updates | Mass assignment vulnerability allows privilege escalation | Enforce strict schema picking / stripping |
| Sanitizing input instead of validating | Sanitization can be bypassed with clever encoding | Validate against strict allowlists; reject invalid data |

---

## 8. Performance Characteristics

- **Schema Validation Throughput**:
  - **Zod**: Validates ~50,000 to 200,000 objects per second per core.
  - **Ajv (JSON Schema compiler)**: Generates JIT-compiled validation functions, validating up to 1,500,000 objects per second per core.
  - **Valibot**: Tree-shakeable, $< 1$KB bundle size for frontend clients.

---

## 9. Engine Internals

1. **Parameterized Query Wire Protocol**:
   - Database drivers (like `pg` for PostgreSQL) do not perform string interpolation in JavaScript.
   - The driver issues a `Parse` message with the query string and parameter data types, followed by a `Bind` message containing the raw binary parameter values. The SQL parser never interprets parameters as SQL keywords.

---

## 10. Best Practices & Design Guidelines

- **Validate at the Perimeter**: Validate all incoming requests (HTTP bodies, query params, headers, WebSockets) immediately at the controller layer before passing data to domain services.
- **Favor Allowlists Over Denylists**: Never try to block "bad" characters (denylist). Always define exactly what "good" characters and patterns are allowed (allowlist).
- **Enforce Length Limits**: Set explicit maximum lengths on all string fields to prevent memory exhaustion and ReDoS attacks.

---

## 11. Real-World Case Study

### Mitigating a Critical NoSQL Injection Authentication Bypass
```javascript
// THE VULNERABILITY:
// A user authentication endpoint parsed incoming JSON using Express and queried MongoDB directly:
// const { email, password } = req.body;
// const user = await db.collection('users').findOne({ email, password });

// THE ATTACK:
// An attacker posted:
// { "email": "admin@company.com", "password": { "$ne": null } }
// MongoDB evaluated the query as: "Find user where email is admin AND password is NOT null".
// The database returned the admin user record, generating an admin JWT token!

// THE REMEDY: Strict Schema Enforcement
import { z } from 'zod';

const AuthCredentialsSchema = z.object({
  email: z.string().email().toLowerCase(),
  password: z.string().min(8).max(128), // Enforces string; rejects object operators!
}).strict();

export async function loginHandler(req, res) {
  const result = AuthCredentialsSchema.safeParse(req.body);
  if (!result.success) {
    res.writeHead(400, { 'Content-Type': 'application/json' });
    return res.end(JSON.stringify({ error: 'Malformed request credentials' }));
  }

  const { email, password } = result.data;
  // Guaranteed string types: NoSQL operator injection is physically impossible!
  const user = await db.collection('users').findOne({ email, password: hashPassword(password) });
  // Handle login...
}
```

---

## 12. Interview Questions

<details>
<summary>1. What is the "Parse, Don't Validate" principle?</summary>
Instead of running ad-hoc boolean validation checks on untrusted data and keeping it in a loosely typed format, "Parse, Don't Validate" parses untrusted input into a strongly typed, validated domain structure at the system boundary. Once successfully parsed, downstream code relies on guaranteed invariants without needing defensive checks.
</details>

<details>
<summary>2. How does a Parameterized Query prevent SQL Injection?</summary>
In a parameterized query, the SQL statement and the user-supplied parameters are transmitted to the database engine in separate protocol packets. The database compiles the SQL query structure and execution plan before the parameters are bound. Because the parameters are treated strictly as literal data values, they can never alter the syntactic structure of the query.
</details>

<details>
<summary>3. What is NoSQL Injection and how does it occur in Node.js MongoDB applications?</summary>
NoSQL injection occurs when an endpoint accepts arbitrary JSON input into database query selectors without type checking. If an attacker submits a MongoDB operator object (such as <code>{"password": {"$ne": ""}}</code>) instead of a plain string, MongoDB evaluates the operator, allowing attackers to bypass authentication or extract unauthorized records.
</details>

<details>
<summary>4. Why is child_process.execFile() safer than child_process.exec()?</summary>
<code>child_process.exec()</code> spawns an operating system shell (such as <code>/bin/sh</code> or <code>cmd.exe</code>) to execute the command string, allowing attackers to chain commands using shell metacharacters (<code>;</code>, <code>|</code>, <code>&&</code>). <code>child_process.execFile()</code> executes the binary directly without spawning a shell, passing arguments as a clean array where characters like <code>;</code> are treated strictly as string parameters.
</details>

<details>
<summary>5. What is a Path Traversal attack and how do you prevent it?</summary>
A Path Traversal attack occurs when an attacker uses dot-dot-slash sequences (<code>../../</code>) in a filename parameter to access files outside the intended directory (e.g. <code>../../etc/passwd</code>). It is prevented by resolving the absolute path using <code>path.resolve()</code> and verifying that the resulting path strictly begins with the authorized base directory prefix.
</details>

<details>
<summary>6. What is a Mass Assignment vulnerability and how do schema validators prevent it?</summary>
Mass assignment occurs when client input is passed directly into database creation or update methods without filtering. Attackers can include unauthorized fields (like <code>role: 'admin'</code> or <code>isVerified: true</code>). Schema validators prevent this using <code>.strict()</code> or explicit property picking, discarding any un-whitelisted fields.
</details>

<details>
<summary>7. What is ReDoS (Regular Expression Denial of Service)?</summary>
ReDoS occurs when a regular expression with nested or overlapping quantifiers (e.g. <code>(a+)+$</code>) experiences catastrophic backtracking when matching a non-matching string. The CPU time grows exponentially with input length, freezing the single-threaded Node.js event loop at 100% CPU.
</details>

<details>
<summary>8. Why is input allowlisting superior to denylisting?</summary>
Denylisting attempts to enumerate and block all known dangerous characters or patterns. Because attack payloads can be disguised using alternate encodings, Unicode variations, or obscure syntax quirks, denylists are almost always bypassed. Allowlists define the exact set of acceptable formats and reject everything else by default.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Build a Strict Pagination Query Validator with Zod
Implement a validator using Zod that validates query parameters `{ page, limit, sort, order }`, ensuring `page` is an integer $\ge 1$ (default 1), `limit` is an integer between 1 and 100 (default 20), `sort` is a string allowlist (`'name'`, `'date'`, `'price'`), and `order` is strictly `'asc'` or `'desc'` (default `'asc'`).

<details>
<summary>View Solution</summary>

```javascript
import { z } from 'zod';

export const PaginationQuerySchema = z.object({
  page: z.coerce.number().int().min(1).default(1),
  limit: z.coerce.number().int().min(1).max(100).default(20),
  sort: z.enum(['name', 'date', 'price']).default('date'),
  order: z.enum(['asc', 'desc']).default('asc'),
});

export function parsePagination(query) {
  return PaginationQuerySchema.parse(query);
}

// Test:
const parsed = parsePagination({ page: '2', limit: '50', sort: 'price' });
console.log(parsed); // { page: 2, limit: 50, sort: 'price', order: 'asc' }
```
</details>

### Exercise 2: Safe In-Jail File Path Resolver
Write a function `resolveSafePath(baseDirectory, userInputPath)` that returns the resolved absolute path if it resides within `baseDirectory`, or throws a `SecurityError` if the path attempts to escape via directory traversal.

<details>
<summary>View Solution</summary>

```javascript
import path from 'node:path';

export function resolveSafePath(baseDir, userInput) {
  const root = path.resolve(baseDir);
  // Normalize and resolve path relative to root
  const target = path.resolve(root, `.${path.normalize(userInput)}`);

  // Verify that target starts with root + separator
  if (!target.startsWith(root + path.sep) && target !== root) {
    throw new Error(`Path traversal attempt blocked: "${userInput}"`);
  }

  return target;
}
```
</details>
