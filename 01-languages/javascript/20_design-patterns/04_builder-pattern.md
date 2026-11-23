# The Builder Pattern in JavaScript

> Step-by-step object construction, fluent method chaining, query builders, and immutable state assembly.

| Metadata | Details |
| :--- | :--- |
| **Level** | Intermediate to Advanced |
| **Prerequisites** | [Objects](file:///e:/Learning/engineering-notes/01-languages/javascript/03_objects-and-arrays/01_object-fundamentals.md), [Classes](file:///e:/Learning/engineering-notes/01-languages/javascript/05_this-and-oop/05_classes.md) |
| **Time to read** | 22 minutes |
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

When constructing complex objects with numerous optional parameters, configuration flags, or dependencies, constructors quickly degenerate into the **Telescoping Constructor Anti-Pattern**: constructors accepting 10 or more positional parameters (e.g., `new Request(url, method, headers, body, timeout, retries, auth, agent, cache, credentials)`). Callers are forced to pass confusing sequences of `null` and `undefined` to reach desired options.

The **Builder Pattern** separates the step-by-step construction of a complex object from its actual representation. By exposing a **Fluent Interface** (method chaining returning `this` or a new builder state), callers can configure only the necessary parameters in any order, culminating in a final `.build()` method that validates structural invariants before producing the finalized object.

---

## 2. Core Concepts

### Fluent Method Chaining
The defining characteristic of the Builder pattern in JavaScript is fluent chaining: each setter method mutates internal configuration (or returns an immutable clone) and returns the builder instance (`return this`). This produces readable, domain-specific declarative code:
```javascript
const query = new QueryBuilder()
  .select('id', 'name')
  .from('users')
  .where('active', true)
  .limit(10)
  .build();
```

### Mutable vs Immutable Builders
- **Mutable Builder**: Each chaining method modifies the builder's own internal fields and returns `this`. High performance and low allocation overhead, but cannot be safely reused across branches.
- **Immutable Builder**: Each chaining method returns a **new instance** of the builder containing the updated state. Allows branching configurations (e.g., creating a base API request builder and branching it into separate GET and POST builders) without cross-contamination.

### Validation at Build Time
Instead of checking invariants across dozens of constructor parameters, validation logic is centralized inside `.build()`. If mandatory combinations are missing or values are mutually exclusive, `.build()` throws descriptive validation errors before any invalid object is returned to the system.

---

## 3. How It Works Internally

```
+--------------------------------------------------------------------------+
| BUILDER LIFECYCLE                                                        |
+--------------------------------------------------------------------------+
                                    |
  1. new RequestBuilder('https://api.example.com')
     -> Allocates builder state: { url, headers: {}, method: 'GET' }
                                    |
  2. .setMethod('POST')             | Mutates builder state
  3. .addHeader('Auth', 'Bearer X') |
  4. .setBody({ id: 101 })          |
                                    |
  5. .build()                       | Validation & Assembly
                                    v
+--------------------------------------------------------------------------+
| VALIDATION & FREEZING STEP                                               |
| - Verify: If method === 'GET', body MUST be null                         |
| - Verify: URL protocol must be HTTPS                                     |
| - Assemble: Construct Request instance & Object.freeze()                 |
+--------------------------------------------------------------------------+
                                    |
                                    v
+--------------------------------------------------------------------------+
| FINAL IMMUTABLE PRODUCT OBJECT                                           |
| { url, method: 'POST', headers: {...}, body: '{"id":101}' }              |
+--------------------------------------------------------------------------+
```

---

## 4. Syntax & API Reference

### Classic Class-Based Builder
```javascript
export class UserProfileBuilder {
  #username;
  #email;
  #bio = '';
  #roles = ['user'];

  constructor(username, email) {
    if (!username || !email) throw new Error('Username and email are required');
    this.#username = username;
    this.#email = email;
  }

  setBio(bio) {
    this.#bio = bio;
    return this; // Enable chaining
  }

  addRole(role) {
    this.#roles.push(role);
    return this; // Enable chaining
  }

  build() {
    return Object.freeze({
      username: this.#username,
      email: this.#email,
      bio: this.#bio,
      roles: Object.freeze([...this.#roles]),
    });
  }
}
```

---

## 5. Practical Examples

### Example 1: Robust HTTP Request Builder
```javascript
export class HttpRequestBuilder {
  constructor(baseUrl) {
    this.baseUrl = baseUrl;
    this.path = '';
    this.method = 'GET';
    this.headers = new Map();
    this.queryParams = new URLSearchParams();
    this.body = null;
    this.timeoutMs = 5000;
  }

  setPath(path) {
    this.path = path.startsWith('/') ? path : `/${path}`;
    return this;
  }

  setMethod(method) {
    this.method = method.toUpperCase();
    return this;
  }

  setHeader(key, value) {
    this.headers.set(key, value);
    return this;
  }

  addParam(key, value) {
    this.queryParams.append(key, value);
    return this;
  }

  setJsonBody(data) {
    this.body = JSON.stringify(data);
    this.headers.set('Content-Type', 'application/json');
    return this;
  }

  setTimeout(ms) {
    this.timeoutMs = ms;
    return this;
  }

  build() {
    const queryString = this.queryParams.toString();
    const fullUrl = `${this.baseUrl}${this.path}${queryString ? `?${queryString}` : ''}`;

    if (this.method === 'GET' && this.body) {
      throw new Error('GET requests cannot contain a request body');
    }

    return Object.freeze({
      url: fullUrl,
      method: this.method,
      headers: Object.fromEntries(this.headers),
      body: this.body,
      timeoutMs: this.timeoutMs,
    });
  }
}
```

### Example 2: Dynamic SQL Query Builder
```javascript
export class SqlQueryBuilder {
  #table = '';
  #fields = ['*'];
  #conditions = [];
  #limit = null;
  #offset = null;

  from(table) {
    this.#table = table;
    return this;
  }

  select(...fields) {
    if (fields.length > 0) this.#fields = fields;
    return this;
  }

  where(field, operator, value) {
    this.#conditions.push({ field, operator, value });
    return this;
  }

  paginate(page = 1, pageSize = 20) {
    this.#limit = pageSize;
    this.#offset = (page - 1) * pageSize;
    return this;
  }

  build() {
    if (!this.#table) throw new Error('Table name must be specified via from()');

    let sql = `SELECT ${this.#fields.join(', ')} FROM ${this.#table}`;
    const params = [];

    if (this.#conditions.length > 0) {
      const whereClauses = this.#conditions.map((cond, idx) => {
        params.push(cond.value);
        return `${cond.field} ${cond.operator} $${idx + 1}`;
      });
      sql += ` WHERE ${whereClauses.join(' AND ')}`;
    }

    if (this.#limit !== null) sql += ` LIMIT ${this.#limit}`;
    if (this.#offset !== null) sql += ` OFFSET ${this.#offset}`;

    return { sql, params };
  }
}
```

### Example 3: Immutable Functional Builder (Branchable)
```javascript
// Each step returns a fresh builder, enabling branching pipelines
export function createEmailBuilder(state = {}) {
  const currentState = {
    to: [],
    cc: [],
    subject: '',
    html: '',
    ...state,
  };

  return {
    addTo: (email) => createEmailBuilder({ ...currentState, to: [...currentState.to, email] }),
    addCc: (email) => createEmailBuilder({ ...currentState, cc: [...currentState.cc, email] }),
    setSubject: (subject) => createEmailBuilder({ ...currentState, subject }),
    setHtml: (html) => createEmailBuilder({ ...currentState, html }),
    build: () => {
      if (currentState.to.length === 0) throw new Error('At least one recipient required');
      if (!currentState.subject) throw new Error('Subject is required');
      return Object.freeze({ ...currentState });
    },
  };
}

