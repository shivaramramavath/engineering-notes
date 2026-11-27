# Filters & Glassmorphism

| Metadata | Details |
| :--- | :--- |
| **Topic** | CSS Graphical Filters, `backdrop-filter` Glassmorphism, `drop-shadow()` vs `box-shadow`, Stacking Contexts & GPU Compositing |
| **Spec Level** | Filter Effects Module Level 1 / Filter Effects Module Level 2 |
| **Baseline Status** | Widely Available (`filter` Baseline 2015; `backdrop-filter` Baseline 2020 across Chrome 76+, Safari 9+, Firefox 103+, Edge 79+) |
| **Browsers Tested** | Chrome 120+, Firefox 122+, Safari 17.2+, Edge 120+ |
| **Tested With** | Chromium Blink Compositor, Safari CoreAnimation, Firefox WebRender, Lighthouse |
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

Graphical filters allow web applications to execute Photoshop-grade pixel manipulation (blurring, color grading, contrast correction, and alpha shadow generation) directly on the client's GPU without pre-processing images on a server:
- **`filter`**: Processes the element itself along with all of its descendants (e.g., converting a full-color photography grid to black and white with `grayscale(100%)`).
- **`backdrop-filter`**: Processes the rendered pixels that lie *directly beneath* the element in the rendering stack (e.g., blurring background scenery behind a navigation bar or modal dialog).
- **The Glassmorphism Revolution**: Creating frosted glass surfaces where background content is softly diffused and saturated, establishing clear visual hierarchy while preserving contextual spatial awareness.
- **The Stacking Context Trap**: Both `filter` and `backdrop-filter` generate new stacking contexts and containing blocks for `position: fixed` elements, causing unexpected layout positioning bugs if misunderstood.

---

## 2. Core Concepts

### The CSS `filter` Functions
Filters accept one or more space-separated functions evaluated from left to right:
- `blur(<length>)`: Applies Gaussian blur (e.g., `blur(8px)`).
- `brightness(<number-or-percentage>)`: Scales luminance (e.g., `brightness(120%)`).
- `contrast(<number-or-percentage>)`: Adjusts contrast curve (e.g., `contrast(150%)`).
- `drop-shadow(<offset-x> <offset-y> <blur>? <color>?)`: Conforms to alpha transparency channels (PNG cutouts, SVG shapes).
- `grayscale(<number-or-percentage>)`: Desaturates pixels (e.g., `grayscale(100%)`).
- `hue-rotate(<angle>)`: Shifts colors along the color wheel (e.g., `hue-rotate(90deg)`).
- `invert(<number-or-percentage>)`: Inverts color channels (e.g., `invert(100%)`).
- `opacity(<number-or-percentage>)`: Controls alpha transparency on the GPU.
- `saturate(<number-or-percentage>)`: Boosts or mutes color vibrancy (e.g., `saturate(200%)`).
- `sepia(<number-or-percentage>)`: Imparts a warm vintage amber tone.

### `backdrop-filter` & Frosted Glass Mechanics
`backdrop-filter` applies optical effects to whatever lies underneath the element's bounding box:
```css
.frosted-glass {
  background-color: rgba(255, 255, 255, 0.7); /* Must be translucent! */
  backdrop-filter: blur(16px) saturate(180%);
  -webkit-backdrop-filter: blur(16px) saturate(180%); /* Safari compatibility */
  border: 1px solid rgba(255, 255, 255, 0.2);
}
```
**Strict Requirement**: The element's own `background-color` must be translucent (`rgba()`, `hsla()`, or `oklch(... / alpha)`). If the background is opaque (`#ffffff`), the backdrop behind it is completely covered and the filter effect will not be visible!

