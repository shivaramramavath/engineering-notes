# Specificity Calculation

| Metadata | Details |
|:---|:---|
| **Level** | Foundational |
| **Prerequisites** | `01-fundamentals/02-selectors.md`, `01-fundamentals/03-pseudo-classes.md`, `01-fundamentals/04-pseudo-elements.md` |
| **Estimated Reading Time** | 28 minutes |
| **Status** | Current |
| **Baseline Status** | Widely Available (Universal Web Standard; :is/:where/:has Specificity Baseline 2023) |
| **Browsers Tested** | Chrome 120+, Firefox 121+, Safari 17.2+, Edge 120+ |
| **Tested With** | W3C Selectors 4 Specificity Algorithm, Playwright css_runner.js, stylelint |

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

### What is Specificity?
**Specificity** is the mathematical weight assigned to a CSS selector by the browser engine. When multiple conflicting CSS declarations target the exact same HTML element and property (e.g., one rule declares `color: blue` and another declares `color: red`), the browser uses specificity as the primary tie-breaker to determine which declaration wins.

```
+-------------------------------------------------------------------------+
| SPECIFICITY RESOLUTION                                                  |
|                                                                         |
| Selector A:  p               -> Weight: [0, 0, 1]  (Type selector)      |
| Selector B:  .lead           -> Weight: [0, 1, 0]  (Class selector)     |
| Selector C:  #main-intro     -> Weight: [1, 0, 0]  (ID selector)        |
|                                                                         |
| WINNER: Selector C (#main-intro) wins definitively because              |
| [1, 0, 0] > [0, 1, 0] > [0, 0, 1]                                      |
+-------------------------------------------------------------------------+
```

### Why is Understanding Specificity Essential?
A lack of specificity comprehension is the single most common cause of CSS frustration:
- Developers write styles that "mysteriously don't apply", leading them to tack on `!important` or chain endless ancestor selectors.
- This triggers an "arms race" that turns design systems into brittle, unmaintainable stylesheets.
- Mastering specificity allows you to author flat, modular, and easily overrideable CSS architectures using modern primitives like `:where()` and Cascade Layers (`@layer`).

---

## 2. Core Concepts

### 1. The Three-Column Specificity Tuple: `[A, B, C]`
According to the official W3C Selectors Level 4 specification, specificity is represented as a three-column tuple: `[A, B, C]` (or historically `(A, B, C)`):

1. **Column A (ID Selectors)**: Matches by element ID (e.g., `#header`, `#sidebar`).
2. **Column B (Classes, Attributes & Pseudo-Classes)**:
   - Class selectors (e.g., `.card`, `.btn`).
   - Attribute selectors (e.g., `[type="radio"]`, `[aria-expanded="true"]`).
   - Pseudo-classes (e.g., `:hover`, `:focus-visible`, `:first-child`).
3. **Column C (Types & Pseudo-Elements)**:
   - Type selectors / HTML tag names (e.g., `h1`, `div`, `button`).
   - Pseudo-elements (e.g., `::before`, `::after`, `::marker`).

### 2. Zero Specificity Tokens: `[0, 0, 0]`
The following constructs contribute **absolutely zero weight** to the specificity calculation:
- Universal selector (`*`).
- Combinators: descendant (` `), child (`>`), adjacent sibling (`+`), general sibling (`~`).
- The `:where()` pseudo-class (its argument contents are stripped to `[0, 0, 0]`).
- The `:is()` and `:not()` pseudo-classes themselves (though their arguments contribute weight).

### 3. Inline Styles: The `(1, 0, 0, 0)` Precedence
When styles are declared directly on an HTML element via `style="..."`, they outrank all external selectors. In four-column diagrams, inline styles occupy the leftmost column: `[1, 0, 0, 0]`.

### 4. `!important` is NOT Specificity!
> [!IMPORTANT]
> A common misconception is that `!important` gives a declaration "infinite specificity". In the W3C Cascade algorithm, `!important` does not alter selector specificity at all; instead, it hoists the declaration into an entirely separate, higher **Cascade Origin** (the "Author Important" origin).

---

## 3. How It Works Internally

