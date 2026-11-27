# Blend Modes & Compositor Isolation

| Metadata | Details |
| :--- | :--- |
| **Topic** | Color Blending Algorithms, `mix-blend-mode`, `background-blend-mode`, Compositor Isolation (`isolation: isolate`), Duotone Effects |
| **Spec Level** | Compositing and Blending Level 1 / Compositing and Blending Level 2 |
| **Baseline Status** | Widely Available (`mix-blend-mode`, `background-blend-mode`, `isolation` Baseline 2015 across all major browsers) |
| **Browsers Tested** | Chrome 120+, Firefox 122+, Safari 17.2+, Edge 120+ |
| **Tested With** | W3C CSS Validator, Chromium Blink Compositor, Safari CoreGraphics |
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

Blend modes allow CSS to combine the color values of a source graphic with the backdrop pixels beneath it, executing mathematical matrix transformations at the compositor level. In digital design, blending powers editorial duotone photography, dynamic contrast-inverting cursor pointers, watermark overlays, and cinematic image textures.

CSS provides two distinct properties for blending:
1. **`background-blend-mode`**: Blends multiple background images and background colors declared on the *same individual element*.
2. **`mix-blend-mode`**: Blends an entire element (and its children) with whatever *other elements* lie beneath it in the stacking order.

A critical challenge in production blending is **unintended bleed**: when an element styled with `mix-blend-mode: difference` blends not only with its intended card container, but bleeds through and inverts the global page background and footer. The modern CSS solution is **`isolation: isolate`**, which establishes a hard stacking boundary to encapsulate blend calculations.

---

## 2. Core Concepts

### The 16 Standard CSS Blend Modes
Blend modes fall into five mathematical families:

1. **Normal**:
   - `normal`: Standard alpha compositing without color math.
2. **Darkening Modes** (Result is always darker; white is neutral/invisible):
   - `multiply`: Multiplies color channel values ($C_s \times C_b$). Excellent for removing white backgrounds from logos.
   - `darken`: Selects the darker channel of source and backdrop ($\min(C_s, C_b)$).
   - `color-burn`: Darkens the backdrop color to reflect source color.
3. **Lightening Modes** (Result is always lighter; black is neutral/invisible):
   - `screen`: Inverts, multiplies, and inverts back ($1 - (1 - C_s) \times (1 - C_b)$). Excellent for removing black backgrounds from lighting effects.
   - `lighten`: Selects the lighter channel of source and backdrop ($\max(C_s, C_b)$).
   - `color-dodge`: Brightens the backdrop to reflect source color.
4. **Contrast Modes** (Darkens darks, lightens lights; 50% gray is neutral):
   - `overlay`: Multiplies darks and screens lights based on the backdrop color.
   - `soft-light`: Softer, gentler variation of overlay.
   - `hard-light`: Multiplies or screens based on the source color.
5. **Inversion & Cancellation Modes**:
   - `difference`: Subtracts the darker color from the lighter color ($|C_b - C_s|$). Identical colors yield pure black; contrasting colors invert.
   - `exclusion`: Lower-contrast variation of difference.
6. **Component Modes** (Transfers specific color attributes):
   - `hue`, `saturation`, `color`, `luminosity`.

### The Isolation Boundary: `isolation: isolate`
By default, `mix-blend-mode` traverses upward through the entire document tree, blending against every ancestor layer down to the root canvas.
Declaring `isolation: isolate;` on a container:
- Generates a new **Stacking Context**.
- Forces all descendant `mix-blend-mode` calculations to terminate at the container's boundary, preventing blend modes from leaking out to parent sections.

---

## 3. How It Works Internally

```
Compositor Isolation Boundary Mechanics

Without isolation: isolate:
[Element (mix-blend-mode: difference)]
   |  (Blends through everything!)
   v
[Card Background]
   |
   v
[Page Body Canvas]  <=== UNINTENDED: Leaks out and inverts whole page!

With isolation: isolate on Card:
+------------------------------------------------------+
| .card { isolation: isolate; }                        |
|                                                      |
|   [Element (mix-blend-mode: difference)]             |
|      |                                               |
|      v                                               |
|   [Card Background]                                  |
|   =====> BLEND TERMINATES HERE! Boundary contained!  |
+------------------------------------------------------+
[Page Body Canvas] (Remains completely unaffected!)
```

---

## 4. Syntax & Usage

