# JavaScript Destructuring: Arrays, Objects & Pattern Matching

> Deep dive into ECMAScript destructuring assignment syntax, covering nested extraction patterns, default value evaluation semantics, computed keys, and engine-level performance.

| Metadata | Details |
| :--- | :--- |
| **Level** | Core / Intermediate |
| **Prerequisites** | [01_fundamentals/01_variables.md](../01_fundamentals/01_variables.md), [03_objects-and-arrays/01_objects.md](./01_objects.md) |
| **Time to Read** | 21 minutes |
| **Status** | Complete |
| **ECMAScript Baseline** | ES2015 (ES6) / ES2018 (Object Rest/Spread) |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works: Iterator Protocol & Object Lookup](#3-how-it-works-iterator-protocol--object-lookup)
4. [Syntax & API Reference](#4-syntax--api-reference)
5. [Practical Examples](#5-practical-examples)
6. [Edge Cases, Pitfalls & Gotchas](#6-edge-cases-pitfalls--gotchas)
7. [Common Mistakes & Anti-Patterns](#7-common-mistakes--anti-patterns)
8. [Performance Characteristics](#8-performance-characteristics)
9. [Engine Internals: V8 Bytecode Generation](#9-engine-internals-v8-bytecode-generation)
10. [Best Practices & Design Guidelines](#10-best-practices--design-guidelines)
11. [Real-World Case Study: HTTP Request Header & Query Parser](#11-real-world-case-study-http-request-header--query-parser)
12. [Interview Questions & Deep Dives](#12-interview-questions--deep-dives)
13. [Exercises & Challenges](#13-exercises--challenges)

---

## 1. What & Why

Before ECMAScript 2015 (ES6), unpacking values from data structures required verbose, repetitive assignments (`var first = arr[0]; var second = arr[1];`). **Destructuring assignment** allows you to bind variables directly from arrays, objects, maps, or any iterable using pattern-matching syntax.

Beyond syntactic convenience, destructuring is fundamental to modern JavaScript idioms: unpacking React component props, extracting JSON payloads in APIs, swapping variables without temporary storage, and defining functions with optional configuration objects.

---

## 2. Core Concepts

### Object vs. Array Destructuring Mechanisms
- **Array Destructuring**: Relies on the **iterable protocol** (`[Symbol.iterator]`). Elements are unpacked in sequential position.
- **Object Destructuring**: Relies on **named property lookup**. Elements are unpacked by matching property key names, independent of property order.

```javascript
// Array destructuring is positional:
const [first, , third] = ["A", "B", "C"];
console.log(first, third); // "A", "C"

// Object destructuring is nominal:
const { role, id } = { id: 101, role: "admin" };
console.log(id, role); // 101, "admin"
```

### Default Value Evaluation Semantics
Default values trigger **only and strictly when the extracted value is `undefined`**. If a property exists with value `null`, `false`, `0`, or `""`, the default value is **not** used.

```javascript
const { timeout = 5000, retry = 3 } = { timeout: null, retry: undefined };
console.log(timeout); // null (NOT 5000!)
console.log(retry);   // 3 (undefined triggered the default)
```

---

## 3. How It Works: Iterator Protocol & Object Lookup

```
Array Destructuring Mechanism:
const [x, y] = iterableSource;
       │
       ▼
1. const iterator = iterableSource[Symbol.iterator]()
2. const step1 = iterator.next() -> x = step1.value
3. const step2 = iterator.next() -> y = step2.value
4. iterator.return() called if iteration closes early

Object Destructuring Mechanism:
const { a, b: alias } = objectSource;
       │
       ▼
1. Check objectSource != null (throws TypeError if null/undefined)
2. a = objectSource["a"] === undefined ? defaultVal : objectSource["a"]
3. alias = objectSource["b"] === undefined ? defaultVal : objectSource["b"]
```

---

## 4. Syntax & API Reference

### Array Destructuring Patterns
- `[a, b] = arr`: Basic assignment.
- `[a, , b] = arr`: Skip intermediate items with commas.
- `[a, ...rest] = arr`: Collect trailing elements into an array.
- `[a = defaultVal] = arr`: Positional default value.
- `[a, b] = [b, a]`: Variable swap idiom.

### Object Destructuring Patterns
- `{ prop } = obj`: Shorthand binding.
- `{ prop: alias } = obj`: Rename property to local identifier `alias`.
- `{ [dynamicKey]: val } = obj`: Computed property key destructuring.
- `{ prop = defaultValue } = obj`: Property default value.
- `{ prop: alias = defaultValue } = obj`: Aliasing with default fallback.
- `{ a, ...remaining } = obj`: Object rest pattern (ES2018).

---

## 5. Practical Examples

### Example 1: Basic Swapping & Skipping
```javascript
let a = 10;
let b = 20;

// In-place variable swap without temporary variable
[a, b] = [b, a];
console.log(a, b); // 20, 10

// Unpack first and fifth element of an array
const scores = [100, 95, 90, 85, 80];
const [gold, , , , bronze] = scores;
console.log(gold, bronze); // 100, 80
```

### Example 2: Real-World Nested API Payload Extraction
```javascript
const apiResponse = {
  status: 200,
  data: {
    user: {
      id: "usr_99",
      profile: {
        firstName: "Ada",
        lastName: "Lovelace",
        contact: { email: "ada@computing.org" }
      }
    }
  }
};

// Deep extraction with aliasing and default fallback
const {
  data: {
    user: {
      profile: {
        firstName,
        contact: { email: userEmail },
        phone = "N/A"
      }
    }
  }
} = apiResponse;

console.log(`${firstName}'s Email: ${userEmail}, Phone: ${phone}`);
// Output: Ada's Email: ada@computing.org, Phone: N/A
```

### Example 3: Edge Case with Regex Match Destructuring
```javascript
const isoDate = "2026-03-31";
const datePattern = /^(\d{4})-(\d{2})-(\d{2})$/;

// Regex matches return array-like structures where index 0 is full match
const [, year, month, day] = isoDate.match(datePattern) || [];
console.log(`Year: ${year}, Month: ${month}, Day: ${day}`);
// Output: Year: 2026, Month: 03, Day: 31
```

### Example 4: Failure Mode with Destructuring Null/Undefined
```javascript
// Destructuring null or undefined throws TypeError immediately
function parseConfig(config) {
  // Buggy code: throws if config is omitted
  // const { port, host } = config;

  // Defensive safe destructuring:
  const { port = 8080, host = "localhost" } = config || {};
  return { port, host };
}

console.log(parseConfig(null));      // { port: 8080, host: 'localhost' }
console.log(parseConfig(undefined)); // { port: 8080, host: 'localhost' }
```

### Example 5: Idiomatic Options Object with Nested Defaults
```javascript
function initializeServer({
  port = 3000,
  host = "127.0.0.1",
  ssl: { key, cert } = { key: null, cert: null },
  cors: { origins = ["*"], methods = ["GET", "POST"] } = {}
} = {}) {
  return { port, host, ssl: { key, cert }, cors: { origins, methods } };
}

console.log(initializeServer());
/*
{
  port: 3000,
  host: '127.0.0.1',
  ssl: { key: null, cert: null },
  cors: { origins: [ '*' ], methods: [ 'GET', 'POST' ] }
}
*/
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **Reassignment Requires Parentheses**: When destructuring into previously declared variables without `const`/`let`, wrapping parentheses are mandatory:
   ```javascript
   let x, y;
   // SyntaxError: Unexpected token '='
   // { x, y } = { x: 1, y: 2 };
   ({ x, y } = { x: 1, y: 2 }); // Valid!
   ```
2. **`null` Does Not Trigger Defaults**:
   ```javascript
   const { x = 10 } = { x: null };
   console.log(x); // null!
   ```
3. **Array Rest Must Be Last**: You cannot write `const [...first, last] = arr;` (SyntaxError: Rest element must be last element).
4. **Intermediate Undefined in Deep Nesting**: Unpacking `{ a: { b: { c } } }` when `a` is `{}` throws `TypeError: Cannot read properties of undefined (reading 'b')`.
5. **Memory Leak with Rest in Arrays**: `const [first, ...rest] = hugeArray;` creates a slice of the remainder of `hugeArray`, preventing it from being garbage collected if `rest` is retained.

---

## 7. Common Mistakes & Anti-Patterns

| Mistake | Bad Code | Good Code | Why It Matters |
| :--- | :--- | :--- | :--- |
| Omitting parameter fallback `= {}` | `function run({ timeout = 50 })` | `function run({ timeout = 50 } = {})` | Calling `run()` with no arguments throws TypeError. |
| Forgetting parentheses in assignment | `{ a, b } = source;` | `({ a, b } = source);` | JavaScript parses `{` at statement start as a block, not an object. |
| Assuming `null` triggers default | `const { port = 80 } = { port: null }` | `const port = obj.port ?? 80;` | Default destructuring only checks `=== undefined`. |
| Excessive nested destructuring | `const { a: { b: { c: { d } } } } = data;` | Break into smaller, validated steps | Deep destructuring crashes immediately if any ancestor is nullish. |
| Destructuring non-iterables as array | `const [a] = { 0: "hello", length: 1 };` | `const [a] = Array.from(obj);` | Plain objects lack `[Symbol.iterator]` and throw TypeError. |
| Computed key syntax confusion | `const { [key] } = obj;` | `const { [key]: value } = obj;` | Computed properties require an explicit binding identifier. |

---

## 8. Performance Characteristics

- **Shallow Object Destructuring (`const { a, b } = obj`)**: Zero overhead in V8. TurboFan compiles this to direct field offset reads identical to `obj.a` and `obj.b`.
- **Array Destructuring**: If applied to a true Array, V8 bypasses the iterator protocol and emits direct indexed loads (`arr[0]`, `arr[1]`). If applied to a custom iterable, it incurs generator/iterator invocation overhead.
- **Object Rest (`const { a, ...rest } = obj`)**: Has $O(n)$ overhead because the engine must iterate all own enumerable properties of `obj` and allocate a new object for `rest`.

---

## 9. Engine Internals: V8 Bytecode Generation

Consider the following function:
```javascript
function getCoords([x, y]) { return x + y; }
```
When compiled by V8 Ignition into bytecode:
1. `GetIterator`: Looks up `[Symbol.iterator]` on the argument.
2. `Star r1`: Stores iterator record.
3. `CallRuntime [kInlineIteratorNext]`: Advances iterator to step 1; stores value in accumulator.
4. `Star r2` (binds `x`).
5. `CallRuntime [kInlineIteratorNext]`: Advances iterator to step 2.
6. `Star r3` (binds `y`).
7. `IteratorClose`: Ensures `.return()` is called if the iterator is not yet done.

When TurboFan JIT optimizes the function and observes the argument is always a `PACKED_SMI_ELEMENTS` JSArray, it replaces the entire iterator machinery with two direct memory loads: `LoadField[offset_0]` and `LoadField[offset_1]`.

---

## 10. Best Practices & Design Guidelines

1. **Always Supply Default Object Parameter**: In function parameters, write `function f({ a, b } = {})` so callers can execute `f()` without arguments.
2. **Limit Nesting to 2 Levels**: If data is nested more than two levels deep, use optional chaining (`response?.data?.user?.profile`) before destructuring to avoid catastrophic runtime crashes.
3. **Use Destructuring for Named Function Arguments**: Avoid functions with 4+ positional arguments (`connect(host, port, ssl, timeout, retries)`). Use a destructured configuration object instead (`connect({ host, port, ssl, ... })`).

---

## 11. Real-World Case Study: HTTP Request Header & Query Parser

A Node.js microservice router extracting and validating authentication tokens, pagination query parameters, and custom tenant IDs from incoming request payloads.

```javascript
class RequestParser {
  static parseContext(req) {
    const {
      headers: {
        authorization = "",
        "x-tenant-id": tenantId = "default-tenant",
        "accept-encoding": acceptEncoding = "gzip"
      } = {},
      query: {
        page = "1",
        limit = "20",
        sortBy = "createdAt",
        order = "desc"
      } = {},
      params: { id: resourceId = null } = {}
    } = req || {};

    // Extract Bearer token via array destructuring
    const [authScheme, authToken] = authorization.split(" ");
    const token = authScheme?.toLowerCase() === "bearer" ? authToken : null;

    // Sanitize pagination
    const parsedPage = Math.max(1, parseInt(page, 10) || 1);
    const parsedLimit = Math.min(100, Math.max(1, parseInt(limit, 10) || 20));

    return {
      auth: { token, authenticated: Boolean(token) },
      tenantId,
      pagination: {
        page: parsedPage,
        limit: parsedLimit,
        offset: (parsedPage - 1) * parsedLimit,
        sortBy,
        order: order.toLowerCase() === "asc" ? "asc" : "desc"
      },
      resourceId
    };
  }
}

// Verification
const mockReq = {
  headers: {
    authorization: "Bearer eyJhbGciOi...",
    "x-tenant-id": "acme-corp"
  },
  query: { page: "3", limit: "50" },
  params: { id: "res_1234" }
};

const context = RequestParser.parseContext(mockReq);
console.log(context);
/*
{
  auth: { token: 'eyJhbGciOi...', authenticated: true },
  tenantId: 'acme-corp',
  pagination: { page: 3, limit: 50, offset: 100, sortBy: 'createdAt', order: 'desc' },
  resourceId: 'res_1234'
}
*/
```

---

## 12. Interview Questions & Deep Dives

<details>
<summary><strong>1. Why does <code>let { a, b } = undefined;</code> throw a TypeError?</strong></summary>

Under the ECMAScript specification, object destructuring performs `ToObject()` on the right-hand side. `ToObject(undefined)` and `ToObject(null)` throw a `TypeError`. Destructuring primitives like numbers or strings works because `ToObject(42)` boxes the number into a `Number` wrapper object.
</details>

<details>
<summary><strong>2. What is the syntactic reason for needing parentheses around <code>({ a, b } = obj);</code>?</strong></summary>

In JavaScript grammar, a statement starting with an open curly brace `{` is interpreted as a code block, not an object literal or destructuring pattern. Wrapping the statement in parentheses forces the parser to treat the content as an expression.
</details>

<details>
<summary><strong>3. How does array destructuring interact with generators or infinite streams?</strong></summary>

Array destructuring calls the iterator's `.next()` method only as many times as there are elements in the destructuring pattern. If you destructure `const [first, second] = infiniteFibonacci()`, only two values are computed, and the iterator's `.return()` method is automatically called to clean up resources.
</details>

<details>
<summary><strong>4. Under what exact conditions do default values evaluate in destructuring?</strong></summary>

A default value evaluates if and only if the resolved property value is strictly `=== undefined`. If the property is absent, or exists with value `undefined`, the default expression is evaluated. Any other value (`null`, `0`, `false`, `NaN`, `""`) suppresses the default value.
</details>

<details>
<summary><strong>5. Are default values in destructuring evaluated eagerly or lazily?</strong></summary>

They are evaluated **lazily**. The expression assigned as the default value is only computed at runtime if the target property is `undefined`. If the property has a defined value, the default expression is never evaluated.
</details>

<details>
<summary><strong>6. What is the difference between rest in array destructuring vs. rest in object destructuring?</strong></summary>

In array destructuring (`const [head, ...tail] = arr`), `tail` is an `Array` containing remaining items extracted via iteration. In object destructuring (`const { key, ...rest } = obj`), `rest` is a plain `Object` containing all own enumerable string and symbol properties that were not explicitly unpacked.
</details>

<details>
<summary><strong>7. How does computed property destructuring work?</strong></summary>

Computed property keys use square brackets on the left-hand side: `const { [key]: value } = obj;`. The expression inside `[...]` is evaluated at runtime to determine the property key name, and the result is bound to the identifier `value`.
</details>

<details>
<summary><strong>8. Can you destructure an object's prototype properties?</strong></summary>

Yes. Object destructuring performs standard `[[Get]]` property lookups on the target. If the property does not exist as an own property, the prototype chain is traversed. For instance, `const { toString } = 42;` successfully extracts `Number.prototype.toString`.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Safe Configuration Merger (Easy)
Write a function `mergeDefaults(options)` that extracts `host`, `port`, and `ssl` with sensible defaults using destructuring.

<details>
<summary>View Solution</summary>

```javascript
function mergeDefaults(options) {
  const {
    host = "localhost",
    port = 8080,
    ssl = false
  } = options || {};
  return { host, port, ssl };
}

console.log(mergeDefaults({ port: 3000 }));
// { host: 'localhost', port: 3000, ssl: false }
```
</details>

### Exercise 2: Destructuring Map Entries (Easy)
Iterate over a `Map` of currency exchange rates using `for...of` with destructuring to format output strings.

<details>
<summary>View Solution</summary>

```javascript
const rates = new Map([
  ["USD", 1.0],
  ["EUR", 0.92],
  ["GBP", 0.79]
]);

for (const [currency, rate] of rates) {
  console.log(`1 USD = ${rate} ${currency}`);
}
```
</details>

### Exercise 3: Dynamic Property Renaming (Medium)
Create a function `pickAndRename(obj, keyMap)` that extracts specific properties from an object and renames them based on `keyMap`.

<details>
<summary>View Solution</summary>

```javascript
function pickAndRename(obj, keyMap) {
  return Object.entries(keyMap).reduce((acc, [sourceKey, targetKey]) => {
    if (sourceKey in obj) {
      acc[targetKey] = obj[sourceKey];
    }
    return acc;
  }, {});
}

const user = { first_name: "Alan", last_name: "Turing", age: 41 };
console.log(pickAndRename(user, { first_name: "firstName", last_name: "lastName" }));
// { firstName: 'Alan', lastName: 'Turing' }
```
</details>

### Exercise 4: Infinite Sequence Head/Tail Unpacking (Medium)
Implement a generator function `naturals()` and demonstrate unpacking the first 3 elements using array destructuring without hanging an infinite loop.

<details>
<summary>View Solution</summary>

```javascript
function* naturals() {
  let n = 1;
  while (true) {
    yield n++;
  }
}

const [n1, n2, n3] = naturals();
console.log(n1, n2, n3); // 1, 2, 3
```
</details>

### Exercise 5: Deep Flatten Object Schema (Hard)
Write a recursive function `flattenSchema(obj, prefix = '')` that destructures nested objects into single-level dot-notation keys.

<details>
<summary>View Solution</summary>

```javascript
function flattenSchema(obj, prefix = "") {
  return Object.entries(obj).reduce((acc, [key, value]) => {
    const fullPath = prefix ? `${prefix}.${key}` : key;
    if (value && typeof value === "object" && !Array.isArray(value)) {
      Object.assign(acc, flattenSchema(value, fullPath));
    } else {
      acc[fullPath] = value;
    }
    return acc;
  }, {});
}

const config = {
  db: { host: "127.0.0.1", port: 5432, auth: { user: "postgres" } },
  app: { name: "service" }
};
console.log(flattenSchema(config));
/*
{
  'db.host': '127.0.0.1',
  'db.port': 5432,
  'db.auth.user': 'postgres',
  'app.name': 'service'
}
*/
```
</details>

### Exercise 6: Defensive Destructuring Decorator (Hard)
Implement a higher-order wrapper `withValidatedParams(schema, fn)` that validates incoming destructured properties against type predicates before invoking the underlying function.

<details>
<summary>View Solution</summary>

```javascript
function withValidatedParams(schema, fn) {
  return function (params = {}) {
    for (const [key, validator] of Object.entries(schema)) {
      if (!validator(params[key])) {
        throw new TypeError(`Validation failed for argument "${key}" with value: ${params[key]}`);
      }
    }
    return fn(params);
  };
}

const createUser = withValidatedParams(
  {
    username: val => typeof val === "string" && val.length >= 3,
    age: val => typeof val === "number" && val >= 18
  },
  ({ username, age }) => `Created user ${username} aged ${age}`
);

console.log(createUser({ username: "john_doe", age: 25 }));
// Throws TypeError:
// createUser({ username: "jo", age: 25 });
```
</details>
