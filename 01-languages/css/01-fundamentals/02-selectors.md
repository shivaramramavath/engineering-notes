# Selectors: Type, Class, ID & Attribute

| Metadata | Details |
|:---|:---|
| **Level** | Foundational |
| **Prerequisites** | `01-fundamentals/01-syntax-rules-and-declarations.md` |
| **Estimated Reading Time** | 28 minutes |
| **Status** | Current |
| **Baseline Status** | Widely Available (Universal across all modern engines) |
| **Browsers Tested** | Chrome 120+, Firefox 121+, Safari 17.2+, Edge 120+ |
| **Tested With** | W3C Selectors Level 4, Playwright css_runner.js, stylelint |

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

### What are CSS Selectors?
A **CSS Selector** is an expression that identifies which elements in the Document Object Model (DOM) tree should be styled by a declaration block. Selectors range from basic element type matching to intricate relational expressions using combinators, structural attributes, and accessibility attributes.

### Why Do We Need Varied Selector Types?
In early web development, styling was tightly coupled to HTML tags (e.g., `table`, `font`, `center`). As web applications grew into complex component systems, developers required:
1. **Separation of Semantics from Visual Design**: Using **classes** (`.card`, `.btn`) allows HTML to retain semantic tags (`<article>`, `<button>`) while sharing reusable design patterns.
2. **Relational Context**: Styling an element differently based on where it sits in the DOM hierarchy (e.g., direct child `>` vs general descendant ` `).
3. **State-Driven Styling via Attributes**: Tying visual styles directly to HTML accessibility state attributes (`[aria-expanded="true"]`, `[disabled]`) rather than manually toggling duplicate CSS classes.

---

## 2. Core Concepts

### 1. Basic Selectors
- **Universal Selector (`*`)**: Matches every single element in the DOM tree. Commonly used in box-sizing resets (`*, *::before, *::after`).
- **Type Selector (`button`, `p`, `h1`)**: Matches elements by their HTML tag name. Low specificity (`[0, 0, 1]`).
- **Class Selector (`.card`, `.badge`)**: Matches elements possessing that token in their `class` attribute. Medium specificity (`[0, 1, 0]`). Reusable across many elements.
- **ID Selector (`#header`, `#sidebar`)**: Matches the unique element whose `id` attribute matches. High specificity (`[1, 0, 0]`). *Best practice: Avoid using IDs for styling due to specificity inflation.*

### 2. Combinators: Relational DOM Matching
Combinators link two or more simple selectors to express structural relationships:

```
+-------------------------------------------------------------------------+
| SELECTOR COMBINATORS                                                    |
+------------+-----------------------+------------------------------------+
| Combinator | Syntax                | Relationship Tested                |
+------------+-----------------------+------------------------------------+
| Descendant | A B (whitespace)      | B is anywhere inside A's subtree   |
| Child      | A > B                 | B is an immediate direct child of A|
| Next-Sib   | A + B (adjacent)      | B immediately follows A at sibling |
| Subseq-Sib | A ~ B (general sibling| B follows A anywhere as a sibling  |
+------------+-----------------------+------------------------------------+
```

### 3. Attribute Selectors
Attribute selectors match elements based on the presence, exact value, or substring pattern of their HTML attributes:
- **`[attr]`**: Element has attribute `attr` present, regardless of value.
- **`[attr="val"]`**: Attribute value exactly equals `"val"`.
- **`[attr~="val"]`**: Attribute value is a whitespace-separated list of words, one of which is `"val"` (identical to class matching).
- **`[attr|="val"]`**: Attribute value is either exactly `"val"` or begins with `"val-"` (frequently used for language tags like `[lang|="en"]`).
- **`[attr^="val"]`**: Attribute value **begins with** substring `"val"`.
- **`[attr$="val"]`**: Attribute value **ends with** substring `"val"`.
- **`[attr*="val"]`**: Attribute value **contains** substring `"val"` anywhere.
- **Case Modifiers (`i` and `s`)**: `[attr="val" i]` matches case-insensitively; `[attr="val" s]` matches case-sensitively.

---

## 3. How It Works Internally

### Right-to-Left (RTL) Selector Matching Algorithm
A common misconception is that browsers match selectors from left to right. In reality, modern rendering engines (Blink, WebKit, Gecko) evaluate compound selectors **from right to left**:

