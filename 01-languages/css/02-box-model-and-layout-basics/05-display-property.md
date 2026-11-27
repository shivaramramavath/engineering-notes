# The `display` Property

| Metadata | Details |
|:---|:---|
| **Level** | Foundational to Intermediate |
| **Prerequisites** | `02-box-model-and-layout-basics/01-the-box-model.md`, `02-box-model-and-layout-basics/02-box-sizing-content-vs-border.md` |
| **Estimated Reading Time** | 29 minutes |
| **Status** | Current |
| **Baseline Status** | Widely Available (Classic display values: Universal; Two-value display syntax: Baseline 2023) |
| **Browsers Tested** | Chrome 120+, Firefox 121+, Safari 17.2+, Edge 120+ |
| **Tested With** | W3C CSS Display Module Level 3, Playwright css_runner.js, stylelint |

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

### What is the `display` Property?
The **`display`** property is the master switch of the CSS formatting model. It dictates:
1. **Outer Display Type**: How an element behaves relative to its sibling elements in the parent document flow (e.g., does it stack vertically like a `block` or flow horizontally like `inline` text?).
2. **Inner Display Type**: How an element lays out its own direct children (e.g., using `flow`, `flex`, or `grid` layout algorithms).
3. **Box Generation**: Whether an element generates a box in the Render Tree at all (`display: none`, `display: contents`).

### Why Did the CSS Display Specification Evolve?
Historically, developers used single keywords like `block`, `inline`, and `inline-block`. When Flexbox and Grid arrived, vendors introduced awkward compound keywords like `inline-flex` and `inline-grid`.

To bring mathematical rigor to layout, the **W3C CSS Display Module Level 3** decoupled layout into two explicit axes:
$$\text{display: } \langle\text{outer-display-type}\rangle \quad \langle\text{inner-display-type}\rangle$$
Writing `display: inline flex;` makes the architecture crystal clear: the element behaves like an inline box to its siblings, but behaves as a flex container for its children!

---

## 2. Core Concepts

### 1. Traditional Single-Value Display Types
- **`display: block`**: Starts on a new line and expands horizontally to fill $100\%$ of its container's inline space. Fully respects `width`, `height`, padding, and margins.
- **`display: inline`**: Flows inline with surrounding text like words in a sentence. Does **not** break onto a new line. **Ignores `width`, `height`, and vertical margins**. Vertical padding renders visually but does not push adjacent text lines apart.
- **`display: inline-block`**: The hybrid box model. Flows inline with surrounding text without breaking onto a new line, but behaves as an independent block internally, fully respecting `width`, `height`, padding, and margins.
- **`display: none`**: Removes the element completely from the layout tree and accessibility tree. It occupies zero space on screen and cannot receive keyboard focus.

### 2. Modern Two-Value Display Syntax
Standardized across all modern engines (Baseline 2023):
```
+-------------------------------------------------------------------------+
| TWO-VALUE DISPLAY SYNTAX MATRIX                                         |
+---------------------+---------------------+-----------------------------+
| Modern Two-Value    | Legacy Single-Value | Meaning                     |
+---------------------+---------------------+-----------------------------+
| display: block flow | display: block      | Block outside, flow inside  |
| display: inline flow| display: inline     | Inline outside, text inside |
| display: inline flow-root | display: inline-block | Inline outside, BFC inside|
| display: block flex | display: flex       | Block outside, flex inside  |
| display: inline flex| display: inline-flex| Inline outside, flex inside |
| display: block grid | display: grid       | Block outside, grid inside  |
| display: inline grid| display: inline-grid| Inline outside, grid inside |
+---------------------+---------------------+-----------------------------+
```

### 3. Box Suppression: `display: contents`
An architectural superpower: **`display: contents` removes the element's own box from the render tree entirely, promoting its child elements to become direct children of the parent container!**
- Sibling styling, borders, backgrounds, and padding on the `display: contents` element vanish completely.
- Invaluable for CSS Grid and Flexbox layouts where semantic HTML wrappers (like `<form>`, `<section>`, or `<article>`) would otherwise break direct grid column alignment.

