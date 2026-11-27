# Margin & Padding In-Depth

| Metadata | Details |
|:---|:---|
| **Level** | Foundational |
| **Prerequisites** | `02-box-model-and-layout-basics/01-the-box-model.md`, `02-box-model-and-layout-basics/02-box-sizing-content-vs-border.md` |
| **Estimated Reading Time** | 28 minutes |
| **Status** | Current |
| **Baseline Status** | Widely Available (Universal Web Foundation; Logical spacing Baseline 2021) |
| **Browsers Tested** | Chrome 120+, Firefox 121+, Safari 17.2+, Edge 120+ |
| **Tested With** | W3C CSS Box Model Module Level 3, Playwright css_runner.js, stylelint |

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

### What are Margin and Padding?
**Margin** and **Padding** are the two primary spatial properties in CSS. Although both create visual white space around elements, they serve fundamentally opposite structural purposes:
- **Padding**: Creates space **inside** the element's border box. It enlarges the clickable hit area, gives child text room to breathe, and paints the element's background color or background image.
- **Margin**: Creates space **outside** the element's border box. It pushes neighboring sibling elements and parent containers away, is completely transparent, and does not respond to pointer click events.

```
+-----------------------------------------------------------+
| MARGIN (External spacing - Pushes siblings away)          |
|   +---------------------------------------------------+   |
|   | BORDER (Visible perimeter)                        |   |
|   |   +-------------------------------------------+   |   |
|   |   | PADDING (Internal cushion - Paints BG)    |   |   |
|   |   |   +-----------------------------------+   |   |   |
|   |   |   | CONTENT (Text, images, children)  |   |   |   |
|   |   |   +-----------------------------------+   |   |   |
|   |   +-------------------------------------------+   |   |
|   +---------------------------------------------------+   |
+-----------------------------------------------------------+
```

### Why Understanding Spacing Geometry Matters
Improper spacing architecture produces common UI failures:
- Using margins instead of padding on buttons makes tiny icons nearly impossible to tap on touchscreens.
- Misunderstanding `margin: auto` leads developers to wonder why vertical centering fails in standard block layout.
- Negative margins, when understood algorithmically, unlock powerful layouts like overlapping hero banners and full-bleed breakout sections.

---

## 2. Core Concepts

### 1. The `auto` Margin Centering Algorithm
In a standard Block Formatting Context (BFC):
- **Horizontal Auto Margins (`margin-inline: auto`)**: The browser calculates the remaining free horizontal space within the containing block:
  $$\text{Free Space} = \text{Containing Block Width} - \text{Element Border-Box Width}$$
  It divides this free space equally between `margin-left` and `margin-right`, perfectly centering the block horizontally!
- **Vertical Auto Margins (`margin-top: auto`, `margin-bottom: auto`)**:
  > [!NOTE]
  > In standard block flow, vertical auto margins **always compute to `0px`**! Vertical auto margins only distribute space when inside a **Flexbox** or **Grid** formatting context.

### 2. Negative Margins
While padding can never be negative (negative padding is a syntax error and rejected by the parser), **margins can be negative**:
- **Negative `margin-top` / `margin-left`**: Pulls the element upward or to the left, causing it to overlap preceding siblings.
- **Negative `margin-bottom` / `margin-right`**: Pulls *subsequent* sibling elements toward the current element, causing following content to overlap.

### 3. The Percentage Calculation Trap
A fundamental rule of CSS layout:
**Both horizontal AND vertical `margin` and `padding` percentages are calculated relative to the containing block's WIDTH!**
- If a container is $1000\text{px}$ wide and $400\text{px}$ tall:
  - `padding: 5%` computes to $50\text{px}$ on ALL four sides (Top, Right, Bottom, Left).
  - It does NOT compute to $20\text{px}$ on the top and bottom!

### 4. Spacing Systems (The 8px Grid)
Professional design systems avoid arbitrary pixel values (`padding: 13px; margin: 27px;`) by standardizing on an **8-point geometric scale**:
- `4px` ($0.25\text{rem}$): Micro spacing, icon offsets
- `8px` ($0.5\text{rem}$): Compact spacing, tight badges
- `16px` ($1\text{rem}$): Standard base spacing, input padding
- `24px` ($1.5\text{rem}$): Card padding, moderate section gap
- `32px` ($2\text{rem}$): Major layout separation
- `48px` ($3\text{rem}$): Hero section spacing