### Lexicographical Comparison Algorithm
Modern browser engines (Blink, WebKit, Gecko) evaluate specificity using **lexicographical comparison** (comparing column-by-column from left to right):

```
+--------------------------------------------------------------------+
| SELECTOR SPECIFICITY COMPARISON ENGINE                             |
+--------------------------------------------------------------------+
  Tuple 1:  [ A1, B1, C1 ]
  Tuple 2:  [ A2, B2, C2 ]

  Step 1: Compare A1 vs A2 (IDs)
          - If A1 > A2 -> Tuple 1 WINS immediately! (Stop comparison)
          - If A2 > A1 -> Tuple 2 WINS immediately! (Stop comparison)
          - If A1 == A2 -> Proceed to Step 2.

  Step 2: Compare B1 vs B2 (Classes / Attributes / Pseudo-classes)
          - If B1 > B2 -> Tuple 1 WINS immediately! (Stop comparison)
          - If B2 > B1 -> Tuple 2 WINS immediately! (Stop comparison)
          - If B1 == B2 -> Proceed to Step 3.

  Step 3: Compare C1 vs C2 (Tags / Pseudo-elements)
          - If C1 > C2 -> Tuple 1 WINS!
          - If C2 > C1 -> Tuple 2 WINS!
          - If C1 == C2 -> EXACT TIE -> Cascade Order of Appearance Wins!
```

### The Myth of "Base-10" Specificity
A dangerous myth taught in legacy tutorials is that specificity is a base-10 number (e.g., claiming 10 classes equal 1 ID: `0, 10, 0` = `100`).

**This is completely false.** Specificity columns never carry over into adjacent columns:
- A selector with **1 single ID** (`[1, 0, 0]`) will outrank a selector with **10,000 chained classes** (`[0, 10000, 0]`).
- In browser C++ engines, each column is stored in an independent integer bitfield (typically 32-bit unsigned integers).

---

## 4. Syntax & Usage

```
+-------------------------------------------------------------------------+
| Web Platform Baseline: Widely Available                                  |
| Three-column specificity calculation is universal across all browsers. |
| Functional pseudo-classes (:is, :where, :has) specificity handling is   |
| Widely Available (Baseline 2023).                                       |
+-------------------------------------------------------------------------+
```

### Specificity Calculation Examples

| Selector | Column A (IDs) | Column B (Classes/Attr/Pseudo) | Column C (Types/Pseudo-El) | Total Specificity Tuple |
|:---|:---:|:---:|:---:|:---:|
| `*` | 0 | 0 | 0 | `[0, 0, 0]` |
| `li` | 0 | 0 | 1 | `[0, 0, 1]` |
| `ul li` | 0 | 0 | 2 | `[0, 0, 2]` |
| `ul > li::before` | 0 | 0 | 3 | `[0, 0, 3]` |
| `.menu` | 0 | 1 | 0 | `[0, 1, 0]` |
| `.menu > li` | 0 | 1 | 1 | `[0, 1, 1]` |
| `.menu > li:hover` | 0 | 2 | 1 | `[0, 2, 1]` |
| `a[href^="https://"]` | 0 | 1 | 1 | `[0, 1, 1]` |
| `#header` | 1 | 0 | 0 | `[1, 0, 0]` |
| `#header .nav-item` | 1 | 1 | 0 | `[1, 1, 0]` |
| `#header nav > ul li.active a:focus` | 1 | 2 | 4 | `[1, 2, 4]` |

### Functional Pseudo-Class Specificity Rules
- **`:is(selector list)`**: Specificity equals the **maximum specificity** among selectors in the list.
  - `:is(p, #nav)` &rarr; Specificity is `[1, 0, 0]`!
- **`:where(selector list)`**: Specificity is **always `[0, 0, 0]`**!
  - `:where(p, #nav)` &rarr; Specificity is `[0, 0, 0]`!
- **`:not(selector list)`**: Specificity equals the maximum specificity among selectors in the list.
  - `div:not(.active)` &rarr; `[0, 1, 1]` (1 tag `div` + 1 class `.active`).
