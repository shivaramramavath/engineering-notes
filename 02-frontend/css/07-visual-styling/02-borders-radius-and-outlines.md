# Borders, Corner Radii & Outlines

| Metadata | Details |
| :--- | :--- |
| **Topic** | Border Mechanics, Elliptical Corner Radii, Logical Radii, Outlines & `outline-offset`, `:focus-visible`, Windows High Contrast Mode Defense |
| **Spec Level** | CSS Backgrounds and Borders Module Level 3 / CSS Basic User Interface Module Level 4 |
| **Baseline Status** | Widely Available (`border`, `border-radius`, `outline` Baseline 2015; `:focus-visible` Baseline 2022; Logical border radii Baseline 2021) |
| **Browsers Tested** | Chrome 120+, Firefox 122+, Safari 17.2+, Edge 120+ |
| **Tested With** | W3C CSS Validator, Chromium LayoutNG, Safari WebKit, Windows High Contrast Mode |
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

Borders, corner radii, and outlines define the perimeter geometry and interactive focus states of user interface components. While they appear simple on the surface, confusing their underlying rendering behaviors is one of the most common causes of layout shift and accessibility violations:
- **Box Model Displacement**: Borders occupy physical space inside the box model (`box-sizing: border-box`). Dynamically adding or increasing a border on hover triggers layout reflows and jitter.
- **Non-Displacing Outlines**: Outlines are drawn outside the box model. They occupy zero space, never cause layout shifts, and support negative or positive spacing via `outline-offset`.
- **The `:focus-visible` Standard (Baseline 2022)**: Eliminating jarring blue outline rectangles for mouse and touchscreen clicks while preserving high-visibility focus indicators for keyboard-only users.
- **Windows High Contrast Mode (WHCM)**: In `forced-colors: active`, browsers strip `box-shadow` and background colors. Elements without borders blend invisibly into the canvas unless protected by transparent borders.

---

## 2. Core Concepts

### Border Properties & Logical Sides
- `border-width`: Thin, medium, thick, or explicit lengths (`1px`, `0.2rem`).
- `border-style`: `solid`, `dashed`, `dotted`, `double`, `groove`, `ridge`, `inset`, `outset`, `none`.
- `border-color`: Any CSS color, defaults to `currentColor`.
- **Logical Side Properties**:
  - `border-block-start` (Top in horizontal LTR)
  - `border-block-end` (Bottom)
  - `border-inline-start` (Left in LTR, Right in RTL)
  - `border-inline-end` (Right in LTR, Left in RTL)

### `border-radius` (Circular vs Elliptical)
`border-radius` curves the outer border edge and clips the background painting area:
- **Circular Corners**: A single radius value (`border-radius: 12px;`).
- **Pill / Fully Rounded**: `border-radius: 9999px;` (clamped automatically by the browser to half the shorter dimension).
- **Elliptical Corners**: Declared with a forward slash separating horizontal and vertical radii:
  ```css
  /* horizontal-radii / vertical-radii */
  border-radius: 40px 20px / 20px 40px;
  ```
- **Logical Radii (Baseline 2021)**:
  - `border-start-start-radius` (Top-Left in LTR)
  - `border-start-end-radius` (Top-Right in LTR)
  - `border-end-start-radius` (Bottom-Left in LTR)
  - `border-end-end-radius` (Bottom-Right in LTR)

### Outlines & `outline-offset`
Unlike borders, outlines:
1. Do not take up space in the document flow (zero layout reflow).
2. Are drawn outside (or inside) the element without altering its bounding box.
3. Support `outline-offset: 4px;` (creates an empty air gap between the element and the outline).
4. Can accept negative offsets (`outline-offset: -4px;`) to draw the ring entirely inside the element.

### The Accessible Focus Law
```css
/* FORBIDDEN: Anti-accessibility violation */
button:focus {
  outline: none; /* Strips keyboard navigation indicator! */
}

/* BEST PRACTICE: Modern Baseline focus-visible */
button:focus-visible {
  outline: 2px solid #2563eb;
  outline-offset: 2px;
}
```

---

## 3. How It Works Internally

```
Border vs Outline Box Model Anatomy

+--------------------------------------------------------------+
| Outline (Drawn outside; occupies ZERO layout width/height)   |
|   <--- outline-offset (Air gap) --->                         |
|   +------------------------------------------------------+   |
|   | Border (Occupies box-sizing space)                   |   |
|   |   +----------------------------------------------+   |   |
|   |   | Padding                                      |   |   |
|   |   |   +--------------------------------------+   |   |   |
|   |   |   | Content Box                          |   |   |   |
|   |   |   +--------------------------------------+   |   |   |
|   |   +----------------------------------------------+   |   |
|   +------------------------------------------------------+   |
+--------------------------------------------------------------+
```