// Branching usage:
const baseNotification = createEmailBuilder()
  .setSubject('System Update')
  .setHtml('<p>Maintenance tonight</p>');

const emailUser1 = baseNotification.addTo('alice@example.com').build();
const emailUser2 = baseNotification.addTo('bob@example.com').build();
```

### Example 4: Complex UI Modal Dialog Builder
```javascript
export class ModalBuilder {
  constructor(title) {
    this.title = title;
    this.buttons = [];
    this.isDismissible = true;
    this.contentHtml = '';
  }

  setContent(html) {
    this.contentHtml = html;
    return this;
  }

  disableDismiss() {
    this.isDismissible = false;
    return this;
  }

  addButton(label, onClick, variant = 'primary') {
    this.buttons.push({ label, onClick, variant });
    return this;
  }

  build() {
    const modalElement = document.createElement('div');
    modalElement.className = 'modal-container';
    modalElement.innerHTML = `
      <div class="modal-header"><h3>${this.title}</h3></div>
      <div class="modal-body">${this.contentHtml}</div>
      <div class="modal-footer"></div>
    `;

    const footer = modalElement.querySelector('.modal-footer');
    this.buttons.forEach((btn) => {
      const buttonEl = document.createElement('button');
      buttonEl.textContent = btn.label;
      buttonEl.className = `btn btn-${btn.variant}`;
      buttonEl.addEventListener('click', btn.onClick);
      footer.appendChild(buttonEl);
    });

    return modalElement;
  }
}
```

### Example 5: Building Domain-Specific Options with Defaults
```javascript
export class ServerConfigBuilder {
  #port = 3000;
  #host = '0.0.0.0';
  #corsOrigins = ['*'];
  #maxConnections = 1000;

