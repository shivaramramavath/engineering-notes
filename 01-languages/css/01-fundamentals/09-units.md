# CSS Units: Absolute vs Relative

| Metadata | Details |
|:---|:---|
| **Level** | Foundational |
| **Prerequisites** | `01-fundamentals/01-syntax-rules-and-declarations.md`, `01-fundamentals/08-inheritance-initial-unset-revert.md` |
| **Estimated Reading Time** | 29 minutes |
| **Status** | Current |
| **Baseline Status** | Widely Available (`px`, `rem`, `em`, `vw`, `vh`: Universal; `svh`, `lvh`, `dvh`, `cqi`: Baseline 2023) |
| **Browsers Tested** | Chrome 120+, Firefox 121+, Safari 17.2+, Edge 120+ |
| **Tested With** | W3C CSS Values and Units Level 4, Playwright css_runner.js, stylelint |

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

### What are CSS Units?
Every distance, dimension, font size, animation duration, and angle in CSS requires an explicit **unit of measurement**. Units in CSS are partitioned into two fundamental families:
1. **Absolute Units**: Fixed physical or anchored units (e.g., `px`, `cm`, `mm`, `in`, `pt`) that do not change based on viewport dimensions or parent element font sizes.
2. **Relative Units**: Dynamic units (e.g., `rem`, `em`, `ch`, `vw`, `vh`, `svh`, `dvh`, `cqi`, `%`) whose computed pixel values are calculated relative to another metric (such as root font size, parent font size, viewport dimensions, or container queries).

### Why Do We Need Modern Relative Units?
In the early web, websites were designed for fixed $1024 \times 768$ desktop monitors using absolute pixels (`px`). Today, interfaces must render gracefully across smartphones ($375\text{px}$), tablets, laptops, $4\text{K}$ monitors ($3840\text{px}$), and assistive technologies with user-configured font scales ($200\%$).

Using flexible, accessible relative units (`rem`, `ch`, `dvh`) ensures that layouts fluidly adapt to device screens and respect user accessibility preferences without clipping content.

---

## 2. Core Concepts

### 1. Absolute Units: The CSS Reference Pixel (`px`)
In CSS, **`1px` is NOT a single physical hardware pixel** on your screen!
The W3C defines a CSS pixel as a **visual reference angle**: the angle of $1/96$th of an inch viewed at arm's length.
- On a standard $96\text{ DPI}$ monitor: $1\text{ CSS px} = 1\text{ physical hardware pixel}$.
- On a high-density "Retina" smartphone display (e.g., $3\times$ DPR): $1\text{ CSS px} = 3 \times 3 = 9\text{ physical hardware pixels}$!

Other absolute units are anchored to pixels: $1\text{in} = 96\text{px}$, $1\text{pt} = 4/3\text{px}$ ($72\text{pt} = 1\text{in}$).

### 2. Relative Font Units: `rem`, `em`, `ch`
- **`rem` (Root EM)**: Relative to the font size of the root element (`<html>`). If root font size is $16\text{px}$, then $1\text{rem} = 16\text{px}$, $1.5\text{rem} = 24\text{px}$, $2\text{rem} = 32\text{px}$. *Crucial for accessibility: Scales automatically when users increase default browser font size.*
- **`em`**: Relative to the font size of the **current element** (for properties like `padding` or `margin`), or relative to the **parent element's font size** (when used on `font-size`). Caution: `em` compounds in nested trees!
- **`ch`**: The advance width of the `"0"` (zero) glyph in the current font. Ideal for controlling accessible paragraph line lengths (`max-width: 65ch`).
- **`ex`**: The x-height of the current font (height of lowercase `"x"`).

### 3. The Modern Viewport Units Family
To resolve the infamous mobile 100vh bug (where browser address bars overlap content), CSS Values 4 introduced three distinct viewport variants:

