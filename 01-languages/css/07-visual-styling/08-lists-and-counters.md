# Lists, Markers & CSS Counters

| Metadata | Details |
| :--- | :--- |
| **Topic** | List Styling, `::marker` Pseudo-Element, CSS Counters (`counter-increment`, `counters()`), Custom Counter Styles (`@counter-style`), VoiceOver List Role Bug |
| **Spec Level** | CSS Lists and Counters Module Level 3 / CSS Pseudo-Elements Module Level 4 |
| **Baseline Status** | Widely Available (CSS Counters Baseline 2015; `::marker` Baseline 2020; `@counter-style` Baseline 2021) |
| **Browsers Tested** | Chrome 120+, Firefox 122+, Safari 17.2+, Edge 120+ |
| **Tested With** | NVDA, VoiceOver on iOS/macOS, Chromium LayoutNG, W3C CSS Validator |
| **Module** | [07-visual-styling](README.md) |

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

HTML lists (`<ul>`, `<ol>`, `<li>`) provide structural semantics for grouped content, navigation menus, and ordered procedures. While default browser list styling consists of plain black dots and basic arabic numerals, modern CSS transforms lists into rich editorial interfaces:
- **`::marker` Pseudo-Element**: Customizing bullet colors, custom string symbols (emojis, arrows), font weights, and sizes without inserting hacky auxiliary `<span>` tags.
- **Nested Hierarchical Counters**: Generating automated outline numbering schemes (e.g., `1.0`, `1.1`, `1.1.2`) across deeply nested documentation articles via `counter-increment` and `counters()`.
- **Custom `@counter-style` Rules**: Defining localized or branded counting systems (e.g., Roman numerals, circled numbers, custom alphanumeric sequences) directly in CSS.
- **The VoiceOver `list-style: none` Bug**: When WebKit (Safari on macOS/iOS) encounters `list-style: none`, its accessibility engine infers that the author intended to remove list semantics, stripping the list role from the accessibility tree unless protected by `role="list"`.

---

## 2. Core Concepts

### `list-style` Shorthand & Position
```css
list-style: <type> <position>? <image>?;
```
- `list-style-type`: `disc`, `circle`, `square`, `decimal`, `lower-roman`, `none`, etc.
- `list-style-position`:
  - `outside` (Default): Marker sits outside the principal block box, hanging into the margin.
  - `inside`: Marker sits inside the line box, wrapping inline with the text.

### The Modern `::marker` Pseudo-Element
Targeting `li::marker` allows styling the bullet or number directly. Per W3C specification, `::marker` supports a **strictly restricted subset of CSS properties**:
- `color`
- `font-*` properties (`font-family`, `font-size`, `font-weight`, etc.)
- `content` (Can replace default bullets with strings, symbols, or emojis: `content: "👉 ";`)
- `text-combine-upright` & `unicode-bidi`
- `animation` & `transition` (for supported properties)
*Note: Properties like `margin`, `padding`, `background`, and `box-shadow` are completely forbidden on `::marker`*.

### CSS Counters Engine
Counters operate like CSS variables that increment automatically through the DOM tree:
1. `counter-reset: <name> [<initial-value>]?`: Initializes a counter on a parent container (defaults to 0).
2. `counter-increment: <name> [<step>]?`: Increments the counter on each child item (defaults to +1).
3. `counter(<name>, <style>?)`: Evaluates the current integer value formatted with a counter style.
4. `counters(<name>, "<separator>", <style>?)`: Generates nested numbering strings (e.g., `1.2.3`).

---

## 3. How It Works Internally

```
Nested Hierarchical Counter Architecture

<ol class="tree">                         [counter-reset: section 0]
  <li>Section A                           [counter-increment: section -> 1]
    <ol>                                  [counter-reset: section 0 (Nested scope!)]
      <li>Sub-item 1</li>                 [counter-increment: section -> 1.1]
      <li>Sub-item 2</li>                 [counter-increment: section -> 1.2]
    </ol>
  </li>
  <li>Section B                           [counter-increment: section -> 2]
    <ol>
      <li>Sub-item 1</li>                 [counter-increment: section -> 2.1]
    </ol>
  </li>
</ol>
```

### The Safari VoiceOver `list-style: none` Heuristic
WebKit contains an accessibility heuristic: if an author sets `list-style: none;` on a `<ul>` or `<ol>`, the browser engine assumes the list is purely an arbitrary presentation wrapper (such as a horizontal navbar). Consequently, VoiceOver ceases announcing "List, 4 items" and instead treats list items as generic paragraph containers.