### Baseline & Feature Support
- **`mix-blend-mode`**: Baseline Widely Available (Chrome 41+, Safari 8+, Firefox 32+, Edge 79+).
- **`background-blend-mode`**: Baseline Widely Available (Chrome 35+, Safari 8+, Firefox 30+, Edge 79+).
- **`isolation: isolate`**: Baseline Widely Available (Chrome 41+, Safari 8+, Firefox 36+, Edge 79+).

```css
/* Duotone Photo Card using background-blend-mode */
.duotone-photo {
  width: 100%;
  height: 300px;
  background-image: url("portrait.jpg");
  background-color: #2563eb; /* Vibrant brand blue */
  background-size: cover;
  background-blend-mode: luminosity; /* Renders image in shades of blue */
}

/* Encapsulated Mix Blend Mode */
.card-wrapper {
  isolation: isolate; /* Critical boundary */
  position: relative;
  background-color: #ffffff;
}

.card-title {
  mix-blend-mode: difference;
  color: #ffffff;
}
```

---

## 5. Practical Examples

### Example 1: Inverted High-Contrast Dynamic Cursor Pointer
```html
<div class="viewport-canvas">
  <div class="custom-cursor"></div>
  <h1>Precision Engineering</h1>
  <div class="dark-box"></div>
</div>
```

```css
.viewport-canvas {
  position: relative;
  min-height: 400px;
  background-color: #ffffff;
  color: #000000;
  overflow: hidden;
}

.dark-box {
  width: 200px;
  height: 200px;
  background-color: #000000;
}

/* Cursor circle automatically inverts color:
   White over black backgrounds, Black over white backgrounds! */
.custom-cursor {
  position: fixed;
  width: 32px;
  height: 32px;
  border-radius: 50%;
  background-color: #ffffff;
  mix-blend-mode: difference;
  pointer-events: none;
  z-index: 9999;
  transform: translate(-50%, -50%);
}
```

### Example 2: Spotify-Style Spotify Duotone Photography
```html
<div class="artist-duotone-card">
  <img src="artist.jpg" alt="Artist Profile" class="base-image">
  <div class="color-layer-blue"></div>
  <div class="color-layer-coral"></div>
</div>
```

```css
.artist-duotone-card {
  position: relative;
  width: 320px;
  height: 400px;
  overflow: hidden;
  border-radius: 12px;
  isolation: isolate; /* Traps blending inside card */
  background-color: #000000;
}

.base-image {
  width: 100%;
  height: 100%;
  object-fit: cover;
  filter: grayscale(100%) contrast(120%);
}

.color-layer-blue {
  position: absolute;
  inset: 0;
  background-color: #1e3a8a; /* Deep navy shadows */
  mix-blend-mode: multiply;
}

.color-layer-coral {
  position: absolute;
  inset: 0;
  background-color: #f43f5e; /* Vibrant coral highlights */
  mix-blend-mode: screen;
}
```

### Example 3: Knockout Text Over Background Video or Image
```html
<div class="knockout-container">
  <img src="mountain.jpg" alt="" class="bg-media">
  <h1 class="cutout-title">ALPS</h1>
</div>
```

```css
.knockout-container {
  position: relative;
  width: 100%;
  height: 350px;
  background-color: #ffffff;
  overflow: hidden;
  display: flex;
  align-items: center;
  justify-content: center;
  isolation: isolate;
}

.bg-media {
  position: absolute;
  inset: 0;
  width: 100%;
  height: 100%;
  object-fit: cover;
}

.cutout-title {
  position: relative;
  z-index: 2;
  font-family: system-ui, sans-serif;
  font-size: 8rem;
  font-weight: 900;
  letter-spacing: -0.05em;
  color: #000000;
  background-color: #ffffff;
  mix-blend-mode: screen; /* Cuts out black text against white backing */
  margin: 0;
  width: 100%;
  height: 100%;
  display: flex;
  align-items: center;
  justify-content: center;
}
```

### Example 4: Texture Grain Overlay via `overlay`
```css
/* Adds tactile film grain to card surfaces without extra DOM elements */
.grainy-surface {
  background-color: #2563eb;
  background-image:
    url("/assets/textures/noise.png"),
    linear-gradient(to bottom, #1d4ed8, #1e3a8a);
  background-blend-mode: overlay, normal;
  background-size: 120px 120px, 100% 100%;
}
```

### Example 5: Inverting Logo on Floating Navigation Pill
```css
.floating-pill-nav {
  position: sticky;
  top: 1rem;
  margin-inline: auto;
  width: max-content;
  padding: 0.5rem 1.5rem;
  border-radius: 9999px;
  background-color: rgba(0, 0, 0, 0.8);
  backdrop-filter: blur(8px);
  isolation: isolate;
}

.floating-pill-nav .brand-logo {
  mix-blend-mode: exclusion;
  color: #ffffff;
}
```

