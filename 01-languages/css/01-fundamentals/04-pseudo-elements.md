# Pseudo-Elements

| Metadata | Details |
|:---|:---|
| **Level** | Foundational to Intermediate |
| **Prerequisites** | `01-fundamentals/01-syntax-rules-and-declarations.md`, `01-fundamentals/02-selectors.md` |
| **Estimated Reading Time** | 28 minutes |
| **Status** | Current |
| **Baseline Status** | Widely Available (`::before`, `::after`, `::marker`, `::placeholder`, `::backdrop`) |
| **Browsers Tested** | Chrome 120+, Firefox 121+, Safari 17.2+, Edge 120+ |
| **Tested With** | W3C CSS Pseudo-Elements Level 4, Playwright css_runner.js, stylelint |

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

### What is a Pseudo-Element?
A **Pseudo-Element** is an abstraction that represents a **virtual or phantom sub-element** that does not exist as an independent node in the HTML Document Object Model (DOM) tree. Pseudo-elements are designated by a double-colon prefix (`::`):

```
       p::first-line { font-weight: bold; }
       |     |
Element+     +--- Pseudo-Element (targets the virtual first rendered line of text)
```

### Why Do We Use Pseudo-Elements?
1. **Generated Decorative Content (`::before`, `::after`)**: Insert visual embellishments, icons, ribbons, overlays, and quotation marks without cluttering HTML markup with redundant `<span>` or `<div>` tags.
2. **Typographic Refinement (`::first-letter`, `::first-line`)**: Apply editorial styling (such as drop caps) to dynamic text where line breaks change responsively based on viewport width.
3. **Form & Native Widget Control (`::placeholder`, `::file-selector-button`, `::marker`)**: Style internal browser-rendered components (list bullets, input placeholders, file picker buttons) cleanly.
4. **Overlay Modals (`::backdrop`)**: Style the dark translucent scrim behind native `<dialog>` modals and HTML5 Fullscreen elements.

---

## 2. Core Concepts

### 1. The Single Colon (`:`) vs Double Colon (`::`) Distinction
- **CSS Level 1 & 2**: Both pseudo-classes and pseudo-elements used a single colon (`:first-line`, `:hover`).
- **CSS Level 3 & 4 (Modern Standard)**: The W3C introduced the double-colon syntax (`::`) for pseudo-elements to cleanly distinguish them from pseudo-classes (`:`).
- *Browser Backward Compatibility*: Modern browser parsers still accept single colon for the four original pseudo-elements (`:before`, `:after`, `:first-letter`, `:first-line`), but **all modern code must use double colons (`::`)**. Modern pseudo-elements like `::marker` and `::backdrop` *require* `::`.

### 2. Generated Content: `::before` and `::after`
- `::before` generates an inline child element positioned **as the very first child** inside the targeted element.
- `::after` generates an inline child element positioned **as the very last child** inside the targeted element.
- **The Mandatory `content` Property**: Neither `::before` nor `::after` will be created in the Render Tree unless the `content` property is declared (even if an empty string: `content: "";`).

### 3. List Item Markers: `::marker`
Targets the automatic bullet point in an unordered list (`<ul>`) or number in an ordered list (`<ol>`), as well as the disclosure triangle of a `<summary>` element.
- *Permitted Properties*: Limited to `color`, `font-*`, `content`, `text-combine-upright`, and `animation`/`transition`.

### 4. Dialog & Fullscreen: `::backdrop`
A full-screen box rendered immediately below a `<dialog>` element displayed with `showModal()`, or an element displayed via the Fullscreen API:
```css
dialog::backdrop {
  background-color: rgba(15, 23, 42, 0.7);
  backdrop-filter: blur(4px);
}
```

### 5. Highlight Pseudo-Elements: `::selection`
Controls the foreground text color and background highlight color when a user drags their cursor over text:
- *Permitted Properties*: `color`, `background-color`, `text-decoration`, `text-shadow`.

---

## 3. How It Works Internally

### Anonymous Box Generation in the Render Tree
Pseudo-elements do not exist in the DOM tree returned by `document.querySelectorAll()`. Instead, they are generated dynamically by the browser engine during **Render Tree construction**:

