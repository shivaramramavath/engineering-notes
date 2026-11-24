# Concept Note Template — Node.js Backend Curriculum

> **Source of Truth** for all concept notes in `03-backend/nodejs/`. Every concept note MUST contain all 13 sections in this exact order with required subsections, tables, code blocks, and details tags.

---

```markdown
# [Concept Title]

> [1-2 sentence executive summary of the concept, what problem it solves, and its role in modern Node.js server-side architecture.]

| Metadata | Details |
| :--- | :--- |
| **Node.js Baseline** | Node.js 22 LTS / 24+ |
| **Module System** | ESM (`"type": "module"`) / CJS interop |
| **Runtime Target** | V8 + libuv event loop |
| **Framework / Driver** | Node native / Express 5 / Fastify 5 / Hono |
| **Tested With** | Node.js 22.x / 24.x, node:test, Vitest, TypeScript 5.7+ |

---

## 1. Concept Overview & Mental Model

### 1.1 Why This Exists & Architectural Role
[Explain the core problem, why older paradigms (e.g. synchronous blocking calls, callback hell, unhandled promise rejections, naive buffering) fell short, and how this modern Node.js API works.]

### 1.2 Data Flow & System Diagram
```
[ Incoming Network Request / Event ] ──> [ libuv Event Demultiplexer / Reactor ]
                                                      │
                                                      ▼
[ V8 Call Stack / Worker Thread ] <──> [ Node.js Runtime Kernel / Threadpool ]
                                                      │
                                                      ▼
