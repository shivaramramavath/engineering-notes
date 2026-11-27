# Pseudo-Classes

| Metadata | Details |
|:---|:---|
| **Level** | Foundational to Intermediate |
| **Prerequisites** | `01-fundamentals/01-syntax-rules-and-declarations.md`, `01-fundamentals/02-selectors.md` |
| **Estimated Reading Time** | 30 minutes |
| **Status** | Current |
| **Baseline Status** | Widely Available (`:has()`, `:is()`, `:where()` Baseline 2023; core pseudo-classes Baseline Universal) |
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

### What is a Pseudo-Class?
A **Pseudo-Class** is a keyword preceded by a single colon (`:`) added to a selector that specifies a **special state** of the selected element(s) or expresses **structural relationships** that cannot be targeted with standard HTML tags, IDs, or classes alone.

```
       button:hover { background-color: #0284c7; }
         |      |
Selector +      +--- Pseudo-Class (targets element when mouse hovers over it)
```

### Why are Pseudo-Classes Indispensable?
1. **Dynamic User Interactivity**: Styling elements as users hover, focus, activate, or navigate via keyboard without requiring imperative JavaScript event listeners (`mouseenter`, `focusin`).
2. **Form State Synchronization**: Mirroring HTML5 validation states (`:valid`, `:invalid`, `:required`, `:checked`, `:user-invalid`) in real-time UI.
3. **Structural Intelligence**: Targeting elements based on their exact mathematical index (`:nth-child(2n+1)`) or emptiness (`:empty`).
4. **Relational Context (`:has`)**: The "previously impossible" capability to style a parent element based on whether a specific child exists.

---

## 2. Core Concepts

### 1. User Action Pseudo-Classes
- **`:hover`**: Matches when the user designates an element with a pointing device (mouse, trackpad).
- **`:active`**: Matches while an element is being activated by the user (e.g., between mouse button press and release).
- **`:focus`**: Matches when an element receives keyboard focus or mouse focus.
- **`:focus-visible`**: The modern accessibility standard. Matches **only when the browser determines focus should be visually evident** (e.g., keyboard `Tab` navigation), suppressing distracting rings on mouse clicks.
- **`:focus-within`**: Matches an element if the element *itself* or *any of its descendants* currently has focus (ideal for highlighting entire form field groups).

### 2. Structural & Tree-Child Pseudo-Classes
- **`:root`**: Matches the root element of the document (`<html>` in HTML). Identical specificity to a class (`[0, 1, 0]`), used globally for design tokens.
- **`:empty`**: Matches elements that have **no children at all** (neither element nodes nor text/whitespace).
- **`:first-child` / `:last-child`**: Matches the very first or last child of its parent.
- **`:only-child`**: Matches an element that has no siblings.
- **`:nth-child(An+B)`**: Evaluates formula $An+B$ against 1-based child indices.
- **`:nth-of-type(An+B)`**: Evaluates index filtering strictly among sibling elements of the **same HTML tag**.

### 3. Modern Functional Pseudo-Classes
- **`:is(s1, s2)`**: Matches elements matching any selector in the forgiving list. Specificity equals the **highest specificity** of any selector in its list.
- **`:where(s1, s2)`**: Functionally identical to `:is()`, but its specificity is **always `[0, 0, 0]`**! Perfect for CSS resets and base utility libraries that developers can easily override.
- **`:not(s)`**: Negation selector; matches elements not matching selector `s`.
- **`:has(relative-selector)`**: The relational parent selector. Matches an element if the provided relative selector matches within its subtree or sibling context.

---

## 3. How It Works Internally

### DOM State Bits & Dynamic Invalidation
Browser engines (Blink, WebKit, Gecko) maintain a bitfield of **Element State Flags** on every DOM node in memory:

```
+--------------------------------------------------------------------+
| DOM ELEMENT NODE (C++ Element Object)                              |
| - ElementType: BUTTON                                              |
| - State Flags: [ IS_HOVERED: 0 | IS_FOCUSED: 1 | IS_ACTIVE: 0 ]    |
| - Child Index: 3                                                   |
+--------------------------------------------------------------------+
                                |
                                v
+--------------------------------------------------------------------+
| EVENT DISPATCH (e.g., User presses TAB)                            |
| 1. OS sends keyboard event to Browser Window                       |
| 2. FocusController marks element State Flag: IS_FOCUSED = 1        |
| 3. Set :focus-visible bit = 1 (since input came from Keyboard)     |
+--------------------------------------------------------------------+
                                |
                                v
+--------------------------------------------------------------------+
| STYLE INVALIDATION ENGINE                                          |
| - Checks InvalidationRuleSet: Are there rules matching             |
|   ":focus-visible" on <button>?                                    |
| - Yes -> Schedules dirty subtree style recalculation               |
| - Recalculates ComputedStyle -> Repaints outline without re-layout |
+--------------------------------------------------------------------+
```