### 4. Block Formatting Context: `display: flow-root`
Generates a block container box that establishes a **new Block Formatting Context (BFC)**. It isolates internal floats and contains child margins without clipping content (unlike legacy `overflow: hidden`).

---

## 3. How It Works Internally

### Box Generation & The Render Tree
During the layout pipeline, the style resolver checks the element's computed `display` value:

```
+--------------------------------------------------------------------+
| DOM ELEMENT NODE (e.g. <div class="widget">)                       |
+--------------------------------------------------------------------+
                                |
                                v
+--------------------------------------------------------------------+
| STYLE RESOLVER: Inspect computed display value                     |
+--------------------------------------------------------------------+
        |                            |                        |
 [display: none]            [display: contents]      [display: flex]
        |                            |                        |
        v                            v                        v
+------------------+        +------------------+    +------------------+
| NO RENDER BOX    |        | UNWRAP CONTAINER |    | GENERATE RENDER  |
| Excluded from    |        | Box deleted;     |    | BOX (RenderFlex) |
| layout & AOM tree|        | children hoisted |    | Direct children  |
| (Zero paint/DOM) |        | to parent tree   |    | become FlexItems |
+------------------+        +------------------+    +------------------+
```

### `display: none` vs `visibility: hidden` vs `opacity: 0`
- **`display: none`**: Element is completely removed from the Render Tree. Takes $0\text{px}$ space, triggers layout reflow when toggled, and is hidden from screen readers.
- **`visibility: hidden`**: The element's box **still occupies its full layout space** and pushes siblings away, but is invisible and ignores pointer events.
- **`opacity: 0`**: The element remains fully in layout, occupies space, is invisible, but **can still receive click events and keyboard focus**!

---

## 4. Syntax & Usage

```
+-------------------------------------------------------------------------+
| Web Platform Baseline: Widely Available                                  |
| - block, inline, inline-block, none: Universal Web Standards.           |
| - flow-root, contents: Widely Available (Baseline 2020).                 |
| - Two-value display syntax: Widely Available (Baseline 2023).           |
+-------------------------------------------------------------------------+
```

### Modern Two-Value Syntax
```css
/* Inline badge that arranges internal icon and text via Flexbox */
.inline-badge {
  display: inline flex;
  align-items: center;
  gap: 0.5rem;
  padding: 0.25rem 0.75rem;
  background-color: #e0f2fe;
  color: #0369a1;
  border-radius: 9999px;
  font-size: 0.875rem;
}
```

### Unwrapping Containers with `display: contents`
```css
/* Removes the <form> wrapper so input fields participate directly in the parent grid */
.grid-form-wrapper {
  display: contents;
}
```

---

## 5. Practical Examples

### Example 1: Inline vs Inline-Block Formatting Contrast
```html
<p>
  Check out this <span class="box-inline">Inline Box</span> and this
  <span class="box-inline-block">Inline-Block Box</span> inside a paragraph.
</p>
```
```css
/* Non-replaced inline element */
.box-inline {
  display: inline;
  padding: 10px;        /* Bleeds vertically into lines above and below! */
  width: 200px;         /* IGNORED! Width has no effect on inline boxes */
  background: #fef08a;
}

/* Hybrid inline-block element */
.box-inline-block {
  display: inline-block;
  padding: 6px 12px;    /* Pushes line height cleanly */
  width: 160px;         /* RESPECTED! Element locks to 160px width */
  background: #bae6fd;
  text-align: center;
  border-radius: 4px;
}
```

### Example 2: Real-World Two-Value Inline Flex Widget
```html
<nav class="action-bar">
  <span>Document Status:</span>
  <button class="status-pill">
    <span class="indicator-dot"></span>
    Live
  </button>
</nav>
```
```css
.action-bar {
  display: block flow;
  padding: 1rem;
  background: #ffffff;
  border-bottom: 1px solid #e2e8f0;
}

/* Modern Two-Value syntax: Behaves inline to text, but flex internally */
.status-pill {
  display: inline flex;
  align-items: center;
  gap: 6px;
  padding: 4px 10px;
  border-radius: 9999px;
  border: 1px solid #cbd5e1;
  background-color: #f8fafc;
  cursor: pointer;
}

.indicator-dot {
  width: 8px;
  height: 8px;
  border-radius: 50%;
  background-color: #10b981;
}

.status-pill:focus-visible {
  outline: 2px solid #0284c7;
  outline-offset: 2px;
}
```