**The Fix**: When building semantic lists that need list styles removed visually, explicitly add `role="list"` to the HTML element:
```html
<ul class="clean-list" role="list">
  <li>Item 1</li>
</ul>
```

---

## 4. Syntax & Usage

### Baseline & Feature Support
- **Core Lists & CSS Counters**: Baseline Widely Available.
- **`::marker` Pseudo-Element**: Baseline 2020 (Chrome 86+, Safari 14+, Firefox 68+, Edge 86+).
- **`@counter-style`**: Baseline 2021 (Chrome 91+, Safari 17+, Firefox 33+).

```css
/* Custom Colored Marker */
ul.feature-list li::marker {
  color: #2563eb;
  font-size: 1.25em;
  content: "✦ ";
}

/* Automated Nested Document Outline */
ol.doc-outline {
  counter-reset: outline-counter;
  list-style: none;
  padding-left: 0;
}

ol.doc-outline li {
  counter-increment: outline-counter;
}

ol.doc-outline li::before {
  content: counters(outline-counter, ".") " ";
  font-weight: 700;
  color: #0f172a;
}
```

---

## 5. Practical Examples

### Example 1: Editorial Numbered Steps with Custom Bubble Markers
```html
<ol class="step-guide" role="list">
  <li>
    <h4>Provision Cloud Infrastructure</h4>
    <p>Initialize Terraform configuration and execute remote state locking.</p>
  </li>
  <li>
    <h4>Configure Ingress Controller</h4>
    <p>Deploy Nginx reverse proxy with automated TLS certificate issuance.</p>
  </li>
  <li>
    <h4>Deploy Microservice Clusters</h4>
    <p>Execute rolling Kubernetes deployment across multi-availability zones.</p>
  </li>
</ol>
```

```css
.step-guide {
  list-style: none;
  counter-reset: step-counter;
  padding: 0;
  display: flex;
  flex-direction: column;
  gap: 1.5rem;
}

.step-guide li {
  counter-increment: step-counter;
  position: relative;
  padding-left: 3.5rem;
}

/* Generate custom circle number badge via ::before (since ::marker forbids padding/background) */
.step-guide li::before {
  content: counter(step-counter);
  position: absolute;
  left: 0;
  top: 0;
  width: 2.25rem;
  height: 2.25rem;
  border-radius: 50%;
  background-color: #2563eb;
  color: #ffffff;
  font-weight: 700;
  display: flex;
  align-items: center;
  justify-content: center;
  font-size: 0.875rem;
}

.step-guide h4 {
  margin: 0 0 0.25rem 0;
  font-size: 1.125rem;
}

.step-guide p {
  margin: 0;
  color: #64748b;
  line-height: 1.5;
}
```

### Example 2: Multi-Level Legal Document Outline
```html
<ol class="legal-tree" role="list">
  <li>
    Definitions and Interpretations
    <ol role="list">
      <li>Affiliates and Subcontractors</li>
      <li>Confidential Information</li>
    </ol>
  </li>
  <li>
    Intellectual Property Rights
    <ol role="list">
      <li>Ownership of Work Product</li>
      <li>Open Source Software Exceptions</li>
    </ol>
  </li>
</ol>
```

```css
.legal-tree,
.legal-tree ol {
  list-style: none;
  counter-reset: legal-section;
  padding-left: 1.5rem;
}

.legal-tree {
  padding-left: 0;
}

.legal-tree li {
  counter-increment: legal-section;
  margin-block: 0.5rem;
  line-height: 1.6;
}

.legal-tree li::before {
  content: counters(legal-section, ".") " ";
  font-weight: 700;
  color: #1e3a8a;
  margin-right: 0.5rem;
}
```

### Example 3: Custom Emoji Checkmark Lists via `::marker`
```html
<ul class="checklist" role="list">
  <li>Deterministic reproducible builds</li>
  <li>Cryptographic artifact signing</li>
  <li>Zero-vulnerability container scanning</li>
</ul>
```

```css
.checklist {
  list-style: none;
  padding-left: 1.5rem;
}

.checklist li {
  padding-left: 0.5rem;
  margin-bottom: 0.5rem;
}

.checklist li::marker {
  content: "✅ ";
  font-size: 1rem;
}
```

### Example 4: Custom Roman Numerals via `@counter-style`
```css
/* Creates custom parenthesized uppercase roman numerals: (I), (II), (III) */
@counter-style paren-roman {
  system: additive;
  range: 1 20;
  additive-symbols: 10 X, 9 IX, 5 V, 4 IV, 1 I;
  prefix: "(";
  suffix: ") ";
}

.roman-list {
  list-style-type: paren-roman;
  padding-left: 2rem;
}
```