### How `:has()` Invalidates Subtrees
Historically, browser architects avoided `:has()` for 20 years fearing it would cause catastrophic rendering slowdowns ($O(N^2)$ selector complexity).

Modern engines resolved this using **Invalidation Sets**:
- When a DOM node mutates (e.g., class added to an `<img>`), Blink and WebKit look up an internal hash table of "ancestor features affected by child mutations".
- Only ancestor nodes flagged with a `:has()` dependency have their styles recalculated, preserving 60 FPS performance.

---

## 4. Syntax & Usage

```
+-------------------------------------------------------------------------+
| Web Platform Baseline: Widely Available                                  |
| - :is(), :where(), :focus-visible: Widely Available (Baseline 2021)     |
| - :has(): Widely Available (Baseline 2023 - All engines supported)       |
| - :nth-child(An+B of S): Widely Available (Baseline 2023)                |
+-------------------------------------------------------------------------+
```

### The LVHA Link Ordering Rule
When styling interactive links, declarations must follow the **LVHA order** due to cascade source order:
```css
/* 1. Link (unvisited) */
a:link { color: #2563eb; }

/* 2. Visited */
a:visited { color: #7c3aed; }

/* 3. Hover */
a:hover { color: #1d4ed8; }

/* 4. Active (must come after hover to show mouse-down state) */
a:active { color: #dc2626; }
```

### Structural `:nth-child` Formulas
```css
/* Every even child (2, 4, 6, ...) */
li:nth-child(even) { background: #f8fafc; }

/* Every odd child (1, 3, 5, ...) */
li:nth-child(odd) { background: #ffffff; }

/* Every 3rd child starting from the 1st (1, 4, 7, 10, ...) */
li:nth-child(3n + 1) { font-weight: bold; }

/* The first 3 children only */
li:nth-child(-n + 3) { border-top: 2px solid #0284c7; }

/* Modern: nth-child OF selector (Baseline 2023) */
li:nth-child(2 of .featured) { background: #fef08a; }
```

---

## 5. Practical Examples

### Example 1: Accessible Interactive Button States
```css
.btn {
  display: inline-flex;
  align-items: center;
  justify-content: center;
  padding: 0.5rem 1rem;
  border-radius: 6px;
  background-color: #0284c7;
  color: #ffffff;
  border: 1px solid transparent;
  cursor: pointer;
  transition: background-color 0.15s ease;
}

/* Mouse hover */
.btn:hover {
  background-color: #0369a1;
}

/* Click / Activation */
.btn:active {
  background-color: #075985;
  transform: translateY(1px);
}

/* Clear keyboard focus indicator, suppressed on mouse click */
.btn:focus-visible {
  outline: 3px solid #38bdf8;
  outline-offset: 2px;
}

/* Disabled state */
.btn:disabled {
  opacity: 0.6;
  cursor: not-allowed;
}
```

### Example 2: Real-World Relational Styling with `:has()`
Style a card container based on whether it contains a featured banner or an image:
```html
<article class="card">
  <h2>Standard Card</h2>
  <p>No media attached.</p>
</article>

<article class="card">
  <img src="banner.jpg" alt="Preview">
  <h2>Media Card</h2>
</article>
```
```css
.card {
  display: flex;
  flex-direction: column;
  padding: 1.5rem;
  background: #ffffff;
  border: 1px solid #e2e8f0;
  border-radius: 8px;
}

/* Automatically switch card to grid layout IF it contains an image */
.card:has(> img) {
  display: grid;
  grid-template-columns: 120px 1fr;
  gap: 1.5rem;
  padding: 1rem;
}

.card > img {
  width: 100%;
  height: 100%;
  object-fit: cover;
  border-radius: 6px;
}
```