### Radius Clamping Algorithm
When the sum of adjacent radii exceeds the length of the border box edge, the browser applies a proportional scaling factor ($f$):
$$f = \min\left(\frac{L_{\text{top}}}{r_{\text{top-left}} + r_{\text{top-right}}}, \frac{L_{\text{bottom}}}{r_{\text{bottom-left}} + r_{\text{bottom-right}}}, \dots\right)$$
If $f < 1$, all radii are multiplied by $f$, ensuring corners curve smoothly without overlapping or creating sharp kinks.

---

## 4. Syntax & Usage

### Baseline & Feature Support
- **Core `border` & `border-radius`**: Baseline Widely Available.
- **`outline-offset`**: Baseline Widely Available (Chrome 1+, Safari 1.2+, Firefox 1.5+).
- **`:focus-visible`**: Baseline 2022 (Chrome 86+, Safari 15.4+, Firefox 85+).
- **Logical Border Radii**: Baseline 2021 (Chrome 89+, Safari 15+, Firefox 66+).

```css
/* Standard Component Surface */
.card {
  border: 1px solid #e2e8f0;
  border-radius: 12px;
  background-color: #ffffff;
}

/* Accessible Interactive Focus Ring */
.card:focus-visible {
  outline: 3px solid #2563eb;
  outline-offset: 3px;
}

/* Pill Badge */
.pill-badge {
  border-radius: 9999px;
  padding: 0.25rem 0.75rem;
}
```

---

## 5. Practical Examples

### Example 1: Reflow-Free Border Hover Effect
```css
/* ANTI-PATTERN (Causes layout shake):
.btn:hover { border: 2px solid #2563eb; }
Adding 2px expands the element's box model, nudging surrounding text! */

/* PATTERN A (Pre-allocated transparent border): */
.btn-reflow-free {
  border: 2px solid transparent;
  padding: 0.5rem 1rem;
  transition: border-color 0.2s ease;
}

.btn-reflow-free:hover {
  border-color: #2563eb;
}

/* PATTERN B (Using outline instead of border): */
.btn-outline-hover {
  border: none;
  outline: 2px solid transparent;
  outline-offset: -2px; /* Inset outline */
  transition: outline-color 0.2s ease;
}

.btn-outline-hover:hover {
  outline-color: #2563eb;
}
```

### Example 2: Organic Elliptical Card Geometry
```html
<div class="organic-banner">
  <h3>Algorithmic Fluid Dynamics</h3>
  <p>Simulating complex turbulence vectors across computational meshes.</p>
</div>
```

```css
.organic-banner {
  max-width: 440px;
  padding: 2rem;
  background: linear-gradient(135deg, #1e293b, #0f172a);
  color: #ffffff;
  
  /* Elliptical asymmetric corner geometry */
  border-radius: 40px 12px 35px 8px / 12px 35px 8px 40px;
  border: 1px solid rgba(255, 255, 255, 0.1);
}
```

### Example 3: Dual-Ring Focus Indicator for Universal Contrast
```css
/* A single blue focus ring is invisible against blue backgrounds!
   A dual-ring (white inner offset, black outer ring) guarantees high contrast on ANY surface. */
.universal-focus-button {
  padding: 0.75rem 1.5rem;
  background-color: #2563eb;
  color: #ffffff;
  border: none;
  border-radius: 6px;
  cursor: pointer;
}

.universal-focus-button:focus-visible {
  /* White gap + dark outer ring */
  outline: 2px solid #000000;
  outline-offset: 2px;
  box-shadow: 0 0 0 2px #ffffff;
}
```

### Example 4: Windows High Contrast Mode Border Defense
```css
/* In WHCM (forced-colors: active), box-shadows are stripped by the OS.
   A card relying purely on box-shadow vanishes into the background.
   Adding a transparent border guarantees a crisp system border appears in WHCM! */
.whcm-safe-card {
  background-color: #ffffff;
  border-radius: 8px;
  box-shadow: 0 4px 6px -1px rgba(0, 0, 0, 0.1);
  
  /* WHCM safety defense */
  border: 1px solid transparent;
}

@media (forced-colors: active) {
  .whcm-safe-card {
    border-color: CanvasText;
  }
}
```