### Example 5: Custom Counter for Dynamic Card Numbering
```html
<div class="card-grid">
  <article class="data-card"><h3>Ingestion Engine</h3></article>
  <article class="data-card"><h3>Transformation Pipeline</h3></article>
  <article class="data-card"><h3>OLAP Storage</h3></article>
</div>
```

```css
.card-grid {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(240px, 1fr));
  gap: 1.5rem;
  counter-reset: card-index;
}

.data-card {
  counter-increment: card-index;
  padding: 1.5rem;
  background-color: #ffffff;
  border: 1px solid #e2e8f0;
  border-radius: 8px;
  position: relative;
}

.data-card::before {
  content: "Module 0" counter(card-index);
  font-size: 0.75rem;
  font-weight: 700;
  text-transform: uppercase;
  color: #3b82f6;
  letter-spacing: 0.05em;
  display: block;
  margin-bottom: 0.5rem;
}
```

---

## 6. Best Practices

1. **Always Add `role="list"` When Removing `list-style`**: Preserve screen reader list announcements in Safari/WebKit when declaring `list-style: none`.
2. **Use `::before` If You Need Badges or Backgrounds for Numbers**: The `::marker` pseudo-element strictly forbids `background`, `padding`, `border`, and `width`. For rounded bubble numbers, use `::before` paired with CSS counters.
3. **Always Declare `counter-reset` on the Immediate Parent**: Reset the counter on the `<ol>` or `<ul>` wrapper rather than the `<body>` so counter scopes remain modular and isolated.
4. **Use `counters()` with String Separator for Hierarchical Outlines**: Setting `content: counters(item, ".")` automatically produces `1.1`, `1.2`, `1.2.1` across nested trees.
5. **Set Accessible Spacing on Custom Markers**: Ensure markers do not collide with text by adding a space character in the string (`content: "👉 ";` or `margin-right: 0.5rem;`).
6. **Use `@counter-style` for Localized Numbering**: Support regional alphabets (Hebrew, Japanese, Armenian) without hardcoding custom HTML entities.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Correction |
| :--- | :--- | :--- |
| **`list-style: none` stripping VoiceOver list semantics** | WebKit accessibility engine treats unstyled lists as generic `<div>` containers. | Add `role="list"` directly to the `<ul>` or `<ol>`. |
| **Attempting `background` or `padding` on `::marker`** | The W3C spec forbids box model properties on `::marker`; properties are ignored. | Use `::before` with a CSS counter. |
| **Declaring `counter-increment` on the parent container** | Increments the counter once for the whole container rather than per list item. | Put `counter-increment` on the `<li>`. |
| **Forgetting to scope counters** | Nested lists continue incrementing parent counters (e.g., child starts at 2 instead of 1). | Declare `counter-reset` on the nested `<ol>`. |
| **Using `list-style-position: inside` on multi-line text** | Wraps subsequent lines directly underneath the bullet rather than indenting cleanly. | Keep `outside` and adjust `padding-left`. |
| **Hardcoding numbers in HTML (`1.`, `2.`)** | If an author deletes item #2, all subsequent items must be manually renumbered. | Use CSS counters or standard `<ol>`. |

---

## 8. Performance, Accessibility & Browser Support

### VoiceOver & Screen Reader Compatibility
When `role="list"` is properly preserved, screen readers announce:
- Entering the list: *"List, 3 items"*
- Each item: *"Item 1 of 3"*, followed by the content.
- Custom string markers (`content: "✦ "`) are ignored by screen reader item-count engines, preserving accessible navigation.

---

## 9. Comparison / When to Use What

| Feature | Styling Freedom | Box Model Support | Nested Hierarchies | Best Used For |
| :--- | :--- | :--- | :--- | :--- |
| **`list-style-type`** | Very Low (Keywords only) | None | Limited | Simple native bullet lists |
| **`::marker`** | Medium (Color, font, string) | **Forbidden** | Good | Custom colored bullets, simple emoji markers |
| **CSS Counters + `::before`** | **Maximum (Complete CSS)** | **Full box model** | **Excellent (`counters()`)** | Circular step badges, timeline nodes, legal outlines |

---

## 10. Interview Questions

<details>
<summary>1. What is the Safari/WebKit VoiceOver <code>list-style: none</code> bug, and how is it resolved?</summary>

In WebKit (Safari on macOS and iOS), if a developer sets `list-style: none;` on a `<ul>` or `<ol>` element, the browser's accessibility engine infers that the list is being used purely for visual layout (such as a horizontal navbar or card grid). To avoid cluttering screen reader output, WebKit strips the `list` role from the accessibility tree, causing VoiceOver to read list items as plain unstructured text.