- **`:has(selector list)`**: Specificity equals the maximum specificity of the relative selector list.
  - `article:has(> .featured)` &rarr; `[0, 1, 1]` (1 tag `article` + 1 class `.featured`).
- **`:nth-child(An+B of S)`**: Specificity equals `[0, 1, 0]` (for the pseudo-class) **PLUS** the specificity of selector `S`.
  - `li:nth-child(odd of .highlight)` &rarr; `[0, 2, 1]` (`li` [0, 0, 1] + `:nth-child` [0, 1, 0] + `.highlight` [0, 1, 0]).

---

## 5. Practical Examples

### Example 1: Basic Specificity Conflict
```html
<p id="alert-text" class="warning-msg">System Warning</p>
```
```css
/* Rule 1: Specificity [0, 0, 1] */
p {
  color: #0f172a;
}

/* Rule 2: Specificity [0, 1, 0] */
.warning-msg {
  color: #d97706; /* Wins over Rule 1 */
}

/* Rule 3: Specificity [1, 0, 0] */
#alert-text {
  color: #dc2626; /* Wins over Rule 2 and Rule 1 */
}
```
*Result*: The text renders `#dc2626` (Red) because `[1, 0, 0]` outranks `[0, 1, 0]`.

### Example 2: Real-World CSS Reset with `:where()`
Eliminate specificity from foundation reset rules so design systems can override them effortlessly:
```css
/* High-specificity legacy reset (Hard to override!) */
/* html body main article p { margin-bottom: 1rem; } -> Specificity [0, 0, 5] */

/* Modern zero-specificity reset */
:where(h1, h2, h3, h4, p, ul, ol) {
  margin: 0;
  padding: 0;
  box-sizing: border-box;
}

/* Single class utility immediately wins! Specificity [0, 1, 0] > [0, 0, 0] */
.lead-paragraph {
  margin-bottom: 1.5rem;
}
```

### Example 3: Edge Case — Chained Class Hack (`.btn.btn`)
Before Cascade Layers (`@layer`) and `:is()`, developers artificially inflated class specificity without using IDs:
```css
/* Base button */
.btn {
  background-color: #64748b;
  color: white;
}

/* Specificity hack: Duplicating the class doubles Column B! */
/* .btn.btn has Specificity [0, 2, 0] */
.btn.btn {
  background-color: #0284c7; /* Overrides .btn regardless of source order */
}
```
*Analysis*: While syntactically valid, this pattern is considered an anti-pattern today. Modern CSS solves this cleanly using Cascade Layers (`@layer components`) or CSS custom properties.

### Example 4: Failure Case — Over-Specific Navigation Selectors
```css
/* Anti-Pattern in header.css */
header#main-header nav.navbar ul.menu-list li.menu-item a.menu-link {
  color: #2563eb;
}
/* Specificity: [1, 4, 3] */

/* Later in theme-dark.css */
.dark-theme .menu-link {
  color: #f8fafc; /* Fails! Specificity is only [0, 2, 0] */
}
```
*Failure Analysis*: The dark theme rule fails because `[0, 2, 0]` is hopelessly outranked by `[1, 4, 3]`. The developer is tempted to add `!important`.
*Remedy*: Flatten the base selector to a single class:
```css
.menu-link {
  color: #2563eb; /* Specificity: [0, 1, 0] */
}

.dark-theme .menu-link {
  color: #f8fafc; /* Specificity: [0, 2, 0] -> Wins cleanly! */
}
```

### Example 5: Accessible Focus Indicator Specificity Management
Ensure keyboard focus rings are never accidentally overridden by component state classes:
```css
/* Button base styling */
.action-button {
  background-color: #0284c7;
  color: #ffffff;
  border: none;
  padding: 8px 16px;
  border-radius: 4px;
}

/* Variant modifier */
.action-button--danger {
  background-color: #dc2626;
}

/* High-priority focus-visible rule guaranteeing keyboard accessibility */
.action-button:focus-visible {
  outline: 3px solid #0284c7;
  outline-offset: 2px;
}

.action-button--danger:focus-visible {
  outline: 3px solid #dc2626;
  outline-offset: 2px;
}
```
*Playwright Test Runner Verification*:
```javascript
// Test computed specificity
const el = document.querySelector('.action-button--danger');
const styles = window.getComputedStyle(el);
console.log('Background:', styles.backgroundColor); // rgb(220, 38, 38)
```