```
Given Selector:  nav.primary-nav > ul > li.active a
                 [ Ancestors ]          [ KEY SELECTOR ]
```

```
+--------------------------------------------------------------------+
| DOM ELEMENT CURRENTLY BEING EVALUATED (e.g., an <a> node)          |
+--------------------------------------------------------------------+
                                |
                                v
+--------------------------------------------------------------------+
| 1. TEST KEY SELECTOR (Rightmost): Is this element an 'a'?          |
|    - If NO  -> Reject immediately (Skip entire subtree evaluation) |
|    - If YES -> Proceed to parent check                             |
+--------------------------------------------------------------------+
                                |
                                v
+--------------------------------------------------------------------+
| 2. TEST PARENT: Is immediate parent a 'li' with class '.active'?   |
|    - If NO  -> Reject                                              |
|    - If YES -> Proceed upwards                                     |
+--------------------------------------------------------------------+
                                |
                                v
+--------------------------------------------------------------------+
| 3. TEST ANCESTOR CHAIN: Is parent an 'ul' inside 'nav.primary-nav'?|
|    - If MATCH -> Rule applies to the <a> element!                  |
+--------------------------------------------------------------------+
```

### Why Right-to-Left Evaluation is Vastly Faster
On an average webpage, there might be 2,000 DOM elements. If the engine started at the left (`nav.primary-nav`), it would find the navigation, traverse down thousands of descendants, only to find most aren't `<a>` tags. By starting at the **Key Selector** (the target node being styled), the engine can immediately disqualify 99% of DOM nodes before inspecting any ancestor trees!

---

## 4. Syntax & Usage

```
+-------------------------------------------------------------------------+
| Web Platform Baseline: Widely Available                                  |
| Basic, combinator, and substring attribute selectors are universally   |
| supported in all browsers since Chrome 1, Firefox 3.5, Safari 3.1.      |
| Case modifiers [attr="val" i] are Widely Available (Baseline 2020).     |
+-------------------------------------------------------------------------+
```

### Combinator Syntax Examples
```css
/* 1. Descendant: Matches any <p> inside .article */
.article p {
  line-height: 1.6;
}

/* 2. Direct Child: Matches only <li> directly under .menu */
.menu > li {
  list-style: none;
}

/* 3. Adjacent Sibling: Matches only the first <p> immediately after an <h1> */
h1 + p {
  font-size: 1.25rem;
  font-weight: 500;
}

/* 4. General Sibling: Matches all <p> siblings that appear after <img> */
img ~ p {
  color: #475569;
}
```

### Substring Attribute Syntax Examples
```css
/* Matches all secure HTTPS links */
a[href^="https://"] {
  color: #0284c7;
}

/* Matches PDF document downloads */
a[href$=".pdf"]::after {
  content: " (PDF)";
  font-size: 0.75em;
}

/* Matches external links containing domain */
a[href*="github.com"] {
  font-weight: 600;
}
```

---

## 5. Practical Examples

### Example 1: Basic Structural Layout with Combinators
```html
<nav class="breadcrumb">
  <a href="/">Home</a>
  <span>/</span>
  <a href="/docs">Docs</a>
  <span>/</span>
  <span class="current">Selectors</span>
</nav>
```
```css
.breadcrumb > a {
  color: #2563eb;
  text-decoration: none;
}

.breadcrumb > a:hover {
  text-decoration: underline;
}

/* Style separator spans that immediately follow links */
.breadcrumb > a + span {
  margin: 0 0.5rem;
  color: #94a3b8;
}

.breadcrumb > .current {
  color: #0f172a;
  font-weight: 600;
}
```

### Example 2: Real-World Visual Indicators via Attribute Selectors
Automatically append visual cues to external or insecure links without touching markup:
```css
/* Base link styling */
a {
  color: #2563eb;
  text-decoration: underline;
}

/* Style external links */
a[target="_blank"] {
  cursor: alias;
}

/* Target file types automatically */
a[href$=".pdf"]::after {
  content: " [PDF]";
  color: #dc2626;
  font-size: 0.8em;
}

a[href$=".zip"]::after {
  content: " [ZIP]";
  color: #d97706;
  font-size: 0.8em;
}
```