### Example 3: Edge Case — Unwrapping Semantic Form in CSS Grid
```html
<div class="user-grid">
  <div class="grid-header">User Information</div>
  <!-- Semantic <form> wrapper would normally break 2-column grid layout -->
  <form class="sub-form" action="/submit">
    <label for="name">Name:</label>
    <input type="text" id="name">
    <label for="email">Email:</label>
    <input type="email" id="email">
  </form>
</div>
```
```css
.user-grid {
  display: grid;
  grid-template-columns: 120px 1fr;
  gap: 1rem;
  max-width: 500px;
}

.grid-header {
  grid-column: 1 / -1;
  font-weight: 700;
}

/* Unwraps the form! The <label> and <input> become direct grid items */
.sub-form {
  display: contents;
}
```

### Example 4: Failure Case — Screen Reader Blindness via `display: none`
```html
<!-- Anti-Pattern: visually hidden text hidden from screen readers! -->
<button class="icon-search" aria-label="Search">
  <svg width="16" height="16">...</svg>
  <span class="sr-text">Search our documentation</span>
</button>
```
```css
/* Anti-Pattern: display: none completely removes element from Accessibility Tree! */
.sr-text {
  display: none; /* Screen reader ignores this text completely! */
}

/* Production Pattern: Visually Hidden utility */
.visually-hidden {
  position: absolute;
  width: 1px;
  height: 1px;
  padding: 0;
  margin: -1px;
  overflow: hidden;
  clip-path: inset(50%);
  white-space: nowrap;
  border: 0;
}
```
*Failure Analysis*: `display: none` completely deletes the node from the browser's **Accessibility Object Model (AOM)**. Blind users relying on screen readers hear zero text!
*Solution*: Use the `.visually-hidden` clip-path pattern.

### Example 5: Production Clean BFC via `display: flow-root`
```css
.media-container {
  /* Automatically clears child floats and contains child margins */
  display: flow-root;
  padding: 1.5rem;
  background: #ffffff;
  border: 1px solid #e2e8f0;
  border-radius: 8px;
}

.media-thumbnail {
  float: left;
  margin-right: 1.5rem;
  margin-bottom: 1rem;
  width: 120px;
  border-radius: 6px;
}
```
*Playwright Test Runner Verification*:
```javascript
// Test computed display property
const pill = document.querySelector('.status-pill');
const computed = window.getComputedStyle(pill);
console.log('Computed Display:', computed.display); // "inline-flex" (or "inline flex" in modern engines)
```

---

## 6. Best Practices

1. **Use `display: flow-root` Instead of `overflow: hidden` for Clearing Floats**: Avoid the side-effect of clipping dropdown menus and tooltips by using semantic BFC creation.
2. **Never Use `display: none` for Screen-Reader Only Text**: Use the accessible `.visually-hidden` pattern so assistive software can voice accessible instructions.
3. **Adopt Two-Value Syntax in Modern Codebases**: Express layout intentions clearly (`display: inline flex;` or `display: block grid;`).
4. **Use `display: contents` Sparingly and Responsibly**: Test with screen readers when using `display: contents` on semantic lists or tables to verify the accessibility tree is preserved.
5. **Remember `display: inline` Ignores `width` and `height`**: If dimensions fail to apply to a `<span>`, check if you forgot to declare `display: inline-block` or `display: block`.
6. **Prefer Flexbox/Grid Over `display: inline-block` for Grids**: `inline-block` elements are affected by HTML whitespace between tags, introducing phantom $4\text{px}$ gaps that break percentage calculations.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Root Cause | Symptom / Failure | Correct Pattern |
|:---|:---|:---|:---|
| Declaring `width` on an `inline` element | Inline boxes have no configurable width | Width and height declarations are completely ignored. | Set `display: inline-block` or `block`. |
| Ghost $4\text{px}$ gap between `inline-block` elements | Whitespace in HTML source code | Two $50\%$ width `inline-block` columns wrap unexpectedly. | Use Flexbox (`display: flex`) instead. |
| Using `display: none` for accessible labels | Removes node from Accessibility Tree | Screen readers fail to announce button purpose; WCAG violation. | Use `.visually-hidden` clip pattern. |
| Using `display: contents` on buttons or inputs | Replaced elements cannot be unwrapped | Button fails to render or loses click boundaries. | Only use `display: contents` on semantic wrappers (`div`, `form`). |
| Expecting `display: contents` to preserve borders | Box is completely deleted | Declared border, padding, and background vanish. | Borders must be styled on child elements. |
| Forgetting `line-height` impact on `inline-block` | Inline baseline alignment | Baseline alignment shifts neighboring text oddly. | Add `vertical-align: middle` or `top`. |