### `drop-shadow()` vs `box-shadow`
- `box-shadow`: Always casts a shadow conforming to the element's rectangular or curved `border-radius` perimeter. On a transparent PNG or SVG icon, `box-shadow` draws a rectangle around the entire image box.
- `filter: drop-shadow()`: Evaluates the **alpha channel** of the rendered content. It ignores transparent pixels and casts a realistic shadow around individual vector paths, cutouts, or speech-bubble arrows.

---

## 3. How It Works Internally

```
Comparison of box-shadow vs filter: drop-shadow() on a PNG Star

1. box-shadow: 0 4px 8px black
   +-----------------------+
   |                       |
   |         / \           |
   |       /     \         |  ===> Casts shadow around the outer
   |      /   *   \        |       RECTANGLE bounding box!
   |     /_________\       |
   +-----------------------+
   [--- Black Rectangular Shadow ---]

2. filter: drop-shadow(0 4px 8px black)
             / \
           /     \            ===> Casts shadow conforming to the
          /   *   \                EXACT ALPHA CONTOURS of the star!
         /_________\
       [-- Star Shadow --]
```

### The Compositor Stacking Context Side-Effect
Whenever `filter` (other than `none`) or `backdrop-filter` is applied to an element:
1. The browser engine forces the element to become a **new Stacking Context**.
2. The element becomes the **containing block** for all its descendant elements, including any descendants styled with `position: fixed` or `position: absolute`.
3. A child element with `position: fixed` will be anchored to the filtered element's bounding box rather than the browser viewport!

---

## 4. Syntax & Usage

### Baseline & Feature Support
- **`filter`**: Baseline Widely Available.
- **`backdrop-filter`**: Baseline 2020 (Chrome 76+, Safari 9+, Firefox 103+, Edge 79+).
- Note: Safari requires the `-webkit-backdrop-filter` vendor prefix for backward compatibility.

```css
/* Production Frosted Glass Header */
.glass-navbar {
  position: sticky;
  top: 0;
  z-index: 100;
  width: 100%;
  padding: 1rem 2rem;
  
  /* Fallback for browsers with backdrop-filter disabled */
  background-color: rgba(255, 255, 255, 0.95);
}

@supports (backdrop-filter: blur(12px)) or (-webkit-backdrop-filter: blur(12px)) {
  .glass-navbar {
    background-color: rgba(255, 255, 255, 0.75);
    -webkit-backdrop-filter: blur(12px) saturate(180%);
    backdrop-filter: blur(12px) saturate(180%);
    border-bottom: 1px solid rgba(255, 255, 255, 0.3);
  }
}
```

---

## 5. Practical Examples

### Example 1: Glassmorphic Modal Dialog
```html
<div class="modal-backdrop">
  <div class="glass-dialog">
    <h3>Confirm Deployment</h3>
    <p>Are you sure you want to deploy build v4.2.0 to primary production clusters?</p>
    <div class="dialog-actions">
      <button type="button" class="btn-cancel">Cancel</button>
      <button type="button" class="btn-confirm">Deploy Now</button>
    </div>
  </div>
</div>
```

```css
.modal-backdrop {
  position: fixed;
  inset: 0;
  background-color: rgba(15, 23, 42, 0.6);
  -webkit-backdrop-filter: blur(8px);
  backdrop-filter: blur(8px);
  display: flex;
  align-items: center;
  justify-content: center;
  z-index: 1000;
}

.glass-dialog {
  max-width: 480px;
  width: 90%;
  padding: 2rem;
  border-radius: 16px;
  background-color: rgba(255, 255, 255, 0.85);
  -webkit-backdrop-filter: blur(16px) saturate(150%);
  backdrop-filter: blur(16px) saturate(150%);
  border: 1px solid rgba(255, 255, 255, 0.4);
  box-shadow: 0 20px 25px -5px rgba(0, 0, 0, 0.2);
  color: #0f172a;
}

.dialog-actions {
  display: flex;
  justify-content: flex-end;
  gap: 0.75rem;
  margin-block-start: 1.5rem;
}

.btn-confirm {
  background-color: #2563eb;
  color: #ffffff;
  padding: 0.5rem 1rem;
  border: none;
  border-radius: 6px;
  cursor: pointer;
}

.btn-cancel {
  background: none;
  border: 1px solid #cbd5e1;
  padding: 0.5rem 1rem;
  border-radius: 6px;
  cursor: pointer;
}

.btn-confirm:focus-visible,
.btn-cancel:focus-visible {
  outline: 2px solid #2563eb;
  outline-offset: 2px;
}
```