### Example 3: Edge Case — Case-Insensitive Attribute Matching
Legacy systems frequently output mixed-case MIME types or file extensions (`.PDF`, `.Pdf`, `.pdf`):
```html
<a href="/reports/annual-audit.PDF">Download Report</a>
<a href="/reports/q4-summary.pdf">Quarterly Summary</a>
```
```css
/* Using the 'i' flag matches .pdf, .PDF, and .Pdf identically! */
a[href$=".pdf" i] {
  padding-left: 20px;
  background: url('/icons/pdf-icon.svg') no-repeat left center;
}
```

### Example 4: Failure Case — Over-Qualified Selector Bloat
```css
/* Anti-Pattern: Over-qualified high-specificity selector chain */
html body div#app main.main-content section.hero div.container h1.hero-title {
  font-size: 3rem;
  color: #0f172a;
}
```
*Failure Analysis*:
1. **Extreme Specificity**: Specificity is `[1, 3, 5]`. Overriding this title elsewhere requires another monstrous selector or `!important`.
2. **Brittle Coupling**: If `section.hero` is changed to `<header class="hero">`, the entire selector breaks silently.
3. **Engine Evaluation Overhead**: The browser engine must traverse 8 ancestor steps on every matching attempt.
*Correct Pattern*:
```css
/* Clean, decoupled, resilient single-class selector */
.hero-title {
  font-size: 3rem;
  color: #0f172a;
}
```

### Example 5: Accessible State Styling with ARIA Attributes
Rather than writing `.accordion-button.is-active`, style directly against standard ARIA states:
```html
<button type="button" class="accordion-trigger" aria-expanded="false" aria-controls="section-1">
  Section Overview
</button>
<div id="section-1" class="accordion-panel" hidden>
  Panel Content
</div>
```
```css
.accordion-trigger {
  display: flex;
  justify-content: space-between;
  width: 100%;
  padding: 1rem;
  background: #f8fafc;
  border: 1px solid #cbd5e1;
  font-weight: 600;
  cursor: pointer;
}

.accordion-trigger:focus-visible {
  outline: 2px solid #0284c7;
  outline-offset: 2px;
}

/* Style active state directly from ARIA attribute */
.accordion-trigger[aria-expanded="true"] {
  background-color: #e0f2fe;
  border-color: #38bdf8;
  color: #0369a1;
}

.accordion-trigger[aria-expanded="true"]::after {
  content: "▲";
}

.accordion-trigger[aria-expanded="false"]::after {
  content: "▼";
}
```
*Playwright Test Runner Verification*:
```javascript
// Test ARIA attribute matching
const btn = document.querySelector('.accordion-trigger');
btn.setAttribute('aria-expanded', 'true');
const computed = window.getComputedStyle(btn);
console.log('Active Background:', computed.backgroundColor); // rgb(224, 242, 254)
```

---

## 6. Best Practices

1. **Keep Selectors Flat & Shallow**: Prefer single class selectors (`.nav-item`) over deep descendant chains (`nav ul li a`).
2. **Never Use ID Selectors for Styling**: An ID adds `[1, 0, 0]` to specificity, which makes future overrides painful. Use IDs strictly for JavaScript anchors and accessibility `aria-labelledby`.
3. **Prefer Direct Child (`>`) Over Descendant (` `) When Feasible**: Child combinators stop evaluating immediately after the parent, reducing ancestor backtracking.
4. **Style Accessibility States via ARIA Attributes**: Bind visual toggles to `[aria-expanded="true"]`, `[aria-selected="true"]`, and `[aria-disabled="true"]` so visual UI and accessibility states stay in sync.
5. **Use the `i` Modifier for File Extension Matching**: Guarantee file download links (`[href$=".pdf" i]`) match regardless of server or user casing.
6. **Avoid Over-Qualifying Selectors**: Writing `button.btn` or `div.card` adds unnecessary tag specificity and prevents reusing the class on other HTML elements.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Root Cause | Symptom / Failure | Correct Pattern |
|:---|:---|:---|:---|
| Over-qualifying class with tag (`div.card`) | Habit from desktop software | Cannot reuse `.card` on `<section>` or `<article>`; adds tag specificity. | Use clean `.card`. |
| Writing `ul li` descendant selector | Unintentional deep matching | Matches nested dropdown lists unexpectedly, breaking sub-menu layout. | Use direct child combinator: `.menu > li`. |
| Relying on ID selectors for reusable components | Misunderstanding specificity | Cannot override component styles without `!important` or chaining IDs. | Use class selectors (`.profile-card`). |
| Missing quotes in attribute selectors with spaces | Syntax misunderstanding | `[data-role=admin user]` fails to parse; entire rule is discarded. | Always quote attribute values: `[data-role="admin user"]`. |
| Chaining too many descendant combinators | CSS written strictly following HTML tree | Slow selector matching; fragile code that breaks on any HTML refactor. | Use BEM or utility naming (`.article__title`). |
| Forgetting that adjacent sibling (`+`) requires immediacy | Misunderstanding combinator | `.alert + p` does not style `<p>` if an empty `<div class="divider">` sits between them. | Use general sibling (`~`) if intermediate siblings exist. |