```
+--------------------------------------------------------------------+
| HTML DOM TREE                                                      |
| <button class="btn">Click Me</button>                              |
+--------------------------------------------------------------------+
                                |
                                v
+--------------------------------------------------------------------+
| CSSOM MATCHING & PSEUDO-ELEMENT RESOLVER                           |
| Matched Rules:                                                     |
| - .btn { display: inline-flex; }                                   |
| - .btn::before { content: "★"; color: gold; }                      |
+--------------------------------------------------------------------+
                                |
                                v
+--------------------------------------------------------------------+
| RENDER TREE (Visual Layout Tree)                                   |
| RenderFlexBox (.btn)                                               |
|   |-- AnonymousInlineBox (::before, content: "★")                  |
|   \-- RenderText ("Click Me")                                      |
+--------------------------------------------------------------------+
                                |
                                v
+--------------------------------------------------------------------+
| ACCESSIBILITY TREE (AOM) EXPOSURE                                  |
| Accessible Name: "★ Click Me"                                      |
| (Screen readers speak generated text unless flagged with alt text) |
+--------------------------------------------------------------------+
```

### Screen Reader VoiceOver / NVDA Interaction
Because modern browser engines project generated content into the **Accessibility Object Model (AOM)**, text inside `content: "★"` is voiced aloud by screen readers (e.g., *"Black star Click Me"*).
To prevent assistive software from reading purely decorative characters, modern CSS provides the **alternative speech syntax**:
```css
/* Provide empty alternative text for screen readers! */
.btn::before {
  content: "★" / "";
}
```

---

## 4. Syntax & Usage

```
+-------------------------------------------------------------------------+
| Web Platform Baseline: Widely Available                                  |
| ::before, ::after, ::first-line, ::first-letter: Universal               |
| ::placeholder, ::selection, ::backdrop: Widely Available                |
| ::marker: Widely Available (Baseline 2021)                               |
| content: "icon" / "alt text": Widely Available (Baseline 2023)           |
+-------------------------------------------------------------------------+
```

### Generated Content Syntax
```css
/* Inserting decorative quotation mark */
blockquote::before {
  content: "“";
  font-size: 3rem;
  color: #94a3b8;
  line-height: 0;
  vertical-align: -0.4em;
  margin-right: 0.25rem;
}
```

### Custom List Marker Syntax
```css
/* Custom colorful numbered list */
ol {
  list-style-type: decimal-leading-zero;
}

ol li::marker {
  color: #0284c7;
  font-weight: 700;
  font-variant-numeric: tabular-nums;
}
```

### Selection Highlight Syntax
```css
/* Universal selection color scheme */
::selection {
  background-color: #0284c7;
  color: #ffffff;
}
```

---

## 5. Practical Examples

### Example 1: Basic Ribbon Badge via `::after`
```html
<div class="card featured">
  <h3>Pro Plan</h3>
  <p>$29 / month</p>
</div>
```
```css
.card {
  position: relative;
  padding: 2rem;
  border: 1px solid #e2e8f0;
  border-radius: 8px;
  background: #ffffff;
  overflow: hidden;
}

/* Diagonal 'Featured' corner banner */
.card.featured::after {
  content: "POPULAR";
  position: absolute;
  top: 18px;
  right: -32px;
  transform: rotate(45deg);
  background-color: #0284c7;
  color: #ffffff;
  font-size: 0.65rem;
  font-weight: 800;
  letter-spacing: 0.05em;
  padding: 4px 36px;
  text-align: center;
  box-shadow: 0 2px 4px rgba(0, 0, 0, 0.1);
}
```

### Example 2: Real-World Dropdown Tooltip Arrow via `::before` & `::after`
```html
<div class="tooltip-container">
  <button class="trigger">Hover me</button>
  <div class="tooltip-bubble">Helpful information</div>
</div>
```
```css
.tooltip-container {
  position: relative;
  display: inline-block;
}

.tooltip-bubble {
  position: absolute;
  bottom: calc(100% + 8px);
  left: 50%;
  transform: translateX(-50%);
  background-color: #0f172a;
  color: #ffffff;
  padding: 6px 12px;
  border-radius: 4px;
  font-size: 0.875rem;
  white-space: nowrap;
  pointer-events: none;
}

/* Construct CSS triangle arrow using borders on ::after */
.tooltip-bubble::after {
  content: "";
  position: absolute;
  top: 100%;
  left: 50%;
  transform: translateX(-50%);
  border-width: 6px;
  border-style: solid;
  border-color: #0f172a transparent transparent transparent;
}
```

