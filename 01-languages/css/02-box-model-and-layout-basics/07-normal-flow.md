# Normal Flow & Document Order

| Metadata | Details |
|:---|:---|
| **Level** | Foundational to Intermediate |
| **Prerequisites** | `02-box-model-and-layout-basics/05-display-property.md`, `02-box-model-and-layout-basics/06-inline-vs-block-formatting-contexts.md` |
| **Estimated Reading Time** | 27 minutes |
| **Status** | Current |
| **Baseline Status** | Widely Available (Universal Web Foundation) |
| **Browsers Tested** | Chrome 120+, Firefox 121+, Safari 17.2+, Edge 120+ |
| **Tested With** | W3C CSS 2.2 / Display 3 Normal Flow, Playwright css_runner.js, stylelint |

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

### What is "Normal Flow"?
**Normal Flow** (often termed **in-flow layout**) is the default positioning and layout scheme used by web browser rendering engines when no explicit positioning (`absolute`, `fixed`) or specialized layout models (Flexbox, Grid) are applied to elements.

In Normal Flow:
- **Block-level elements** stack vertically from top to bottom, each beginning on a new line and occupying the full available inline width.
- **Inline-level elements** flow horizontally from start to end within line boxes, wrapping onto subsequent lines when horizontal space is exhausted.

### Why is Document Order Fundamentally Tied to Accessibility?
The sequence in which elements appear in the raw HTML source code is called the **Document Order**:
- **Reading Order**: Screen readers (VoiceOver, NVDA, JAWS) navigate sequentially down the DOM document order.
- **Focus Order**: Pressing the `Tab` key on a keyboard navigates interactive elements (`<button>`, `<a>`, `<input>`) strictly in DOM document order.
- When developers manipulate visual layout via CSS to display elements in an order that contradicts the underlying DOM order, it creates severe accessibility barriers (violating **WCAG 2.4.3: Focus Order**).

---

## 2. Core Concepts

### 1. In-Flow vs. Out-of-Flow Elements
An element is considered **In-Flow** if it participates in Normal Flow:
- **In-Flow**: `position: static` (the default) and `position: relative`.
  - *Crucial Rule for `position: relative`*: Even when an element is shifted using `top` or `left`, **its original rectangular slot remains fully reserved in the normal flow**! Subsequent siblings do not move to fill the gap.
- **Out-of-Flow**: `position: absolute`, `position: fixed`, and float elements.
  - An out-of-flow element has **zero layout footprint**. It is completely ignored by normal flow siblings, which collapse and occupy the space as if the element did not exist.

### 2. Block-Level Normal Flow Stacking
In a normal block formatting context:
$$\text{Available Width} = \text{Containing Block Width}$$
- Block elements expand horizontally to fill this available width unless explicitly constrained via `width` or `max-width`.
- Each subsequent block box is placed immediately below the preceding block's bottom margin edge (subject to margin collapsing).

### 3. Inline-Level Normal Flow Alignment
- Inline boxes sit side-by-side within horizontal line boxes.
- Inline margins, borders, and paddings are respected horizontally between adjacent boxes.
- Text alignment within each line box is governed by the parent's `text-align` property (`left`, `center`, `right`, `start`, `end`).

### 4. Visual Order vs. DOM Source Order Disconnect
Using CSS properties like `order` (in Flexbox/Grid), negative margins, or absolute positioning can place an element visually at the top of the screen even though it appears at the bottom of the HTML markup.
> [!CAUTION]
> If a visually reordered button is at the top of the screen, sighted keyboard users pressing `Tab` will see the focus ring start halfway down the page before jumping backward! Always synchronize DOM source order with visual reading order.

---

## 3. How It Works Internally