---

## 8. Performance, Accessibility & Browser Support

### Selector Engine Performance & Key Selectors
In modern browser engines, selector matching represents less than **5% of total frame time**. However, deeply nested descendant selectors with universal wildcards (e.g., `body div * .item`) force the engine to check every DOM ancestor up to `document.documentElement` for every node.

*Performance Rule*: Keep the **Key Selector** (the rightmost token) specific. Avoid `body div *` or universal key selectors.

### Accessibility Integration
Styling via attribute selectors fosters an **accessibility-first architecture**:
- If an engineer forgets to update the `aria-expanded` attribute when expanding a drawer, the drawer will not visually open. This forces developers to implement accessible HTML attributes to achieve the desired visual state!

---

## 9. Comparison / When to Use What

| Selector | Specificity | Reusability | Ideal Use Case |
|:---|:---|:---|:---|
| **Type (`p`, `button`)** | `[0, 0, 1]` | Document-wide | Global CSS resets, typography baselines |
| **Class (`.btn`)** | `[0, 1, 0]` | High (Reusable) | Primary design system components & utilities |
| **Attribute (`[attr="val"]`)**| `[0, 1, 0]` | High | Dynamic states (`aria-*`), form types (`input[type="email"]`) |
| **ID (`#nav`)** | `[1, 0, 0]` | None (Unique) | Anchors, ARIA target references (NOT for styling) |
| **Child (`>`)** | Sum of tokens | Structural | Immediate container-item relationships |
| **Adjacent (`+`)** | Sum of tokens | Contextual | First-element spacing, label-input spacing |

---

## 10. Interview Questions

<details>
<summary><strong>1. Explain why modern browser engines evaluate CSS selectors from right to left instead of left to right.</strong></summary>

Browsers evaluate selectors from right to left (RTL) for **computational efficiency**. When the browser recalculates styles for a specific DOM node, it tests the **Key Selector** (the rightmost part of the selector). If the current node does not match the key selector, the engine can immediately reject the rule and stop evaluating. If engines evaluated left to right, they would have to search thousands of subtrees and descendants for every selector before finding whether the target element matched.
</details>

<details>
<summary><strong>2. What is the difference between the adjacent sibling combinator (`+`) and the general sibling combinator (`~`)?</strong></summary>

The **adjacent sibling combinator (`A + B`)** matches element `B` only if `B` is the **immediately following sibling** of `A` in the DOM tree (sharing the same parent with no intervening siblings). The **general sibling combinator (`A ~ B`)** matches any element `B` that appears **anywhere after `A`** among its siblings, even if other elements sit between them.
</details>

<details>
<summary><strong>3. What is the specificity of an attribute selector like `[type="checkbox"]` compared to a class selector?</strong></summary>

An attribute selector has the **exact same specificity** as a class selector: `[0, 1, 0]`. In the specificity hierarchy, attribute selectors, class selectors, and pseudo-classes all contribute to the middle (B) column.
</details>

<details>
<summary><strong>4. How do the `[attr~="val"]` and `[attr*="val"]` attribute selectors differ?</strong></summary>

- `[attr~="val"]` checks for an **exact whole word match** in a space-separated list of words (e.g., `class~="active"` matches `"nav active item"` but does NOT match `"nav-active"` or `"inactivate"`).
- `[attr*="val"]` checks for a **substring match** anywhere inside the attribute string (e.g., `href*="docs"` matches `"https://example.com/docs/intro"` and `"https://docs.org"`).
</details>