---

## 6. Best Practices

1. **Always Encapsulate `mix-blend-mode` with `isolation: isolate`**: Any component container utilizing `mix-blend-mode` on its children must declare `isolation: isolate;` to prevent blend calculations from leaking out into surrounding page sections.
2. **Use `multiply` to Knock Out White Backgrounds**: If an asset logo has an unwanted white background, applying `mix-blend-mode: multiply;` turns pure white pixels 100% transparent while preserving black details.
3. **Use `screen` to Knock Out Black Backgrounds**: If a video or particle overlay has a pure black background, `mix-blend-mode: screen;` renders black pixels completely transparent while preserving light particles.
4. **Check Contrast on Dynamic Inversion**: While `mix-blend-mode: difference` produces an inverted color, 50% gray against 50% gray cancels out to 0 (black), creating an invisible zone. Always verify legibility across all expected background states.
5. **Prefer `background-blend-mode` for Static Textures**: When blending gradients with images on the same element, `background-blend-mode` is simpler, cleaner, and has lower compositor overhead than `mix-blend-mode`.
6. **Disable Blending in Windows High Contrast Mode**: WHCM strips blend modes and forces system contrast colors.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Correction |
| :--- | :--- | :--- |
| **Omitting `isolation: isolate` on parent** | `mix-blend-mode` bleeds out, inverting parent sections and the entire document body. | Add `isolation: isolate;` to the immediate container. |
| **Using `background-blend-mode` on child elements** | `background-blend-mode` only blends layers *on the same element*; it ignores sibling DOM nodes. | Use `mix-blend-mode` to blend between separate elements. |
| **Expecting `difference` to work on 50% gray** | Mid-gray subtracted from mid-gray computes to pure black, destroying contrast. | Ensure backgrounds have sufficient polarity (very light or very dark). |
| **Blending over elements with transparent backgrounds** | If the backdrop is transparent, blend modes have no pixel data to calculate against. | Ensure the container has an explicit `background-color`. |
| **Heavy blend mode performance on animations** | Blending full-screen video with complex layers on low-end GPUs causes frame drops. | Limit blended surfaces to isolated component containers. |
| **Using blend modes on critical text without fallback** | Assistive tech or print stylesheets that ignore blend modes may render text unreadable. | Provide strong baseline color contrast before applying blend modes. |

---

## 8. Performance, Accessibility & Browser Support

### Accessibility Compliance (WCAG 2.2 AA)
- **Criterion 1.4.3 (Contrast Minimum)**: Text relying on `mix-blend-mode: difference` must maintain **4.5:1** contrast across all states. If the background contains complex mid-tone gradients, the contrast ratio can drop below compliance thresholds.
- **High Contrast Mode**: In `forced-colors: active`, browser engines disable CSS blend modes. Never rely on blend modes to make text legible.

### Compositor Stacking & GPU Rendering
`mix-blend-mode` forces the browser's graphics engine to rasterize the element and its backdrop into separate intermediate GPU render targets, run the blending shader equation, and write the output back to the frame buffer. Stacking many blended layers together increases GPU memory bandwidth usage.

---

## 9. Comparison / When to Use What

| Dimension | `mix-blend-mode` | `background-blend-mode` |
| :--- | :--- | :--- |
| **Scope** | Between separate DOM elements | Inside a single element's background stack |
| **Bleed Risk** | High (Requires `isolation: isolate`) | **Zero** (Completely self-contained) |
| **Use Cases** | Inverted cursor, video overlays, text cutouts | Duotone photos, noise textures, gradient tints |
| **Markup Overhead**| Requires separate DOM layers | **Zero** (Single CSS declaration) |

---

## 10. Interview Questions

<details>
<summary>1. What is the fundamental difference between <code>mix-blend-mode</code> and <code>background-blend-mode</code>?</summary>

- `background-blend-mode` blends the multiple background layers (such as multiple `background-image` declarations and the `background-color`) declared **on the exact same element**. It is completely self-contained and never interacts with surrounding DOM elements.
- `mix-blend-mode` blends the content and background of an entire element with the **rendered pixels of other elements that lie beneath it** in the DOM stacking order.
</details>

<details>
<summary>2. Why is <code>isolation: isolate</code> essential when working with <code>mix-blend-mode</code>?</summary>

By default, an element with `mix-blend-mode` blends against all stacking contexts underneath it, all the way down to the root `<html>` canvas. This frequently causes the blend mode to leak outside its component card, altering the colors of the main page background, adjacent cards, or footer. 