---

## 8. Performance, Accessibility & Browser Support

### Layout Pipeline Cost of Toggling `display`
- Toggling `display: none` to `display: block` destroys and rebuilds the node's **LayoutBox** in C++ memory, forcing a complete **Style Recalculation, Layout Reflow, and Repaint** across the subtree.
- If you simply need to hide an element visually while preserving layout geometry, toggle `visibility: hidden` or `opacity: 0` for significantly faster frame rates.

### Accessibility Tree Preservation
- Early implementations of `display: contents` (prior to 2021) had bugs in certain browsers that stripped semantic roles from `<table>` and `<ul>` elements. Modern engines have resolved this, but caution is recommended when unwrapping critical semantic landmarks.

---

## 9. Comparison / When to Use What

| Value | Generates Box? | Outer Behavior | Inner Behavior | Width/Height Allowed? |
|:---|:---:|:---|:---|:---:|
| **`block`** | Yes | Block (New line, 100% width) | Flow | **YES** |
| **`inline`** | Yes | Inline (Flows with text) | Inline Text | **NO** |
| **`inline-block`** | Yes | Inline (Flows with text) | Flow-Root (BFC) | **YES** |
| **`flex`** | Yes | Block | Flexbox | **YES** |
| **`inline flex`** | Yes | Inline | Flexbox | **YES** |
| **`grid`** | Yes | Block | CSS Grid | **YES** |
| **`contents`** | **NO (Unwrapped)**| None (Children promoted)| Children inherit | No (Box deleted) |
| **`none`** | **NO (Hidden)** | Excluded from render tree | Excluded | No |

---

## 10. Interview Questions

<details>
<summary><strong>1. Explain the architectural difference between an element's "outer display type" and "inner display type".</strong></summary>

In the CSS Display Module Level 3:
- The **outer display type** dictates how the element itself interacts with its sibling elements in the parent's formatting context (e.g., `block` forces the element onto a new line and expands inline, while `inline` flows smoothly alongside text).
- The **inner display type** dictates the formatting context established for the element's direct children (e.g., `flow` arranges children in normal document flow, while `flex` or `grid` activates specialized layout algorithms).
The modern two-value syntax (`display: inline flex;`) makes both axes explicit.
</details>

<details>
<summary><strong>2. What is `display: contents`, and what happens to an element's borders, backgrounds, and padding when applied?</strong></summary>

`display: contents` suppresses the generation of the element's own principal box in the Render Tree. The element itself is "unwrapped":
- Any `border`, `background`, `padding`, or `margin` declared directly on the element **are not rendered at all**.
- The element's child elements are projected upward to become direct children of the parent container's layout context (extremely useful for CSS Grid and Flexbox).
- However, the element still exists in the DOM tree, can respond to JavaScript event bubbling, and can pass inherited properties (like `color`) down to its children.
</details>

<details>
<summary><strong>3. Why does `display: none` impact accessibility differently than `visibility: hidden`?</strong></summary>

- **`display: none`**: Completely removes the element from both the visual Render Tree and the browser's **Accessibility Object Model (AOM)**. Assistive technologies (screen readers like NVDA and VoiceOver) treat the element as if it does not exist, ignoring its text entirely.
- **`visibility: hidden`**: Hides the element visually and silences it from screen readers, but **preserves its exact physical layout box** on screen, reserving space and pushing neighboring elements away.
</details>

<details>
<summary><strong>4. Why do `inline-block` elements have a phantom 4px horizontal gap between them in HTML?</strong></summary>