---

## 3. How It Works Internally

### Margin Auto Resolution in Blink / WebKit
During the **Layout (Reflow)** phase in Chromium's `LayoutBlock::ComputeInlineMargins`:

```
+--------------------------------------------------------------------+
| BLOCK LEVEL ELEMENT (e.g. div with width: 400px; margin: 0 auto;)  |
+--------------------------------------------------------------------+
                                |
                                v
+--------------------------------------------------------------------+
| CONTAINING BLOCK WIDTH DETERMINATION (e.g., Parent Width = 1000px) |
+--------------------------------------------------------------------+
                                |
                                v
+--------------------------------------------------------------------+
| AUTO MARGIN CALCULATION:                                           |
| Available Space = 1000px - 400px = 600px                           |
| If margin-left == auto AND margin-right == auto:                   |
|   margin-left  = 600px / 2 = 300px                                 |
|   margin-right = 600px / 2 = 300px                                 |
|   ===> Element is centered precisely at X = 300px!                 |
+--------------------------------------------------------------------+
```

- If `width` is `auto` (default for block elements), the element expands to fill $1000\text{px}$, leaving $0\text{px}$ of free space. Therefore, `margin: auto` has no visible centering effect on block elements without an explicit `width` or `max-width`!

---

## 4. Syntax & Usage

```
+-------------------------------------------------------------------------+
| Web Platform Baseline: Widely Available                                  |
| Physical margin and padding are universal web foundations.              |
| Logical margin-inline, margin-block, padding-inline, padding-block are  |
| Widely Available (Baseline 2021).                                       |
+-------------------------------------------------------------------------+
```

### Logical Spacing Declarations
```css
.card {
  /* Vertical padding: 24px top and bottom */
  padding-block: 1.5rem;

  /* Horizontal padding: 32px left and right in LTR */
  padding-inline: 2rem;

  /* Centered horizontally with bottom margin */
  margin-inline: auto;
  margin-block-end: 2rem;
}
```

---

## 5. Practical Examples

### Example 1: Standard 8-Point Spacing System Tokens
```css
:root {
  --space-1: 0.25rem; /* 4px */
  --space-2: 0.5rem;  /* 8px */
  --space-3: 0.75rem; /* 12px */
  --space-4: 1rem;    /* 16px */
  --space-6: 1.5rem;  /* 24px */
  --space-8: 2rem;    /* 32px */
  --space-12: 3rem;   /* 48px */
}

.article-card {
  padding: var(--space-6);
  margin-block-end: var(--space-8);
  background: #ffffff;
  border: 1px solid #e2e8f0;
  border-radius: 8px;
}
```

### Example 2: Real-World Overlapping Floating Card via Negative Margin
```html
<section class="hero-header">
  <h1>Company Intelligence</h1>
</section>
<main class="floating-dashboard">
  <h2>Live Analytics</h2>
  <p>System metrics overview.</p>
</main>
```
```css
.hero-header {
  height: 240px;
  background-color: #0f172a;
  color: #ffffff;
  padding: 3rem 1.5rem;
  text-align: center;
}

.floating-dashboard {
  /* Negative top margin pulls card 60px upward into hero header! */
  margin-top: -60px;
  margin-inline: auto;
  width: min(90%, 800px);
  padding: 2rem;
  background-color: #ffffff;
  border-radius: 12px;
  box-shadow: 0 10px 25px -5px rgba(0, 0, 0, 0.15);
  position: relative; /* Establishes stacking context so card sits on top */
}
```

### Example 3: Edge Case — Full-Bleed Breakout in a Padded Container
Allow an image or banner to break out of a padded container and span edge-to-edge:
```html
<div class="content-container">
  <p>Introductory paragraph text...</p>
  <img src="panoramic.jpg" alt="Landscape" class="full-bleed-media">
  <p>Subsequent article prose...</p>
</div>
```
```css
.content-container {
  max-width: 65ch;
  margin-inline: auto;
  padding-inline: 1.5rem; /* 24px padding on sides */
}

/* Negative margin counteracts container padding! */
.full-bleed-media {
  width: calc(100% + 3rem); /* 100% + 48px */
  margin-inline: -1.5rem;   /* Pulls left and right by 24px */
  max-width: none;
  display: block;
}
```