### Example 2: Speech Bubble with Alpha `drop-shadow()`
```html
<div class="speech-bubble">
  Deployment pipeline completed in 42 seconds!
</div>
```

```css
.speech-bubble {
  position: relative;
  background-color: #ffffff;
  padding: 1rem 1.5rem;
  border-radius: 8px;
  color: #1e293b;
  font-weight: 500;
  display: inline-block;

  /* Casts shadow around BOTH the rectangle and the triangle pointer */
  filter: drop-shadow(0 4px 6px rgba(0, 0, 0, 0.15));
}

.speech-bubble::after {
  content: "";
  position: absolute;
  top: 100%;
  left: 24px;
  border-width: 8px;
  border-style: solid;
  border-color: #ffffff transparent transparent transparent;
}
```

### Example 3: Dynamic Image Dark-Mode Adaptor
```css
/* Inverts and dims light-themed diagrams for dark mode without redrawing assets */
[data-theme="dark"] .light-mode-diagram {
  filter: invert(90%) hue-rotate(180deg) contrast(110%);
}
```

### Example 4: Interactive Image Color Grading on Hover
```css
.gallery-card img {
  width: 100%;
  height: 240px;
  object-fit: cover;
  border-radius: 8px;
  filter: grayscale(100%) contrast(120%) brightness(90%);
  transition: filter 0.3s cubic-bezier(0.16, 1, 0.3, 1), transform 0.3s ease;
}

.gallery-card:hover img {
  filter: grayscale(0%) contrast(100%) brightness(100%);
  transform: scale(1.02);
}
```

### Example 5: Resolving the `position: fixed` Child Trap
```css
/* THE BUG:
.parent {
  filter: blur(0px); // Or any filter
}
.child-modal {
  position: fixed; // TRAPPED inside .parent! Does not cover viewport!
}
*/

/* THE ARCHITECTURAL FIX:
Keep modal dialogs as direct siblings of <body>, outside any filtered containers! */
body > .modal-root {
  position: fixed;
  inset: 0;
  z-index: 9999;
}
```

---

## 6. Best Practices

1. **Always Provide a Solid/Translucent Fallback for `backdrop-filter`**: In low-power devices, virtual machines, or older browsers, `backdrop-filter` may be disabled in browser flags. Set an opaque or semi-opaque fallback `background-color`.
2. **Always Include `-webkit-backdrop-filter`**: iOS Safari and macOS Safari historically require the vendor prefix for hardware-accelerated frosted glass.
3. **Use `filter: drop-shadow()` for Irregular Shapes and SVGs**: Use `drop-shadow()` when you need shadows on PNG cutouts, transparent badges, and speech-bubble pointers.
4. **Beware of the Stacking Context Side-Effect**: Never apply `filter` or `backdrop-filter` to the `<body>` or high-level layout wrappers if child components rely on `position: fixed` relative to the viewport.
5. **Pair `blur()` with `saturate()` for Frosted Glass**: A pure `blur(12px)` looks muddy and washed out. Adding `saturate(180%)` restores luminous color vibrancy to the blurred background.
6. **Limit the Surface Area of `backdrop-filter`**: Blurring large full-screen scrollable panes causes noticeable GPU frame drops on mobile devices. Restrict glassmorphism to compact navigation bars, cards, or floating menus.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Correction |
| :--- | :--- | :--- |
| **Opaque background with `backdrop-filter`** | Setting `background: #ffffff;` completely hides the blurred backdrop beneath it. | Use translucent `background: rgba(255, 255, 255, 0.7);`. |
| **Omitting `-webkit-backdrop-filter`** | Glassmorphism fails completely on older Safari and iOS devices. | Declare both `-webkit-backdrop-filter` and `backdrop-filter`. |
| **`box-shadow` on transparent PNG or speech bubble** | Draws an ugly black rectangle around the bounding box of the image. | Use `filter: drop-shadow(...)`. |
| **`position: fixed` inside a filtered container** | Filter creates a new containing block; fixed child locks to parent instead of viewport. | Move the fixed element outside the filtered parent. |
| **Applying heavy `backdrop-filter` to entire scrolling tables** | Forces massive GPU recompositing on every scroll frame, killing 60fps performance. | Restrict backdrop filters to small sticky headers or modals. |
| **Missing accessible fallback in high-contrast mode** | Transparent glass backgrounds can become illegible in Windows High Contrast Mode. | Provide `@media (forced-colors: active)` solid background fallbacks. |