Because `inline-block` elements participate in an inline formatting context, the browser engine treats them like words in a sentence. Any whitespace (spaces, tabs, newlines) between the closing `</div>` and the opening `<div>` in your HTML source code is tokenized as a single space character, which is rendered as approximately $4\text{px}$ of spacing (depending on font size). Flexbox eliminates this problem entirely because flex containers do not render inter-tag whitespace.
</details>

<details>
<summary><strong>5. What does `display: flow-root` do, and why was it introduced?</strong></summary>

`display: flow-root` generates a block container box that establishes a **new Block Formatting Context (BFC)**. It was introduced to provide a clean, semantic replacement for the "clearfix" hack and the `overflow: hidden` hack. A `flow-root` container automatically contains internal floating children and contains vertical child margins, without clipping dropdowns or shadows.
</details>

<details>
<summary><strong>6. Can an inline element with `display: inline` have margins?</strong></summary>

An inline element **can have horizontal margins** (`margin-left`, `margin-right`, `margin-inline`), which push neighboring inline words away. However, vertical margins (`margin-top`, `margin-bottom`, `margin-block`) are completely ignored by the layout engine and have zero effect on inline elements.
</details>

<details>
<summary><strong>7. How does the browser layout engine handle `display: none` during style invalidation?</strong></summary>

When an element's computed display property transitions to `none`, the layout engine detaches the element's `LayoutObject` from the layout tree, destroys its GPU composited layer (if present), and schedules a Layout pass on the parent container to close the vacated space.
</details>

<details>
<summary><strong>8. What is `display: list-item`?</strong></summary>

`display: list-item` generates a principal block box along with an automatic `::marker` pseudo-element box (displaying the bullet or number). It is the default display mode of HTML `<li>` elements and can be applied to any HTML element to give it list item capabilities.
</details>

---

## 11. Practice Exercises

### Exercise 1: Build an Inline-Flex Badge (Easy)
**Task**: Build a `.pill-tag` class that flows inline with text, contains an icon and a label separated by a 6px gap using Flexbox, and has a 20px height with rounded corners.

<details>
<summary><strong>View Solution</strong></summary>

```css
.pill-tag {
  display: inline-flex; /* Or modern: display: inline flex; */
  align-items: center;
  gap: 6px;
  height: 20px;
  padding: 0 8px;
  border-radius: 9999px;
  background-color: #e0f2fe;
  color: #0369a1;
  font-size: 0.75rem;
  font-weight: 600;
  vertical-align: middle;
}
```
*Explanation*: `inline-flex` allows the badge to sit inside text while utilizing Flexbox alignment internally.
</details>

---

### Exercise 2: Accessible Visually-Hidden Utility (Easy)
**Task**: Write the canonical `.visually-hidden` CSS class that hides text visually from sighted users while keeping it fully announced by screen readers.

<details>
<summary><strong>View Solution</strong></summary>

```css
.visually-hidden {
  position: absolute;
  width: 1px;
  height: 1px;
  padding: 0;
  margin: -1px;
  overflow: hidden;
  clip-path: inset(50%);
  white-space: nowrap;
  border: 0;
}
```
*Explanation*: Avoids `display: none` so the element remains intact in the Accessibility Tree.
</details>

---

### Exercise 3: Unwrap Semantic Wrapper in Flexbox (Medium)
**Task**: An author has `<div class="flex-row"><div class="auth-wrapper"><button>Login</button><button>Register</button></div></div>`. Use `display: contents` to allow both buttons to become direct flex items of `.flex-row`.

<details>
<summary><strong>View Solution</strong></summary>

```css
.flex-row {
  display: flex;
  gap: 1rem;
}

.auth-wrapper {
  /* Unwraps the wrapper box completely */
  display: contents;
}
```
*Explanation*: `display: contents` deletes the `.auth-wrapper` box from layout, promoting both buttons to direct flex items.
</details>

---

### Exercise 4: Clean Float Clearing via `display: flow-root` (Medium)
**Task**: An article card contains a floating image: `<div class="card"><img class="float-img"> <p>Text...</p></div>`. The image extends beyond the bottom border of `.card` (collapsing parent). Fix it cleanly without using clearfix hacks or `overflow: hidden`.

<details>
<summary><strong>View Solution</strong></summary>