### Example 3: Edge Case — `:nth-child(An+B of S)` Sibling Filtering
Traditional `:nth-child` counts all sibling elements, causing bugs when elements are hidden:
```html
<ul>
  <li class="item hidden">Item 1 (Hidden)</li>
  <li class="item visible">Item 2</li>
  <li class="item visible">Item 3</li>
  <li class="item visible">Item 4</li>
</ul>
```
```css
/* Buggy classic nth-child: Item 2 is child index 2, so it gets styled even though it's 1st visible! */
/* .item.visible:nth-child(odd) { ... } */

/* Solution: Modern 'of S' filtering counts ONLY matching siblings! */
li:nth-child(odd of .visible) {
  background-color: #f1f5f9;
}
```

### Example 4: Failure Case — `:where()` vs `:is()` Specificity Trap
```css
/* Case A: Written with :is() -> Takes specificity of #sidebar! Specificity = [1, 0, 0] */
:is(section, #sidebar) a {
  color: #ef4444; /* High specificity! Very hard to override! */
}

/* Later in stylesheet: Developer tries to override for footer links */
section.footer a { /* Specificity = [0, 1, 2] -> LOSES to :is() above! */
  color: #3b82f6; /* Fails to apply! */
}
```
*Remedy*: Use `:where()` for broad contextual groupings so specificity stays at zero:
```css
/* Correct: Specificity = [0, 0, 1] (only the 'a' tag counts!) */
:where(section, #sidebar) a {
  color: #ef4444;
}

/* Successfully overrides because [0, 1, 2] > [0, 0, 1] */
section.footer a {
  color: #3b82f6;
}
```

### Example 5: Accessible Real-Time Form Validation (`:user-invalid`)
Traditional `:invalid` shows red borders immediately on blank forms before the user has even typed anything. The modern `:user-invalid` pseudo-class fires **only after the user interacts and tabs away**:
```html
<form class="contact-form">
  <div class="field-group">
    <label for="email">Work Email</label>
    <input type="email" id="email" required placeholder="you@company.com">
    <span class="error-msg">Please enter a valid email address.</span>
  </div>
</form>
```
```css
.field-group {
  position: relative;
  margin-bottom: 1.5rem;
}

input {
  width: 100%;
  padding: 0.75rem;
  border: 1px solid #cbd5e1;
  border-radius: 6px;
  font-size: 1rem;
}

input:focus-visible {
  outline: 2px solid #0284c7;
  border-color: transparent;
}

/* Initially hide error message */
.error-msg {
  display: none;
  color: #dc2626;
  font-size: 0.875rem;
  margin-top: 0.25rem;
}

/* Modern: Only trigger after user interaction and blur */
input:user-invalid {
  border-color: #ef4444;
  background-color: #fef2f2;
}

/* Reveal error message only when input is user-invalid */
input:user-invalid + .error-msg {
  display: block;
}
```
*Playwright Test Runner Verification*:
```javascript
// Test user-invalid behavior
const input = document.querySelector('#email');
input.value = 'invalid-email-address';
input.dispatchEvent(new Event('blur')); // user commits and leaves
console.log('Validity check:', input.checkValidity()); // false
```

---

## 6. Best Practices

1. **Always Use `:focus-visible` Over Bare `:focus`**: Never remove outlines on `:focus` without supplying a high-contrast replacement for keyboard navigators on `:focus-visible`.
2. **Use `:where()` for CSS Resets and Base Styles**: Wrap base library styles in `:where()` to guarantee zero specificity, allowing consumers to override rules with single class names.
3. **Adopt `:user-invalid` for Form Feedback**: Replace aggressive `:invalid` styling with `:user-invalid` to avoid premature validation errors on freshly loaded forms.
4. **Follow LVHA Order for Links**: Always declare `:link`, `:visited`, `:hover`, and `:active` in strict sequence to avoid cascade state masking.
5. **Leverage `:has()` to Eliminate Unnecessary JavaScript**: Replace DOM mutation scripts that toggle parent classes with pure CSS `:has()` relational selectors.
6. **Use `:focus-within` for Accessible Composite Widgets**: Highlight entire card containers or table rows when an internal action link or button receives keyboard focus.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Root Cause | Symptom / Failure | Correct Pattern |
|:---|:---|:---|:---|
| Removing focus outline with `outline: none` | Aesthetic preference | Keyboard users cannot navigate the site; fails WCAG 2.4.7. | Use `:focus-visible` with a distinct outline. |
| Incorrect link pseudo-class order (e.g., hover before visited) | Cascade source order misunderstanding | Visited links never show hover color because `:visited` overrides `:hover`. | Follow strict LVHA order: Link, Visited, Hover, Active. |
| Using `:invalid` on required empty inputs | Premature validation | Fresh form inputs turn bright red before user even types. | Use `:user-invalid`. |
| Writing `:nth-child` expecting it to match only classes | Misinterpreting child index vs class filter | `.item:nth-child(2)` selects nothing if child #2 is a `<div>` without `.item`. | Use modern `li:nth-child(2 of .item)`. |
| Over-nesting `:has()` selectors | Complex DOM relational logic | Noticeable rendering lag on pages with 10,000+ DOM nodes. | Keep `:has()` selectors shallow (e.g., direct child `:has(> .img)`). |
| Expecting `:empty` to ignore whitespace | Whitespace constitutes a Text Node | `<div> </div>` is NOT matched by `:empty`. | Ensure HTML element has zero whitespace between tags: `<div></div>`. |

