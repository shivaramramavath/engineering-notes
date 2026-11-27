# Inheritance: `initial`, `inherit`, `unset`, `revert`

| Metadata | Details |
|:---|:---|
| **Level** | Foundational to Intermediate |
| **Prerequisites** | `01-fundamentals/01-syntax-rules-and-declarations.md`, `01-fundamentals/06-cascade-origins-importance-and-order.md` |
| **Estimated Reading Time** | 28 minutes |
| **Status** | Current |
| **Baseline Status** | Widely Available (`initial`, `inherit`, `unset`: Universal; `revert`: Baseline 2020; `revert-layer`: Baseline 2022) |
| **Browsers Tested** | Chrome 120+, Firefox 121+, Safari 17.2+, Edge 120+ |
| **Tested With** | W3C CSS Cascading and Inheritance Level 5, Playwright css_runner.js, stylelint |

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

### What is Value Inheritance?
**Inheritance** is the mechanism by which certain CSS property values propagate down the DOM tree from an ancestor element to its descendants. When no CSS rule targets a specific element for a property, the browser must decide what value to assign:
- If the property is **naturally inherited** (e.g., `color`, `font-family`), the element adopts its parent's computed value.
- If the property is **non-inherited** (e.g., `margin`, `padding`, `border`), the element receives the property's specification default.

### Why Do Universal Value Keywords Exist?
CSS provides five universal keywords that can be assigned to **any CSS property**:
1. `inherit`
2. `initial`
3. `unset`
4. `revert`
5. `revert-layer`

These keywords grant authors absolute control over value resolution, enabling clean component resets, defensive styling against third-party overrides, and surgical rollback through the cascade origins.

---

## 2. Core Concepts

### 1. Naturally Inherited vs. Non-Inherited Properties

```
+-------------------------------------------------------------------------+
| INHERITANCE PROFILE MATRIX                                              |
+--------------------------+----------------------------------------------+
| Category                 | Properties Included                          |
+--------------------------+----------------------------------------------+
| Naturally Inherited      | color, font-family, font-size, font-weight,   |
| (Typography & Visibility)| line-height, letter-spacing, text-align,     |
|                          | visibility, cursor, list-style, quotes       |
+--------------------------+----------------------------------------------+
| Non-Inherited            | margin, padding, border, background, width,  |
| (Box Model & Layout)     | height, min-width, display, position, top,   |
|                          | overflow, z-index, transform, opacity        |
+--------------------------+----------------------------------------------+
```
*Design Logic*: You want child paragraphs to inherit the document font family automatically; you do *not* want a child paragraph to inherit the 50px margin or 2px border of its parent container!

### 2. Custom Properties (`--token`) Inherit Universally
CSS Custom Properties (Variables) **inherit by default**. If you declare `--card-bg: white;` on `:root` or `.card`, every nested child element has access to `--card-bg` unless shadowed by a closer ancestor.

### 3. The 5 Universal Value Keywords
- **`inherit`**: Explicitly forces an element to take the computed value of its direct parent, even for non-inherited properties (`border: inherit;`).
- **`initial`**: Sets the property to the **official W3C specification initial default**. Note: this is often NOT what you expect (`display: initial` is `inline`, even on a `<div>`!).
- **`unset`**: Acts as a chameleon:
  - If the property is naturally inherited &rarr; acts like `inherit`.
  - If the property is non-inherited &rarr; acts like `initial`.
- **`revert`**: Rolls back the cascade to the value defined in the **User Agent (browser default)** or User stylesheet. (e.g., `display: revert` makes `<div>` a `block` and `<span>` an `inline`!).
- **`revert-layer`**: Rolls back the value to the previous **Cascade Layer** (`@layer`) within the Author origin.

### 4. The Universal Reset: `all`
The `all` shorthand property applies to all CSS properties at once (except `direction` and `unicode-bidi`):
```css
.unstyled-button {
  all: unset; /* Strips browser borders, backgrounds, padding, and font styles */
  cursor: pointer;
}
```

