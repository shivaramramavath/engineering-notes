# Clip Path & CSS Masks

| Metadata | Details |
| :--- | :--- |
| **Topic** | Vector Clipping (`clip-path`), Geometric Shapes, `path()` Syntax, Alpha & Luminance Masks (`mask-image`), Gradient Edge Fades |
| **Spec Level** | CSS Masking Module Level 1 |
| **Baseline Status** | Widely Available (`clip-path` Baseline 2015; `mask-image` Baseline 2020 with `-webkit-` prefix support) |
| **Browsers Tested** | Chrome 120+, Firefox 122+, Safari 17.2+, Edge 120+ |
| **Tested With** | W3C CSS Validator, Chromium LayoutNG, Safari WebKit, Playwright |
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

By default, every HTML element is an unyielding rectangular box. Even when styled with `border-radius: 50%`, the underlying hit-testing region and pointer event boundaries historically remained rectangular boxes.

CSS Masking Level 1 liberates web design from rectangular boundaries through two primary mechanisms:
1. **`clip-path`**: A binary (all-or-nothing) vector clipping region. Any pixel inside the clipping path is 100% visible; any pixel outside is 100% discarded. Crucially, **clipped areas do not register pointer events or mouse clicks**, allowing underlying content to be clicked through.
2. **`mask-image`**: A continuous-tone opacity mask (alpha or luminance). Allows soft gradients, textures, and feathered edges to dictate the transparency of an element.

Together, these properties allow developers to create angled hero section dividers, animated shape transitions, carousel edge fade-outs, and organic non-rectangular UI cards without loading heavy raster graphics.

---

## 2. Core Concepts

### `clip-path` Basic Shapes
- `inset(<top> <right> <bottom> <left> [round <radius>]?)`: Insets a rectangular clipping box with optional rounded corners.
- `circle(<radius>? [at <position>]?)`: Clips to a perfect circle (e.g., `circle(50% at center)`).
- `ellipse(<radius-x> <radius-y>? [at <position>]?)`: Clips to an ellipse.
- `polygon(<x1> <y1>, <x2> <y2>, ...)`: Defines a custom polygon with three or more coordinate pairs (percentages or lengths).
- `path("<svg-path-string>")`: Accepts raw SVG path data commands (`M`, `C`, `L`, `Z`) directly inside CSS.
- `url(#svg-clip-id)`: References an inline `<clipPath>` element in the SVG DOM.

### CSS Masking (`mask-image`)
While `clip-path` is strictly binary (on or off), `mask-image` operates like a grayscale photographic stencil:
- `mask-image`: Points to a gradient (`linear-gradient(...)`) or image asset (`url(...)`).
- `mask-mode`:
  - `alpha` (Default): Uses the alpha transparency channel of the mask image (opaque pixels reveal content, transparent pixels hide content).
  - `luminance`: Uses the brightness of the mask image (pure white reveals content, pure black hides content).
- `mask-composite`: Combines multiple masks using Boolean operations (`add`, `subtract`, `intersect`, `exclude`).

### Morphing & Animation Rule
`clip-path` can be transitioned and animated via `@keyframes` or `transition`, but with one **strict mathematical requirement**:
> The starting and ending shapes must use the **exact same shape function** with the **exact same number of coordinate points** (e.g., morphing an 8-point polygon into an 8-point star). If vertex counts differ, interpolation fails and snaps abruptly.

---

## 3. How It Works Internally

```
clip-path vs mask-image Rendering Mechanics

1. clip-path: polygon(0 0, 100% 0, 100% 80%, 0 100%)
   +------------------------------------+
   | Inside Vector: 100% OPAQUE         |
   | Pointer clicks REGISTER here       |
   \                                    |
    \-----------------------------------+
     Outside Vector: 100% DISCARDED
     (Zero pointer events! Underlying buttons CAN BE CLICKED!)

2. mask-image: linear-gradient(to right, black 80%, transparent 100%)
   [100% Visible] ===> [75%] ===> [50%] ===> [25%] ===> [0% Transparent]
   (Smooth feathered gradient fade-out across horizontal axis)
```

---

## 4. Syntax & Usage

### Baseline & Feature Support
- **`clip-path` Basic Shapes (`circle`, `polygon`, `inset`)**: Baseline Widely Available (Chrome 55+, Safari 10.1+, Firefox 54+, Edge 79+).
- **`clip-path: path()`**: Baseline 2023 (Chrome 88+, Safari 13.1+, Firefox 97+).
- **`mask-image`**: Baseline 2020 (Requires `-webkit-mask-*` vendor prefix in Safari/WebKit).