### Example 3: Edge Case — Modern Screen-Reader Alt Text in `content`
Preventing screen readers from announcing decorative bullet symbols or emojis:
```css
/* Bad Practice: Announces "Warning sign Database Connection Failed" */
.alert-error::before {
  content: "⚠️ ";
}

/* Modern Accessible Practice: Browser displays emoji, but announces empty alt string */
.alert-error::before {
  content: "⚠️ " / "";
}
```

### Example 4: Failure Case — Attempting to Position Pseudo-Elements on Replaced Elements
```html
<!-- Replaced elements cannot hold internal child nodes -->
<img src="avatar.jpg" alt="User avatar" class="avatar">
<input type="text" class="input-field">
```
```css
/* Anti-Pattern: FAILS completely in all browsers */
.avatar::after {
  content: "Online";
  position: absolute;
  bottom: 0;
}

.input-field::before {
  content: "Search";
}
```
*Failure Analysis*: In HTML, **replaced elements** (`<img>`, `<input>`, `<video>`, `<iframe>`, `<br>`, `<hr>`) have their internal content rendered outside the CSS formatting model. Because `::before` and `::after` insert virtual *children* inside an element, **they cannot be applied to replaced elements**.
*Solution*: Wrap the `<img>` or `<input>` in a container `<div>` or `<label>` and attach the pseudo-element to the container.

### Example 5: Production-Grade Modal with Blurred Backdrop
```html
<dialog id="subscribe-dialog" class="modal">
  <form method="dialog">
    <h2>Subscribe to Updates</h2>
    <p>Get weekly CSS architecture insights.</p>
    <button type="submit" class="btn-primary">Confirm</button>
  </form>
</dialog>
```
```css
.modal {
  border: none;
  border-radius: 12px;
  padding: 2rem;
  max-width: 480px;
  width: 90%;
  box-shadow: 0 20px 25px -5px rgba(0, 0, 0, 0.2);
}

/* Style the native browser modal scrim */
.modal::backdrop {
  background-color: rgba(15, 23, 42, 0.6);
  backdrop-filter: blur(6px);
}

.btn-primary {
  background-color: #0284c7;
  color: #ffffff;
  padding: 0.5rem 1rem;
  border: none;
  border-radius: 6px;
  cursor: pointer;
}

.btn-primary:focus-visible {
  outline: 2px solid #0284c7;
  outline-offset: 2px;
}
```
*Playwright Test Runner Verification*:
```javascript
// Test dialog backdrop support
const dialog = document.querySelector('dialog');
console.log('Dialog has showModal:', typeof dialog.showModal === 'function'); // true
```

---

## 6. Best Practices

1. **Always Use Modern Double-Colon (`::`) Syntax**: Reserve single colons exclusively for pseudo-classes to maintain clear semantic distinction.
2. **Always Define `content: ""` on `::before` and `::after`**: Without `content`, the pseudo-element will not generate an anonymous box in the render tree.
3. **Mute Decorative Characters for Screen Readers**: Use the alternative text slash syntax: `content: "▶" / ""` to prevent screen readers from voicing noise.
4. **Never Attach Pseudo-Elements to Replaced Elements**: Do not attempt to use `::before` or `::after` directly on `<img>`, `<input>`, or `<video>`.
5. **Remember Default `display: inline`**: By default, `::before` and `::after` are inline boxes. If you need dimensions (`width`, `height`), explicitly declare `display: block`, `inline-block`, or `flex`.
6. **Use `::backdrop` for Accessible Dialogs**: Leverage native browser `::backdrop` rather than creating custom fixed overlay `<div>` elements.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Root Cause | Symptom / Failure | Correct Pattern |
|:---|:---|:---|:---|
| Omitting the `content` property on `::before` | Forgetting mandatory descriptor | Pseudo-element fails to render; completely invisible in DOM. | Always declare `content: "";`. |
| Setting `width` and `height` without changing `display` | `::before` defaults to `display: inline` | Width and height declarations are ignored by browser engine. | Declare `display: block` or `display: inline-block`. |
| Attaching `::after` directly to `<input>` or `<img>` | Replaced elements cannot contain children | Pseudo-element is ignored and does not render. | Wrap in parent element; attach to parent. |
| Inaccessible icon insertion via `content: "❌"` | Screen reader reads raw Unicode text | Screen reader announces confusing symbols to visually impaired users. | Use `content: "❌" / ""` with hidden accessible text. |
| Expecting `::first-letter` to support all properties | Spec restricts typographic fragment styles | Setting `display: flex` or `position: absolute` on `::first-letter` fails. | Only use `color`, `font`, `margin`, `padding`, `background`. |
| Using single colon for modern pseudo-elements | Legacy habit from CSS2 | `input:placeholder` or `li:marker` fails in strict CSS parsers. | Use modern double colon: `::placeholder`, `::marker`. |