---

## 8. Performance, Accessibility & Browser Support

### Performance Profiles of Pseudo-Classes
- **Near-Zero Cost**: `:root`, `:first-child`, `:hover`, `:active`, `:focus-visible`.
- **Moderate Cost**: `:nth-child(An+B)`, `:where()`, `:is()`.
- **Engine Workload Considerations**: Complex `:has()` chains with ancestor combinators (e.g., `body:has(.a .b .c)`). Modern browsers optimize this via Bloom filters, but shallow child checks (`.card:has(> .avatar)`) are recommended for critical paths.

### Accessibility Safeguards
- **WCAG 2.4.7 (Focus Visible)**: Requires keyboard focus indicators to be clearly visible.
- **Privacy Protections on `:visited`**: Modern browsers restrict styles inside `:visited` strictly to `color`, `background-color`, `border-color`, and SVG `fill`/`stroke`. Browsers block `getComputedStyle()` from reading visited colors and forbid background images inside `:visited` to prevent malicious history sniffing!

---

## 9. Comparison / When to Use What

| Selector | Forgiving? | Specificity Impact | Primary Purpose |
|:---|:---|:---|:---|
| **`:is(A, B)`** | Yes | Highest specificity among arguments | Grouping complex selector lists cleanly |
| **`:where(A, B)`** | Yes | **Zero `[0, 0, 0]`** | Resets, defaults, easily overridable styles |
| **`:not(A)`** | No | Specificity of argument `A` | Excluding specific states or elements |
| **`:has(A)`** | Yes | Specificity of argument `A` | Styling parent/ancestor based on children |
| **`:focus`** | N/A | `[0, 1, 0]` | Mouse AND keyboard focus (inputs) |
| **`:focus-visible`**| N/A | `[0, 1, 0]` | Keyboard focus only (buttons, links) |

---

## 10. Interview Questions

<details>
<summary><strong>1. What is the fundamental difference between `:is()` and `:where()`?</strong></summary>

Both `:is()` and `:where()` take a forgiving selector list as arguments and match elements matching any selector in that list. The crucial difference is **specificity calculation**:
- `:is()` takes on the specificity of the **most specific selector** in its argument list (e.g., `:is(p, #nav)` has specificity `[1, 0, 0]`).
- `:where()` **always has a specificity of `[0, 0, 0]`**, regardless of what selectors are passed into it. This makes `:where()` ideal for base stylesheets, component libraries, and CSS resets that must be easily overridden.
</details>

<details>
<summary><strong>2. Why was `:has()` historically called the "parent selector", and what makes it more powerful than just styling parents?</strong></summary>

While `:has()` can style an immediate parent based on its child (`form:has(> input:invalid)`), it is far more than a parent selector. It is a **relational selector** capable of:
1. Ancestor styling (`article:has(.author-badge)`).
2. Previous sibling styling (`h2:has(+ p.lead)`—styling a heading based on the sibling that follows it!).
3. Cross-branch state synchronization (`body:has(#nav-toggle:checked) .sidebar`).
</details>

<details>
<summary><strong>3. Explain the difference between `:focus`, `:focus-visible`, and `:focus-within`.</strong></summary>

- **`:focus`**: Matches when an element receives focus through any mechanism (mouse click, touch tap, or keyboard navigation).
- **`:focus-visible`**: Matches only when the browser heuristics determine focus should be visually emphasized—almost universally when navigating via keyboard (`Tab` key). It avoids showing focus rings when clicking buttons with a mouse.
- **`:focus-within`**: Matches an element if the element itself OR any descendant inside its DOM subtree has focus.
</details>

<details>
<summary><strong>4. Why must pseudo-classes for links follow the LVHA (Link, Visited, Hover, Active) order?</strong></summary>