```
+-------------------------------------------------------------------------+
| VIEWPORT UNIT FAMILIES                                                  |
+------------+------------------------------------------------------------+
| Unit Type  | Behavior on Mobile with Collapsible Browser Toolbars       |
+------------+------------------------------------------------------------+
| Large      | Assumes browser URL bar is RETRACTED (Maximum screen area) |
| (lvh, lvw) | Content can be overlapped when toolbars expand.            |
+------------+------------------------------------------------------------+
| Small      | Assumes browser URL bar is EXPANDED (Minimum screen area)  |
| (svh, svw) | 100svh is GUARANTEED never to be covered by browser UI!    |
+------------+------------------------------------------------------------+
| Dynamic    | Adjusts continuously in real-time as user scrolls and      |
| (dvh, dvw) | browser toolbars expand/retract.                           |
+------------+------------------------------------------------------------+
```

### 4. Container Query Units
When using CSS Container Queries (`container-type: inline-size`):
- **`cqw` / `cqh`**: $1\%$ of container's width / height.
- **`cqi` / `cqb`**: $1\%$ of container's inline / block size.
- **`cqmin` / `cqmax`**: The smaller / larger value between `cqi` and `cqb`.

---

## 3. How It Works Internally

### The Device Pixel Ratio (DPR) & Physical Rasterization

```
+--------------------------------------------------------------------+
| CSS CODE DECLARATION: width: 100px;                                |
+--------------------------------------------------------------------+
                                |
                                v
+--------------------------------------------------------------------+
| BROWSER ENGINE COMPUTED STYLE: 100 CSS Pixels                      |
+--------------------------------------------------------------------+
                                |
                                v
+--------------------------------------------------------------------+
| GPU RASTERIZATION PIPELINE                                         |
| Multiplies by window.devicePixelRatio (DPR):                       |
| - Standard Display (DPR = 1.0) -> Renders on 100 physical pixels   |
| - 4K Desktop Display (DPR = 1.5) -> Renders on 150 physical pixels |
| - High-End Phone (DPR = 3.0)   -> Renders on 300 physical pixels   |
+--------------------------------------------------------------------+
```

### Percentage Calculation Contexts
Percentages (`%`) evaluate against different boxes depending on the property:
- **`width`, `max-width`, `left`, `right`**: Relative to the containing block's **width**.
- **`height`, `top`, `bottom`**: Relative to the containing block's **height** (fails/collapses if parent has `height: auto`).
- **`padding` and `margin`**: **BOTH horizontal AND vertical padding/margin are calculated relative to the containing block's WIDTH!**
  - *Why?*: If vertical padding were calculated relative to parent height, altering padding would alter parent height, causing an infinite layout recalculation loop!

---

## 4. Syntax & Usage

```
+-------------------------------------------------------------------------+
| Web Platform Baseline: Widely Available                                  |
| px, rem, em, %, vw, vh, ch are universal across all web browsers.      |
| svh, lvh, dvh, cqi are Widely Available (Baseline 2023 - All engines).  |
+-------------------------------------------------------------------------+
```

### Standard Units Declaration Syntax
```css
:root {
  /* Establish root typographic scale */
  font-size: 100%; /* Defaults to 16px, respects user OS setting */
}

.hero {
  /* Full viewport height guaranteed not to clip behind mobile address bar */
  min-height: 100svh;
  padding: 2rem 1.5rem;
}

.readable-article {
  /* Optimal reading line length: ~65 characters */
  max-width: 65ch;
  margin-inline: auto;
  font-size: 1.125rem; /* 18px */
  line-height: 1.6;
}
```

---

## 5. Practical Examples

### Example 1: Accessible Typographic Scale with `rem`
```css
/* Base document settings */
html {
  font-size: 100%; /* 16px default */
}

/* Fluid typographic hierarchy using rem */
h1 {
  font-size: 2.5rem;   /* 40px */
  margin-bottom: 1rem; /* 16px */
  line-height: 1.2;
}

h2 {
  font-size: 1.75rem;  /* 28px */
  margin-bottom: 0.75rem; /* 12px */
  line-height: 1.3;
}

p {
  font-size: 1rem;     /* 16px */
  margin-bottom: 1.5rem; /* 24px */
  line-height: 1.6;
}
```
*Accessibility Verification*: If a user with low vision changes their browser preference to $24\text{px}$ default font size, every single heading and paragraph automatically scales up by $150\%$ without any text truncation.