---

## 3. How It Works Internally

### The 4-Stage Value Processing Pipeline
Every CSS property on every DOM element progresses through four distinct value phases in the browser engine's C++ style resolver:

```
+--------------------------------------------------------------------+
| 1. SPECIFIED VALUE                                                 |
| Value declared in stylesheet (e.g., "1.5rem", "inherit", "auto")   |
+--------------------------------------------------------------------+
                                |
                                v
+--------------------------------------------------------------------+
| CASCADE RESOLUTION                                                 |
| If no specified value exists:                                      |
| - Is property inherited? -> Inherit parent's Computed Value        |
| - Property NOT inherited? -> Use Property Initial Value Table      |
+--------------------------------------------------------------------+
                                |
                                v
+--------------------------------------------------------------------+
| 2. COMPUTED VALUE                                                  |
| Relative units resolved (e.g., "1.5rem" -> 24px; colors to RGBA)   |
| *This is the exact value inherited by child DOM elements!*        |
+--------------------------------------------------------------------+
                                |
                                v
+--------------------------------------------------------------------+
| 3. USED VALUE                                                      |
| Layout geometry calculated (e.g., "width: auto" -> 412.5px)       |
+--------------------------------------------------------------------+
                                |
                                v
+--------------------------------------------------------------------+
| 4. ACTUAL VALUE                                                    |
| Device pixel snapping applied (e.g., rounded to physical pixels)   |
+--------------------------------------------------------------------+
```

### Inheritance Passes the *Computed Value*, Not the Formula!
A vital distinction: when an element inherits from its parent, it inherits the **Computed Value** (the resolved pixel length), **never the specified relative unit**:
- Parent has `font-size: 16px; line-height: 1.5;` &rarr; Parent's computed `line-height` is `24px`.
- If parent had unitless `line-height: 1.5`, the child inherits the number `1.5` and recalculates based on its *own* font-size.
- But if parent had `line-height: 150%` or `line-height: 1.5em`, the computed `24px` is passed down, causing cramped text if the child has a larger `font-size`!

---

## 4. Syntax & Usage

```
+-------------------------------------------------------------------------+
| Web Platform Baseline: Widely Available                                  |
| inherit, initial, unset are universal across all web browsers.          |
| revert is Widely Available (Baseline 2020 - Chrome 84, FF 67, Safari 9.1)|
| revert-layer is Widely Available (Baseline 2022).                       |
+-------------------------------------------------------------------------+
```

### Universal Keywords Syntax
```css
/* Force child to share parent border */
.card-header {
  border-bottom: inherit;
}

/* Reset property to W3C spec default */
.highlight {
  color: initial; /* Resets to CanvasText / black */
}

/* Unset typography vs layout */
.quote-box {
  all: unset; /* Resets typography to inherit, layout to initial */
}

/* Roll back to native browser UI */
.custom-input {
  appearance: revert;
  display: revert;
}
```

---

## 5. Practical Examples

### Example 1: Basic Border Inheritance via `inherit`
```html
<div class="card">
  <div class="card-body">
    Card content
  </div>
</div>
```
```css
.card {
  border: 2px solid #0284c7;
  padding: 1rem;
}

/* Borders are not naturally inherited; use inherit to match parent border */
.card-body {
  border-left: inherit; /* Copies the 2px solid #0284c7 border */
  padding-left: 1rem;
}
```

### Example 2: Real-World Unstyled Button with `all: unset`
Native `<button>` elements ship with aggressive User Agent styling (gray gradient, beveled border, padding). Reset them cleanly:
```html
<button type="button" class="icon-btn" aria-label="Settings">
  ⚙️
</button>
```
```css
.icon-btn {
  all: unset; /* Completely eliminates native button border, bg, and padding */
  display: inline-flex;
  align-items: center;
  justify-content: center;
  width: 40px;
  height: 40px;
  border-radius: 50%;
  cursor: pointer;
  transition: background-color 0.15s ease;
}

.icon-btn:hover {
  background-color: #f1f5f9;
}

.icon-btn:focus-visible {
  outline: 2px solid #0284c7;
  outline-offset: 2px;
}
```