  setPort(port) {
    if (port < 1 || port > 65535) throw new RangeError('Port must be 1-65535');
    this.#port = port;
    return this;
  }

  setHost(host) {
    this.#host = host;
    return this;
  }

  setAllowedOrigins(...origins) {
    this.#corsOrigins = origins;
    return this;
  }

  build() {
    return Object.freeze({
      port: this.#port,
      host: this.#host,
      corsOrigins: Object.freeze([...this.#corsOrigins]),
      maxConnections: this.#maxConnections,
    });
  }
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **Forgetting `.build()`**: A common bug with builder APIs is configuring the builder chain but forgetting to invoke the terminal `.build()` method. The calling code ends up with the `Builder` instance instead of the constructed product object.
2. **Reusing Mutable Builders**: In a mutable builder, calling `.build()` does not reset the builder state. If the builder is reused without clearing fields, state from the first object leaks into the second object.
3. **Over-using Builders for Simple Objects**: If an object only has 2 or 3 properties, introducing a Builder adds boilerplate without adding value. Standard ES6 object literals with destructuring (`{ name, age = 18 } = {}`) are far more idiomatic for simple configurations.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Why It Breaks | Correct Pattern |
| :--- | :--- | :--- |
| Forgetting `return this;` in setters | Breaks method chaining with `TypeError: cannot read property of undefined` | Always return `this` (or fresh instance) |
| Omitting `.build()` validation | Allows constructing invalid or broken product objects | Centralize validation and invariant checks in `.build()` |
| Reusing mutable builders for multiple objects | Causes state contamination between objects | Use immutable builders or create fresh builder instances |
| Using Builder when a simple object literal suffices | Unnecessary abstraction and class boilerplate | Use simple object options with defaults for small configs |
| Returning mutable inner arrays in product | External callers can mutate the constructed object | Freeze or copy inner arrays in `.build()` |

---

## 8. Performance Characteristics

- **Mutable Builders**: Extremely fast. Property assignments update internal slots directly without allocating intermediate objects.
- **Immutable Builders**: Allocates a shallow clone at every method call in the chain. In high-frequency hot paths (e.g. rendering 10,000 entities per second), prefer mutable builders.

---

## 9. Engine Internals

1. **Inline Cache Stability in Method Chaining**:
   - Because `return this` always returns the identical builder instance reference, V8 compiles method chaining sequences into monomorphic inline caches with direct branch jumps.
2. **Hidden Class Transitions in Builders**:
   - Initialize all builder fields upfront in the builder constructor (even if `null`) so the builder object itself maintains a stable Hidden Class throughout the configuration chain.

---

## 10. Best Practices & Design Guidelines

- **Centralize Structural Invariants in `.build()`**: Only perform comprehensive cross-field validation inside `.build()` to prevent throwing errors before the caller has finished chaining.
- **Freeze the Built Product**: Always wrap the constructed product object in `Object.freeze()` to guarantee immutability after construction.
- **Provide Static `create()` Shorthands**: Add `static create(...args)` to simplify instantiation without typing `new` (e.g., `RequestBuilder.create('http...')`).

---

## 11. Real-World Case Study

### Production JWT Token Generator Builder
```javascript
import crypto from 'node:crypto';

export class JwtBuilder {
  #header = { alg: 'HS256', typ: 'JWT' };
  #payload = {};
  #secret = null;

  setSubject(userId) {
    this.#payload.sub = userId;
    return this;
  }

  setIssuer(issuer) {
    this.#payload.iss = issuer;
    return this;
  }

  setExpiresIn(seconds) {
    this.#payload.exp = Math.floor(Date.now() / 1000) + seconds;
    return this;
  }

  setClaim(key, value) {
    this.#payload[key] = value;
    return this;
  }

  signWith(secret) {
    this.#secret = secret;
    return this;
  }

  build() {
    if (!this.#secret) throw new Error('JWT cannot be built without a signature secret');
    if (!this.#payload.sub) throw new Error('JWT subject (sub) is required');

    const encode = (obj) => Buffer.from(JSON.stringify(obj)).toString('base64url');
    const headerEncoded = encode(this.#header);
    const payloadEncoded = encode(this.#payload);
    const message = `${headerEncoded}.${payloadEncoded}`;

    const signature = crypto
      .createHmac('sha256', this.#secret)
      .update(message)
      .digest('base64url');

    return `${message}.${signature}`;
  }
}
```

---

## 12. Interview Questions

<details>
<summary>1. What problem does the Builder pattern solve?</summary>
It solves the Telescoping Constructor anti-pattern, where constructors accept long lists of positional parameters with many optional fields. The Builder pattern separates object construction from representation, allowing complex objects to be assembled step-by-step through a readable fluent interface.
</details>

<details>
<summary>2. How do you implement method chaining in a JavaScript Builder?</summary>
By ensuring that every setter method on the builder returns the builder instance itself (<code>return this;</code>) in a mutable builder, or returns a newly cloned builder instance in an immutable builder.
</details>

<details>
<summary>3. What is the difference between a mutable and an immutable Builder?</summary>
A mutable builder modifies its own internal state on every method call and returns <code>this</code> (zero intermediate allocations). An immutable builder returns a new builder instance on every method call, allowing configuration branching without mutating earlier builder states.
</details>

<details>
<summary>4. Why should cross-property validation occur in .build() rather than individual setters?</summary>
Individual setters only see one property at a time and cannot know what other setters will be invoked later in the chain. Placing validation in <code>.build()</code> ensures that all properties have been configured before verifying required combinations and mutually exclusive constraints.
</details>

<details>
<summary>5. How does the Builder pattern compare to passing an options object to a constructor?</summary>
An options object (<code>new Service({ optA, optB })</code>) is effective for simple configurations. However, the Builder pattern provides stronger encapsulation, allows step-by-step conditional configuration, enables method-based documentation and auto-complete, and supports complex multi-step assembly (like SQL or query builders).
</details>

<details>
<summary>6. How do you guarantee the immutability of the object returned by .build()?</summary>
By calling <code>Object.freeze()</code> on the constructed object and making defensive copies (or freezing) any internal arrays or nested objects to prevent external callers from mutating state after construction.
</details>

<details>
<summary>7. What is the risk of reusing a mutable builder instance?</summary>
If a mutable builder is not reset between calls, properties configured for the first object will unintentionally persist and contaminate subsequent objects constructed with that same builder.
</details>

<details>
<summary>8. How can a Builder facilitate branching configurations?</summary>
By implementing an immutable builder where each setter returns a new builder instance. Callers can define a baseline configuration and branch it into different variations without modifying the shared base builder.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Build a Markdown Document Builder
Implement a `MarkdownBuilder` class providing `addHeading(text, level)`, `addParagraph(text)`, `addBulletList(items)`, and `addCodeBlock(code, lang)` methods, returning the finalized markdown string upon calling `.build()`.

<details>
<summary>View Solution</summary>

```javascript
export class MarkdownBuilder {
  constructor() {
    this.sections = [];
  }

  addHeading(text, level = 1) {
    const prefix = '#'.repeat(Math.min(Math.max(level, 1), 6));
    this.sections.push(`${prefix} ${text}\n`);
    return this;
  }

  addParagraph(text) {
    this.sections.push(`${text}\n`);
    return this;
  }

  addBulletList(items) {
    const list = items.map(item => `- ${item}`).join('\n');
    this.sections.push(`${list}\n`);
    return this;
  }

  addCodeBlock(code, lang = '') {
    this.sections.push(`\`\`\`${lang}\n${code}\n\`\`\`\n`);
    return this;
  }