### Example 4: Failure Case — Vertical `margin: auto` Failure in Block Flow
```html
<div class="parent-box">
  <div class="child-box">I want to be centered vertically!</div>
</div>
```
```css
/* Anti-Pattern: Does NOT center vertically in standard block layout */
.parent-box {
  height: 300px;
  background: #f1f5f9;
}

.child-box {
  width: 200px;
  height: 100px;
  background: #0284c7;
  color: white;
  margin: auto; /* Centers horizontally, but margin-top and margin-bottom compute to 0! */
}
```
*Failure Analysis*: In CSS 2.1 block layout, vertical auto margins compute to zero.
*Modern Solution*: Turn the parent into a **Flexbox** or **Grid** container:
```css
.parent-box {
  display: flex; /* Now vertical margin: auto works! */
  height: 300px;
}

.child-box {
  margin: auto; /* Perfectly centered both vertically and horizontally! */
}
```

### Example 5: Accessible Interactive Click Target with Padding
```html
<button type="button" class="close-btn" aria-label="Close dialog">
  <svg width="16" height="16" viewBox="0 0 16 16" fill="currentColor">
    <path d="M4 4l8 8m0-8l-8 8" stroke="currentColor" stroke-width="2"/>
  </svg>
</button>
```
```css
.close-btn {
  /* Reset button defaults */
  background: transparent;
  border: none;
  cursor: pointer;
  color: #64748b;

  /* SVG is only 16x16px; add 14px padding to guarantee 44x44px touch target */
  padding: 14px;
  display: inline-flex;
  align-items: center;
  justify-content: center;
  border-radius: 50%;
  transition: background-color 0.15s ease, color 0.15s ease;
}

.close-btn:hover {
  background-color: #f1f5f9;
  color: #0f172a;
}

.close-btn:focus-visible {
  outline: 2px solid #0284c7;
  outline-offset: 2px;
}
```
*Playwright Test Runner Verification*:
```javascript
// Test computed bounding client rect
const btn = document.querySelector('.close-btn');
const rect = btn.getBoundingClientRect();
console.log('Touch Target Width:', rect.width);   // 44px (16 + 28)
console.log('Touch Target Height:', rect.height); // 44px (16 + 28)
```

---

## 6. Best Practices

1. **Standardize on an 8-Point Spacing Scale**: Use consistent rem multiples (`0.5rem`, `1rem`, `1.5rem`, `2rem`) for all padding and margins.
2. **Use Padding to Expand Clickable Touch Targets**: Expand small icons to the WCAG-compliant $44 \times 44\text{px}$ minimum size using padding, not margin.
3. **Use Logical Spacing Properties**: Adopt `margin-inline`, `margin-block`, `padding-inline`, and `padding-block` to support internationalization.
4. **Never Use Vertical `margin: auto` Without Flex or Grid**: Remember that vertical auto margins compute to $0\text{px}$ in standard block flow.
5. **Center Blocks with Explicit Width and `margin-inline: auto`**: Ensure the element has a declared `width` or `max-width` (like `max-width: 1200px`) for auto centering to function.
6. **Prefer Flex/Grid `gap` Over Sibling Margins**: When laying out lists or grids, use `gap: 1rem` on the parent container rather than managing `margin-right` or `margin-bottom` on children.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Root Cause | Symptom / Failure | Correct Pattern |
|:---|:---|:---|:---|
| Expecting vertical `margin: auto` to center in block flow | Block layout specification | Element stays glued to the top of the container. | Set `display: flex` or `grid` on parent. |
| Using margin on small button icons | Margins don't receive clicks | Users struggle to tap icon on mobile screens; fails WCAG. | Use padding to enlarge hit area. |
| Expecting vertical percentage padding to use height | Percentage calculation rules | Vertical padding is unexpectedly huge on desktop screens. | Remember: percentage uses container WIDTH. |
| Attempting negative padding (`padding: -10px`) | Padding cannot be negative | Syntax error; declaration ignored completely. | Use negative margins (`margin: -10px`). |
| Forgetting `position: relative` with negative margins | Stacking context order | Pulled element slips underneath preceding sibling. | Add `position: relative` to lift element above. |
| Using `margin-inline: auto` without setting a width | Block elements default to 100% width | Free space is 0; centering has no visual effect. | Declare `max-width` or explicit width. |