```css
/* Angled Slanted Card Header */
.slanted-header {
  clip-path: polygon(0 0, 100% 0, 100% 85%, 0 100%);
}

/* Horizontal Carousel Edge Fade-Out */
.fading-carousel {
  overflow-x: auto;
  -webkit-mask-image: linear-gradient(to right, transparent, black 10%, black 90%, transparent);
  mask-image: linear-gradient(to right, transparent, black 10%, black 90%, transparent);
}
```

---

## 5. Practical Examples

### Example 1: Angled Section Divider (Diagonal Slash)
```html
<section class="angled-hero-section">
  <div class="hero-content">
    <h1>High-Throughput Streaming Engines</h1>
    <p>Processing terabytes of real-time telemetry with sub-millisecond p99 latencies.</p>
  </div>
</section>
```

```css
.angled-hero-section {
  position: relative;
  background: linear-gradient(135deg, #0f172a, #1e293b);
  color: #ffffff;
  padding: 5rem 2rem 8rem 2rem;
  
  /* Diagonal angle across the bottom edge */
  clip-path: polygon(0 0, 100% 0, 100% calc(100% - 60px), 0 100%);
  margin-block-end: -60px; /* Pulls next section up to meet the angle */
}

.hero-content {
  max-width: 65ch;
  margin-inline: auto;
}
```

### Example 2: Carousel Edge Fade Mask (Scroll Hint)
```html
<div class="carousel-fade-wrapper">
  <div class="scroll-track">
    <div class="card">Node 01</div>
    <div class="card">Node 02</div>
    <div class="card">Node 03</div>
    <div class="card">Node 04</div>
    <div class="card">Node 05</div>
  </div>
</div>
```

```css
.carousel-fade-wrapper {
  position: relative;
  width: 100%;
  
  /* Fades left and right edges to signal horizontal scrollability */
  -webkit-mask-image: linear-gradient(
    to right,
    transparent 0%,
    black 40px,
    black calc(100% - 40px),
    transparent 100%
  );
  mask-image: linear-gradient(
    to right,
    transparent 0%,
    black 40px,
    black calc(100% - 40px),
    transparent 100%
  );
}

.scroll-track {
  display: flex;
  gap: 1rem;
  overflow-x: auto;
  padding: 1rem 40px;
}

.card {
  flex: 0 0 200px;
  height: 120px;
  background-color: #2563eb;
  color: #ffffff;
  display: flex;
  align-items: center;
  justify-content: center;
  border-radius: 8px;
}
```

### Example 3: Polygon Morphing Animation on Hover
```html
<button type="button" class="morph-badge">
  <span>Hover to Expand</span>
</button>
```

```css
.morph-badge {
  padding: 1rem 2rem;
  background-color: #0f172a;
  color: #38bdf8;
  font-weight: 700;
  border: none;
  cursor: pointer;

  /* State 1: Chamfered 8-point polygon (cut corners) */
  clip-path: polygon(
    12px 0%, calc(100% - 12px) 0%,
    100% 12px, 100% calc(100% - 12px),
    calc(100% - 12px) 100%, 12px 100%,
    0% calc(100% - 12px), 0% 12px
  );

  transition: clip-path 0.3s cubic-bezier(0.16, 1, 0.3, 1), background-color 0.2s ease;
}

.morph-badge:hover {
  background-color: #1e293b;
  /* State 2: Flattened rectangle with identical 8 vertex count */
  clip-path: polygon(
    0% 0%, 100% 0%,
    100% 0%, 100% 100%,
    100% 100%, 0% 100%,
    0% 100%, 0% 0%
  );
}

.morph-badge:focus-visible {
  outline: 2px solid #38bdf8;
  outline-offset: 4px;
}
```

### Example 4: Hexagonal Avatar Grid
```css
.hex-avatar {
  width: 120px;
  height: 120px;
  object-fit: cover;
  /* Mathematically precise regular hexagon */
  clip-path: polygon(
    50% 0%,
    100% 25%,
    100% 75%,
    50% 100%,
    0% 75%,
    0% 25%
  );
  transition: transform 0.2s ease;
}

.hex-avatar:hover {
  transform: scale(1.08);
}
```

### Example 5: Inset Clipping with Rounded Corners
```css
/* Clips interior contents with a 10px inset and 16px corner radius */
.inset-clipped-card {
  clip-path: inset(10px round 16px);
  background-color: #0f172a;
  padding: 2rem;
  color: #ffffff;
}
```