---

## 8. Performance, Accessibility & Browser Support

### Performance of Generated Elements
Because pseudo-elements do not exist in the JavaScript DOM tree, they create **zero JavaScript DOM node overhead**. The engine manages them entirely within the internal layout engine. However, animating expensive properties (like `box-shadow` or `width`) on pseudo-elements still triggers GPU/CPU repaints identically to standard DOM nodes.

### Accessibility Tree Exposure
- Prior to 2018, screen reader support for generated content was inconsistent.
- Today, all major screen readers (NVDA, JAWS, VoiceOver, Narrator) parse CSS generated content and voice it unless an empty alternative text string (`content: "..." / ""`) is provided.

---

## 9. Comparison / When to Use What

| Feature | Syntax | Creates Box? | Target Scope |
|:---|:---|:---|:---|
| **Pseudo-Class** | `:hover`, `:focus` | No | State or structural condition of existing element |
| **Pseudo-Element** | `::before`, `::after`| Yes (Anonymous) | Virtual child element inside container |
| **Typographic Fragment**| `::first-line` | Yes (Fragment) | Portion of text split by viewport line breaks |
| **Component Subpart** | `::marker` | Yes | Native bullet point or disclosure triangle |
| **Modal Scrim** | `::backdrop` | Yes (Full-screen)| Translucent layer behind modal `<dialog>` |

---

## 10. Interview Questions

<details>
<summary><strong>1. What is the fundamental difference between a pseudo-class and a pseudo-element?</strong></summary>

A **pseudo-class** (single colon `:`) selects an **existing element** based on its dynamic state (e.g., `:hover`, `:checked`) or its structural position in the DOM (e.g., `:nth-child`, `:has`). It does not create any new DOM or render boxes. A **pseudo-element** (double colon `::`) creates an **abstraction or virtual sub-element** that does not exist in the HTML DOM tree (e.g., `::before`, `::after`, `::marker`, `::first-line`).
</details>

<details>
<summary><strong>2. Why can't you attach `::before` and `::after` pseudo-elements to an `<img>` or `<input>` element?</strong></summary>

Elements like `<img>`, `<input>`, `<video>`, and `<iframe>` are **replaced elements**. Their contents are not rendered via the CSS box formatting model; instead, their visual representation is supplied externally by the browser or operating system. Because `::before` and `::after` insert generated content *inside* the element's box tree (as the first and last child), replaced elements have no internal child box formatting context and therefore cannot host pseudo-elements.
</details>

<details>
<summary><strong>3. What happens if you forget to declare the `content` property on a `::before` pseudo-element?</strong></summary>

The browser engine will **not generate the anonymous render box at all**. The CSS specification defines `content: normal` as the default value for pseudo-elements, which computes to "none" for `::before` and `::after`. Without an explicit `content` declaration (such as `content: ""` or `content: "text"`), the pseudo-element remains uninstantiated in the layout engine.
</details>

<details>
<summary><strong>4. How does the modern `content: "icon" / "alt text"` syntax improve web accessibility?</strong></summary>

Historically, any text or Unicode symbol declared in `content: "★"` was read aloud by screen readers, creating confusing auditory noise for visually impaired users. CSS Generated Content Level 3 introduced alternative speech syntax: `content: "★" / "Favorite"` or `content: "★" / ""`. The browser displays the first value on screen, while assistive technologies voice the second string (or ignore it completely if empty).
</details>

<details>
<summary><strong>5. Which CSS properties are allowed on the `::marker` pseudo-element?</strong></summary>