<details>
<summary><strong>5. Why is over-qualifying a selector like `div.container` considered an anti-pattern?</strong></summary>

Over-qualifying selectors creates two distinct problems:
1. It artificially raises the specificity by adding an element tag (`[0, 1, 1]` instead of `[0, 1, 0]`), making it harder to override.
2. It tightly couples CSS to the HTML element type. If the markup is later refactored from a `<div>` to a semantic `<section>` or `<main>`, the style silently breaks.
</details>

<details>
<summary><strong>6. How does the case-sensitivity modifier `i` work in attribute selectors?</strong></summary>

Adding the identifier `i` (or `I`) before the closing square bracket of an attribute selector (e.g., `[href$=".pdf" i]`) forces the browser engine to perform an ASCII case-insensitive comparison of the attribute value. This ensures matching against uppercase or mixed-case values like `.PDF` or `.Pdf`.
</details>

<details>
<summary><strong>7. What happens when an attribute selector is written without a value (e.g., `input[disabled]`)?</strong></summary>

Writing `[attr]` tests for the **presence of the attribute** on the HTML element, regardless of what value it contains (even if empty `disabled=""` or containing arbitrary text). It matches whenever the attribute node exists in the element's DOM attribute map.
</details>

<details>
<summary><strong>8. Why should you avoid universal selectors inside deep descendant chains (e.g., `.card * span`)?</strong></summary>

A universal selector `*` inside a descendant chain forces the browser engine's matcher to evaluate every intermediary ancestor between `.card` and `span`. This breaks bloom filter optimizations and increases style recalculation time on large DOM trees.
</details>

---

## 11. Practice Exercises

### Exercise 1: Select First Paragraph After a Heading (Easy)
**Task**: Write the selector that applies a bold font weight (`600`) and larger font size (`1.125rem`) only to the introductory paragraph that immediately follows an `<h2>` element.

<details>
<summary><strong>View Solution</strong></summary>

```css
h2 + p {
  font-weight: 600;
  font-size: 1.125rem;
}
```
*Explanation*: The adjacent sibling combinator (`+`) targets only the immediate next sibling `<p>`.
</details>

---

### Exercise 2: Target External Telephone and Email Links (Easy)
**Task**: Write attribute selectors that style telephone links (`tel:`) and email links (`mailto:`) with different text colors.

<details>
<summary><strong>View Solution</strong></summary>

```css
a[href^="tel:"] {
  color: #059669; /* Green for phone links */
}

a[href^="mailto:"] {
  color: #7c3aed; /* Purple for email links */
}
```
*Explanation*: The prefix attribute selector `[href^="..."]` matches strings starting with the respective URI scheme.
</details>

---

### Exercise 3: Style Direct Children Only (Medium)
**Task**: Given the HTML structure below, write a rule that adds a 1px border around top-level list items only, without adding borders to nested dropdown sub-items.
```html
<ul class="nav-tree">
  <li>Item 1
    <ul>
      <li>Sub-item 1A</li>
    </ul>
  </li>
  <li>Item 2</li>
</ul>
```

<details>
<summary><strong>View Solution</strong></summary>

```css
.nav-tree > li {
  border: 1px solid #cbd5e1;
  padding: 0.5rem;
  margin-bottom: 0.25rem;
}
```
*Explanation*: Using the child combinator (`>`) ensures only direct children of `.nav-tree` receive the border, avoiding child inheritance down into nested lists.
</details>

---

### Exercise 4: Case-Insensitive Document Extension Styling (Medium)
**Task**: Write a single CSS rule that matches document download links for Word documents (`.doc` and `.docx`), regardless of uppercase or lowercase URL extensions.

<details>
<summary><strong>View Solution</strong></summary>

```css
a[href*=".doc" i] {
  font-weight: 500;
  text-decoration: underline dotted;
}
```
*Explanation*: The substring selector `*=` combined with the `i` flag matches `.doc` and `.docx` across any casing.
</details>

---

### Exercise 5: Accessible Toggle Switch via Checked Attribute (Hard)
**Task**: Build a pure-CSS toggle switch layout where the visual slider styles change when the native checkbox is checked, using sibling combinators.

<details>
<summary><strong>View Solution</strong></summary>