---

## 6. Best Practices

1. **Always Include `-webkit-mask-image`**: Safari and WebKit browsers strictly require the `-webkit-` vendor prefix for CSS masking properties (`-webkit-mask-image`, `-webkit-mask-size`, `-webkit-mask-repeat`).
2. **Match Vertex Counts Exactly for Transitions**: When animating `clip-path: polygon()`, ensure every keyframe state has the exact same number of points in the same order.
3. **Remember That Clipped Areas Are Click-Through**: Outside the vector boundaries of `clip-path`, pointer events pass through completely to underlying elements.
4. **Use `calc()` Inside Polygon Coordinates**: Combining percentages with fixed pixels (`calc(100% - 20px) 100%`) guarantees angled cuts and chamfers remain proportional across all screen sizes.
5. **Use Mask Gradients for Overflow Fades**: Avoid using floating absolute `div` elements with linear gradients for scroll fading; `mask-image` applies directly to the scroll container and never blocks scroll wheel events.
6. **Provide Accessible Focus Rings for Clipped Controls**: If a button has custom clipping, ensure `:focus-visible` outlines or inset box-shadows remain visible and are not clipped off.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Correction |
| :--- | :--- | :--- |
| **Omitting `-webkit-mask-image`** | CSS masking fails completely in Safari and iOS mobile browsers. | Always declare both `-webkit-mask-image` and `mask-image`. |
| **Animating `polygon()` with mismatched vertices** | Browser cannot calculate point interpolation; animation fails and snaps discretely. | Match point counts exactly across all animation states. |
| **Using `overflow: hidden` expecting non-rectangular clipping** | `overflow: hidden` strictly clips to rectangular (or rounded) boxes. | Use `clip-path: polygon(...)` for non-rectangular geometry. |
| **Clipping out the element's focus ring** | Outset focus rings drawn outside the `clip-path` boundary are discarded. | Use inset focus states or apply `clip-path` to an inner wrapper. |
| **Expecting `clip-path` to create soft feathered edges** | `clip-path` is strictly binary (hard vector edge). | Use `mask-image` for soft, feathered, or gradient alpha masks. |
| **Using `path()` without responsive scaling** | SVG path strings use fixed pixel coordinates that don't scale automatically. | Use percentages in `polygon()` or SVG `<clipPath clipPathUnits="objectBoundingBox">`. |

---

## 8. Performance, Accessibility & Browser Support

### GPU Vector Rasterization
- `clip-path: polygon()` and basic shapes (`circle`, `ellipse`, `inset`) are evaluated on the GPU. Modern compositor engines tessellate basic shapes into geometric triangle meshes, running at full 60fps/120fps hardware speed.
- `mask-image` requires an off-screen alpha composition pass, making it slightly heavier than `clip-path`, but substantially more performant than JavaScript-driven canvas drawing.

### Pointer Event Hit-Testing
Unlike `opacity: 0` or `visibility: hidden` (which can still block clicks unless `pointer-events: none` is set), the area clipped away by `clip-path` **does not exist in the hit-testing tree**. Clicks pass directly through to whatever element sits underneath the clipped void.

---

## 9. Comparison / When to Use What

| Feature | Alpha Edge | Hit-Testing Outside Shape | Animation Support | Best Use Case |
| :--- | :--- | :--- | :--- | :--- |
| **`clip-path: polygon`** | Razor-sharp (Binary) | Passes through (Clickable) | Yes (Matching vertices) | Angled section dividers, hexagons, chamfered cards |
| **`clip-path: circle`** | Razor-sharp (Binary) | Passes through (Clickable) | Yes (Smooth radius) | Expanding circular reveals, circular avatars |
| **`mask-image` (Gradient)**| Soft / Feathered | Blocks clicks (Entire box) | Yes (Color stops) | Carousel edge fades, bottom fade-outs |
| **`mask-image` (Asset)** | Soft / Texture | Blocks clicks (Entire box) | Limited | Grunge textures, brush strokes, custom stencils |

---

## 10. Interview Questions

<details>
<summary>1. What is the fundamental difference between <code>clip-path</code> and <code>mask-image</code>?</summary>

- `clip-path` creates a **binary vector clipping path**. Every pixel is either 100% inside (visible) or 100% outside (invisible). It produces razor-sharp vector edges and clips the element's hit-testing boundary so pointer events pass through clipped areas.
- `mask-image` creates an **alpha or luminance transparency mask**. It allows continuous shades of transparency (feathered edges, linear gradient fade-outs, semi-transparent textures). However, the element's rectangular hit-testing box remains intact unless `pointer-events` is modified.
</details>