To preserve list layout stability and avoid infinite reflow loops, the W3C strictly limits the properties that can be declared on `::marker`:
- All `font-*` properties (e.g., `font-size`, `font-weight`, `font-family`).
- `color`.
- `content`.
- `text-combine-upright`, `direction`, `unicode-bidi`.
- `animation` and `transition`.
Properties like `display`, `padding`, `margin`, `position`, and `width` are invalid on `::marker`.
</details>

<details>
<summary><strong>6. How does the browser layout engine handle `::first-line` when the viewport is resized?</strong></summary>

The `::first-line` pseudo-element represents a **dynamic typographic fragment**, not a fixed set of words. As the browser window resizes, text reflows. The browser dynamically recalculates which words fit on the first rendered line and updates the styling in real time, moving words into or out of the `::first-line` formatting context.
</details>

<details>
<summary><strong>7. What is `::backdrop`, and what elements create it?</strong></summary>

`::backdrop` is a pseudo-element generated automatically by the browser engine behind any element displayed in the browser's **Top Layer**. This occurs when an HTML `<dialog>` element is opened using the JavaScript `.showModal()` method, or when an element enters full-screen mode via the `Element.requestFullscreen()` API. It covers the entire viewport between the normal document and the top-layer element.
</details>

<details>
<summary><strong>8. Can you inspect or query a pseudo-element using JavaScript?</strong></summary>

You cannot access pseudo-elements using `document.querySelector()` because they do not exist as nodes in the DOM tree. However, you can inspect their computed styles via the second argument of `window.getComputedStyle()`:
```javascript
const btn = document.querySelector('.btn');
const beforeStyles = window.getComputedStyle(btn, '::before');
console.log(beforeStyles.content);
```
</details>

---

## 11. Practice Exercises

### Exercise 1: Create a Circular Counter with `::before` (Easy)
**Task**: Write the CSS for an ordered list item `ol.steps > li` that replaces default numbers with a modern circular blue badge using `::before` and CSS counters.

<details>
<summary><strong>View Solution</strong></summary>

```css
ol.steps {
  list-style: none;
  counter-reset: step-counter;
  padding-left: 0;
}

ol.steps > li {
  counter-increment: step-counter;
  display: flex;
  align-items: center;
  margin-bottom: 0.75rem;
  font-size: 1rem;
}

ol.steps > li::before {
  content: counter(step-counter);
  display: inline-flex;
  align-items: center;
  justify-content: center;
  width: 28px;
  height: 28px;
  border-radius: 50%;
  background-color: #0284c7;
  color: #ffffff;
  font-weight: 700;
  font-size: 0.875rem;
  margin-right: 0.75rem;
  flex-shrink: 0;
}
```
*Explanation*: `counter-reset` and `counter-increment` drive dynamic numbers rendered cleanly inside a styled `::before` flex container.
</details>

---

### Exercise 2: Style Custom List Bullets via `::marker` (Easy)
**Task**: Style an unordered list `<ul class="checklist">` so that its bullet markers render as green checkmark characters (`"✓"`) with bold weight.

<details>
<summary><strong>View Solution</strong></summary>

```css
ul.checklist {
  padding-left: 1.5rem;
}

ul.checklist li::marker {
  content: "✓ ";
  color: #10b981;
  font-weight: bold;
}
```
*Explanation*: Modern `::marker` allows overriding the bullet glyph directly using `content: "✓ "` while remaining fully aligned with the text baseline.
</details>

---

### Exercise 3: Editorial Drop Cap via `::first-letter` (Medium)
**Task**: Style the first letter of an introductory paragraph `.lead-paragraph` as an elegant editorial drop cap: 3 lines tall, bold, float left, with margin spacing.

<details>
<summary><strong>View Solution</strong></summary>

```css
.lead-paragraph::first-letter {
  font-size: 3.5rem;
  line-height: 0.85;
  font-weight: 800;
  float: left;
  margin-right: 0.5rem;
  margin-top: 0.25rem;
  color: #0f172a;
  font-family: serif;
}
```
*Explanation*: `::first-letter` targets the initial character and allows floating it alongside subsequent paragraph lines.
</details>

---

### Exercise 4: Gradient Selection Highlight (Medium)
**Task**: Configure a global `::selection` rule that gives highlighted text a dark background `#0f172a` and vibrant cyan text `#38bdf8`.