### Example 5: Gradient Border via `border-image` or Pseudo-Element
```css
/* Pure CSS gradient border with rounded corners */
.gradient-border-card {
  position: relative;
  background-color: #0f172a;
  border-radius: 12px;
  padding: 1.5rem;
  color: #ffffff;
  
  /* Double background technique: clip inner to padding, outer to border */
  border: 2px solid transparent;
  background-image:
    linear-gradient(#0f172a, #0f172a),
    linear-gradient(135deg, #38bdf8, #818cf8, #c084fc);
  background-origin: border-box;
  background-clip: padding-box, border-box;
}
```

---

## 6. Best Practices

1. **Never Remove Outlines Without `:focus-visible` Replacement**: Never write `outline: none;` on its own. Always provide a clear, high-contrast replacement for keyboard navigators via `:focus-visible`.
2. **Use `outline-offset` to Prevent Focus Collisions**: Add `outline-offset: 2px;` or `3px;` to float the focus ring slightly away from the button perimeter.
3. **Pre-allocate Border Widths to Prevent Hover Layout Jitter**: Never transition `border-width` from `0px` to `2px` on hover. Set a baseline `2px solid transparent;` and transition only `border-color`.
4. **Always Include `border: 1px solid transparent` on Shadowed Cards**: Ensures containers remain clearly defined in Windows High Contrast Mode when `box-shadow` is stripped.
5. **Use Logical Radii for Multilingual Internationalization**: Use `border-start-start-radius` and `border-start-end-radius` so that asymmetrical corners mirror seamlessly in RTL languages.
6. **Use `border-radius: 9999px` for Pill Shapes**: Do not use `border-radius: 50%` on non-square buttons (which produces distorted ovals); `9999px` safely clamps to perfect semicircular end caps.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Correction |
| :--- | :--- | :--- |
| **`outline: none` without `:focus-visible`** | Traps keyboard-only and screen reader users; severe WCAG 2.4.7 violation. | Use `:focus-visible { outline: 2px solid ... }`. |
| **`border-radius: 50%` on a rectangle** | Distorts the button into an awkward squashed egg or oval shape. | Use `border-radius: 9999px;` for pill buttons. |
| **Hover effects adding a border** | Increases the box model dimensions, causing surrounding elements to jump. | Use `border: 1px solid transparent` or an `outline`. |
| **Single-color focus ring on matching background** | A blue focus ring on a blue button or hero section is completely invisible. | Use dual-ring focus (outline + outline-offset + box-shadow). |
| **Relying solely on `box-shadow` for card edges** | Windows High Contrast Mode removes all shadows, leaving cards visually boundary-less. | Add `border: 1px solid transparent;`. |
| **`border-image` breaking `border-radius`** | The CSS spec states `border-image` overrides and strips `border-radius` clipping. | Use the double background or pseudo-element technique. |

---

## 8. Performance, Accessibility & Browser Support

### Accessibility Compliance (WCAG 2.2 AA)
- **Criterion 2.4.7 (Focus Visible)**: Any keyboard-operable interface must have a visible focus indicator.
- **Criterion 2.4.11 (Focus Appearance - Level AAA)**: Focus indicators must have a contrast ratio of at least **3:1** against adjacent unfocused colors and a minimum perimeter thickness of 2px.

### GPU Layering of Outlines
Because `outline` is rendered during the paint phase without changing layout geometry, animating `outline-color` or `outline-offset` is significantly more performant than animating `border-width`.

---

## 9. Comparison / When to Use What

| Feature | Occupies Box Space | Radii Support | Offset Support | Primary Purpose |
| :--- | :--- | :--- | :--- | :--- |
| **`border`** | **Yes** (Alters dimensions) | Full (`border-radius`) | None | Surface framing, structure, table cells |
| **`outline`** | **No** (Zero layout footprint)| Modern browsers | **Yes** (`outline-offset`) | Accessible keyboard focus rings, hover halos |
| **`box-shadow`** | **No** | Full (`border-radius`) | Inset / Spread | Elevation, soft ambient lighting |

---

## 10. Interview Questions

<details>
<summary>1. What is the fundamental difference between <code>border</code> and <code>outline</code> in the CSS box model?</summary>

A `border` is an integral component of the CSS Box Model. It occupies physical layout dimensions (unless `box-sizing: border-box` is used, in which case it absorbs content/padding space). Adding or changing `border-width` alters the element's bounding box and triggers layout reflow of adjacent DOM elements.