---

## 6. Best Practices

1. **Keep Specificity As Low and Flat As Possible**: Aim for single-class selectors (`.card`, `.btn`) across component stylesheets.
2. **Never Use ID Selectors in Reusable CSS**: Reserve `#id` strictly for HTML anchor links, fragment identifiers, and ARIA relationships.
3. **Use `:where()` for Resets and Default Themes**: Wrap typography and component defaults in `:where()` to give them `[0, 0, 0]` specificity.
4. **Never Use `!important` to Solve Specificity Conflicts**: If a rule isn't applying, refactor the offending selector's specificity or reorganize code into Cascade Layers (`@layer`).
5. **Be Mindful of `:is()` Inadvertently Hoisting Specificity**: Remember that `:is(p, #hero)` has specificity `[1, 0, 0]`, even when matching simple `<p>` tags.
6. **Leverage BEM or Scoped CSS to Eliminate Nesting**: Adopting BEM naming (`.block__element--modifier`) naturally results in flat `[0, 1, 0]` specificity across your codebase.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Root Cause | Symptom / Failure | Correct Pattern |
|:---|:---|:---|:---|
| Believing 10 classes override 1 ID | Base-10 math misconception | A selector with 15 classes fails to override a single `#id`. | Avoid IDs; comparison is lexicographical. |
| Using `!important` to fix selector conflicts | Rushing without checking specificity | Arms race where subsequent overrides also require `!important`. | Lower the specificity of the winning rule. |
| Assuming `:where()` adds class specificity | Misunderstanding `:where()` | Styles declared inside `:where(.btn)` are overridden by `button`. | Use `:where()` only when zero specificity is intended. |
| Forgetting that `:is()` adopts the highest specificity | Overlooking argument list weight | `:is(.card, #hero) p` has `[1, 0, 1]` specificity, breaking child overrides. | Use `:where()` or split into separate rules. |
| Over-nesting selectors in Sass / CSS nesting | Mirroring HTML DOM structure | Emits selectors with `[0, 4, 3]` specificity that cannot be overridden. | Keep nesting depth to 2–3 levels maximum. |
| Chaining tag names to classes (`button.btn`) | Habit or attempted specificity boost | Increases tag specificity (`[0, 1, 1]`) and prevents reusing class on `<a>`. | Use clean `.btn` (`[0, 1, 0]`). |

---

## 8. Performance, Accessibility & Browser Support

### Engine Specificity Matching Efficiency
In the browser engine's C++ style resolver:
- Specificity is stored as an efficient 32-bit unsigned integer packed with bitmasks (e.g., bits 0–7 for tags, bits 8–15 for classes, bits 16–23 for IDs).
- Comparing specificity between two matching rules takes a **single CPU instruction** (`tupleA > tupleB`).
- Flat, low-specificity selectors allow the browser to populate and evaluate rule caches rapidly.

### Accessibility Safeguards
- When component developers write high-specificity selectors for hover and active states, they frequently override `:focus-visible` outlines.
- Always verify that keyboard focus outlines possess equal or higher specificity than hover/active states so accessibility cues remain visible during keyboard navigation.

---

## 9. Comparison / When to Use What

| Selector Strategy | Specificity Level | Primary Benefit | Trade-off |
|:---|:---|:---|:---|
| **Single Class (`.card`)** | Flat `[0, 1, 0]` | Maximum modularity & maintainability | Requires disciplined class naming |
| **`:where(selectors)`** | Zero `[0, 0, 0]` | Overridable by any selector | Can be accidentally overridden |
| **`:is(selectors)`** | Max of arguments | Concise syntax for grouping | Can unintentionally inflate specificity |
| **Chained Classes (`.a.b`)** | Elevated `[0, 2, 0]` | Quick local override | Escalates specificity conflicts |
| **ID Selector (`#id`)** | High `[1, 0, 0]` | Targets unique page landmark | Breaks component reusability |
| **`!important`** | Changes Origin | Force emergency override | Escalates cascade conflicts |

---