### Example 3: Edge Case — `initial` vs `revert` on Display
A notorious trap for web developers:
```html
<div class="container">
  <div class="box-a">Box A</div>
  <div class="box-b">Box B</div>
</div>
```
```css
/* DANGEROUS: W3C initial for 'display' is 'inline'! */
.box-a {
  display: initial; /* Causes the <div> to render as an INLINE element! */
}

/* CORRECT: Reverts to User Agent default for <div> ('block') */
.box-b {
  display: revert; /* Correctly renders as a block element */
}
```
*Verification*: In DevTools Computed pane, `.box-a` has `display: inline`, breaking grid/block layouts. `.box-b` correctly resolves to `display: block`.

### Example 4: Failure Case — `color: initial` Breaking in Dark Mode
```css
:root {
  color-scheme: dark;
  background-color: #0f172a;
  color: #f8fafc; /* White text for dark mode */
}

/* Anti-pattern: Developer wants to "reset" text color */
.subtext {
  color: initial; /* Spec initial for color is black (#000000)! */
}
```
*Failure Analysis*: The `.subtext` text turns pure black (`#000000`) on a dark slate background (`#0f172a`), creating an unreadable contrast violation.
*Correct Pattern*:
```css
/* Use 'inherit' or 'unset' to preserve dark theme font color */
.subtext {
  color: inherit;
  opacity: 0.8;
}
```

### Example 5: Production-Grade Form Reset via `revert`
When building custom inputs, you sometimes need to reset an input back to its operating system default state:
```css
.legacy-form input[type="text"] {
  /* Discard bespoke author styling and revert to OS platform styling */
  all: revert;
  margin: 0.5rem 0;
}
```
*Playwright Test Runner Verification*:
```javascript
// Test computed values
const btn = document.querySelector('.icon-btn');
const computed = window.getComputedStyle(btn);
console.log('Padding:', computed.padding); // "0px"
console.log('Cursor:', computed.cursor);   // "pointer"
```

---

## 6. Best Practices

1. **Use Unitless Numbers for `line-height`**: Always declare `line-height: 1.5;` rather than `line-height: 150%;` or `1.5em` so child elements inherit the ratio and recalculate based on their own `font-size`.
2. **Prefer `revert` Over `initial` for Resets**: `initial` frequently introduces unexpected defaults (e.g., `display: inline` on `<div>`). `revert` restores familiar User Agent browser defaults.
3. **Use `all: unset` for Clean Interactive Primitives**: Strip native button and list styles with `all: unset;` before applying design system classes.
4. **Always Pair `all: unset` on Buttons with `:focus-visible`**: Removing all styles strips the default focus ring; immediately supply an accessible `:focus-visible` outline.
5. **Leverage CSS Custom Properties for Configurable Inheritance**: Custom properties inherit through the entire DOM tree, making them ideal for cascading themes (`--surface-color`).
6. **Use `color: inherit` for Links Inside Headings**: By default, browsers give `<a>` an author or UA blue color. Make it blend seamlessly with heading text via `color: inherit`.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Root Cause | Symptom / Failure | Correct Pattern |
|:---|:---|:---|:---|
| Using `display: initial` to reset an element | Spec defines initial display as `inline` | `<div>` or `<section>` collapses into an inline element, breaking layout. | Use `display: revert` or `display: block`. |
| Using percentage `line-height: 150%` on `body` | Percentage computed at root and passed down | Child headers with large font sizes overlap lines due to tiny inherited line height. | Use unitless line-height: `line-height: 1.5`. |
| Using `color: initial` in dark themes | Initial color is black (`CanvasText`) | Black text rendered invisibly against dark backgrounds. | Use `color: inherit` or design token variables. |
| Forgetting that `margin` and `padding` do not inherit | Box model properties are non-inherited | Expecting children to automatically center or pad like parent. | Explicitly declare padding or use flexbox/grid gap. |
| Stripping button styles with `all: unset` without cursor | `all: unset` resets cursor to `default` | Custom button does not show pointer cursor on hover. | Explicitly add `cursor: pointer;`. |
| Confusing `unset` with `initial` | Misunderstanding the chameleon behavior | Inherited properties (like `color`) continue to inherit instead of resetting to default. | Use `initial` if you truly want the spec default. |