All four link pseudo-classes have identical specificity (`[0, 1, 0]`). Therefore, conflicts between them are resolved strictly by **source order in the stylesheet** (the last declared rule wins). If `:hover` were placed before `:visited`, a visited link would have its `:visited` declaration win over `:hover`, making it impossible to see visual feedback when hovering over previously visited links. Similarly, `:active` must follow `:hover` so that clicking a hovered element reflects the active state.
</details>

<details>
<summary><strong>5. How does `:nth-child(An+B of S)` solve the classic limitation of `:nth-child`?</strong></summary>

Classic `:nth-child(n)` counts every sibling element in the parent regardless of class. If you write `.card:nth-child(even)`, the browser looks at child index 2; if child index 2 does not have the class `.card`, nothing is styled. With the modern syntax `li:nth-child(even of .card)`, the browser first filters the siblings matching selector `.card`, and then applies the even/odd index counting *only to that filtered subset*.
</details>

<details>
<summary><strong>6. What privacy restrictions do browsers enforce on `:visited` styling, and why?</strong></summary>

In early browsers, attackers used `:visited` to determine which websites a user had visited by testing links against millions of URLs and checking `getComputedStyle()` or using timing attacks. Today, browsers enforce strict protections:
1. Only `color`, `background-color`, `border-color`, `outline-color`, and SVG `fill`/`stroke` can be modified.
2. Background images cannot be applied inside `:visited`.
3. `getComputedStyle()` always lies to JavaScript, returning the unvisited color to prevent script-based history sniffing.
</details>

<details>
<summary><strong>7. Why is `:empty` sensitive to whitespace, and how does HTML formatting impact it?</strong></summary>

According to the DOM specification, whitespace (spaces, tabs, newlines) between HTML tags creates a **Text Node**. Because `:empty` requires the element to have zero child nodes of any kind, an element written as `<div> </div>` contains a child Text Node and fails to match `:empty`. To match `:empty`, the tag must be closed with no characters inside: `<div></div>`.
</details>

<details>
<summary><strong>8. What is the difference between `:invalid` and `:user-invalid`?</strong></summary>

`:invalid` matches any form field whose current value violates its constraint validation attributes (e.g., an empty `<input required>`). This causes new, untouched forms to display harsh red error styles on initial page load. In contrast, `:user-invalid` matches only after the user has actively interacted with the input (typed and committed a change or blurred focus), creating a vastly superior user experience.
</details>

---

## 11. Practice Exercises

### Exercise 1: Strip Focus Rings for Mouse but Keep for Keyboard (Easy)
**Task**: Write the CSS rule for `.interactive-tile` that eliminates default browser focus outlines on mouse clicks, but renders a crisp 2px solid `#3b82f6` outline with a 2px offset when a user navigates using the keyboard.

<details>
<summary><strong>View Solution</strong></summary>

```css
.interactive-tile:focus {
  outline: none; /* Safe because :focus-visible immediately provides accessible replacement */
}

.interactive-tile:focus-visible {
  outline: 2px solid #3b82f6;
  outline-offset: 2px;
}
```
*Explanation*: Styling `:focus-visible` ensures accessibility compliance while maintaining visual cleanliness for pointer devices.
</details>

---

### Exercise 2: Style Alternating Table Rows with Modern Syntax (Easy)
**Task**: Write an `:nth-child` rule that shades alternating table rows (`#f8fafc`) across only rows with class `.data-row`, ignoring rows with class `.banner-row`.

<details>
<summary><strong>View Solution</strong></summary>

```css
tr:nth-child(even of .data-row) {
  background-color: #f8fafc;
}
```
*Explanation*: The `of .data-row` clause creates a filtered sibling index, ensuring correct striping even if `.banner-row` elements are interspersed.
</details>

---

### Exercise 3: Highlight Parent Form Group on Child Focus (Medium)
**Task**: A form has a `.form-group` containing a label and an input. Write a rule that changes the `.form-group`'s border color to `#0284c7` and adds a subtle box shadow whenever any input inside that group receives focus.

<details>
<summary><strong>View Solution</strong></summary>

```css
.form-group {
  border: 1px solid #cbd5e1;
  border-radius: 8px;
  padding: 1rem;
  transition: border-color 0.2s, box-shadow 0.2s;
}

.form-group:focus-within {
  border-color: #0284c7;
  box-shadow: 0 0 0 3px rgba(2, 132, 199, 0.15);
}
```
*Explanation*: `:focus-within` activates on the parent `.form-group` whenever an inner `<input>` has focus.
</details>