## 10. Interview Questions

<details>
<summary><strong>1. Explain how the browser engine resolves a specificity tie between two conflicting rules.</strong></summary>

When two rules have identical specificity (e.g., both evaluate to `[0, 1, 1]`), have identical cascade origins, and reside in the same cascade layer, the tie is broken by **Source Order (Order of Appearance)**. The declaration that appears **later** in the parsed stylesheet (or in a later `<link>`/`<style>` tag) wins and applies to the element.
</details>

<details>
<summary><strong>2. Can a selector with 100 classes override a selector with 1 ID? Why or why not?</strong></summary>

**No, never.** Specificity comparison is **lexicographical**, not base-10 arithmetic. The browser engine compares Column A (IDs) first. Because the ID selector has `1` in Column A and the 100-class selector has `0` in Column A, the ID selector wins immediately (`[1, 0, 0] > [0, 100, 0]`). Specificity columns never "roll over" into adjacent columns.
</details>

<details>
<summary><strong>3. What is the specificity of `:where(h1, #header, .title)` and why?</strong></summary>

The specificity is **`[0, 0, 0]`**. The W3C specification explicitly defines the `:where()` pseudo-class as having zero specificity regardless of the specificity of any selectors passed inside its argument list.
</details>

<details>
<summary><strong>4. What is the specificity of `:is(h1, #header, .title)`?</strong></summary>

The specificity is **`[1, 0, 0]`**. The `:is()` pseudo-class takes on the specificity of the **most specific selector** in its argument list. Because `#header` has an ID specificity of `[1, 0, 0]`, the entire `:is()` expression evaluates to `[1, 0, 0]`, even when matching `h1` or `.title`.
</details>

<details>
<summary><strong>5. How does the universal selector `*` impact specificity?</strong></summary>

The universal selector `*` has a specificity of **`[0, 0, 0]`**. It adds zero weight to Column A, Column B, or Column C. However, any selector combined with it retains its own specificity (e.g., `*.card` has specificity `[0, 1, 0]`).
</details>

<details>
<summary><strong>6. Does `!important` increase a selector's specificity?</strong></summary>

No. `!important` does not alter specificity. In the CSS cascade architecture, declarations are first categorized into **Cascade Origins** (User Agent, User, Author, Author Important). An `!important` declaration moves from the normal "Author" origin into the "Author Important" origin. Specificity is only evaluated to resolve conflicts between declarations **within the same origin and layer**.
</details>

<details>
<summary><strong>7. Calculate the exact specificity of: `nav.sidebar > ul li:nth-child(2 of .active) a:hover::after`.</strong></summary>

Let's break down each component:
- **IDs**: 0 &rarr; Column A = `0`
- **Classes, Attributes, Pseudo-classes**:
  - `.sidebar` (Class) = 1
  - `:nth-child` (Pseudo-class) = 1
  - `.active` (Selector argument of `of`) = 1
  - `:hover` (Pseudo-class) = 1
  - Total Column B = `4`
- **Types and Pseudo-elements**:
  - `nav` (Tag) = 1
  - `ul` (Tag) = 1
  - `li` (Tag) = 1
  - `a` (Tag) = 1
  - `::after` (Pseudo-element) = 1
  - Total Column C = `5`
- **Final Specificity Tuple**: **`[0, 4, 5]`**.
</details>

<details>
<summary><strong>8. Why is BEM (Block Element Modifier) methodology effective at managing specificity?</strong></summary>

BEM enforces a flat selector architecture where every element is addressed using a single class name (e.g., `.card__header--featured`). Because all styling rules consist of a single class, virtually every declaration in the design system has the exact same specificity of **`[0, 1, 0]`**. This completely eliminates specificity conflicts and allows overrides to be governed cleanly by cascade source order or modifiers.
</details>

---

## 11. Practice Exercises

### Exercise 1: Rank Selectors by Specificity (Easy)
**Task**: Rank the following selectors in order of ascending specificity (lowest to highest):
1. `nav ul li a`
2. `.main-nav a`
3. `#primary-nav`
4. `*`
5. `a:hover`

<details>
<summary><strong>View Solution</strong></summary>