---

## 8. Performance, Accessibility & Browser Support

### Inheritance Engine Optimization
Browser engines (Blink, Gecko, WebKit) optimize inheritance using **Inherited Style Sharing**:
- Nodes that inherit all styles from their parent without local overrides share the exact same `ComputedStyle` memory buffer instance in C++.
- This avoids allocating redundant memory blocks for thousands of text nodes in large documents.

### Accessibility Safeguards
- Stripping styles with `all: unset` removes native high-contrast borders and keyboard focus indicators on form controls. Always test with keyboard navigation to verify focus indicators are reinstated.

---

## 9. Comparison / When to Use What

| Keyword | Naturally Inherited Property | Non-Inherited Property | Typical Best Use Case |
|:---|:---|:---|:---|
| **`inherit`** | Inherits parent computed value | Inherits parent computed value | Forcing `border`, `background`, or `color` from parent |
| **`initial`** | Sets to W3C spec default | Sets to W3C spec default | Pure isolation (caution: `display` becomes `inline`) |
| **`unset`** | Acts as `inherit` | Acts as `initial` | General clean reset via `all: unset` |
| **`revert`** | Rolls back to UA/User default | Rolls back to UA/User default | Restoring native element semantics (`display: block`) |
| **`revert-layer`**| Rolls back to previous `@layer` | Rolls back to previous `@layer` | Multi-tier component overrides in `@layer` |

---

## 10. Interview Questions

<details>
<summary><strong>1. What is the crucial difference between `display: initial` and `display: revert` on a `<div>` element?</strong></summary>

The W3C specification defines the `initial` value of the `display` property as `inline` for **all** elements. Therefore, writing `div { display: initial; }` will force the `<div>` to behave as an inline element (like a `<span>`), collapsing its width and height. In contrast, `display: revert` rolls back the cascade to the **User Agent stylesheet**, where the browser's default rule `div { display: block; }` is restored.
</details>

<details>
<summary><strong>2. Explain why unitless `line-height: 1.5` is vastly preferred over percentage `line-height: 150%` on ancestor elements.</strong></summary>

Inheritance passes down the **Computed Value**, not the raw declaration:
- When you declare `body { font-size: 16px; line-height: 150%; }`, the browser computes $16\text{px} \times 1.5 = 24\text{px}$. The fixed pixel value **`24px`** is then inherited by all child elements. If a child `<h1>` has `font-size: 48px`, its line height remains `24px`, causing large text to violently overlap.
- When you declare `line-height: 1.5;` (unitless), the browser passes the raw multiplier **`1.5`** down the inheritance tree. The `<h1>` computes its line height as $48\text{px} \times 1.5 = 72\text{px}$, scaling proportionally.
</details>

<details>
<summary><strong>3. How does the `unset` keyword decide whether to inherit or reset to initial?</strong></summary>

`unset` is defined as a contextual keyword:
- If the target property is **naturally inherited** (such as `color`, `font-family`, or `visibility`), `unset` acts identically to **`inherit`**.
- If the target property is **non-inherited** (such as `margin`, `padding`, or `border`), `unset` acts identically to **`initial`**.
</details>

<details>
<summary><strong>4. What does `all: unset` do on an HTML `<button>` element?</strong></summary>