---

## 8. Performance, Accessibility & Browser Support

### GPU Compositor Layers & Pixel Fill Rates
`backdrop-filter` is computationally expensive:
1. The GPU compositor must render all layers underneath the frosted element into an off-screen texture buffer.
2. It executes a multi-pass Gaussian blur shader kernel over that texture buffer.
3. It paints the blurred texture onto the screen.
4. It renders the translucent element and its text content on top.
If elements underneath the frosted glass are animating or scrolling, this 4-step pipeline executes on **every single frame (60 to 120 times per second)**. Keep blurred surfaces compact.

---

## 9. Comparison / When to Use What

| Feature | Targets | Alpha Contour Aware | Creates Stacking Context | Performance Cost |
| :--- | :--- | :--- | :--- | :--- |
| **`filter: blur()`** | Element & its children | Yes | **Yes** | Moderate |
| **`backdrop-filter: blur()`** | Elements *behind* element | N/A (Bounding box) | **Yes** | **High** (Texture copy) |
| **`box-shadow`** | Border box perimeter | No (Rectangular/Radii) | No | Low to Moderate |
| **`filter: drop-shadow()`** | Element & alpha pixels | **Yes** | **Yes** | Moderate |

---

## 10. Interview Questions

<details>
<summary>1. What is the fundamental difference between <code>filter</code> and <code>backdrop-filter</code>?</summary>