<details>
<summary>2. What happens to mouse pointer events (clicks, hovers) in the area clipped away by <code>clip-path</code>?</summary>

According to the W3C CSS Masking Level 1 specification, pointer events must not be dispatched to an element outside its clipping path. 
If an element is clipped into a triangle, clicking in the empty space outside the triangle will pass directly through to whatever DOM element sits underneath it on the screen.
</details>

<details>
<summary>3. What is the strict rule for animating or transitioning between two <code>clip-path: polygon()</code> shapes?</summary>

Interpolation between two polygons requires that **both polygons have the exact same number of vertices (points)**, and that the coordinates are defined in the exact same clockwise or counter-clockwise order. 
If State A has 6 points and State B has 8 points, the browser's mathematical interpolation engine cannot establish a 1-to-1 vertex mapping, causing the animation to fail and jump abruptly between states.
</details>

<details>
<summary>4. Why is <code>-webkit-mask-image</code> still required in modern web development?</summary>

While `mask-image` is part of the W3C CSS Masking Level 1 specification, the WebKit rendering engine (which powers Safari on macOS and iOS) historically implemented CSS masking using the `-webkit-` prefix. To this day, Safari requires `-webkit-mask-image`, `-webkit-mask-size`, and `-webkit-mask-repeat` to properly render gradient and asset masks.
</details>

<details>
<summary>5. How does <code>clip-path: inset()</code> differ from <code>overflow: hidden</code>?</summary>

- `overflow: hidden` clips children that extend beyond the element's padding box (or border box). It cannot clip the element's own background or borders.
- `clip-path: inset(10px round 8px)` clips the **element itself** (including its own background, borders, and shadows) inwards by 10px, with an 8px corner radius.
</details>

<details>
<summary>6. How can you implement a responsive SVG path clipping using CSS?</summary>

By defining an inline `<svg>` containing a `<clipPath id="my-clip" clipPathUnits="objectBoundingBox">` and using relative coordinates between $0.0$ and $1.0$:
```html
<svg width="0" height="0">
  <clipPath id="wave" clipPathUnits="objectBoundingBox">
    <path d="M 0,0 L 1,0 L 1,0.8 C 0.7,1 0.3,0.6 0,0.8 Z" />
  </clipPath>
</svg>
```
In CSS: `clip-path: url(#wave);`. Because `clipPathUnits="objectBoundingBox"` is set, the vector coordinates scale proportionally regardless of element width and height.
</details>

<details>
<summary>7. What is the difference between <code>mask-type: alpha</code> and <code>mask-type: luminance</code>?</summary>