`isolation: isolate` establishes a new stacking context on the parent container. This tells the compositor that no descendant elements may blend with anything outside this container, confining the blend effect safely within the component boundaries.
</details>

<details>
<summary>3. How does the <code>multiply</code> blend mode work mathematically, and why does it make white backgrounds transparent?</summary>

In the `multiply` blend mode, the normalized color channel values (0.0 to 1.0) of the source and backdrop are multiplied together:
$$C_{\text{result}} = C_{\text{source}} \times C_{\text{backdrop}}$$
Because pure white has a normalized value of $1.0$:
$$1.0 \times C_{\text{backdrop}} = C_{\text{backdrop}}$$
Multiplying by white results in the original backdrop color unchanged. Consequently, any white areas in a graphic (like a JPEG logo with a white background) become completely transparent when placed over a colored backdrop.
</details>

<details>
<summary>4. How does the <code>screen</code> blend mode work mathematically, and why does it make black backgrounds transparent?</summary>

The `screen` blend mode inverts both the source and backdrop colors, multiplies them, and inverts the result:
$$C_{\text{result}} = 1 - (1 - C_{\text{source}}) \times (1 - C_{\text{backdrop}})$$
Because pure black has a normalized value of $0.0$:
$$1 - (1 - 0.0) \times (1 - C_{\text{backdrop}}) = 1 - 1 \times (1 - C_{\text{backdrop}}) = C_{\text{backdrop}}$$
Screening with black leaves the backdrop completely unchanged, making black backgrounds transparent. It is the mathematical opposite of `multiply` and is universally used for light effects, lens flares, and sparks.
</details>

<details>
<summary>5. How does <code>mix-blend-mode: difference</code> create an auto-inverting text or cursor effect?</summary>

The `difference` blend mode subtracts the darker color from the lighter color:
$$C_{\text{result}} = |C_{\text{backdrop}} - C_{\text{source}}|$$
If the source text is pure white ($1.0, 1.0, 1.0$):
- Over a pure black backdrop ($0, 0, 0$): $|0 - 1| = 1$ (Renders pure white).
- Over a pure white backdrop ($1, 1, 1$): $|1 - 1| = 0$ (Renders pure black).
This automatically inverts the text or cursor pointer as it moves across alternating black and white sections.
</details>

<details>
<summary>6. What happens when an element with <code>mix-blend-mode</code> has an ancestor with <code>opacity: 0.99</code>?</summary>

Per the CSS Compositing and Stacking Context specifications, applying any property that generates a new stacking context (such as `opacity < 1`, `transform`, `filter`, or `isolation: isolate`) on an ancestor establishes an implicit isolation boundary. Descendant `mix-blend-mode` effects cannot blend with elements outside that stacking context.
</details>

<details>
<summary>7. What is the neutral color for the <code>overlay</code> blend mode?</summary>

The neutral color for `overlay` is **50% gray** (`rgb(128, 128, 128)`). In `overlay`, pixels darker than 50% gray are multiplied (darkened), and pixels lighter than 50% gray are screened (lightened). Pixels that are exactly 50% gray produce zero change to the underlying image.
</details>

<details>
<summary>8. How do you create a duotone effect using purely CSS on a single element?</summary>

Combine a grayscale image with a linear or solid gradient using `background-blend-mode`:
```css
.duotone {
  background-image: url("photo.jpg");
  background-color: #2563eb;
  background-blend-mode: luminosity;
  background-size: cover;
}
```
`luminosity` preserves the brightness and shadows of the photo while adopting the hue and saturation of the background color.
</details>

---

## 11. Practice Exercises

### Exercise 1: Build an Inverted Reading Cursor
**Goal**: Create an interactive fixed cursor circle that inverts whatever text and background it hovers over.

<details>
<summary>View Solution</summary>

```css
.inverting-cursor {
  position: fixed;
  width: 24px;
  height: 24px;
  border-radius: 50%;
  background-color: #ffffff;
  mix-blend-mode: difference;
  pointer-events: none;
  z-index: 10000;
  transform: translate(-50%, -50%);
}
```
</details>

### Exercise 2: Knock Out a JPEG Logo White Background
**Goal**: Place a JPEG partner logo on a colored card and eliminate the white rectangular box using blend modes.

<details>
<summary>View Solution</summary>

```css
.partner-card {
  background-color: #f1f5f9;
  padding: 2rem;
  border-radius: 8px;
}

.partner-logo-jpeg {
  display: block;
  max-width: 160px;
  /* Multiplies logo pixels; pure white pixels become 100% transparent */
  mix-blend-mode: multiply;
}
```
</details>