- `filter` applies graphical effects (blur, contrast, invert) to the **element itself and all of its child descendants**.
- `backdrop-filter` applies graphical effects to the **pixels rendered directly behind the element** (in the element's background canvas), while leaving the element itself and its child contents (text, icons) completely sharp and unfiltered.
</details>

<details>
<summary>2. Why does <code>filter: drop-shadow()</code> produce a different visual result than <code>box-shadow</code> on an SVG or transparent PNG?</summary>

`box-shadow` is computed strictly against the element's CSS box model boundary (`border-box` and `border-radius`). It ignores internal pixel transparency and draws a shadow around the outer rectangular box.

`filter: drop-shadow()` operates directly on the alpha channel of the rendered pixels. It inspects the alpha mask of the image or SVG vector paths, drawing the shadow only around non-transparent pixels, allowing shadows to conform to complex icons, cutouts, and speech-bubble tail pointers.
</details>

<details>
<summary>3. Why does applying <code>filter</code> to a container break child elements with <code>position: fixed</code>?</summary>

Per the W3C CSS Filter Effects and CSS Transforms specifications, applying any value other than `none` to `filter` or `backdrop-filter` causes the element to act as a **containing block for all descendant elements**, including those with `position: fixed`. 

Normally, a `position: fixed` element is positioned relative to the browser viewport. When placed inside a filtered ancestor, its positioning coordinate space becomes trapped relative to the filtered parent's bounding box, preventing full-screen overlays or modals from functioning.
</details>

<details>
<summary>4. Why is a translucent background mandatory for <code>backdrop-filter</code> to be visible?</summary>

The browser renders layers from back to front. If an element has an opaque background (e.g., `background-color: #ffffff;`), that solid color is painted over the element's entire surface area, completely occluding the blurred backdrop buffer underneath. To see the frosted glass effect, the element's background must have alpha transparency (e.g., `rgba(255, 255, 255, 0.7)`).
</details>

<details>
<summary>5. What are the performance hazards associated with <code>backdrop-filter: blur()</code> during scrolling?</summary>

To render `backdrop-filter`, the GPU must perform an off-screen readback of all composited layers behind the element, run a 2-pass Gaussian convolution shader to blur the texture, and then paint it before rendering the element's foreground. 

During scrolling, the content behind the glass changes on every frame, forcing the GPU to re-capture and re-blur the backdrop 60–120 times per second. On mobile GPUs, this can saturate the memory bus and cause severe frame drops.
</details>

<details>
<summary>6. Does <code>filter: drop-shadow()</code> support the <code>spread-radius</code> or <code>inset</code> parameters?</summary>

No. The `drop-shadow()` filter function syntax only accepts:
`drop-shadow(<offset-x> <offset-y> <blur-radius>? <color>?)`. 
It does not support `spread-radius` because calculating an expanded alpha contour mask requires complex morphological dilation shaders. It also does not support `inset`.
</details>

<details>
<summary>7. How do you create an accessible fallback for frosted glass navigation bars?</summary>

Set an opaque or semi-opaque fallback `background-color` on the element by default. Then wrap the glassmorphism properties in an `@supports (backdrop-filter: blur(10px)) or (-webkit-backdrop-filter: blur(10px))` query to progressively enhance browsers that support hardware-accelerated backdrop filtering.
</details>

<details>
<summary>8. Why does combining <code>blur()</code> with <code>saturate()</code> produce superior glassmorphism?</summary>

A pure Gaussian blur averages neighboring pixel colors, which naturally cancels out high-frequency color variations and produces a washed-out, desaturated milky gray appearance. Adding `saturate(150% - 180%)` artificially boosts the vibrancy of the blurred colors, producing an authentic, premium optical glass appearance reminiscent of modern operating system UIs (macOS, iOS, Windows Mica).
</details>

---

## 11. Practice Exercises

### Exercise 1: Build a Production Frosted Glass Sticky Header
**Goal**: Build a sticky navbar with accessible progressive enhancement fallbacks and high-vibrancy frosted glass.

<details>
<summary>View Solution</summary>

```css
.sticky-glass-nav {
  position: sticky;
  top: 0;
  z-index: 50;
  width: 100%;
  padding: 1rem 2rem;
  background-color: #ffffff; /* Solid fallback */
  border-bottom: 1px solid #e2e8f0;
}

@supports (backdrop-filter: blur(16px)) or (-webkit-backdrop-filter: blur(16px)) {
  .sticky-glass-nav {
    background-color: rgba(255, 255, 255, 0.7);
    -webkit-backdrop-filter: blur(16px) saturate(180%);
    backdrop-filter: blur(16px) saturate(180%);
    border-bottom: 1px solid rgba(255, 255, 255, 0.3);
  }
}
```
</details>

### Exercise 2: Cast a Precision Alpha Shadow on an SVG Icon
**Goal**: Use `filter: drop-shadow()` to cast a shadow on an SVG shield icon that conforms to its vector contours.

<details>
<summary>View Solution</summary>

```html
<svg class="shield-icon" viewBox="0 0 24 24">
  <path d="M12 2L3 7v6c0 5.55 3.84 10.74 9 12 5.16-1.26 9-6.45 9-12V7l-9-5z" fill="#2563eb" />
</svg>
```

```css
.shield-icon {
  width: 64px;
  height: 64px;
  /* Casts shadow around shield curve instead of bounding square */
  filter: drop-shadow(0 8px 12px rgba(37, 99, 235, 0.35));
}
```
</details>

### Exercise 3: Vintage Photo Filter Preset
**Goal**: Combine multiple filter functions to create a warm vintage photograph effect on hover.

<details>
<summary>View Solution</summary>

```css
.vintage-photo {
  width: 100%;
  max-width: 400px;
  border-radius: 8px;
  filter: sepia(40%) contrast(110%) brightness(95%) saturate(85%);
  transition: filter 0.3s ease;
}

.vintage-photo:hover {
  filter: sepia(0%) contrast(100%) brightness(100%) saturate(100%);
}
```
</details>

### Exercise 4: Frosted Glass Floating Action Card
**Goal**: Build a dark-mode frosted glass card that diffuses light gradients underneath.

<details>
<summary>View Solution</summary>

```css
.dark-glass-card {
  padding: 1.5rem;
  border-radius: 12px;
  background-color: rgba(15, 23, 42, 0.65);
  -webkit-backdrop-filter: blur(12px) brightness(110%);
  backdrop-filter: blur(12px) brightness(110%);
  border: 1px solid rgba(255, 255, 255, 0.1);
  color: #f8fafc;
}
```
</details>

### Exercise 5: Debug and Fix the Trapped Fixed Modal
**Goal**: Refactor markup to prevent a `position: fixed` modal from becoming trapped inside a blurred hero container.

<details>
<summary>View Solution</summary>

```html
<!-- INCORRECT:
<div class="blurred-hero" style="filter: blur(4px);">
  <div class="modal" style="position: fixed; inset: 0;">...</div>
</div>
-->

<!-- CORRECT ARCHITECTURE: -->
<div class="blurred-hero" style="filter: blur(4px);">
  <h1>Background Content</h1>
</div>

<div class="modal-root" style="position: fixed; inset: 0; z-index: 9999;">
  <div class="modal-dialog">Cleanly fills viewport!</div>
</div>
```
</details>

### Exercise 6: Invert Light Logos in Dark Mode
**Goal**: Adapt a black partner logo for dark mode using CSS filters without loading a second image asset.

<details>
<summary>View Solution</summary>

```css
.partner-logo {
  height: 40px;
  filter: none;
}

[data-theme="dark"] .partner-logo {
  filter: invert(100%) opacity(80%);
}
```
</details>

---

## 12. Summary / Cheat Sheet

| Filter Property / Function | Example | Role |
| :--- | :--- | :--- |
| `filter: blur()` | `filter: blur(8px);` | Blurs element and all its children |
| `backdrop-filter: blur()` | `backdrop-filter: blur(16px);` | Blurs content *behind* translucent element |
| `filter: drop-shadow()` | `drop-shadow(0 4px 6px #000)` | Conforms shadow to alpha channel / SVG contours |
| `saturate()` | `saturate(180%)` | Restores vibrancy to blurred glassmorphic surfaces |
| Stacking Context | (Side-effect of filters) | Filtered elements become containing blocks for `fixed` children |

---

## 13. Further Reading
- [W3C Filter Effects Module Level 1](https://www.w3.org/TR/filter-effects-1/)
- [MDN Web Docs: filter](https://developer.mozilla.org/en-US/docs/Web/CSS/filter)
- [MDN Web Docs: backdrop-filter](https://developer.mozilla.org/en-US/docs/Web/CSS/backdrop-filter)
- [web.dev: Creative effects with CSS filters](https://web.dev/learn/css/filters/)
- [Sara Soueidan: The Stacking Context Trap](https://www.sarasoueidan.com/)

---

[Previous: 03-box-shadows-and-text-shadows.md](03-box-shadows-and-text-shadows.md) | [Module README](README.md) | [Next: 05-blend-modes-and-isolation.md](05-blend-modes-and-isolation.md)