  build() {
    return this.sections.join('\n').trim();
  }
}
```
</details>

### Exercise 2: Strict HTTP Header Builder
Implement a builder `HeaderBuilder` that validates header keys (must not contain spaces or control characters) and accumulates multiple values for identical headers (e.g. `Set-Cookie` or `Accept`), returning a clean headers object on `.build()`.

<details>
<summary>View Solution</summary>

```javascript
export class HeaderBuilder {
  #headers = new Map();

  setHeader(name, value) {
    if (!name || /\s/.test(name)) {
      throw new Error(`Invalid header name: "${name}"`);
    }
    const normalizedKey = name.toLowerCase();

    if (!this.#headers.has(normalizedKey)) {
      this.#headers.set(normalizedKey, []);
    }
    this.#headers.get(normalizedKey).push(String(value));
    return this;
  }

  setBearerToken(token) {
    return this.setHeader('Authorization', `Bearer ${token}`);
  }

  setContentType(mimeType) {
    return this.setHeader('Content-Type', mimeType);
  }

  build() {
    const result = {};
    for (const [key, values] of this.#headers) {
      // If single value, store as string; if multiple, join with comma (or keep array for set-cookie)
      result[key] = key === 'set-cookie' ? values : values.join(', ');
    }
    return Object.freeze(result);
  }
}
```
</details>