- `mask-type: alpha`: Transparency is determined solely by the alpha channel of the mask image (opaque pixels = visible; 0 alpha = invisible). Color channels are completely ignored.
- `mask-type: luminance`: Transparency is determined by the relative luminance (brightness) of the mask image. Pure white pixels (#ffffff) render 100% opaque, pure black pixels (#000000) render 100% transparent, and mid-grays render semi-transparent.
</details>

<details>
<summary>8. Why should you avoid putting an accessible <code>:focus-visible</code> ring on a clipped button without an inner wrapper?</summary>

`outline` and `box-shadow` are rendered outside the element's border box. Because `clip-path` ruthlessly discards *everything* outside its geometric perimeter, any focus ring cast outside the clipped boundary will be sliced off and rendered invisible, violating WCAG 2.4.7 (Focus Visible). 

**Solution**: Apply the `clip-path` to an inner background element, leaving the interactive parent's focus ring unclipped.
</details>

---

## 11. Practice Exercises

### Exercise 1: Build a Diagonal Angled Hero Section
**Goal**: Create an angled hero container that slants upward by 50px at the bottom edge.

<details>
<summary>View Solution</summary>

```css
.slanted-hero {
  padding: 4rem 2rem 6rem;
  background: linear-gradient(to right, #1e3a8a, #0f172a);
  color: #ffffff;
  clip-path: polygon(0 0, 100% 0, 100% calc(100% - 50px), 0 100%);
}
```
</details>

### Exercise 2: Carousel Edge Fade-Out Mask
**Goal**: Create a scrollable container with feathered transparent fades on the left and right edges.

<details>
<summary>View Solution</summary>

```css
.feathered-scroll-pane {
  overflow-x: auto;
  width: 100%;
  -webkit-mask-image: linear-gradient(
    to right,
    transparent 0%,
    black 32px,
    black calc(100% - 32px),
    transparent 100%
  );
  mask-image: linear-gradient(
    to right,
    transparent 0%,
    black 32px,
    black calc(100% - 32px),
    transparent 100%
  );
}
```
</details>

### Exercise 3: Regular Hexagon Avatar
**Goal**: Clip a square profile picture into a symmetrical hexagon.

<details>
<summary>View Solution</summary>

```css
.avatar-hexagon {
  width: 100px;
  height: 100px;
  object-fit: cover;
  clip-path: polygon(
    50% 0%,
    100% 25%,
    100% 75%,
    50% 100%,
    0% 75%,
    0% 25%
  );
}
```
</details>

### Exercise 4: 8-Point Vertex Morphing Button
**Goal**: Transition a clipped tag from a chamfered diamond to a full rectangle on hover.

<details>
<summary>View Solution</summary>

```css
.morph-pill {
  padding: 0.75rem 1.5rem;
  background-color: #2563eb;
  color: #ffffff;
  font-weight: 600;
  border: none;
  cursor: pointer;
  
  /* 8 vertices: chamfered corners */
  clip-path: polygon(
    10px 0, calc(100% - 10px) 0,
    100% 10px, 100% calc(100% - 10px),
    calc(100% - 10px) 100%, 10px 100%,
    0 calc(100% - 10px), 0 10px
  );
  transition: clip-path 0.25s ease;
}

.morph-pill:hover {
  /* 8 vertices: flat rectangle */
  clip-path: polygon(
    0 0, 100% 0,
    100% 0, 100% 100%,
    100% 100%, 0 100%,
    0 100%, 0 0
  );
}

.morph-pill:focus-visible {
  outline: 2px solid #0f172a;
  outline-offset: 4px;
}
```
</details>

### Exercise 5: Circular Expanding Reveal via `circle()`
**Goal**: Animate a circular reveal overlay from 0% radius to 100% radius.

<details>
<summary>View Solution</summary>

```css
@keyframes circleExpand {
  from {
    clip-path: circle(0% at center);
  }
  to {
    clip-path: circle(100% at center);
  }
}

.reveal-overlay {
  position: absolute;
  inset: 0;
  background-color: #0f172a;
  animation: circleExpand 0.6s cubic-bezier(0.16, 1, 0.3, 1) forwards;
}

@media (prefers-reduced-motion: reduce) {
  .reveal-overlay {
    animation: none;
    clip-path: none;
  }
}
```
</details>

### Exercise 6: Bottom Vignette Fade on Image Card
**Goal**: Mask an image so its bottom 30% smoothly fades into the dark card background.

<details>
<summary>View Solution</summary>

```css
.card-image-masked {
  width: 100%;
  height: 250px;
  object-fit: cover;
  -webkit-mask-image: linear-gradient(to bottom, black 60%, transparent 100%);
  mask-image: linear-gradient(to bottom, black 60%, transparent 100%);
}
```
</details>

---

## 12. Summary / Cheat Sheet

| Property | Value Example | Primary Role |
| :--- | :--- | :--- |
| `clip-path` | `polygon(0 0, 100% 0, ...)` | Binary vector cut; outside clicks pass through |
| `clip-path: circle()` | `circle(50% at center)` | Symmetrical circular clipping mask |
| `clip-path: inset()` | `inset(10px round 8px)` | Inset rectangular box with rounded corners |
| `mask-image` | `linear-gradient(...)` | Soft alpha gradient mask for feathered fades |
| `-webkit-mask-image` | `url(mask.svg)` | Required vendor prefix for Safari/iOS support |
| Polygon Animation | `transition: clip-path` | Requires **exact same vertex count** |

---

## 13. Further Reading
- [W3C CSS Masking Module Level 1](https://www.w3.org/TR/css-masking-1/)
- [MDN Web Docs: clip-path](https://developer.mozilla.org/en-US/docs/Web/CSS/clip-path)
- [MDN Web Docs: mask-image](https://developer.mozilla.org/en-US/docs/Web/CSS/mask-image)
- [Clippy: CSS clip-path maker](https://bennettfeely.com/clippy/)
- [web.dev: How to work with CSS shapes and clipping](https://web.dev/learn/css/masking/)

---

[Previous: 05-blend-modes-and-isolation.md](05-blend-modes-and-isolation.md) | [Module README](README.md) | [Next: 07-transforms-2d-3d.md](07-transforms-2d-3d.md)