### Example 2: Real-World Mobile Full-Screen Hero (`dvh` & `svh`)
Solving the mobile browser address bar collapse bug:
```html
<section class="hero-screen">
  <div class="hero-content">
    <h1>Experience Modern Web Architecture</h1>
    <button class="cta-btn">Get Started</button>
  </div>
</section>
```
```css
.hero-screen {
  display: grid;
  place-items: center;
  /* Fallback for legacy browsers */
  min-height: 100vh;
  /* Modern safe viewport: never clipped by mobile navigation bars */
  min-height: 100svh;
  background-color: #0f172a;
  color: #ffffff;
  padding: 1.5rem;
}

.cta-btn {
  background-color: #0284c7;
  color: #ffffff;
  padding: 0.75rem 1.5rem;
  border-radius: 6px;
  border: none;
  font-size: 1rem;
  cursor: pointer;
}

.cta-btn:focus-visible {
  outline: 2px solid #38bdf8;
  outline-offset: 2px;
}
```

### Example 3: Edge Case — Compounding `em` in Nested Trees
A classic CSS layout disaster caused by `em` inheriting parent font sizes:
```html
<ul class="nested-list">
  <li>Level 1
    <ul>
      <li>Level 2
        <ul>
          <li>Level 3</li>
        </ul>
      </li>
    </ul>
  </li>
</ul>
```
```css
/* Anti-Pattern: em on font-size compounds exponentially! */
.nested-list li {
  font-size: 1.2em; /* 1st level: 19.2px, 2nd: 23px, 3rd: 27.6px! */
}

/* Production Fix: Use rem for predictable font sizing */
.nested-list li {
  font-size: 1.125rem; /* Stays locked at 18px across all nested levels */
}

/* em IS appropriate for padding that scales proportionally with font size */
.nested-list li {
  padding-left: 0.5em; /* Scales proportionally if font-size changes */
}
```

### Example 4: Failure Case — Legacy `100vh` on iOS Safari
```css
/* Broken on mobile Safari and Chrome */
.modal-overlay {
  position: fixed;
  inset: 0;
  height: 100vh; /* Extends underneath the browser bottom navigation bar! */
}
```
*Failure Analysis*: On mobile devices, `100vh` is calculated based on the maximum screen height with browser toolbars hidden. When the user opens the page and the address bar is visible, the bottom $60\text{px}$ of the modal (often holding the "Save" or "Close" button) is hidden beneath the address bar!
*Solution*: Use `100dvh` or `100svh`.

### Example 5: Accessible Prose Container with `ch` Units
The Web Content Accessibility Guidelines (WCAG) and typographic research demonstrate that line lengths between $50$ and $75$ characters per line provide the highest reading speed and lowest cognitive fatigue:
```css
.article-prose {
  /* Automatically locks text width to exactly 65 zero characters */
  max-width: 65ch;
  margin-inline: auto; /* Center horizontally */
  padding-inline: 1.5rem;
  font-family: system-ui, -apple-system, sans-serif;
  font-size: 1.125rem;
  line-height: 1.7;
  color: #334155;
}
```
*Playwright Test Runner Verification*:
```javascript
// Test computed pixel width of 65ch container
const el = document.querySelector('.article-prose');
const computed = window.getComputedStyle(el);
console.log('Max Width:', computed.maxWidth); // Computes to exact px based on font
```

---

## 6. Best Practices