`all: unset` resets every CSS property on the button:
- Non-inherited properties (`background`, `border`, `padding`, `margin`, `box-shadow`) reset to their specification initial values (transparent background, no borders, 0 padding).
- Inherited properties (`color`, `font-family`, `font-size`) inherit directly from the button's parent element.
- The button effectively becomes a plain text container without button styling, requiring authors to explicitly define dimensions, padding, cursor, and focus indicators.
</details>

<details>
<summary><strong>5. Which properties are excluded from the `all` shorthand property?</strong></summary>

The `all` shorthand resets all properties **except `direction` and `unicode-bidi`**. These two properties control writing direction (LTR vs RTL) and bidirectional text algorithms, which are fundamental to document language parsing and could break internationalized text if reset indiscriminately.
</details>

<details>
<summary><strong>6. Do CSS Custom Properties (`--my-var`) inherit by default?</strong></summary>

**Yes, absolutely.** Custom properties are defined by the W3C specification to inherit down the entire DOM tree by default. A custom property declared on `:root` or an ancestor container is accessible by every child element inside its subtree unless explicitly shadowed by a closer declaration.
</details>

<details>
<summary><strong>7. What happens when you declare `border: inherit;` on a child element?</strong></summary>

Because `border` is not a naturally inherited property, children do not have borders by default. Writing `border: inherit;` explicitly forces the child element to copy the computed border width, border style, and border color directly from its immediate parent container.
</details>

<details>
<summary><strong>8. How does `revert-layer` differ from `revert`?</strong></summary>

- `revert` traverses up the **Cascade Origins** hierarchy (rolling back Author styles to the User or User Agent stylesheet).
- `revert-layer` traverses up the **Cascade Layers** hierarchy within the *same* Author origin. It rolls back the property to whatever value was declared in the previous cascade layer. If no previous layer declared a value, it falls back to `revert`.
</details>

---

## 11. Practice Exercises

### Exercise 1: Inherit Text Color on Hyperlinks (Easy)
**Task**: In many browsers, `<a>` tags default to blue. Write a single-property CSS rule that forces all links inside `.article-body h2` to match the exact computed text color of their parent heading.

<details>
<summary><strong>View Solution</strong></summary>

```css
.article-body h2 a {
  color: inherit;
  text-decoration: underline;
}
```
*Explanation*: `color: inherit` tells the link to discard default link colors and take the heading's computed color.
</details>

---

### Exercise 2: Reset an Unstyled List with `all: unset` (Easy)
**Task**: Write the class `.nav-list` that removes all default bullets, margins, and padding from a `<ul>` element using `all: unset;`, and then configures it as a horizontal flex row with a 1rem gap.

<details>
<summary><strong>View Solution</strong></summary>

```css
.nav-list {
  all: unset;
  display: flex;
  gap: 1rem;
  list-style: none;
}
```
*Explanation*: `all: unset` strips default browser margins and paddings, allowing clean flex configuration.
</details>

---

### Exercise 3: Fix Overlapping Line Height Inheritance (Medium)
**Task**: An application declares `body { font-size: 16px; line-height: 24px; }`. A developer notices that headings `h1 { font-size: 40px; }` look horribly cramped because lines overlap. Fix the issue on the `body` declaration so all descendants inherit a healthy, proportional line height.

<details>
<summary><strong>View Solution</strong></summary>

```css
body {
  font-size: 16px;
  /* Change fixed 24px length to proportional unitless multiplier */
  line-height: 1.5;
}
```
*Explanation*: Using unitless `1.5` allows `h1` to inherit the multiplier, computing $40\text{px} \times 1.5 = 60\text{px}$ line-height.
</details>

---

### Exercise 4: Restore Browser Default Form Inputs with `revert` (Medium)
**Task**: A legacy global reset broke all text inputs: `input { border: none; background: transparent; }`. Write a selector for `.standard-form input[type="text"]` that restores default browser borders and backgrounds using the `revert` keyword.

<details>
<summary><strong>View Solution</strong></summary>

```css
.standard-form input[type="text"] {
  border: revert;
  background: revert;
}
```
*Explanation*: `revert` rolls back to the browser's User Agent stylesheet (`html.css`), restoring default beveled/inset borders.
</details>