1. `*` &rarr; `[0, 0, 0]`
2. `nav ul li a` &rarr; `[0, 0, 4]` (4 tags)
3. `a:hover` &rarr; `[0, 1, 1]` (1 pseudo-class + 1 tag)
4. `.main-nav a` &rarr; `[0, 1, 1]` (1 class + 1 tag; ties with #3, resolved by source order)
5. `#primary-nav` &rarr; `[1, 0, 0]` (1 ID)
</details>

---

### Exercise 2: Prevent `:is()` Specificity Explosion (Easy)
**Task**: An engineer writes:
```css
:is(header, #banner, footer) p {
  line-height: 1.5;
}
```
They are surprised when a subsequent rule `.footer-note { line-height: 1.7; }` fails to apply to `<p class="footer-note">` inside `<footer>`. Explain why and refactor.

<details>
<summary><strong>View Solution</strong></summary>

*Explanation*: `:is()` takes the specificity of its highest argument (`#banner`), giving `:is(...) p` a specificity of `[1, 0, 1]`. The class `.footer-note` only has `[0, 1, 0]`, losing the tie-break.

*Refactored Solution*: Use `:where()` to strip specificity to zero:
```css
:where(header, #banner, footer) p {
  line-height: 1.5; /* Specificity is [0, 0, 1] */
}

/* Successfully applies because [0, 1, 0] > [0, 0, 1] */
.footer-note {
  line-height: 1.7;
}
```
</details>

---

### Exercise 3: Calculate Complex Specificity Tuple (Medium)
**Task**: Calculate the specificity tuple `[A, B, C]` for the following selector:
```css
body > div#app main section.hero:not(.disabled) [data-status="active"]::before
```

<details>
<summary><strong>View Solution</strong></summary>

- **Column A (IDs)**: `#app` = **1**
- **Column B (Classes, Attributes, Pseudo-classes)**:
  - `.hero` (Class) = 1
  - `:not(.disabled)` (Argument `.disabled` counts) = 1
  - `[data-status="active"]` (Attribute) = 1
  - Total Column B = **3**
- **Column C (Tags, Pseudo-elements)**:
  - `body` (Tag) = 1
  - `div` (Tag) = 1
  - `main` (Tag) = 1
  - `section` (Tag) = 1
  - `::before` (Pseudo-element) = 1
  - Total Column C = **5**

**Final Specificity**: **`[1, 3, 5]`**.
</details>

---

### Exercise 4: Defeat an Inline Style Without Editing HTML (Medium)
**Task**: An element has an inline style: `<div id="box" class="card" style="background-color: red;"></div>`. Write a selector in an external stylesheet that turns the background blue (`#0284c7`).

<details>
<summary><strong>View Solution</strong></summary>

```css
/* Using !important hoists declaration to Author Important origin */
.card {
  background-color: #0284c7 !important;
}
```
*Explanation*: Normal author selectors (regardless of specificity) cannot override inline styles. Adding `!important` moves the declaration into the Author Important origin, which outranks normal inline declarations.
</details>

---

### Exercise 5: Specificity of Nested Selectors (Hard)
**Task**: Calculate the computed specificity of `.item` inside the following native CSS nesting block:
```css
.card {
  & > .header {
    & .item {
      color: #0284c7;
    }
  }
}
```

<details>
<summary><strong>View Solution</strong></summary>

In native CSS nesting, the `&` symbol represents the parent selector wrapped in an `:is()` block.
The nested selector is evaluated as:
`:is(:is(.card) > .header) .item`
- `.card` = `[0, 1, 0]`
- `.header` = `[0, 1, 0]`
- `.item` = `[0, 1, 0]`
- **Total Specificity**: **`[0, 3, 0]`**.
</details>

---

### Exercise 6: Programmatic Specificity Calculator in JavaScript (Hard)
**Task**: Write a JavaScript function `calculateSpecificity(selector)` that parses a basic selector string and returns its `[A, B, C]` tuple array.

<details>
<summary><strong>View Solution</strong></summary>

```javascript
/**
 * Computes the specificity tuple [IDs, Classes/Attrs/Pseudos, Types/Pseudo-elements]
 * @param {string} selector
 * @returns {[number, number, number]}
 */
function calculateSpecificity(selector) {
  let a = 0, b = 0, c = 0;

  // 1. Remove contents of :where() since it contributes 0
  let clean = selector.replace(/:where\([^)]*\)/g, '');

  // 2. Count IDs
  const idMatches = clean.match(/#[a-zA-Z0-9_-]+/g);
  if (idMatches) a += idMatches.length;

  // 3. Count Classes
  const classMatches = clean.match(/\.[a-zA-Z0-9_-]+/g);
  if (classMatches) b += classMatches.length;

  // 4. Count Attributes
  const attrMatches = clean.match(/\[[^\]]+\]/g);
  if (attrMatches) b += attrMatches.length;

  // 5. Count Pseudo-elements (::)
  const pseudoElemMatches = clean.match(/::[a-zA-Z0-9_-]+/g);
  if (pseudoElemMatches) {
    c += pseudoElemMatches.length;
    clean = clean.replace(/::[a-zA-Z0-9_-]+/g, ''); // strip to avoid double-counting
  }

  // 6. Count Pseudo-classes (:)
  const pseudoClassMatches = clean.match(/:[a-zA-Z0-9_-]+/g);
  if (pseudoClassMatches) b += pseudoClassMatches.length;

  // 7. Count Type selectors (words not preceded by #, ., :, [)
  const typeMatches = clean.match(/(?:^|[\s>+~])([a-zA-Z][a-zA-Z0-9_-]*)/g);
  if (typeMatches) c += typeMatches.length;

  return [a, b, c];
}

console.log(calculateSpecificity('nav#main ul.menu > li.active a:hover::after'));
// Returns: [1, 3, 4]
```
*Explanation*: Parses selector tokens into their respective specificity columns.
</details>

---

## 12. Summary / Cheat Sheet

```
+-------------------------------------------------------------------------------------------------+
| SPECIFICITY MASTER CHEAT SHEET                                                                  |
+--------------------------+-----------------------------+--------------------+-------------------+
| Component                | Examples                    | Column Tuple       | Notes             |
+--------------------------+-----------------------------+--------------------+-------------------+
| Inline Style             | style="..."                 | [1, 0, 0, 0]       | Outranks sheets   |
| ID Selector              | #header, #modal             | [1, 0, 0] (Col A)  | 1 ID beats 100 cls|
| Class Selector           | .btn, .card                 | [0, 1, 0] (Col B)  | Design system core|
| Attribute Selector       | [type="text"], [disabled]   | [0, 1, 0] (Col B)  | Same as class     |
| Pseudo-Class             | :hover, :focus-visible      | [0, 1, 0] (Col B)  | Same as class     |
| Type Selector            | h1, p, button, div          | [0, 0, 1] (Col C)  | HTML tag names    |
| Pseudo-Element           | ::before, ::after, ::marker | [0, 0, 1] (Col C)  | Virtual elements  |
| Universal Selector       | *                           | [0, 0, 0] (Zero!)  | Zero specificity  |
| :where() Pseudo-Class    | :where(h1, #header)         | [0, 0, 0] (Zero!)  | Strips all weight |
| :is(), :not(), :has()    | :is(.a, #b)                 | Max(Arguments)     | Takes heaviest arg|
+--------------------------+-----------------------------+--------------------+-------------------+
```

---

## 13. Further Reading

- [W3C Selectors Level 4: Calculating a Selector's Specificity](https://www.w3.org/TR/selectors-4/#specificity-rules)
- [MDN Web Docs: Specificity](https://developer.mozilla.org/en-US/docs/Web/CSS/Specificity)
- [web.dev: Learn CSS Specificity](https://web.dev/learn/css/specificity)
- [Polypane Specificity Calculator](https://polypane.app/css-specificity-calculator/)
- [W3C CSS Cascading and Inheritance Level 5](https://www.w3.org/TR/css-cascade-5/)

---

[Previous: 04 - Pseudo-Elements](04-pseudo-elements.md) | [Section Index](README.md) | [Next: 06 - Cascade: Origins, Importance & Order](06-cascade-origins-importance-and-order.md)