[ Non-blocking I/O Stream / DB Socket ] ──> [ Outgoing Stream / Response ]
```

---

## 2. Naive vs. Idiomatic Comparison

### Naive Implementation (Anti-Pattern / Legacy)
```javascript
// Anti-pattern: Blocking synchronous call, missing error propagation, or unhandled rejection
```
*Why this fails*: [Explain event loop starvation, memory leaks, unhandled exceptions, or security vulnerabilities.]

### Idiomatic Implementation (Modern Node.js)
```javascript
// Modern idiom: Non-blocking async/await, proper stream pipelines, structured error handling
```
*Why this succeeds*: [Explain event loop efficiency, backpressure safety, resource cleanup, and type correctness.]

---

## 3. Five Progressive Examples

### Example 1: Basic / Foundational Usage
- **Goal**: [Core objective]
- **Code**:
```javascript
// Code snippet
```
- **Execution & Output**:
```text
[ Real server output / console log / HTTP response ]
```
- **Explanation**: [How it works]

### Example 2: Real-World Production Scenario
- **Goal**: [Production backend feature]
- **Code**:
```javascript
// Code snippet
```
- **Execution & Output**:
```text
[ Real output ]
```
- **Explanation**: [Explanation]

### Example 3: Edge Case Handling
- **Goal**: [Handling backpressure, socket hangup, timeout, or stream truncation]
- **Code**:
```javascript
// Code snippet
```
- **Execution & Output**:
```text
[ Real output ]
```
- **Explanation**: [Explanation]

### Example 4: Failure Mode & Recovery
- **Goal**: [Handling database failure, unhandled rejection, or network timeout]
- **Code**:
```javascript
// Code snippet
```
- **Execution & Output**:
```text
[ Real output / Error stack ]
```
- **Explanation**: [Explanation]

### Example 5: Idiomatic Composition & Advanced Pattern
- **Goal**: [Complete pipeline composition with AsyncLocalStorage, streams, or worker threads]
- **Code**:
```javascript
// Code snippet
```
- **Execution & Output**:
```text
[ Real output ]
```
- **Explanation**: [Explanation]

---

## 4. Under the Hood & Internals
[Low-level architecture: libuv event loop phase (Timers, Pending I/O, Poll, Check, Close), threadpool allocation, V8 heap allocation, memory buffers, or OS syscalls.]

---

## 5. Security & Defence-in-Depth
- **Attack Vector**: [e.g. ReDoS, Prototype Pollution, SQL/NoSQL Injection, Path Traversal, Event Loop Starvation]
- **Vulnerability Explanation**: [Why naive code is vulnerable]
- **Hardened Implementation**:
```javascript
// Hardened code with Zod validation, parameterization, and security guards
```

---

## 6. Common Mistakes & Anti-Patterns (Table)

| Anti-Pattern | Root Cause | Observable Symptom | Correct Architecture |
| :--- | :--- | :--- | :--- |
| `fs.readFileSync` in server request | Synchronous blocking I/O | Event loop stalls, TTFB degrades under load | Use `fs.promises` or streams |
| Raw `.pipe()` without error handling | Missing error propagation | Memory leak, open socket descriptor | Use `stream.pipeline()` |
| String concatenation in SQL | Missing parameterization | SQL injection vulnerability | Parameterized queries (`$1`, `?`) |
| Uncaught promise rejection | Missing catch in promise chain | Process crash or unhandledRejection event | Global handler + async/await try/catch |
| Mutating `req.query` directly | Express 5 read-only getter | TypeError thrown at runtime | Treat `req.query` as immutable read-only |
| Insecure Buffer constructor | Deprecated `new Buffer()` | Information disclosure / uninitialized memory | `Buffer.alloc()` or `Buffer.from()` |

---

## 7. Performance & Optimization Guide
- **Throughput & Event Loop Health**: [Techniques to keep event loop lag < 10ms]
- **Memory Allocation & Garbage Collection**: [Buffer allocation vs heap allocations]
- **Streaming & Backpressure**: [Preventing memory spikes when transferring large payloads]

---

## 8. TypeScript & Strict Typing Recipes
```typescript
// Fully typed interfaces, strict generic signatures, and Zod schema validation
```

---

## 9. Testing & Quality Verification
- **Unit & Integration Test (`node:test` or Vitest)**:
```javascript
// Test suite
```
- **API Endpoint Test (Supertest)**:
```javascript
// Supertest request spec
```

---

## 10. When NOT to Use This Pattern
- [Trade-off 1: When CPU-bound tasks require worker threads or a separate service in Go/Rust]
- [Trade-off 2: When simpler synchronous CLI scripting does not require complex async pipelines]
- [Trade-off 3: When in-memory caching exceeds process heap limits and requires Redis]

---

## 11. Frequently Asked Interview Questions

<details>
<summary>1. [Staff-Level Question 1]</summary>
[Deep architectural answer with code reference]
</details>

<details>
<summary>2. [Question 2]</summary>
[Answer]
</details>

<details>
<summary>3. [Question 3]</summary>
[Answer]
</details>

<details>
<summary>4. [Question 4]</summary>
[Answer]
</details>

<details>
<summary>5. [Question 5]</summary>
[Answer]
</details>

<details>
<summary>6. [Question 6]</summary>
[Answer]
</details>

<details>
<summary>7. [Question 7]</summary>
[Answer]
</details>

<details>
<summary>8. [Question 8]</summary>
[Answer]
</details>

---

## 12. Hands-on Practice Exercises

<details>
<summary>Exercise 1 (Easy): [Task Title]</summary>
- **Objective**: [Goal]
- **Solution**:
```javascript
// Solution
```
</details>

<details>
<summary>Exercise 2 (Easy): [Task Title]</summary>
- **Objective**: [Goal]
- **Solution**:
```javascript
// Solution
```
</details>

<details>
<summary>Exercise 3 (Medium): [Task Title]</summary>
- **Objective**: [Goal]
- **Solution**:
```javascript
// Solution
```
</details>

<details>
<summary>Exercise 4 (Medium): [Task Title]</summary>
- **Objective**: [Goal]
- **Solution**:
```javascript
// Solution
```
</details>

<details>
<summary>Exercise 5 (Hard): [Task Title]</summary>
- **Objective**: [Goal]
- **Solution**:
```javascript
// Solution
```
</details>

<details>
<summary>Exercise 6 (Hard): [Task Title]</summary>
- **Objective**: [Goal]
- **Solution**:
```javascript
// Solution
```
</details>

---

## 13. Cross-References & Further Reading
- [Related Track Note: Streams Architecture](../02-core-modules/README.md)
- [Official Node.js Documentation](https://nodejs.org/docs/latest/api/)
```
