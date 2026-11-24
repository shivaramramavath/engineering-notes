# Concept Note Template — ReactJS Curriculum

> **Source of Truth** for all concept notes in `02-frontend/reactjs/`. Every concept note MUST adhere strictly to these 13 numbered sections.

```markdown
# [Concept Name]

- **Topic**: `[module-folder]`
- **React version**: React 19+ (or version when introduced, e.g., React 18+, React 16.8+)
- **Tested with**: React 19.0.0, Vite 6.0, TypeScript 5.7, Vitest 3.0
- **Bundle / compiler notes**: [React Compiler auto-memoization behavior / Vite tree-shaking notes]

---

## 1. Concept Overview

[Explain WHAT the concept is and WHY it exists in React. Explain the mental model, problem statement, when to use, and when NOT to use. Contrast naive approaches with idiomatic React patterns.]

```
[ ASCII Diagram or Flowchart illustrating component lifecycle, render phase vs commit phase, fiber trees, or data flow ]
```

---

## 2. Key Mechanics & Architecture

[Deep dive into how React handles this concept internally. Fiber reconciliation, Hook dispatcher state, scheduler priorities, batching, event delegation, or DOM synchronization.]

### Technical Characteristics Table
| Property / Feature | Behavior / Specification |
| :--- | :--- |
| **Execution Phase** | [Render / Commit / Layout / Passive Effect] |
| **Batching Semantics** | [Automatic batching via Transitions / Microtask] |
| **StrictMode Behavior** | [Double invoked in dev / idempotency requirement] |
| **React Compiler Impact** | [Auto-memoized / requires manual memoization if compiler disabled] |

---

## 3. Real-World Code Examples

### Example 1: Basic / Foundational Pattern
```tsx
// Goal: Basic usage demonstrating fundamental syntax
```
Output:
```
// Exact real output from console or rendered markup
```

### Example 2: Production / Real-World Integration
```tsx
// Goal: Integrated real-world scenario (e.g. data fetching, form handling, compound UI)
```
Output:
```
// Exact real output
```

### Example 3: Edge Case / Complex Scenario
```tsx
// Goal: Handling race conditions, unmount cleanups, rapid state transitions
```
Output:
```
// Exact real output
```

### Example 4: Failure Mode / Pitfall & Remediation
```tsx
// Goal: Reproduce a common bug (stale closure, infinite loop, batching trap) and fix it
```
Output:
```
// Exact real error/warning output and the working solution
```

### Example 5: Idiomatic Modern React 19 Pattern
```tsx
// Goal: Modern React 19 pattern (e.g., using Actions, use(), ref prop, compiler idioms)
```
Output:
```
// Exact real output
```

---

## 4. Common Anti-Patterns & Gotchas

| Bad Practice | Root Cause | Clean Solution |
| :--- | :--- | :--- |
| [Anti-pattern 1] | [Why it fails in React] | [Idiomatic solution] |
| [Anti-pattern 2] | [Why it fails in React] | [Idiomatic solution] |
| [Anti-pattern 3] | [Why it fails in React] | [Idiomatic solution] |
| [Anti-pattern 4] | [Why it fails in React] | [Idiomatic solution] |
| [Anti-pattern 5] | [Why it fails in React] | [Idiomatic solution] |
| [Anti-pattern 6] | [Why it fails in React] | [Idiomatic solution] |

---

## 5. React Internals: Beneath the Abstraction

[Deep technical breakdown of Fiber nodes, work-in-progress trees, lane-based priority scheduling, effect lists, or synthetic event delegation.]

---

## 6. Performance, Bundling & React Compiler

[How this feature affects re-renders, bundle size, code-splitting, tree-shaking, and how the React Compiler transforms it at build time.]

---

## 7. Accessibility (a11y) & Usability

[Accessibility imperatives: keyboard navigation (Enter/Space/Tab/Arrows), focus management, ARIA roles/attributes (`aria-expanded`, `aria-live`, `aria-describedby`), and WCAG 2.2 guidelines.]

---

## 8. Testing & Verification Patterns

[Vitest + `@testing-library/react` unit tests, `userEvent` interactions, and Playwright verification patterns.]

```tsx
import { render, screen } from '@testing-library/react';
import userEvent from '@testing-library/user-event';
// Complete test suite
```

---

## 9. Version History & Migration (React 17 -> 18 -> 19)

| React Version | Behavior / API Status | Migration Notes |
| :--- | :--- | :--- |
| **React 17** | [Legacy behavior] | [Migration step] |
| **React 18** | [Concurrent features, automatic batching] | [Migration step] |
| **React 19** | [Current standard: Actions, use(), ref prop] | [Recommended modern practice] |

---

## 10. Security & Hardening

[XSS prevention, sanitization, open redirects, safe prop spreading, CSRF tokens in Server Actions, secure token storage.]

---

## 11. Real-World Case Study

[Architecture scenario from production (e.g., scaling an enterprise dashboard, managing optimistic chat messages, building a resilient payment form).]

---

## 12. Interview Q&A

<details>
<summary>1. [Staff / Senior Interview Question 1]</summary>
[Deep, technically rigorous answer explaining React internals and trade-offs.]
</details>

<details>
<summary>2. [Interview Question 2]</summary>
[Answer]
</details>

<details>
<summary>3. [Interview Question 3]</summary>
[Answer]
</details>

<details>
<summary>4. [Interview Question 4]</summary>
[Answer]
</details>

<details>
<summary>5. [Interview Question 5]</summary>
[Answer]
</details>

<details>
<summary>6. [Interview Question 6]</summary>
[Answer]
</details>

<details>
<summary>7. [Interview Question 7]</summary>
[Answer]
</details>

<details>
<summary>8. [Interview Question 8]</summary>
[Answer]
</details>

---

## 13. Practical Challenges & Exercises

<details>
<summary>Exercise 1 (Easy): [Title]</summary>
[Task specification and complete verified solution.]
```tsx
// Solution
```
</details>

<details>
<summary>Exercise 2 (Easy): [Title]</summary>
[Task specification and complete verified solution.]
```tsx
// Solution
```
</details>

<details>
<summary>Exercise 3 (Medium): [Title]</summary>
[Task specification and complete verified solution.]
```tsx
// Solution
```
</details>

<details>
<summary>Exercise 4 (Medium): [Title]</summary>
[Task specification and complete verified solution.]
```tsx
// Solution
```
</details>

<details>
<summary>Exercise 5 (Hard): [Title]</summary>
[Task specification and complete verified solution.]
```tsx
// Solution
```
</details>

<details>
<summary>Exercise 6 (Hard): [Title]</summary>
[Task specification and complete verified solution.]
```tsx
// Solution
```
</details>
```