1. **Use `rem` for Font Sizes and Global Spacing**: Guarantee accessibility compliance by building layouts with `rem` so user font preferences scale the interface.
2. **Never Set `html { font-size: 10px; }` (The "62.5% Hack")**: Historically used to make $1\text{rem} = 10\text{px}$, setting fixed font sizes on `html` destroys accessibility for users who configure larger browser default font sizes.
3. **Use `ch` for Reading Text Containers**: Cap article and editorial paragraphs with `max-width: 65ch` to guarantee optimal typographic line lengths.
4. **Use `em` for Local Component Padding**: Apply `em` to button padding (`padding: 0.5em 1em`) so the padding scales proportionally when the button font size is modified.
5. **Use `svh` or `dvh` Instead of `vh` on Mobile**: Prevent buttons from disappearing under mobile browser address bars by adopting small (`svh`) or dynamic (`dvh`) viewport units.
6. **Use `px` Strictly for Sub-pixel Borders and Shadows**: Absolute pixels remain ideal for hairline dividers (`border: 1px solid`) and box shadows (`box-shadow: 0 4px 6px`).

---

## 7. Common Mistakes & Pitfalls

| Mistake | Root Cause | Symptom / Failure | Correct Pattern |
|:---|:---|:---|:---|
| Using `px` for typography | Habit from graphic design software | Text does not scale when visually impaired user zooms browser font. | Use `rem` for font sizes. |
| Overusing `em` on nested font-size | Compounding multiplication | Nested menus or comments grow uncontrollably large or tiny. | Use `rem` for `font-size`; reserve `em` for padding. |
| Using `100vh` for full-screen hero sections | Mobile address bar dynamic height | Bottom buttons clip behind mobile navigation chrome. | Use `min-height: 100svh` or `100dvh`. |
| Assuming vertical percentage padding uses height | Misunderstanding containing block spec | Vertical padding is much larger than expected on wide screens. | Percentage padding uses container WIDTH for both axes. |
| Forgetting viewport scrollbar width with `100vw` | Desktop scrollbar is part of `100vw` | Causes horizontal scrollbars and layout jank on Windows browsers. | Use `width: 100%` on container blocks instead of `100vw`. |
| Hardcoding `html { font-size: 62.5%; }` | Convenient mental math ($1\text{rem}=10\text{px}$) | Breaks third-party component libraries expecting $16\text{px}$ base. | Embrace standard $16\text{px}$ rem math ($1\text{rem}=16\text{px}$). |

---

## 8. Performance, Accessibility & Browser Support

### Performance of Dynamic Viewport Units (`dvh`)
- While `dvh` recalculates dynamically on scroll, modern browser engines (Chromium Blink, WebKit) handle toolbar expansion via composited viewport metadata.
- However, animating heavy CSS transitions tied to `dvh` during fast momentum scrolling can trigger layout recalculations.
- *Best Practice*: For static hero sections, prefer **`min-height: 100svh`**; it provides full viewport height without recalculating on every scroll frame!

### Accessibility Safeguards
- **WCAG 1.4.4 (Resize Text)**: Requires text to be scalable up to $200\%$ without assistive technology and without loss of content or functionality. Using `rem` units guarantees compliance.

---

## 9. Comparison / When to Use What

| Unit | Scaled By | Compounding? | Ideal Use Case |
|:---|:---|:---:|:---|
| **`rem`** | Root `<html>` font size | **No** | Font sizes, layout grid gaps, margins |
| **`em`** | Current / parent font size | **Yes** | Button padding, icon alignment relative to text |
| **`px`** | Hardware angle ($1/96\text{ in}$) | **No** | Borders (`1px solid`), small drop shadows |
| **`ch`** | Width of `"0"` character | **No** | Prose line length (`max-width: 65ch`) |
| **`svh`** | Small viewport height (mobile bar out)| **No** | Mobile heroes, full-screen dialogs |
| **`dvh`** | Real-time viewport height | **No** | Interactive canvas overlays, full-screen menus |
| **`cqi`** | Container Query inline size | **No** | Component-relative responsive typography |

---

## 10. Interview Questions

<details>
<summary><strong>1. Why is a CSS pixel (`px`) described as an "angular measurement" rather than a physical hardware dot?</strong></summary>