**Resolution**: Add an explicit ARIA attribute to the HTML element: `<ul class="clean-list" role="list">`. This forces WebKit to maintain the list semantics in the accessibility tree regardless of CSS styling.
</details>

<details>
<summary>2. What CSS properties are permitted on the <code>::marker</code> pseudo-element, and what properties are strictly forbidden?</summary>

According to the W3C CSS Pseudo-Elements Level 4 specification, `::marker` only supports a restricted set of properties:
- **Allowed**: `color`, `font-*` properties (`font-size`, `font-family`, `font-weight`, etc.), `content`, `text-combine-upright`, `unicode-bidi`, and CSS animations/transitions on supported properties.
- **Forbidden**: All box model properties (`width`, `height`, `margin`, `padding`, `border`, `background`, `box-shadow`, `display`, `position`). Attempting to add background or padding to `::marker` fails silently.
</details>

<details>
<summary>3. What is the difference between <code>counter()</code> and <code>counters()</code> in CSS?</summary>

- `counter(<name>, <style>?)`: Evaluates only the **innermost** counter value of that name in the current scope, returning a single formatted number (e.g., `1`, `2`, `3`).
- `counters(<name>, "<separator>", <style>?)`: Traverses the entire nested chain of counter scopes with that name and concatenates them using the provided separator string, producing hierarchical outline numbers (e.g., `1.1`, `1.2`, `1.2.1`).
</details>

<details>
<summary>4. How does counter scoping work with nested lists in CSS?</summary>

When an element declares `counter-reset: my-counter;`, it creates a **new counter scope** for itself and its descendants. If a nested `<ol>` also declares `counter-reset: my-counter;`, it instantiates an isolated child counter scope that starts back at 0. Descendant `<li>` items increment this child counter without mutating the parent counter, enabling nested outline numbering.
</details>

<details>
<summary>5. How does <code>list-style-position: inside</code> differ from <code>list-style-position: outside</code>?</summary>

- `outside` (default): The marker box is placed outside the principal block box, hanging in the margin. When list text wraps across multiple lines, all lines align cleanly flush with each other.
- `inside`: The marker box is treated as an inline box placed at the very start of the first line of content. When text wraps across multiple lines, subsequent lines wrap directly underneath the bullet marker, rather than indenting.
</details>

<details>
<summary>6. What is <code>@counter-style</code>, and what problem does it solve?</summary>

`@counter-style` is a CSS at-rule that allows developers to author custom counting systems. Instead of being restricted to standard browser keywords (`decimal`, `disc`, `lower-roman`), developers can define:
- Custom glyph symbols (`symbols: ⚀ ⚁ ⚂ ⚃ ⚄ ⚅;`).
- Custom prefixes and suffixes (e.g., `prefix: "("; suffix: ") ";`).
- Additive systems (Roman numerals) or cyclical repeating systems.
</details>

<details>
<summary>7. How can you style a numbered step list with circular colored number badges?</summary>

Because `::marker` does not support `background` or `padding`, you must use CSS Counters with `::before`:
```css
ol { list-style: none; counter-reset: steps; }
li { counter-increment: steps; position: relative; padding-left: 2.5rem; }
li::before {
  content: counter(steps);
  position: absolute; left: 0;
  width: 2rem; height: 2rem; border-radius: 50%;
  background: #2563eb; color: #fff;
  display: flex; align-items: center; justify-content: center;
}
```
</details>

<details>
<summary>8. What happens if you call <code>counter-increment</code> without a preceding <code>counter-reset</code>?</summary>

Per the CSS Lists specification, if `counter-increment` references a counter name that has not been explicitly initialized with `counter-reset`, the browser implicitly initializes a counter with that name on the root `<html>` element with an initial value of 0. However, this creates a single global counter that cannot be reset for independent components.
</details>

---

## 11. Practice Exercises

### Exercise 1: Accessible Custom Checklist via `::marker`
**Goal**: Build a checklist with green checkmark markers that maintains full VoiceOver list announcements.

<details>
<summary>View Solution</summary>

```html
<ul class="task-checklist" role="list">
  <li>Unit test coverage exceeds 90%</li>
  <li>End-to-end integration tests passed</li>
  <li>Lighthouse accessibility score is 100</li>
</ul>
```

```css
.task-checklist {
  list-style: none;
  padding-left: 1.5rem;
}

.task-checklist li::marker {
  content: "✓ ";
  color: #10b981;
  font-weight: 800;
  font-size: 1.1rem;
}
```
</details>

### Exercise 2: Hierarchical Legal Documentation Tree
**Goal**: Use `counters()` to render a 3-level deep outline formatted as `1.0`, `1.1`, `1.1.1`.

