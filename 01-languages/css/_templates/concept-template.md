# [Concept Title]

| Metadata | Details |
|:---|:---|
| **Level** | [Foundational / Intermediate / Advanced / Architectural] |
| **Prerequisites** | [[Prerequisite Note 1]](relative-path-1.md), [[Prerequisite Note 2]](relative-path-2.md) |
| **Estimated Reading Time** | [XX] minutes |
| **Status** | [Current / Draft] |
| **Baseline Status** | [Widely Available / Newly Available / Limited Availability / Experimental] |
| **Browsers Tested** | Chrome 120+, Firefox 121+, Safari 17.2+, Edge 120+ |
| **Tested With** | Playwright css_runner.js, getComputedStyle(), stylelint |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. [Practical Examples](#5-practical-examples)
6. [Best Practices](#6-best-practices)
7. [Common Mistakes & Pitfalls](#7-common-mistakes--pitfalls)
8. [Performance, Accessibility & Browser Support](#8-performance-accessibility--browser-support)
9. [Comparison / When to Use What](#9-comparison--when-to-use-what)
10. [Interview Questions](#10-interview-questions)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary / Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

### What is [Concept]?
[Concise formal definition of the feature or property.]

### Why Does It Exist?
[Historical context, problems with legacy approaches, and real-world engineering benefits.]

---

## 2. Core Concepts

[Detailed breakdown of mental models, terminology, and spec definitions.]

```
[ASCII or text diagram illustrating key concepts]
```

---

## 3. How It Works Internally

### The Engine Pipeline & Algorithm
[Detailed explanation of the CSSWG layout algorithm, cascade resolution, stacking context creation, paint layers, or GPU compositing.]

```
[Detailed ASCII or Mermaid flowchart showing spec lifecycle]
```

---

## 4. Syntax & Usage

### Baseline Conformance Matrix

```
+--------------------------------------------------------------------------+
| FEATURE / PROPERTY       | BASELINE STATUS | ENGINES SUPPORTED (2024-2026)|
+--------------------------+-----------------+------------------------------+
| [Feature]                | Widely Available| Blink, Gecko, WebKit (100%)  |
+--------------------------------------------------------------------------+
```

### Formal Syntax Reference
```css
/* Syntax example */
selector {
  property: value;
}
```

---

## 5. Practical Examples

### Example 1: Basic — [Simple Usage]
**Goal**: [Explain goal]

```html
<!-- HTML Structure -->
<div class="box">Content</div>
```

```css
/* CSS Styles */
.box {
  /* ... */
}
```

**Measured / Computed Output**:
```
getComputedStyle(.box):
- property: value
- getBoundingClientRect(): width: 100px, height: 50px
```
**Explanation**: [Why this works]

### Example 2: Real-World — [Production Component]
[Same structure]

### Example 3: Edge Case — [Complex Scenario]
[Same structure]

### Example 4: Failure Case — [Anti-Pattern to Avoid]
[Same structure]

### Example 5: Accessible / Production-Grade — [Robust Component]
[Same structure]

---

## 6. Best Practices

1. **[Best Practice 1]**: [Explanation]
2. **[Best Practice 2]**: [Explanation]
3. **[Best Practice 3]**: [Explanation]
4. **[Best Practice 4]**: [Explanation]
5. **[Best Practice 5]**: [Explanation]
6. **[Best Practice 6]**: [Explanation]

---

## 7. Common Mistakes & Pitfalls

| Mistake | Severity | Consequence | Correction |
|:---|:---|:---|:---|
| [Mistake 1] | Critical / High / Medium | [Impact] | [Fix] |
| [Mistake 2] | Critical / High / Medium | [Impact] | [Fix] |
| [Mistake 3] | Critical / High / Medium | [Impact] | [Fix] |
| [Mistake 4] | Critical / High / Medium | [Impact] | [Fix] |
| [Mistake 5] | Critical / High / Medium | [Impact] | [Fix] |
| [Mistake 6] | Critical / High / Medium | [Impact] | [Fix] |

---

## 8. Performance, Accessibility & Browser Support

### Performance
[Layout thrashing, paint costs, compositor-only animation, will-change considerations.]

### Accessibility
[WCAG contrast compliance, :focus-visible, prefers-reduced-motion, forced-colors adaptation.]

### Browser Support & Fallbacks
[Baseline status and progressive enhancement fallback using @supports.]

---

## 9. Comparison / When to Use What

### [Feature] vs [Alternative]

| Scenario / Criterion | [Feature] | [Alternative 1] | [Alternative 2] |
|:---|:---|:---|:---|
| **Primary Use** | ... | ... | ... |
| **Performance** | ... | ... | ... |
| **Complexity** | ... | ... | ... |

---

## 10. Interview Questions

### Q1: [Technical Interview Question 1]
<details>
<summary>View Answer</summary>

[Comprehensive answer with technical explanation.]
</details>

[Q2 through Q8 with details]

---

## 11. Practice Exercises

### Exercise 1: Easy — [Exercise Title]
**Problem**: [Problem description]

<details>
<summary>View Solution</summary>

```css
/* Solution */
```
</details>

[Exercises 2 through 6 with details]

---

## 12. Summary / Cheat Sheet

- [Key Takeaway 1]
- [Key Takeaway 2]
- [Key Takeaway 3]
- [Key Takeaway 4]

---

## 13. Further Reading

- [W3C Specification Title](https://www.w3.org/TR/...)
- [MDN Web Docs Article](https://developer.mozilla.org/en-US/docs/Web/CSS/...)
- [web.dev Guide](https://web.dev/...)

---

[Previous: [Previous Note]](previous-note.md) | [Section Index](README.md) | [Next: [Next Note]](next-note.md)