An `outline` is painted outside the box model. It occupies zero physical space in the document flow, never shifts surrounding elements, supports negative or positive spacing via `outline-offset`, and can be drawn outside or inside an element without triggering a layout recalculation.
</details>

<details>
<summary>2. Why was <code>:focus-visible</code> introduced to replace <code>:focus</code>?</summary>

Historically, developers applied `button:focus { outline: none; }` because clicking a button with a mouse or touchscreen caused an unsightly browser focus rectangle to linger on the button. However, removing outlines completely broke navigation for keyboard-only users (who rely on `Tab` to navigate).

`:focus-visible` utilizes smart browser heuristics:
- When a user interacts with a mouse, pointer, or touchscreen, `:focus-visible` does **not** trigger.
- When a user navigates using the keyboard (`Tab`, arrow keys), `:focus-visible` **does** trigger, providing an accessible focus ring only when needed.
</details>

<details>
<summary>3. Why does <code>border-radius: 9999px</code> create a perfect pill button while <code>border-radius: 50%</code> does not?</summary>

- `border-radius: 50%` creates an ellipse whose horizontal radius is 50% of the width and vertical radius is 50% of the height. On a rectangular button (e.g., 200px wide by 50px tall), this produces an oval or egg shape.
- `border-radius: 9999px` requests an enormous corner curve. The W3C Corner Overlapping algorithm automatically scales down excessive radii proportionally. Because the height (50px) is smaller than the width, the browser clamps the radius to half the height (25px), resulting in flat horizontal top/bottom edges and perfectly semicircular ends.
</details>

<details>
<summary>4. Why should design systems include <code>border: 1px solid transparent</code> on cards styled with <code>box-shadow</code>?</summary>

In Windows High Contrast Mode (`forced-colors: active`), the operating system strips author-defined `box-shadow` and `background-color` properties to ensure legibility. 
If a white card on a light page relies solely on `box-shadow` to delineate its perimeter, in WHCM the card's boundary vanishes completely into the background. A `border: 1px solid transparent;` is automatically converted by WHCM into a high-contrast system border (`CanvasText`), keeping the card's boundaries visible.
</details>

<details>
<summary>5. How does the browser handle elliptical corner radii using the forward slash (<code>/</code>) syntax?</summary>

In `border-radius`, values before the slash define the **horizontal radii**, and values after the slash define the **vertical radii**:
```css
border-radius: 40px / 20px;
```
This instructs the corner curve to be an ellipse spanning 40px along the horizontal axis and 20px along the vertical axis.
</details>

<details>
<summary>6. Why does combining <code>border-image</code> with <code>border-radius</code> fail to clip the image to rounded corners?</summary>

According to the W3C CSS Backgrounds and Borders Level 3 specification, `border-image` is designed to slice and stretch a raster or vector graphic over the entire border area. The specification explicitly dictates that when `border-image` is applied, the browser does not clip the image to the curve defined by `border-radius`. To achieve a rounded gradient border, developers must use the dual-background `background-clip` technique or pseudo-elements.
</details>

<details>
<summary>7. What does a negative <code>outline-offset</code> do?</summary>

A positive `outline-offset` pushes the outline outward, creating an empty gap between the border and the outline. A negative `outline-offset` (e.g., `outline-offset: -4px;`) draws the outline **inside** the element's perimeter, creating an inset focus ring that will never collide with tight parent container boundaries.
</details>

<details>
<summary>8. How do CSS Logical Border Radii behave in Right-to-Left (RTL) mode?</summary>

Logical radii (`border-start-start-radius`, `border-start-end-radius`, `border-end-start-radius`, `border-end-end-radius`) map to corners relative to inline and block writing directions. 
In LTR, `border-start-start` is Top-Left. When the page switches to RTL (`dir="rtl"`), `border-start-start` automatically becomes Top-Right without writing a single line of override CSS.
</details>

---

## 11. Practice Exercises

### Exercise 1: Build an Accessible Dual-Ring Focus State
**Goal**: Create an accessible button whose focus indicator is visible on both dark and light backgrounds.

<details>
<summary>View Solution</summary>

```css
.accessible-btn {
  padding: 0.75rem 1.5rem;
  font-size: 1rem;
  font-weight: 600;
  background-color: #0f172a;
  color: #ffffff;
  border: none;
  border-radius: 8px;
  cursor: pointer;
}

.accessible-btn:focus-visible {
  outline: 2px solid #2563eb;
  outline-offset: 3px;
  box-shadow: 0 0 0 2px #ffffff;
}
```
</details>