---

## 8. Performance, Accessibility & Browser Support

### Reflow Triggers
- Modifying `margin` or `padding` forces the browser engine to execute **Layout (Reflow)**, recalculating box boundaries for the element and all downstream flow elements.
- Never animate `margin` or `padding` continuously in `@keyframes` loops (e.g., hovering or bouncing). Use `transform: translateY()` instead, which animates on the GPU compositor at 120 FPS.

### Accessibility Touch Target Sizing
- **WCAG 2.5.5 (Target Size)**: Interactive targets must measure at least $44 \times 44\text{ CSS px}$.
- Padding is hit-tested by the browser's event dispatcher, directly contributing to touch target compliance.

---

## 9. Comparison / When to Use What

| Feature | Margin | Padding | Flex / Grid Gap |
|:---|:---|:---|:---|
| **Location** | Outside border | Inside border | Between sibling items |
| **Paints Background?** | No (Transparent) | **YES** | No |
| **Receives Clicks?** | No | **YES** | No |
| **Supports Negative?** | **YES** | No (Syntax Error) | No |
| **Collapses Vertically?**| **YES** (in block flow)| No | No |
| **Primary Use Case** | External offsets, overlapping | Internal padding, touch size | Uniform spacing in lists/grids |

---

## 10. Interview Questions

<details>
<summary><strong>1. Why are vertical padding and vertical margin percentages calculated relative to the container's width instead of height?</strong></summary>

In standard flow layout, the height of a container is typically determined dynamically by the height of its contents. If vertical padding or margin were calculated relative to the container's height, increasing the child's padding would increase the container's height. This increased height would then trigger another recalculation of the child's padding, resulting in an **infinite layout reflow loop**. To guarantee deterministic, one-pass layout calculation, the W3C specified that both horizontal and vertical padding/margin percentages are calculated relative to the containing block's **width**.
</details>

<details>
<summary><strong>2. Under what conditions does `margin: auto` successfully center an element vertically?</strong></summary>

`margin: auto` will center an element vertically **only when the element is a child of a Flexbox or Grid formatting context**, or when the element is absolutely positioned with `top: 0; bottom: 0; height: [fixed];`. In standard Block Formatting Contexts, vertical auto margins always compute to **`0px`** by specification.
</details>

<details>
<summary><strong>3. How does a negative margin behave differently when applied to `margin-top` versus `margin-bottom`?</strong></summary>

- **Negative `margin-top`**: Pulls the element **itself** upward, causing it to overlap preceding elements in the document flow.
- **Negative `margin-bottom`**: Does *not* move the element itself; instead, it reduces the element's layout footprint in the flow, causing **subsequent sibling elements** to be pulled upward into the current element's space.
</details>

<details>
<summary><strong>4. Can padding take negative values in CSS?</strong></summary>

**No.** The CSS specification explicitly defines padding values as non-negative (`<length [0,∞]>`). If an author writes `padding: -10px;`, the CSS parser rejects the declaration as a syntax error and discards it.
</details>

<details>
<summary><strong>5. What is the difference between `padding-inline` and `padding-left` / `padding-right`?</strong></summary>

`padding-inline` is a **CSS Logical Property** that sets padding along the inline (horizontal in Western languages) axis based on the document's writing mode and direction:
- In Left-to-Right (LTR) languages (English, French), `padding-inline-start` is Left and `padding-inline-end` is Right.
- In Right-to-Left (RTL) languages (Arabic, Hebrew), `padding-inline-start` is Right and `padding-inline-end` is Left.
Using `padding-inline` automatically localizes interface spacing without writing duplicate direction-specific overrides.
</details>

<details>
<summary><strong>6. Why is `margin: 0 auto;` ineffective on an inline element like `<span>`?</strong></summary>