According to the W3C CSS specification, $1\text{px}$ is defined as the visual angle of $1/96$th of an inch viewed at arm's length (roughly $0.0213^\circ$). Because high-density smartphone screens have pixel densities of $400+\text{ PPI}$ while living-room televisions have low PPI viewed from 10 feet away, anchoring CSS pixels to physical hardware dots would make text unreadably microscopic on phones and gargantuan on TVs. The visual angle definition ensures consistent visual perceived size across all devices and viewing distances.
</details>

<details>
<summary><strong>2. What is the mobile 100vh bug, and how do modern dynamic viewport units (`svh`, `lvh`, `dvh`) resolve it?</strong></summary>

On mobile browsers (iOS Safari, Chrome for Android), the address bar and navigation toolbar retract when scrolling down and expand when scrolling up. Historically, `100vh` was fixed to the maximum screen height with toolbars retracted. When a user opened a page with `height: 100vh`, the bottom portion of the container was hidden behind the address bar.
- `svh` (Small Viewport Height): Measures height when toolbars are expanded (guaranteeing content is never clipped).
- `lvh` (Large Viewport Height): Measures height when toolbars are hidden.
- `dvh` (Dynamic Viewport Height): Dynamically recalculates height in real time as toolbars animate.
</details>

<details>
<summary><strong>3. Why do vertical `margin` and `padding` percentages evaluate against the containing block's width instead of height?</strong></summary>

In CSS layout, an element's height typically depends on the height of its children (flow content). If vertical padding or margin were calculated as a percentage of the parent's height, increasing the child's padding would increase the parent's height, which in turn would increase the child's padding again, causing an **infinite layout recalculation loop**. To ensure deterministic, one-pass layout calculations, the W3C specified that both horizontal and vertical padding/margin percentages are calculated relative to the containing block's **width**.
</details>

<details>
<summary><strong>4. What is the compounding problem with `em` units?</strong></summary>

When `em` is used for `font-size`, its value is relative to the parent element's font size. In deeply nested markup (such as nested lists `<ul><li><ul><li>...</li></ul></li></ul>`), declaring `li { font-size: 1.2em; }` causes each nested level to multiply the previous level ($1.2 \times 1.2 \times 1.2 = 1.728$). This causes nested text to balloon in size. Using `rem` avoids this because `rem` is always anchored to the single root font size.
</details>

<details>
<summary><strong>5. Why is setting `width: 100vw` on desktop browsers frequently considered an anti-pattern?</strong></summary>

On desktop operating systems that display traditional non-overlay scrollbars (like Windows), the vertical scrollbar takes up roughly $15\text{px}–17\text{px}$ of screen real estate. The `100vw` unit includes the width of the scrollbar, whereas the document layout does not. Therefore, setting `width: 100vw` makes the element wider than the visible page canvas, triggering an ugly and unnecessary **horizontal scrollbar**. Using `width: 100%` correctly respects the document canvas boundary.
</details>

<details>
<summary><strong>6. What is the difference between `cqw` and `vw`?</strong></summary>

`vw` measures $1\%$ of the global browser window viewport width. `cqw` (Container Query Width) measures $1\%$ of the width of the nearest ancestor element configured as a CSS container (`container-type: inline-size` or `size`). This allows micro-components to scale their font size and padding relative to their parent container widget, regardless of how large the overall browser window is.
</details>

<details>
<summary><strong>7. Why is `ch` considered one of the best typographic units in CSS?</strong></summary>

The `ch` unit represents the advance measure of the `"0"` (zero) character of the element's font. In typography, readable line length is measured by character count (ideally 45–75 characters per line). By setting `max-width: 65ch` on prose elements, the container width scales automatically with the font family and font size, guaranteeing an optimal reading measure regardless of zoom levels.
</details>

<details>
<summary><strong>8. Why should you avoid resetting `html { font-size: 62.5%; }` in modern design systems?</strong></summary>