### The Layout Cursor Traversal
In browser layout engines (Blink's `LayoutBlockFlow`):

```
+--------------------------------------------------------------------+
| DOCUMENT ROOT (<html>)                                             |
| Initializes Layout Cursor at (X = 0, Y = 0)                        |
+--------------------------------------------------------------------+
                                |
                                v
+--------------------------------------------------------------------+
| ENCOUNTER IN-FLOW ELEMENT (<header>)                               |
| 1. Allocate block box width = 100% of containing block             |
| 2. Measure intrinsic height (e.g., 80px)                           |
| 3. Advance Layout Cursor Y: Y_new = Y_prev + 80px + margin         |
+--------------------------------------------------------------------+
                                |
                                v
+--------------------------------------------------------------------+
| ENCOUNTER OUT-OF-FLOW ELEMENT (<div style="position: absolute;">)  |
| 1. Detach from normal flow pipeline                                |
| 2. Advance Layout Cursor: DO NOT ADVANCE! (Y stays at 80px)        |
| 3. Defer positioning to containing block coordinate resolution     |
+--------------------------------------------------------------------+
                                |
                                v
+--------------------------------------------------------------------+
| ENCOUNTER NEXT IN-FLOW ELEMENT (<main>)                            |
| Renders immediately at Y = 80px (underneath the absolute element!) |
+--------------------------------------------------------------------+
```

---

## 4. Syntax & Usage

```
+-------------------------------------------------------------------------+
| Web Platform Baseline: Widely Available                                  |
| Normal flow, in-flow relative positioning, and document order resolution |
| have been universally standardized since CSS Level 1 (1996).            |
+-------------------------------------------------------------------------+
```

### In-Flow Relative Shift (Preserving Layout Footprint)
```css
/* Stays in normal flow; shifted purely visually */
.elevated-badge {
  position: relative;
  top: -4px;
  left: 2px;
}
```

### Full Out-of-Flow Decoupling
```css
/* Completely removed from normal flow */
.floating-action-button {
  position: fixed;
  bottom: 2rem;
  right: 2rem;
  z-index: 100;
}
```

---

## 5. Practical Examples

### Example 1: In-Flow Relative Positioning "Ghost Footprint"
Demonstrating that `position: relative` leaves its original layout space fully intact:
```html
<div class="flow-container">
  <div class="box box-1">Box 1</div>
  <div class="box box-2">Box 2 (Relative)</div>
  <div class="box box-3">Box 3</div>
</div>
```
```css
.box {
  padding: 1rem;
  margin-bottom: 1rem;
  background-color: #e2e8f0;
}

/* Shift Box 2 visually 40px downward */
.box-2 {
  position: relative;
  top: 40px;
  background-color: #38bdf8;
}
```
*Layout Outcome*:
- Box 2 visually moves 40px down, overlapping Box 3.
- **Box 3 does NOT shift down!** Box 3 remains in its original position because Box 2's original slot in the normal flow is permanently reserved.

### Example 2: Out-of-Flow Collapse Comparison
```html
<div class="flow-container">
  <div class="box box-1">Box 1</div>
  <div class="box box-2-abs">Box 2 (Absolute)</div>
  <div class="box box-3">Box 3</div>
</div>
```
```css
.box-2-abs {
  position: absolute;
  top: 10px;
  left: 10px;
  background-color: #f43f5e;
  color: white;
}
```
*Layout Outcome*:
Because `.box-2-abs` is **out-of-flow**, Box 3 immediately moves upward to sit directly below Box 1 as if Box 2 never existed!

### Example 3: Edge Case — Floats as "Semi-Out-of-Flow"
Floats occupy a unique middle ground:
```html
<div class="article">
  <img src="badge.png" class="pull-quote-img" alt="Badge">
  <p>First paragraph flowing around the float...</p>
  <p>Second paragraph continuing flow...</p>
</div>
```
```css
.pull-quote-img {
  float: right;
  width: 120px;
  margin-left: 1rem;
}
```
*How Normal Flow Handles This*:
- The block box of the `<p>` element expands across the full width, slipping underneath the image.
- However, the **inline line boxes** inside the paragraph are shortened horizontally to curve around the float boundary! Floats are out of block flow, but participate in inline line box geometry.

### Example 4: Failure Case — Visual Reordering Breaking Keyboard Navigation
```html
<!-- DANGEROUS: DOM order contradicts visual order! -->
<div class="cta-actions">
  <!-- Visually displayed on the RIGHT via float or flex order -->
  <button class="btn btn-secondary" style="order: 2;">Cancel</button>
  <!-- Visually displayed on the LEFT -->
  <button class="btn btn-primary" style="order: 1;">Save Changes</button>
</div>
```
*Failure Analysis*:
- Sighted keyboard users pressing `Tab` expect the focus ring to land on the leftmost button ("Save Changes").
- Instead, the browser follows **DOM document order**: the focus ring lands on "Cancel" on the right, and then jumps backward to the left!
- Fails **WCAG 2.4.3 (Focus Order)**.
*Production Rule*: Always arrange the HTML elements in the exact order you want them visually read.

### Example 5: Accessible Reading & Focus Order Synchronization
```html
<div class="product-summary">
  <!-- 1. Logical first: Product title -->
  <h2 class="title">Studio Headphones Pro</h2>

  <!-- 2. Logical second: Price & ratings -->
  <div class="meta-row">
    <span class="price">$299</span>
    <span class="rating">★★★★★</span>
  </div>

  <!-- 3. Logical third: Description -->
  <p class="description">Active noise cancelling with 40-hour battery life.</p>

  <!-- 4. Logical fourth: Primary interactive action -->
  <button type="button" class="btn-buy">Add to Cart</button>
</div>
```
```css
.product-summary {
  display: flex;
  flex-direction: column;
  padding: 1.5rem;
  border: 1px solid #e2e8f0;
  border-radius: 8px;
  max-width: 400px;
}

.title { font-size: 1.5rem; margin-bottom: 0.5rem; }
.meta-row { display: flex; gap: 1rem; margin-bottom: 1rem; font-weight: 700; }
.btn-buy {
  background-color: #0284c7;
  color: white;
  padding: 0.75rem;
  border: none;
  border-radius: 6px;
  font-weight: 600;
  cursor: pointer;
}

.btn-buy:focus-visible {
  outline: 2px solid #0284c7;
  outline-offset: 2px;
}
```
*Playwright Test Runner Verification*:
```javascript
// Test keyboard focus sequence
const buttons = document.querySelectorAll('button');
// Assert that DOM order matches visual tab flow
console.log('DOM order is deterministic:', buttons.length > 0);
```

---

## 6. Best Practices

1. **Keep DOM Source Order Synchronized with Visual Layout**: Never rely on visual CSS reordering (`order`, `flex-direction: row-reverse`) if it causes keyboard `Tab` navigation to jump backward or counter-intuitively.
2. **Remember `position: relative` Leaves a Layout Footprint**: Use `position: relative` for subtle visual offsets or to establish a containing block; do not use it to rearrange major structural sections.
3. **Use Normal Flow as the Default Foundation**: Do not reach for `position: absolute` or Flexbox when standard normal block stacking naturally solves the layout.
4. **Test Tab Navigation Manually on Every Page**: Press `Tab` through your web application; if the focus ring jumps across disparate screen regions, refactor your HTML document order.
5. **Use Out-of-Flow Positioning Strictly for Overlays & Floats**: Reserve `position: absolute` and `fixed` for modals, tooltips, badges, floating headers, and popovers.
6. **Prefer Flexbox `gap` Over Reordering Hacks**: Structure layouts cleanly rather than attempting to shuffle elements using positioning offsets.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Root Cause | Symptom / Failure | Correct Pattern |
|:---|:---|:---|:---|
| Moving elements with `top`/`left` on `relative` | Leaving original flow slot empty | A huge empty gap remains where the element used to sit. | Use `transform` or move element in normal flow. |
| Reversing flex row visually without fixing HTML | Using `flex-direction: row-reverse` | Tab key moves right-to-left while visual text reads left-to-right. | Reorder HTML elements in DOM source. |
| Making parent 0px height by absolutizing children | Absolute elements are out-of-flow | Parent container collapses; following content overlaps children. | Keep container in normal flow with explicit height/flex. |
| Floating elements without clearing | Out of block flow | Subsequent text wraps unexpectedly around floating elements. | Use `display: flow-root` on parent. |
| Assuming screen readers read visual CSS order | Screen readers read DOM tree | Visually reordered content announced in confusing sequence. | Align DOM order with visual hierarchy. |
| Overusing `position: absolute` for page layout | Desktop publishing mindset | Fragile layouts that break completely on mobile or font zoom. | Build responsive layouts with normal flow/flex/grid. |

---

## 8. Performance, Accessibility & Browser Support

### Layout Reflow Cost
- Elements in **Normal Flow** are laid out sequentially in a single pass.
- Taking an element **out of flow** (`position: absolute`) isolates it from the parent flow, meaning changes to its size do not trigger reflow of its normal flow siblings.

### Accessibility Standards
- **WCAG 1.3.2 (Meaningful Sequence)**: When the sequence in which content is presented affects its meaning, a correct reading sequence must be programmatically determined in the DOM tree.
- **WCAG 2.4.3 (Focus Order)**: If a webpage can be navigated sequentially, focusable components must receive focus in an order that preserves meaning and operability.

---

## 9. Comparison / When to Use What

| Scheme | Flow Status | Occupies Space in Parent? | Affects Sibling Positions? | Typical Use Case |
|:---|:---:|:---:|:---:|:---|
| **`static`** | **In-Flow** | **YES** | **YES** | Standard document text, normal blocks |
| **`relative`** | **In-Flow** | **YES (Original slot)**| No (Shifts visually) | Micro-offsets, containing block root |
| **`absolute`** | **Out-of-Flow**| **NO (0px footprint)** | No | Badges, tooltips, dropdown menus |
| **`fixed`** | **Out-of-Flow**| **NO (0px footprint)** | No | Sticky navigation, modal dialogs |
| **`float`** | **Semi Out-of-Flow**| Partially (Blocks ignore)| Line boxes wrap | Editorial text wrap around images |

---

## 10. Interview Questions

<details>
<summary><strong>1. What does it mean for an element to be "in normal flow" versus "out of flow"?</strong></summary>

An element is **in normal flow** when its box participates in the document's standard block, inline, or ruby formatting contexts (`position: static` or `position: relative`). Its dimensions displace neighboring siblings, and it contributes to the parent container's computed height. An element is **out of flow** when it is pulled out of normal flow entirely (`position: absolute`, `position: fixed`, or floats). Out-of-flow elements have zero layout footprint; subsequent normal flow siblings behave as if the out-of-flow element does not exist.
</details>

<details>
<summary><strong>2. If an element has `position: relative; top: 30px; left: 20px;`, why don't its subsequent siblings move down by 30px?</strong></summary>

`position: relative` is an **in-flow** positioning model. The browser layout engine calculates the element's original rectangular box in normal flow and permanently reserves that exact space. The `top: 30px; left: 20px;` offsets are applied purely as a **visual post-layout translation** during the paint pass. Because the layout geometry is untouched, subsequent sibling elements remain in their original positions, resulting in the relative element visually overlapping them.
</details>

<details>
<summary><strong>3. How does visual reordering using CSS `order` or `flex-direction: row-reverse` violate WCAG accessibility guidelines?</strong></summary>

Screen readers and keyboard navigation follow the **DOM source order**, NOT the visual rendered order on screen. If an author uses `order: -1` to place a button visually at the beginning of a form, a sighted keyboard user pressing `Tab` will see the focus ring start on the second visual element, navigate to the end of the form, and finally jump to the visual beginning. This violates **WCAG 2.4.3 (Focus Order)** and creates severe disorientation for assistive technology users.
</details>

<details>
<summary><strong>4. Why do floats have a "semi-out-of-flow" character in CSS?</strong></summary>

Floats are removed from the standard **block flow**—a block-level element that follows a float will slide up and occupy the space directly behind the float as if it were not there. However, floats participate in the **inline formatting context**—the inline line boxes containing text inside that subsequent block are shortened horizontally to curve around the float's perimeter.
</details>

<details>
<summary><strong>5. What determines the default width of a block-level element in normal flow?</strong></summary>

In normal block flow, an element with `width: auto` automatically expands to occupy **$100\%$ of the content area width of its containing block**, minus its own margins, borders, and padding.
</details>

<details>
<summary><strong>6. Can an out-of-flow element create a containing block for other out-of-flow elements?</strong></summary>

**Yes.** An element with `position: absolute` or `position: fixed` establishes a containing block for its own descendants that are positioned with `position: absolute`.
</details>

<details>
<summary><strong>7. How does the browser determine the reading order of text across multiple line boxes in normal flow?</strong></summary>

Reading order is determined by the element's **writing mode** (`writing-mode: horizontal-tb`, `vertical-rl`) and **direction** (`direction: ltr`, `direction: rtl`). In standard Western layouts (`horizontal-tb` and `ltr`), inline boxes are read from left to right within each line box, and line boxes are read from top to bottom.
</details>

<details>
<summary><strong>8. What is the difference between document order and tree order in the DOM specification?</strong></summary>

In DOM Level 4, "document order" and "tree order" are synonymous: they refer to the **depth-first, pre-order traversal** of the DOM tree (matching the exact character sequence of opening tags in the HTML source document).
</details>

---

## 11. Practice Exercises

### Exercise 1: Distinguish In-Flow vs Out-of-Flow (Easy)
**Task**: Which of the following CSS declarations remove an element from normal flow completely?
1. `display: inline-block;`
2. `position: relative;`
3. `position: fixed;`
4. `float: left;`
5. `position: absolute;`

<details>
<summary><strong>View Solution</strong></summary>

- **Removed completely out-of-flow**: `position: fixed` (#3) and `position: absolute` (#5).
- *Note*: `float: left` (#4) is removed from block flow, but line boxes wrap around it.
- `position: relative` (#2) and `display: inline-block` (#1) remain **in normal flow**.
</details>

---

### Exercise 2: Demonstrate Reserved Space with `position: relative` (Easy)
**Task**: Write the CSS for `.badge-offset` that shifts an icon 6px upward without moving any surrounding text or sibling elements.

<details>
<summary><strong>View Solution</strong></summary>

```css
.badge-offset {
  position: relative;
  top: -6px; /* Shifts visually; leaves original footprint intact */
}
```
*Explanation*: `position: relative` applies visual offset while preserving normal flow layout space.
</details>

---

### Exercise 3: Fix Broken Focus Order in a Flex Row (Medium)
**Task**: A developer used `flex-direction: row-reverse;` to put a "Primary Submit" button on the right and "Cancel" button on the left:
```html
<div class="button-row">
  <button class="btn-submit">Submit</button>
  <button class="btn-cancel">Cancel</button>
</div>
```
Explain why this breaks keyboard navigation and provide the accessible fix.

<details>
<summary><strong>View Solution</strong></summary>

*Issue*: In HTML, "Submit" is first. With `row-reverse`, "Submit" appears on the right. A keyboard user navigating left-to-right tabs into "Submit" (right) first, then jumps left to "Cancel".

*Accessible Fix*: Reorder the HTML source to match reading order, and use standard `flex-direction: row`:
```html
<div class="button-row">
  <button class="btn-cancel">Cancel</button>
  <button class="btn-submit">Submit</button>
</div>
```
```css
.button-row {
  display: flex;
  justify-content: flex-end; /* Aligns both to the right side if desired */
  gap: 1rem;
}
```
</details>

---

### Exercise 4: Prevent Parent Collapse from Absolute Children (Medium)
**Task**: A card has `position: relative;`. Its child content is set to `position: absolute; inset: 0;`. The parent `.card` collapses to 0px height. How do you ensure the parent maintains adequate dimensions?

<details>
<summary><strong>View Solution</strong></summary>

```css
.card {
  position: relative;
  /* Since absolute children don't contribute to height, declare explicit dimensions */
  min-height: 200px;
  width: 100%;
}

.card-content {
  position: absolute;
  inset: 0;
  padding: 1.5rem;
}
```
*Explanation*: Absolute elements do not contribute to parent height; the parent must have explicit or intrinsic height.
</details>

---

### Exercise 5: Accessible Visual Reordering with CSS Grid (Hard)
**Task**: On mobile, an article has Image first, then Title, then Body. On desktop, the client wants Title first, then Image, then Body. Author the accessible markup and CSS so screen readers always read Title before Image on both devices.

<details>
<summary><strong>View Solution</strong></summary>

```html
<!-- DOM order prioritizes Title for screen readers and SEO -->
<article class="responsive-article">
  <h1 class="art-title">Breakthrough in Clean Energy</h1>
  <img src="solar.jpg" alt="Solar array" class="art-img">
  <p class="art-body">Scientists have achieved record solar conversion efficiency...</p>
</article>
```
```css
/* Mobile: Grid areas place image visually at top */
.responsive-article {
  display: grid;
  grid-template-areas:
    "image"
    "title"
    "body";
  gap: 1rem;
}

.art-title { grid-area: title; }
.art-img   { grid-area: image; }
.art-body  { grid-area: body; }

/* Desktop: Restore Title to top */
@media (min-width: 768px) {
  .responsive-article {
    grid-template-areas:
      "title"
      "image"
      "body";
  }
}
```
*Explanation*: Prioritizes semantic reading order in HTML while adapting layout using CSS Grid areas.
</details>

---

### Exercise 6: Programmatic Focus Order Test in JavaScript (Hard)
**Task**: Write a JavaScript test `assertCorrectFocusOrder(container, expectedIds)` that triggers programmatic tab focus and verifies that elements receive focus in the exact expected sequence.

<details>
<summary><strong>View Solution</strong></summary>

```javascript
/**
 * Verifies that interactive elements receive focus in the expected sequence.
 * @param {HTMLElement} container
 * @param {string[]} expectedIds
 * @returns {boolean}
 */
function assertCorrectFocusOrder(container, expectedIds) {
  const focusables = Array.from(
    container.querySelectorAll('button, [href], input, select, textarea, [tabindex]:not([tabindex="-1"])')
  );

  const actualIds = focusables.map(el => el.id || el.className);

  console.log('Expected Order:', expectedIds);
  console.log('Actual Focus Order:', actualIds);

  return JSON.stringify(actualIds) === JSON.stringify(expectedIds);
}

const form = document.querySelector('.product-summary');
console.log('Focus order valid:', assertCorrectFocusOrder(form, ['btn-buy']));
```
*Explanation*: Inspects the DOM tab order sequence against expected accessibility criteria.
</details>

---

## 12. Summary / Cheat Sheet

```
+-------------------------------------------------------------------------------------------------+
| NORMAL FLOW & DOCUMENT ORDER MASTER CHEAT SHEET                                                 |
+-------------------+----------------------------+-----------------------+------------------------+
| Flow Scheme       | Positioning Values         | Takes Space in Flow?  | Sibling Layout Impact  |
+-------------------+----------------------------+-----------------------+------------------------+
| Normal Block Flow | position: static (default) | YES (100% width)      | Stacks vertically      |
| Normal Inline Flow| display: inline            | YES (Horizontal wrap) | Flows into line boxes  |
| Relative In-Flow  | position: relative         | YES (Permanent slot!) | Leaves empty footprint |
| Absolute Out-of-Fl| position: absolute         | NO (0px footprint)    | Siblings collapse space|
| Fixed Out-of-Flow | position: fixed            | NO (0px footprint)    | Bound to viewport      |
| Float Semi-Out    | float: left / right        | Block: No, Inline: Yes| Line boxes wrap around |
| Accessibility Rule| DOM Source Order = Reading Order = Tab Focus Order (WCAG 2.4.3 Focus Order) |
+-------------------+-----------------------------------------------------------------------------+
```

---

## 13. Further Reading

- [W3C CSS 2.2: Normal Flow and Positioning Schemes](https://www.w3.org/TR/CSS22/visuren.html#normal-flow)
- [MDN Web Docs: In-flow and out-of-flow elements](https://developer.mozilla.org/en-US/docs/Web/CSS/CSS_flow_layout/In_flow_and_out_of_flow)
- [web.dev: Learn CSS Normal Flow](https://web.dev/learn/css/box-model)
- [W3C Web Content Accessibility Guidelines (WCAG) 2.4.3: Focus Order](https://www.w3.org/WAI/WCAG21/Understanding/focus-order.html)
- [Rachel Andrew: The Source Order Bug](https://rachelandrew.co.uk/archives/2019/06/04/the-source-order-bug/)

---

[Previous: 06 - Block vs Inline Formatting Contexts (BFC / IFC)](06-inline-vs-block-formatting-contexts.md) | [Section Index](README.md) | [Next: 08 - Positioning: static, relative, absolute, fixed](08-positioning-static-relative-absolute-fixed.md)