Inline elements do not have a configurable block width; their width is determined strictly by the inline text content within the line box. Because an inline element does not generate a block box with available horizontal distribution space, auto margins on inline elements compute to `0px`. To center it with auto margins, you must change its display property to `display: block` or `display: inline-block` with a declared width.
</details>

<details>
<summary><strong>7. How do negative margins enable "full-bleed" breakout sections inside constrained containers?</strong></summary>

When a parent container has horizontal padding (e.g., `padding-inline: 20px`), child elements are constrained inside that cushion. By applying `margin-inline: -20px;` and `width: calc(100% + 40px);` to a child, the negative margins pull the child outward by 20px on both sides, perfectly neutralizing the parent's padding and allowing the child to stretch edge-to-edge.
</details>

<details>
<summary><strong>8. Why should you prefer CSS Grid/Flexbox `gap` over sibling margins when building lists?</strong></summary>

Using sibling margins (e.g., `.item { margin-bottom: 1rem; }`) requires writing override rules (like `:last-child { margin-bottom: 0; }`) to prevent unwanted spacing at the end of containers. Furthermore, managing margins across wrapping rows is error-prone. The `gap` property applies space strictly **between** adjacent items, automatically omitting space before the first item and after the last item.
</details>

---

## 11. Practice Exercises

### Exercise 1: Center a Fixed-Width Card Horizontally (Easy)
**Task**: Write the CSS for a card `.info-card` with a fixed width of $360\text{px}$ that centers horizontally within its parent container using modern logical properties.

<details>
<summary><strong>View Solution</strong></summary>

```css
.info-card {
  width: 360px;
  margin-inline: auto;
}
```
*Explanation*: `margin-inline: auto` evenly divides horizontal free space, centering the block.
</details>

---

### Exercise 2: Accessible Icon Button Touch Target (Easy)
**Task**: An icon is $20 \times 20\text{px}$. Write the CSS for `.icon-action-btn` using padding so the clickable touch target satisfies the WCAG $44\text{px}$ minimum.

<details>
<summary><strong>View Solution</strong></summary>

```css
.icon-action-btn {
  background: transparent;
  border: none;
  cursor: pointer;
  /* (44px target - 20px icon) / 2 = 12px padding on all sides */
  padding: 12px;
  display: inline-flex;
  align-items: center;
  justify-content: center;
  border-radius: 4px;
}

.icon-action-btn:focus-visible {
  outline: 2px solid #0284c7;
  outline-offset: 2px;
}
```
*Explanation*: $12\text{px}$ padding on all sides turns the $20\text{px}$ icon into a $44 \times 44\text{px}$ interactive target.
</details>

---

### Exercise 3: Vertical Centering with Auto Margins in Flexbox (Medium)
**Task**: Build a hero section `.hero-frame` of height $400\text{px}$ that contains a centered callout `.callout-box`. Use `margin: auto` to center the callout both vertically and horizontally.

<details>
<summary><strong>View Solution</strong></summary>

```css
.hero-frame {
  display: flex; /* Enables vertical auto margin resolution! */
  height: 400px;
  background-color: #f8fafc;
}

.callout-box {
  margin: auto; /* Centers along both X and Y axes */
  padding: 2rem;
  background-color: #ffffff;
  border: 1px solid #cbd5e1;
  border-radius: 8px;
}
```
*Explanation*: Inside a flex container, `margin: auto` absorbs all free space in both directions.
</details>

---

### Exercise 4: Pull Footer Upward with Negative Margin (Medium)
**Task**: A card has an image at the bottom. Use a negative margin on a `.card-footer` pill to pull it $20\text{px}$ upward so it overlaps the bottom of the card image.

<details>
<summary><strong>View Solution</strong></summary>

```css
.card-media {
  display: block;
  width: 100%;
  height: 200px;
  object-fit: cover;
}

.card-footer {
  position: relative; /* Ensure it stacks on top of the image */
  margin-top: -20px;  /* Pulls upward by 20px */
  margin-inline: 1rem;
  background: #ffffff;
  padding: 0.75rem 1rem;
  border-radius: 8px;
  box-shadow: 0 4px 6px rgba(0, 0, 0, 0.1);
}
```
*Explanation*: `margin-top: -20px` pulls the element upward into the preceding image's space.
</details>