---

### Exercise 4: Previous Sibling Styling via `:has()` (Medium)
**Task**: Write a CSS selector using `:has()` that removes the bottom border from an `<h2>` element only when it is immediately followed by a `<p class="lead">` paragraph.

<details>
<summary><strong>View Solution</strong></summary>

```css
h2 {
  border-bottom: 2px solid #e2e8f0;
  padding-bottom: 0.5rem;
  margin-bottom: 1rem;
}

/* Remove bottom border if immediately followed by .lead */
h2:has(+ p.lead) {
  border-bottom: none;
  padding-bottom: 0;
}
```
*Explanation*: `:has(+ p.lead)` matches the `<h2>` if its immediately adjacent next sibling is `p.lead`.
</details>

---

### Exercise 5: Building a Zero-Specificity Component Reset with `:where()` (Hard)
**Task**: Write a reset rule for all semantic list elements (`ul`, `ol`, `menu`) that removes default padding and margin, but ensures its specificity is `[0, 0, 0]` so any consumer class (e.g., `.feature-list`) overrides it without issue.

<details>
<summary><strong>View Solution</strong></summary>

```css
/* Zero specificity reset */
:where(ul, ol, menu) {
  padding-left: 0;
  margin: 0;
  list-style: none;
}

/* Any consumer rule immediately overrides it, even with low specificity */
.article-content ul {
  padding-left: 1.5rem;
  list-style: disc;
}
```
*Explanation*: `:where()` strips specificity down to zero, eliminating battle-of-specificity bugs in design systems.
</details>

---

### Exercise 6: Complete Full-Screen Modal Freeze with `:has()` (Hard)
**Task**: When a `<dialog id="cart-modal">` has the `[open]` attribute, prevent the entire background document from scrolling using a pure CSS `:has()` declaration on the root element.

<details>
<summary><strong>View Solution</strong></summary>

```css
/* Lock background scrolling when modal dialog is open */
html:has(dialog#cart-modal[open]) {
  overflow: hidden;
}
```
*Explanation*: The root `html` element observes whether a matching open dialog exists anywhere in the DOM tree, dynamically applying `overflow: hidden` to freeze background scrolling without JavaScript DOM manipulation.
</details>

---

## 12. Summary / Cheat Sheet

```
+-------------------------------------------------------------------------------------------------+
| PSEUDO-CLASSES MASTER CHEAT SHEET                                                               |
+--------------------------+-----------------------------+--------------------+-------------------+
| Pseudo-Class             | Example                     | Specificity        | Baseline Status   |
+--------------------------+-----------------------------+--------------------+-------------------+
| :hover, :active          | a:hover                     | [0, 1, 0]          | Universal         |
| :focus-visible           | button:focus-visible        | [0, 1, 0]          | Widely Available  |
| :focus-within            | .form-group:focus-within    | [0, 1, 0]          | Widely Available  |
| :user-invalid            | input:user-invalid          | [0, 1, 0]          | Widely Available  |
| :is(A, B)                | :is(h1, h2)                 | Max(A, B)          | Widely Available  |
| :where(A, B)             | :where(ul, ol)              | [0, 0, 0] (ZERO!)  | Widely Available  |
| :not(A)                  | div:not(.active)            | Specificity of A   | Widely Available  |
| :has(A)                  | card:has(> img)             | Specificity of A   | Widely Available  |
| :nth-child(An+B)         | li:nth-child(2n+1)          | [0, 1, 0]          | Universal         |
| :nth-child(An+B of S)    | li:nth-child(odd of .card)  | [0, 1, 0] + Spec(S)| Widely Available  |
+--------------------------+-----------------------------+--------------------+-------------------+
```

---

## 13. Further Reading

- [W3C Selectors Level 4: Pseudo-Classes](https://www.w3.org/TR/selectors-4/#pseudo-classes)
- [MDN Web Docs: Pseudo-classes](https://developer.mozilla.org/en-US/docs/Web/CSS/Pseudo-classes)
- [MDN Web Docs: The :has() relational pseudo-class](https://developer.mozilla.org/en-US/docs/Web/CSS/:has)
- [web.dev: Using :focus-visible](https://web.dev/articles/focus-visible)
- [web.dev: The :has() parent selector](https://web.dev/articles/has-m105)

---

[Previous: 02 - Selectors](02-selectors.md) | [Section Index](README.md) | [Next: 04 - Pseudo-Elements](04-pseudo-elements.md)
