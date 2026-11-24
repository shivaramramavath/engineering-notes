# Concept Note Template — Next.js Curriculum

> **Source of Truth** for all concept notes in `02-frontend/nextjs/`. Every concept note MUST contain all 13 sections in this exact order with required subsections, tables, code blocks, and details tags.

---

```markdown
# [Concept Title]

> [1-2 sentence executive summary of the concept, what problem it solves, and its role in modern Next.js architecture.]

| Metadata | Details |
| :--- | :--- |
| **Next.js Version** | 15.2+ / 16.0 |
| **React Version** | React 19.0.0 |
| **Node Version** | Node.js 20+ LTS (Minimum 18.18) |
| **Router** | App Router (`app/`) |
| **Runtime** | Node.js / Edge |
| **Tested With** | Next.js 15.2, React 19, Vitest 3.0, TypeScript 5.7 |

---

## 1. Concept Overview & Mental Model

### 1.1 Why This Exists & Architectural Role
[Explain the core problem, why older paradigms (e.g. Pages Router, client-side fetch, synchronous request access) fell short, and how this modern API works.]

### 1.2 Data Flow & System Diagram
```
[ Incoming Request / Event ] ──> [ Next.js Routing / Proxy Layer ]
                                                │
                                                ▼
[ React Server Components ] ──> [ Data Access Layer / Cache ] ──> [ Streamed HTML / RSC Payload ]
```

---

## 2. Naive vs. Idiomatic Comparison

### Naive Implementation (Anti-Pattern / Legacy)
```tsx
// Anti-pattern: Synchronous request access, missing boundaries, or fetch waterfalls
```
*Why this fails*: [Explain performance bottlenecks, security risks, or hydration errors.]

### Idiomatic Implementation (Modern Next.js)
```tsx
// Modern idiom: Async request handling, server-only boundaries, explicit caching
```
*Why this succeeds*: [Explain resilience, security boundaries, and streaming benefits.]

---

## 3. Five Progressive Examples

### Example 1: Basic / Foundational Usage
- **Goal**: [Core objective]
- **Code**:
```tsx
// Code snippet
```
- **Execution & Output**:
```text
[ Real server output / console log / HTTP response ]
```
- **Explanation**: [How it works]

### Example 2: Real-World Production Scenario
- **Goal**: [Production feature]
- **Code**:
```tsx
// Code snippet
```
- **Execution & Output**:
```text
[ Real output ]
```
- **Explanation**: [Explanation]

### Example 3: Edge Case Handling
- **Goal**: [Handling network partition, stale token, or dynamic param mismatch]
- **Code**:
```tsx
// Code snippet
```
- **Execution & Output**:
```text
[ Real output ]
```
- **Explanation**: [Explanation]

### Example 4: Failure Mode & Recovery
- **Goal**: [Handling server error, unauthorized access, or validation rejection]
- **Code**:
```tsx
// Code snippet
```
- **Execution & Output**:
```text
[ Real output / HTTP 401/403/500 ]
```
- **Explanation**: [Explanation]

### Example 5: Idiomatic Composition & Advanced Pattern
- **Goal**: [Full composition with Server Actions, Cache Components, or DAL]
- **Code**:
```tsx
// Code snippet
```
- **Execution & Output**:
```text
[ Real output ]
```
- **Explanation**: [Explanation]

---

## 4. Under the Hood & Internals
[Low-level architecture: React Flight protocol, AST transformation, HTTP response streaming, cache storage, or runtime process architecture.]

---

## 5. Security & Defence-in-Depth
- **Attack Vector**: [e.g. Server Action injection, open redirect, middleware bypass, CSRF]
- **Vulnerability Explanation**: [Why naive code is vulnerable]
- **Hardened Implementation**:
```tsx
// Hardened code with Zod validation, authz checks, and server-only markers
```

---

## 6. Common Mistakes & Anti-Patterns (Table)

| Anti-Pattern | Root Cause | Observable Symptom | Correct Architecture |
| :--- | :--- | :--- | :--- |
| Synchronous `cookies()` call | Missing `await` in Next.js 15+ | Runtime Promise rejection or type error | `const store = await cookies();` |
| Middleware-only authorization | CVE-2025-29927 header spoofing | Unauthorized data exfiltration | Authorize in DAL and Server Actions |
| Client bundle secret leakage | Missing `"server-only"` boundary | Secrets visible in browser DevTools bundle | Mark DAL with `import 'server-only'` |
| Accidental dynamic opt-out | Accessing headers in static route | Static prerender fails during `next build` | Move dynamic reads inside Suspense |
| Fetch waterfall in RSC | Sequential awaits without Promise.all | High TTFB, slow initial paint | Colocate fetches with `Promise.all` or parallel streams |
| Unbounded cache stampede | Missing tags or stale revalidation | Database overload during traffic spikes | Use `"use cache"` with `cacheLife` |

---

## 7. Performance & Optimization Guide
- **Core Web Vitals Impact**: [LCP, INP, CLS]
- **RSC Payload Size**: [How to minimize serialized JSON payload across the wire]
- **Streaming & Suspense**: [Unlocking instant TTFB with granular boundaries]

---

## 8. TypeScript & Strict Typing Recipes
```tsx
// Fully typed interfaces, Props with Promise params, Zod schema inference
```

---

## 9. Testing & Quality Verification
- **Unit & Integration Test (Vitest / RTL)**:
```tsx
// Vitest test block
```
- **E2E Browser Test (Playwright)**:
```ts
// Playwright spec
```

---

## 10. When NOT to Use This Pattern
- [Trade-off 1: When client-side state is strictly required]
- [Trade-off 2: When edge runtime constraints disallow Node-specific packages]
- [Trade-off 3: When simple static export (SPA) is sufficient]

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
```tsx
// Solution
```
</details>

<details>
<summary>Exercise 2 (Easy): [Task Title]</summary>
- **Objective**: [Goal]
- **Solution**:
```tsx
// Solution
```
</details>

<details>
<summary>Exercise 3 (Medium): [Task Title]</summary>
- **Objective**: [Goal]
- **Solution**:
```tsx
// Solution
```
</details>

<details>
<summary>Exercise 4 (Medium): [Task Title]</summary>
- **Objective**: [Goal]
- **Solution**:
```tsx
// Solution
```
</details>

<details>
<summary>Exercise 5 (Hard): [Task Title]</summary>
- **Objective**: [Goal]
- **Solution**:
```tsx
// Solution
```
</details>

<details>
<summary>Exercise 6 (Hard): [Task Title]</summary>
- **Objective**: [Goal]
- **Solution**:
```tsx
// Solution
```
</details>

---

## 13. Cross-References & Further Reading
- [Related Track Note: Data Access Layer](file:///E:/Learning/engineering-notes/02-frontend/nextjs/05-data-fetching/README.md)
- [Official Next.js Documentation](https://nextjs.org/docs)
```