<details>
<summary>View Solution</summary>

```css
.outline-tree,
.outline-tree ol {
  list-style: none;
  counter-reset: outline-idx;
  padding-left: 1.5rem;
}

.outline-tree {
  padding-left: 0;
}

.outline-tree li {
  counter-increment: outline-idx;
  margin-block: 0.375rem;
}

.outline-tree li::before {
  content: counters(outline-idx, ".") " ";
  font-weight: 700;
  color: #2563eb;
}
```
</details>

### Exercise 3: Numbered Step Guide with Rounded Badges
**Goal**: Build a process timeline with numbered circular badges using `counter-reset` and `::before`.

<details>
<summary>View Solution</summary>

```css
.timeline-steps {
  list-style: none;
  counter-reset: timeline-count;
  padding: 0;
}

.timeline-steps li {
  counter-increment: timeline-count;
  position: relative;
  padding-left: 3rem;
  margin-bottom: 1.5rem;
}

.timeline-steps li::before {
  content: counter(timeline-count);
  position: absolute;
  left: 0;
  top: 0;
  width: 2rem;
  height: 2rem;
  border-radius: 50%;
  background-color: #0f172a;
  color: #ffffff;
  font-weight: 700;
  font-size: 0.8125rem;
  display: flex;
  align-items: center;
  justify-content: center;
}
```
</details>

### Exercise 4: Custom Roman Counting Style
**Goal**: Define a `@counter-style` rule that formats numbers as bracketed Roman numerals: `[i]`, `[ii]`.

<details>
<summary>View Solution</summary>

```css
@counter-style bracket-roman {
  system: additive;
  range: 1 10;
  additive-symbols: 10 x, 9 ix, 5 v, 4 iv, 1 i;
  prefix: "[";
  suffix: "] ";
}

.bracket-list {
  list-style-type: bracket-roman;
  padding-left: 2rem;
}
```
</details>

### Exercise 5: Auto-Numbered FAQ Accordion
**Goal**: Automatically prefix FAQ question titles with "Q1: ", "Q2: " using CSS counters.

<details>
<summary>View Solution</summary>

```css
.faq-container {
  counter-reset: faq-count;
}

.faq-item {
  counter-increment: faq-count;
  margin-bottom: 1rem;
}

.faq-item h3::before {
  content: "Q" counter(faq-count) ": ";
  color: #2563eb;
}
```
</details>

### Exercise 6: Reversible List Counter
**Goal**: Initialize a countdown list starting at 10 and decrementing downwards.

<details>
<summary>View Solution</summary>

```css
.countdown-list {
  list-style: none;
  counter-reset: countdown 11; /* Resets to 11 so first increment lands on 10 */
  padding-left: 0;
}

.countdown-list li {
  counter-increment: countdown -1; /* Decrements by 1 */
}

.countdown-list li::before {
  content: "T-minus " counter(countdown) "s: ";
  font-weight: 700;
  color: #ef4444;
}
```
</details>

---

## 12. Summary / Cheat Sheet

| Feature | Syntax Example | Key Role |
| :--- | :--- | :--- |
| `::marker` | `li::marker { color: #2563eb; content: "✦ "; }` | Styles list bullet colors and symbols |
| `counter-reset` | `counter-reset: section 0;` | Initializes counter variable on parent container |
| `counter-increment` | `counter-increment: section 1;` | Increments counter variable on child items |
| `counter()` | `content: counter(section);` | Evaluates single counter value in current scope |
| `counters()` | `content: counters(section, ".");`| Evaluates nested hierarchical outline numbers |
| `role="list"` | `<ul role="list">` | Preserves VoiceOver list semantics when styles are removed |

---

## 13. Further Reading
- [W3C CSS Lists and Counters Module Level 3](https://www.w3.org/TR/css-lists-3/)
- [MDN Web Docs: ::marker](https://developer.mozilla.org/en-US/docs/Web/CSS/::marker)
- [MDN Web Docs: Using CSS Counters](https://developer.mozilla.org/en-US/docs/Web/CSS/CSS_counter_styles/Using_CSS_counters)
- [Adrian Roselli: VoiceOver 'list-style: none' Bug](https://adrianroselli.com/2019/02/un-canceling-a-list.html)
- [MDN Web Docs: @counter-style](https://developer.mozilla.org/en-US/docs/Web/CSS/@counter-style)

---

[Previous: 07-transforms-2d-3d.md](07-transforms-2d-3d.md) | [Module README](README.md) | [Next: 09-styling-forms-and-inputs.md](09-styling-forms-and-inputs.md)