<details>
<summary><strong>View Solution</strong></summary>

```css
::selection {
  background-color: #0f172a;
  color: #38bdf8;
}
```
*Explanation*: The `::selection` highlight pseudo-element applies across all text selections in the document.
</details>

---

### Exercise 5: Animated Loading Spinner Button via `::after` (Hard)
**Task**: Build a button class `.btn-loading` that disables user clicks and displays a smooth rotating CSS spinner on its `::after` pseudo-element without modifying the inner button HTML text.

<details>
<summary><strong>View Solution</strong></summary>

```css
.btn-loading {
  position: relative;
  pointer-events: none;
  opacity: 0.8;
  padding-right: 2.75rem; /* Reserve space for spinner */
}

.btn-loading::after {
  content: "";
  position: absolute;
  right: 1rem;
  top: 50%;
  transform: translateY(-50%);
  width: 16px;
  height: 16px;
  border: 2px solid rgba(255, 255, 255, 0.3);
  border-top-color: #ffffff;
  border-radius: 50%;
  animation: btnSpinner 0.6s linear infinite;
}

@keyframes btnSpinner {
  to {
    transform: translateY(-50%) rotate(360deg);
  }
}
```
*Explanation*: The spinner runs on a pseudo-element positioned cleanly inside the button padding, preventing any DOM layout shifts.
</details>

---

### Exercise 6: Read Pseudo-Element Styles in JavaScript (Hard)
**Task**: Write a JavaScript snippet that inspects a `.card.featured` element, extracts the text string rendered inside its `::after` banner, and logs it to the console.

<details>
<summary><strong>View Solution</strong></summary>

```javascript
/**
 * Reads the computed 'content' value from an element's pseudo-element.
 * @param {Element} element
 * @param {string} pseudoName (e.g. '::after')
 * @returns {string}
 */
function getPseudoContent(element, pseudoName) {
  const styles = window.getComputedStyle(element, pseudoName);
  // getComputedStyle returns the content wrapped in quotes: '"POPULAR"'
  return styles.content.replace(/^["']|["']$/g, '');
}

const card = document.querySelector('.card.featured');
console.log('Badge Text:', getPseudoContent(card, '::after')); // "POPULAR"
```
*Explanation*: Passing the pseudo-element string to `window.getComputedStyle()` provides read access to the pseudo-element's computed styles.
</details>

---

## 12. Summary / Cheat Sheet

```
+-------------------------------------------------------------------------------------------------+
| PSEUDO-ELEMENTS MASTER CHEAT SHEET                                                              |
+--------------------------+-----------------------------+--------------------+-------------------+
| Pseudo-Element           | Example Syntax              | Mandatory Rule     | Primary Purpose   |
+--------------------------+-----------------------------+--------------------+-------------------+
| ::before                 | .btn::before                | content: ""        | First child box   |
| ::after                  | .card::after                | content: ""        | Last child box    |
| ::marker                 | li::marker                  | Limited properties | List bullets      |
| ::first-line             | p::first-line               | Limited properties | First text line   |
| ::first-letter           | p::first-letter             | Limited properties | Drop caps         |
| ::placeholder            | input::placeholder          | Style text hints   | Form hints        |
| ::backdrop               | dialog::backdrop            | Native top layer   | Modal dark scrim  |
| ::selection              | ::selection                 | color/background   | Cursor drag text  |
| Alt Speech Syntax        | content: "★" / ""           | Accessibility      | Mute screen reader|
+--------------------------+-----------------------------+--------------------+-------------------+
```

---

## 13. Further Reading

- [W3C CSS Pseudo-Elements Module Level 4](https://www.w3.org/TR/css-pseudo-4/)
- [MDN Web Docs: Pseudo-elements](https://developer.mozilla.org/en-US/docs/Web/CSS/Pseudo-elements)
- [web.dev: Learn CSS Pseudo-elements](https://web.dev/learn/css/pseudo-elements)
- [W3C Generated Content and Replacement](https://www.w3.org/TR/css-content-3/)
- [Adrian Roselli: Accessible generated content](https://adrianroselli.com/2019/02/accessible-css-generated-content.html)

---

[Previous: 03 - Pseudo-Classes](03-pseudo-classes.md) | [Section Index](README.md) | [Next: 05 - Specificity Calculation](05-specificity.md)