---

### Exercise 5: Breakout Full-Bleed Section (Hard)
**Task**: An article container has `padding-inline: 2rem` ($32\text{px}$). Write the CSS for `.breakout-figure` to break out horizontally to the container's outer edges.

<details>
<summary><strong>View Solution</strong></summary>

```css
.article-container {
  max-width: 800px;
  margin-inline: auto;
  padding-inline: 2rem; /* 32px padding on each side */
}

.breakout-figure {
  margin-inline: -2rem; /* Pull left and right by 32px */
  width: calc(100% + 4rem); /* Expand total width by 64px */
  display: block;
}
```
*Explanation*: Negative inline margins neutralize the container's inline padding.
</details>

---

### Exercise 6: Programmatic Spacing Scale Audit in JavaScript (Hard)
**Task**: Write a JavaScript utility function `auditSpacingScale(element)` that reads the computed padding and margins of an element and verifies whether they conform to an 8-point grid scale (multiples of 4px).

<details>
<summary><strong>View Solution</strong></summary>

```javascript
/**
 * Verifies whether an element's padding and margins conform to an 8pt grid (4px increments).
 * @param {HTMLElement} el
 * @returns {{ valid: boolean, nonConforming: string[] }}
 */
function auditSpacingScale(el) {
  const s = window.getComputedStyle(el);
  const properties = [
    'paddingTop', 'paddingRight', 'paddingBottom', 'paddingLeft',
    'marginTop', 'marginRight', 'marginBottom', 'marginLeft'
  ];

  const nonConforming = [];

  for (const prop of properties) {
    const val = parseFloat(s[prop]);
    if (!isNaN(val) && val > 0) {
      // Check if value is a clean multiple of 4px
      if (val % 4 !== 0) {
        nonConforming.push(`${prop}: ${val}px`);
      }
    }
  }

  return {
    valid: nonConforming.length === 0,
    nonConforming
  };
}

const card = document.querySelector('.article-card');
console.log('Spacing Scale Audit:', auditSpacingScale(card));
```
*Explanation*: Verifies that computed spacing conforms to standard design system increments.
</details>

---

## 12. Summary / Cheat Sheet

```
+-------------------------------------------------------------------------------------------------+
| MARGIN & PADDING MASTER CHEAT SHEET                                                             |
+-------------------+----------------------------+-----------------------+------------------------+
| Feature           | Margin                     | Padding               | Key Behavioral Rule    |
+-------------------+----------------------------+-----------------------+------------------------+
| Zone              | Outside border             | Inside border         | Padding paints BG      |
| Hit Testing       | Passes clicks through      | Receives clicks       | Use padding for a11y   |
| Negative Values   | YES (pulls elements)       | NO (Syntax Error!)    | Margins can overlap    |
| margin: auto (H)  | Centers in block flow      | N/A                   | Requires width/max-w   |
| margin: auto (V)  | Computes to 0px in block   | N/A                   | Works in Flex & Grid   |
| % Calculation     | Uses container WIDTH       | Uses container WIDTH  | Both axes use width!   |
| Logical Syntax    | margin-inline, -block      | padding-inline, -block| Internationalization   |
+-------------------+----------------------------+-----------------------+------------------------+
```

---

## 13. Further Reading

- [W3C CSS Box Model Module Level 3](https://www.w3.org/TR/css-box-3/)
- [MDN Web Docs: margin](https://developer.mozilla.org/en-US/docs/Web/CSS/margin)
- [MDN Web Docs: padding](https://developer.mozilla.org/en-US/docs/Web/CSS/padding)
- [web.dev: Learn CSS Margin](https://web.dev/learn/css/margin)
- [CSS-Tricks: The Math Behind CSS Auto Margins](https://css-tricks.com/the-math-behind-css-auto-margins/)

---

[Previous: 02 - Box Sizing: content-box vs border-box](02-box-sizing-content-vs-border.md) | [Section Index](README.md) | [Next: 04 - Margin Collapsing: Rules & Workarounds](04-margin-collapsing.md)