### Exercise 2: Reflow-Free Border Hover Animation
**Goal**: Build a tab navigation item that gains a 3px active bottom border without shifting neighboring tabs.

<details>
<summary>View Solution</summary>

```css
.nav-tab {
  padding: 0.75rem 1rem;
  color: #64748b;
  text-decoration: none;
  border-bottom: 3px solid transparent; /* Pre-allocate space */
  transition: border-color 0.2s ease, color 0.2s ease;
}

.nav-tab:hover,
.nav-tab.active {
  color: #0f172a;
  border-bottom-color: #2563eb;
}
```
</details>

### Exercise 3: RTL-Safe Rounded Card Corners
**Goal**: Style a hero card with rounded corners only on the inline-start side that mirror in RTL.

<details>
<summary>View Solution</summary>

```css
.start-rounded-card {
  border-start-start-radius: 24px;
  border-end-start-radius: 24px;
  border-start-end-radius: 0;
  border-end-end-radius: 0;
  padding: 2rem;
  background-color: #f8fafc;
  border: 1px solid #e2e8f0;
}
```
</details>

### Exercise 4: Construct Inset Focus Outline
**Goal**: Create an image gallery thumbnail whose focus ring appears 4px inside the image boundary.

<details>
<summary>View Solution</summary>

```css
.gallery-thumbnail {
  display: block;
  width: 100%;
  border-radius: 8px;
  cursor: pointer;
}

.gallery-thumbnail:focus-visible {
  outline: 3px solid #ffffff;
  outline-offset: -6px; /* Draws ring inside image */
}
```
</details>

### Exercise 5: Asymmetric Elliptical Leaf Shape
**Goal**: Build a decorative leaf container using 4 distinct horizontal and vertical radii.

<details>
<summary>View Solution</summary>

```css
.leaf-container {
  width: 200px;
  height: 200px;
  background: linear-gradient(135deg, #10b981, #059669);
  border-radius: 0 100% 0 100% / 0 100% 0 100%;
}
```
</details>

### Exercise 6: WHCM Defense Verification
**Goal**: Ensure a floating alert modal maintains visible boundaries in Windows High Contrast Mode.

<details>
<summary>View Solution</summary>

```css
.floating-modal {
  max-width: 450px;
  padding: 2rem;
  background-color: #ffffff;
  border-radius: 12px;
  box-shadow: 0 10px 15px -3px rgba(0, 0, 0, 0.1);
  border: 1px solid transparent; /* High contrast safety */
}

@media (forced-colors: active) {
  .floating-modal {
    border-color: CanvasText;
  }
}
```
</details>

---

## 12. Summary / Cheat Sheet

| Property / Selector | Example | Primary Purpose |
| :--- | :--- | :--- |
| `border-radius` | `border-radius: 12px;` | Curves border and background clipping corners |
| `border-radius` (Pill) | `border-radius: 9999px;` | Clamps to semicircular end caps |
| `border-radius` (Slash)| `border-radius: 40px / 20px;`| Elliptical corner curves |
| `outline` | `outline: 2px solid #2563eb;` | Zero-reflow bounding indicator |
| `outline-offset` | `outline-offset: 3px;` | Gap between element and outline |
| `:focus-visible` | `button:focus-visible` | Keyboard-only accessible focus ring |
| Transparent Border | `border: 1px solid transparent;`| Pre-allocates hover border & protects in WHCM |

---

## 13. Further Reading
- [W3C CSS Backgrounds and Borders Module Level 3](https://www.w3.org/TR/css-backgrounds-3/)
- [MDN Web Docs: border-radius](https://developer.mozilla.org/en-US/docs/Web/CSS/border-radius)
- [MDN Web Docs: outline-offset](https://developer.mozilla.org/en-US/docs/Web/CSS/outline-offset)
- [MDN Web Docs: :focus-visible](https://developer.mozilla.org/en-US/docs/Web/CSS/:focus-visible)
- [Sara Soueidan: A Guide to Designing Accessible Focus Indicators](https://www.sarasoueidan.com/blog/focus-indicators/)

---

[Previous: 01-backgrounds-and-gradients.md](01-backgrounds-and-gradients.md) | [Module README](README.md) | [Next: 03-box-shadows-and-text-shadows.md](03-box-shadows-and-text-shadows.md)