```html
<label class="toggle">
  <input type="checkbox" class="toggle-input">
  <span class="toggle-track"></span>
</label>
```
```css
.toggle {
  position: relative;
  display: inline-block;
  width: 48px;
  height: 24px;
}

.toggle-input {
  opacity: 0;
  width: 0;
  height: 0;
}

.toggle-track {
  position: absolute;
  inset: 0;
  background-color: #cbd5e1;
  border-radius: 9999px;
  cursor: pointer;
  transition: background-color 0.2s ease;
}

/* Style the track when the preceding sibling input is checked */
.toggle-input:checked + .toggle-track {
  background-color: #0284c7;
}

.toggle-input:focus-visible + .toggle-track {
  outline: 2px solid #0284c7;
  outline-offset: 2px;
}
```
*Explanation*: The adjacent sibling combinator (`+`) checks the state of the invisible checkbox and applies visual updates directly to `.toggle-track`.
</details>

---

### Exercise 6: Validate Selector Validity with JavaScript (Hard)
**Task**: Write a JavaScript function `testSelectorValidity(selectorString)` that verifies whether a selector string is syntactically valid in the client browser without throwing an uncaught DOMException.

<details>
<summary><strong>View Solution</strong></summary>

```javascript
/**
 * Tests whether a selector string is valid in the current browser.
 * @param {string} selector
 * @returns {boolean}
 */
function testSelectorValidity(selector) {
  try {
    document.createDocumentFragment().querySelector(selector);
    return true;
  } catch (error) {
    if (error.name === 'SyntaxError') {
      return false;
    }
    throw error;
  }
}

// Test assertions:
console.log(testSelectorValidity('.valid-class > li'));     // true
console.log(testSelectorValidity('a[href$=".pdf" i]'));     // true
console.log(testSelectorValidity('div:broken-selector'));   // false (invalid pseudo)
```
*Explanation*: `querySelector` triggers a `SyntaxError` DOMException when passed a malformed or unsupported selector string, allowing safe detection.
</details>

---

## 12. Summary / Cheat Sheet

```
+-------------------------------------------------------------------------------------------------+
| CSS SELECTORS MASTER CHEAT SHEET                                                                |
+--------------------------+-----------------------------+--------------------+-------------------+
| Selector                 | Syntax Example              | Specificity        | Matching Logic    |
+--------------------------+-----------------------------+--------------------+-------------------+
| Type                     | button, h1                  | [0, 0, 1]          | HTML tag name     |
| Class                    | .card, .btn-primary         | [0, 1, 0]          | class attribute   |
| ID                       | #app, #main-header          | [1, 0, 0]          | id attribute      |
| Attribute Present        | [disabled]                  | [0, 1, 0]          | Attribute exists  |
| Attribute Exact          | [type="email"]              | [0, 1, 0]          | Exact equality    |
| Attribute Prefix         | [href^="https://"]          | [0, 1, 0]          | Starts with       |
| Attribute Suffix         | [href$=".pdf"]              | [0, 1, 0]          | Ends with         |
| Attribute Substring      | [href*="github"]            | [0, 1, 0]          | Contains string   |
| Case Insensitive Flag    | [href$=".pdf" i]            | [0, 1, 0]          | Case-insensitive  |
| Child Combinator         | .menu > li                  | Sum of tokens      | Direct child only |
| Adjacent Sibling         | h2 + p                      | Sum of tokens      | Immediate next sib|
| General Sibling          | img ~ p                     | Sum of tokens      | Any next sibling  |
+--------------------------+-----------------------------+--------------------+-------------------+
```

---

## 13. Further Reading

- [W3C Selectors Level 4 Specification](https://www.w3.org/TR/selectors-4/)
- [MDN Web Docs: CSS Selectors Guide](https://developer.mozilla.org/en-US/docs/Web/CSS/CSS_selectors)
- [MDN Web Docs: Attribute Selectors](https://developer.mozilla.org/en-US/docs/Web/CSS/Attribute_selectors)
- [web.dev: Learn CSS Selectors](https://web.dev/learn/css/selectors)
- [Chromium Engine: How Selector Matching Works](https://chromium.googlesource.com/chromium/src/+/main/third_party/blink/renderer/core/css/README.md)

---

[Previous: 01 - Syntax, Rules & Declarations](01-syntax-rules-and-declarations.md) | [Section Index](README.md) | [Next: 03 - Pseudo-Classes](03-pseudo-classes.md)