---

### Exercise 5: Test Value Calculation Order in JavaScript (Hard)
**Task**: Write a JavaScript snippet that inspects a child element `<div class="child">` nested inside `<div class="parent">` with `font-size: 20px; line-height: 2;`. Verify that the child's computed line-height is proportional when its font-size is 30px.

<details>
<summary><strong>View Solution</strong></summary>

```javascript
// Verification script in Playwright / Browser DOM:
const parent = document.createElement('div');
parent.style.fontSize = '20px';
parent.style.lineHeight = '2';

const child = document.createElement('div');
child.style.fontSize = '30px';

parent.appendChild(child);
document.body.appendChild(parent);

const childComputed = window.getComputedStyle(child);
console.log('Child Computed Line-Height:', childComputed.lineHeight);
// Resolves to: "60px" (30px * 2)

// Clean up
document.body.removeChild(parent);
```
*Explanation*: Confirms that unitless multipliers calculate line-height relative to the child's own font-size.
</details>

---

### Exercise 6: Revert Layer Cascade Step-Down (Hard)
**Task**: Build a 3-layer architecture (`base`, `theme`, `overrides`). Configure `.badge` with background `#3b82f6` in `theme`, `#10b981` in `overrides`, and demonstrate using `revert-layer` on `.badge--default` to roll back to `theme`.

<details>
<summary><strong>View Solution</strong></summary>

```css
@layer base, theme, overrides;

@layer base {
  .badge {
    padding: 4px 8px;
    border-radius: 4px;
    background-color: #e2e8f0;
  }
}

@layer theme {
  .badge {
    background-color: #3b82f6; /* Blue in theme */
    color: white;
  }
}

@layer overrides {
  .badge {
    background-color: #10b981; /* Green in overrides */
  }

  /* Roll back background to theme layer! */
  .badge--default {
    background-color: revert-layer;
  }
}
```
*Explanation*: `.badge--default` in `overrides` steps down the layer stack to retrieve `#3b82f6` from `theme`.
</details>

---

## 12. Summary / Cheat Sheet

```
+-------------------------------------------------------------------------------------------------+
| UNIVERSAL VALUE KEYWORDS MASTER CHEAT SHEET                                                     |
+-------------------+----------------------------+-----------------------+------------------------+
| Keyword           | Behavior on Inherited      | Behavior on Non-Inh.  | Typical Use Case       |
+-------------------+----------------------------+-----------------------+------------------------+
| inherit           | Takes parent computed val  | Takes parent computed | Force border/color     |
| initial           | Sets to W3C spec default   | Sets to W3C default   | Pure spec reset        |
| unset             | Acts as 'inherit'          | Acts as 'initial'     | all: unset reset       |
| revert            | Restores UA/User default   | Restores UA/User def  | Restore native inputs  |
| revert-layer      | Restores previous @layer   | Restores prev @layer  | Multi-layer overrides  |
| all: unset        | Clears element styling     | Clears element styling| Unstyled buttons/lists |
+-------------------+----------------------------+-----------------------+------------------------+
```

---

## 13. Further Reading

- [W3C CSS Cascading and Inheritance Level 5: Inheriting](https://www.w3.org/TR/css-cascade-5/#inheriting)
- [MDN Web Docs: CSS Inheritance](https://developer.mozilla.org/en-US/docs/Web/CSS/Inheritance)
- [MDN Web Docs: revert](https://developer.mozilla.org/en-US/docs/Web/CSS/revert)
- [MDN Web Docs: revert-layer](https://developer.mozilla.org/en-US/docs/Web/CSS/revert-layer)
- [web.dev: Learn CSS Inheritance](https://web.dev/learn/css/inheritance)

---

[Previous: 07 - Cascade Layers (@layer)](07-cascade-layers.md) | [Section Index](README.md) | [Next: 09 - CSS Units: Absolute vs Relative](09-units.md)