### Exercise 3: Encapsulated Blend Card Component
**Goal**: Build a card using `mix-blend-mode: overlay` that is strictly isolated from leaking onto the body canvas.

<details>
<summary>View Solution</summary>

```css
.isolated-card {
  isolation: isolate; /* Critical encapsulation boundary */
  position: relative;
  background-color: #1e293b;
  border-radius: 12px;
  overflow: hidden;
  padding: 2rem;
}

.isolated-card .texture-overlay {
  position: absolute;
  inset: 0;
  background: linear-gradient(135deg, #38bdf8, #818cf8);
  mix-blend-mode: overlay;
  pointer-events: none;
}
```
</details>

### Exercise 4: Pure CSS Film Grain Texture on a Banner
**Goal**: Use `background-blend-mode` to blend a repeating noise pattern over a linear gradient.

<details>
<summary>View Solution</summary>

```css
.grainy-hero-banner {
  height: 350px;
  background-color: #0f172a;
  background-image:
    url("/assets/noise.png"),
    linear-gradient(to bottom right, #1e3a8a, #0f172a);
  background-blend-mode: soft-light, normal;
  background-size: 100px 100px, 100% 100%;
}
```
</details>

### Exercise 5: High-Contrast Duotone Avatar
**Goal**: Turn a standard color profile picture into an emerald duotone badge.

<details>
<summary>View Solution</summary>

```css
.duotone-avatar-container {
  position: relative;
  width: 96px;
  height: 96px;
  border-radius: 50%;
  overflow: hidden;
  isolation: isolate;
  background-color: #064e3b; /* Deep emerald base */
}

.duotone-avatar-container img {
  width: 100%;
  height: 100%;
  object-fit: cover;
  filter: grayscale(100%);
}

.duotone-avatar-container::after {
  content: "";
  position: absolute;
  inset: 0;
  background-color: #10b981; /* Bright emerald tint */
  mix-blend-mode: screen;
}
```
</details>

### Exercise 6: Knockout Cutout Typography
**Goal**: Cut large bold letters out of a solid white overlay to reveal a video background beneath.

<details>
<summary>View Solution</summary>

```css
.video-knockout-wrapper {
  position: relative;
  height: 400px;
  overflow: hidden;
  isolation: isolate;
}

.video-knockout-wrapper video {
  position: absolute;
  inset: 0;
  width: 100%;
  height: 100%;
  object-fit: cover;
}

.knockout-header {
  position: relative;
  width: 100%;
  height: 100%;
  background-color: #ffffff;
  color: #000000;
  mix-blend-mode: screen;
  display: flex;
  align-items: center;
  justify-content: center;
  font-size: 5rem;
  font-weight: 900;
  margin: 0;
}
```
</details>

---

## 12. Summary / Cheat Sheet

| Property / Mode | Core Syntax | Key Practical Role |
| :--- | :--- | :--- |
| `mix-blend-mode` | `mix-blend-mode: difference;` | Blends element with other elements beneath it |
| `background-blend-mode`| `background-blend-mode: overlay;` | Blends backgrounds on the same element |
| `isolation: isolate` | `isolation: isolate;` | Traps blend modes inside container boundary |
| `multiply` | `mix-blend-mode: multiply;` | Darkens; knocks out white backgrounds |
| `screen` | `mix-blend-mode: screen;` | Lightens; knocks out black backgrounds |
| `difference` | `mix-blend-mode: difference;` | Inverts colors; auto-inverting text & cursors |
| `luminosity` | `background-blend-mode: luminosity;`| Creates duotone photography from color tints |

---

## 13. Further Reading
- [W3C Compositing and Blending Level 1](https://www.w3.org/TR/compositing-1/)
- [MDN Web Docs: mix-blend-mode](https://developer.mozilla.org/en-US/docs/Web/CSS/mix-blend-mode)
- [MDN Web Docs: background-blend-mode](https://developer.mozilla.org/en-US/docs/Web/CSS/background-blend-mode)
- [MDN Web Docs: isolation](https://developer.mozilla.org/en-US/docs/Web/CSS/isolation)
- [CSS-Tricks: Basics of CSS Blend Modes](https://css-tricks.com/basics-css-blend-modes/)

---

[Previous: 04-filters-and-backdrop-filter.md](04-filters-and-backdrop-filter.md) | [Module README](README.md) | [Next: 06-clip-path-and-masks.md](06-clip-path-and-masks.md)