While `62.5%` simplifies mental math by setting $1\text{rem} = 10\text{px}$ (assuming $16\text{px}$ default), it creates severe friction when integrating third-party component libraries (like Radix, Shoelace, or Tailwind) that assume the standard $1\text{rem} = 16\text{px}$ ratio. Furthermore, if a user has configured an accessible browser default font of $24\text{px}$, $62.5\%$ results in $15\text{px}$, frustrating their accessibility settings.
</details>

---

## 11. Practice Exercises

### Exercise 1: Build an Optimal Reading Measure Container (Easy)
**Task**: Write the CSS for `.prose-container` that centers an article on screen, ensures the maximum reading line length never exceeds approximately 65 characters, and provides 1rem inline padding on small screens.

<details>
<summary><strong>View Solution</strong></summary>

```css
.prose-container {
  max-width: 65ch;
  margin-inline: auto;
  padding-inline: 1rem;
}
```
*Explanation*: `max-width: 65ch` constrains the line length based on font character width, while `margin-inline: auto` centers the block.
</details>

---

### Exercise 2: Mobile Full-Height Container with Fallback (Easy)
**Task**: Write the CSS for a full-screen mobile splash screen `.splash` that provides a fallback for legacy browsers, but uses modern small viewport height (`svh`) to guarantee buttons are never covered by mobile browser navigation bars.

<details>
<summary><strong>View Solution</strong></summary>

```css
.splash {
  /* Legacy fallback */
  min-height: 100vh;
  /* Modern safe mobile height */
  min-height: 100svh;
  display: flex;
  flex-direction: column;
  justify-content: space-between;
}
```
*Explanation*: Browsers parse sequentially; modern engines overwrite `100vh` with the superior `100svh`.
</details>

---

### Exercise 3: Scale Button Padding Proportionally with `em` (Medium)
**Task**: Build a base button `.btn` and a modifier `.btn--lg`. Use `em` units for padding so that when the modifier increases `font-size`, the padding scales proportionally without needing to re-declare padding values!

<details>
<summary><strong>View Solution</strong></summary>

```css
.btn {
  display: inline-flex;
  align-items: center;
  font-size: 1rem; /* 16px */
  /* Padding is relative to the button's font-size! */
  padding: 0.5em 1em; /* Computes to: 8px top/bottom, 16px left/right */
  border-radius: 0.25rem;
  background-color: #0284c7;
  color: #ffffff;
  border: none;
  cursor: pointer;
}

.btn:focus-visible {
  outline: 2px solid #0284c7;
  outline-offset: 2px;
}

/* Modifier: Modifying ONLY font-size automatically scales padding! */
.btn--lg {
  font-size: 1.5rem; /* 24px -> Padding becomes 12px (0.5em) and 24px (1em)! */
}
```
*Explanation*: Because `em` on padding is relative to the element's *own* font-size, scaling `font-size` scales the button's padding automatically.
</details>

---

### Exercise 4: Prevent Desktop Horizontal Scrollbar Bug (Medium)
**Task**: An engineer writes `body { width: 100vw; overflow-x: hidden; }` to fix an accidental horizontal scrollbar on Windows. Explain why `width: 100vw` was the root cause and provide the clean architectural fix.

<details>
<summary><strong>View Solution</strong></summary>

*Explanation*: `100vw` includes the width of the desktop vertical scrollbar. If a page is taller than the window, the scrollbar appears, and `100vw` causes the body to be $15\text{px}$ wider than the client viewport, creating an artificial horizontal overflow.

*Fix*:
```css
body {
  /* Replace 100vw with 100% */
  width: 100%;
  margin: 0;
  box-sizing: border-box;
}
```
*Explanation*: `width: 100%` calculates width relative to the document root, which strictly excludes the scrollbar.
</details>

---

### Exercise 5: Fluid Container Query Typography with `cqi` (Hard)
**Task**: Configure a card component `.product-card` as an inline-size container. Style its title `.product-card__title` so that its font size scales dynamically at $5\%$ of the container's inline width, clamped between $1.25\text{rem}$ ($20\text{px}$) and $2.5\text{rem}$ ($40\text{px}$).