```css
.card {
  /* Modern clean BFC: stretches card height to enclose the float */
  display: flow-root;
  padding: 1.5rem;
  border: 1px solid #cbd5e1;
}

.float-img {
  float: left;
  margin-right: 1rem;
  width: 100px;
}
```
*Explanation*: `display: flow-root` creates a Block Formatting Context that intrinsically contains floats.
</details>

---

### Exercise 5: Two-Value Display Syntax Modernization (Hard)
**Task**: Rewrite the following legacy CSS declarations using modern two-value display syntax:
1. `display: inline-block;`
2. `display: inline-flex;`
3. `display: inline-grid;`

<details>
<summary><strong>View Solution</strong></summary>

1. `display: inline-block;` &rarr; `display: inline flow-root;`
2. `display: inline-flex;` &rarr; `display: inline flex;`
3. `display: inline-grid;` &rarr; `display: inline grid;`
*Explanation*: Separates the outer inline flow behavior from the inner formatting context.
</details>

---

### Exercise 6: Programmatic Display State Inspection in JavaScript (Hard)
**Task**: Write a JavaScript function `isRenderedInLayout(element)` that verifies whether an element is actively generating a layout box (i.e. not `display: none` and not `display: contents`).

<details>
<summary><strong>View Solution</strong></summary>

```javascript
/**
 * Detects whether an element generates a physical render box in the layout tree.
 * @param {HTMLElement} el
 * @returns {boolean}
 */
function isRenderedInLayout(el) {
  if (!el || !el.isConnected) return false;

  const style = window.getComputedStyle(el);
  const display = style.display;

  if (display === 'none' || display === 'contents') {
    return false;
  }

  // Also check if any ancestor has display: none
  let parent = el.parentElement;
  while (parent) {
    if (window.getComputedStyle(parent).display === 'none') {
      return false;
    }
    parent = parent.parentElement;
  }

  return true;
}

const badge = document.querySelector('.pill-tag');
console.log('Is rendered in layout:', isRenderedInLayout(badge)); // true
```
*Explanation*: Checks both the element's computed display property and its ancestor tree.
</details>

---

## 12. Summary / Cheat Sheet

```
+-------------------------------------------------------------------------------------------------+
| CSS DISPLAY PROPERTY MASTER CHEAT SHEET                                                         |
+-------------------+----------------------------+-----------------------+------------------------+
| Syntax            | Outer Display              | Inner Display         | Core Behavioral Rule   |
+-------------------+----------------------------+-----------------------+------------------------+
| block (block flow)| Block (100% width, newline)| Flow Content          | Standard block element |
| inline(inline flow| Inline (Flows with text)   | Flow Content          | Ignores width & height |
| inline-block      | Inline                     | Flow-Root (BFC)       | Respects width/height  |
| flex (block flex) | Block                      | Flex Container        | Direct children flex   |
| inline-flex       | Inline                     | Flex Container        | Sits inline with text  |
| grid (block grid) | Block                      | Grid Container        | Two-dimensional grid   |
| flow-root         | Block                      | Flow-Root (New BFC)   | Contains floats & marg |
| contents          | None (Box deleted!)        | Promoted children     | Unwraps container      |
| none              | None (Removed from tree)   | None                  | Hidden from AOM and DOM|
+-------------------+----------------------------+-----------------------+------------------------+
```

---

## 13. Further Reading

- [W3C CSS Display Module Level 3](https://www.w3.org/TR/css-display-3/)
- [MDN Web Docs: display](https://developer.mozilla.org/en-US/docs/Web/CSS/display)
- [web.dev: The new two-value display syntax](https://web.dev/articles/display-two-values)
- [Rachel Andrew: Digging into the Display Property](https://www.smashingmagazine.com/2019/04/display-two-value/)
- [W3C CSS Box Model 3: Block Formatting Contexts](https://www.w3.org/TR/css-box-3/#block-formatting-context)

---

[Previous: 04 - Margin Collapsing: Rules & Workarounds](04-margin-collapsing.md) | [Section Index](README.md) | [Next: 06 - Block vs Inline Formatting Contexts (BFC / IFC)](06-inline-vs-block-formatting-contexts.md)