<details>
<summary><strong>View Solution</strong></summary>

```css
.product-card {
  /* 1. Register element as a container */
  container-type: inline-size;
  container-name: product;
  padding: 1.5rem;
  border: 1px solid #e2e8f0;
  border-radius: 8px;
}

.product-card__title {
  /* 2. Fluid clamp scaling using container query units (cqi) */
  font-size: clamp(1.25rem, 5cqi, 2.5rem);
  font-weight: 700;
  color: #0f172a;
  line-height: 1.2;
}
```
*Explanation*: `cqi` evaluates against the `.product-card` width, making the typography responsive to where the widget is placed (in a narrow sidebar vs a wide main column).
</details>

---

### Exercise 6: Programmatic DPR and Viewport Measurement (Hard)
**Task**: Write a JavaScript utility function `measureViewportMetrics()` that returns the current Device Pixel Ratio (DPR), window inner dimensions, and indicates whether the user's browser supports modern dynamic viewport units.

<details>
<summary><strong>View Solution</strong></summary>

```javascript
/**
 * Measures client viewport and display hardware properties.
 * @returns {Object}
 */
function measureViewportMetrics() {
  const dpr = window.devicePixelRatio || 1;
  const innerWidth = window.innerWidth;
  const innerHeight = window.innerHeight;

  // Test dynamic viewport unit support via CSS.supports
  const supportsDVH = typeof CSS !== 'undefined' && CSS.supports('height', '100dvh');
  const supportsSVH = typeof CSS !== 'undefined' && CSS.supports('height', '100svh');

  return {
    dpr,
    physicalPixelWidth: innerWidth * dpr,
    physicalPixelHeight: innerHeight * dpr,
    supportsDVH,
    supportsSVH
  };
}

console.log('Client Metrics:', measureViewportMetrics());
```
*Explanation*: Combines window metrics with `CSS.supports()` to inspect runtime layout capabilities.
</details>

---

## 12. Summary / Cheat Sheet

```
+-------------------------------------------------------------------------------------------------+
| CSS UNITS MASTER CHEAT SHEET                                                                    |
+-------------------+----------------------------+-----------------------+------------------------+
| Unit              | Type                       | Reference Relative To | Primary Use Case       |
+-------------------+----------------------------+-----------------------+------------------------+
| px                | Absolute                   | 1/96th inch visual ang| Hairline borders,shadow|
| rem               | Relative                   | Root <html> font size | Typography, layout gap |
| em                | Relative                   | Element's font size   | Button padding, icons  |
| ch                | Relative                   | Width of "0" character| Max reading line length|
| %                 | Relative                   | Containing block      | Fluid grid columns     |
| svh / svw         | Viewport                   | Small viewport (bars) | Safe mobile hero/modal |
| dvh / dvw         | Viewport                   | Real-time dynamic view| Fullscreen overlay menu|
| lvh / lvw         | Viewport                   | Large viewport        | Background landscapes  |
| cqi / cqw         | Container                  | 1% of container width | Widget component scale |
+-------------------+----------------------------+-----------------------+------------------------+
```

---

## 13. Further Reading

- [W3C CSS Values and Units Module Level 4](https://www.w3.org/TR/css-values-4/)
- [MDN Web Docs: CSS Values and Units](https://developer.mozilla.org/en-US/docs/Web/CSS/CSS_Values_and_Units)
- [web.dev: The large, small, and dynamic viewport units](https://web.dev/articles/viewport-units)
- [CSS-Tricks: The 100vh Problem on Mobile](https://css-tricks.com/the-trick-to-viewport-units-on-mobile/)
- [Ahmad Shadeed: CSS Viewport Units](https://ishadeed.com/article/new-viewport-units/)

---

[Previous: 08 - Inheritance: initial, inherit, unset, revert](08-inheritance-initial-unset-revert.md) | [Section Index](README.md) | [Next: 10 - Colours & Color Spaces](10-colours.md)
