# Backgrounds & Modern Gradients

| Metadata | Details |
| :--- | :--- |
| **Topic** | Background Longhands & Shorthand, Layered Backgrounds, Linear/Radial/Conic Gradients, Modern Color Spaces (`in oklch`), Mesh Gradients |
| **Spec Level** | CSS Backgrounds and Borders Module Level 3 / CSS Images Module Level 4 |
| **Baseline Status** | Widely Available (`background-*`, `linear/radial-gradient` Baseline 2015; `conic-gradient` Baseline 2020; Gradient Color Spaces `in oklch` Baseline 2023) |
| **Browsers Tested** | Chrome 120+, Firefox 122+, Safari 17.2+, Edge 120+ |
| **Tested With** | Chromium Blink Compositor, Safari WebKit, Firefox Gecko, Playwright |
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

Backgrounds in CSS represent one of the most flexible visual surfaces on the web. Rather than merely applying a static solid color, modern CSS background painting supports layered compositing stacks, complex mathematical gradient generators, and wide-gamut color interpolation spaces:
- **Multi-Layer Stacking**: Composing multiple textures, transparent vector motifs, and lighting gradients in a single CSS declaration without adding extra DOM nodes.
- **Modern Gradient Genera**: `linear-gradient()`, `radial-gradient()`, and `conic-gradient()` render resolution-independent graphics computed directly by the browser layout engine.
- **Perceptually Uniform Color Spaces (Baseline 2023)**: Gradients interpolated in legacy `sRGB` suffer from muddy, gray "dead zones" in the midpoint transition between complementary colors (e.g., blue to yellow). By interpolating in modern spaces like `oklch` (`linear-gradient(in oklch to right, blue, yellow)`), transitions maintain vibrant chroma and uniform perceived lightness.
- **Scroll Pinning**: `background-attachment: local` vs `scroll` vs `fixed` dictates how backgrounds pan relative to their parent container or the viewport.

---

## 2. Core Concepts

### The Background Property Suite
- `background-color`: Base solid fill color painted beneath all image layers.
- `background-image`: Comma-separated list of images or generated gradients (`url(...)`, `linear-gradient(...)`).
- `background-repeat`: `repeat`, `no-repeat`, `repeat-x`, `repeat-y`, `space`, `round`.
- `background-position`: Offsets image positioning (`top left`, `50% 50%`, `right 20px bottom 10px`).
- `background-size`: `auto`, `cover` (scales to completely fill box), `contain` (scales to fit entirely within box), or explicit length/percentage (`100% auto`, `24px 24px`).
- `background-attachment`:
  - `scroll` (Default): Fixed relative to the element; scrolls with the main page.
  - `local`: Fixed relative to the element's *contents*; scrolls when the element's internal scroll container scrolls.
  - `fixed`: Fixed relative to the viewport. (Warning: Disables hardware acceleration and creates rendering glitches on mobile iOS).
- `background-origin` & `background-clip`: `border-box`, `padding-box`, `content-box`. (Can also use `background-clip: text` to clip background gradients to glyph text outlines).

### The Multi-Layer Stacking Order
When multiple background images are declared in a comma-separated list:
```css
.hero {
  background-image: url("overlay.png"), linear-gradient(to bottom, #000, #fff), url("base.jpg");
  background-color: #000000;
}
```
**The Golden Rule**: The **first layer listed is rendered on top**, closest to the viewer's eye. The last layer listed is rendered at the bottom, directly above `background-color`.

### Modern Gradient Types
1. **`linear-gradient([angle | to direction], [in color-space], color-stop-1, ...)`**: Transitions colors along a straight directional line.
2. **`radial-gradient([shape] [size] at [position], [in color-space], color-stop-1, ...)`**: Radiates outward from a central focal point (circle or ellipse).
3. **`conic-gradient([from angle] at [position], [in color-space], color-stop-1, ...)`**: Transitions colors sweeping around a center point (like a color wheel or radar sweep).

---

## 3. How It Works Internally

```
Color Interpolation Dead Zone Comparison

1. Legacy sRGB Interpolation (Muddy Midpoint):
   [Blue #0000ff] -------- [Muddy Gray/Brown Void] -------- [Yellow #ffff00]
   (In sRGB, mathematical midpoint drops luminance, causing a dirty desaturated band)

2. Modern OKLCH Interpolation (in oklch):
   [Blue #0000ff] -------- [Vibrant Green Transition] -------- [Yellow #ffff00]
   (In OKLCH, lightness and chroma are perceptually linear, preserving vivid hues)
```

### Stacking Order Layer Diagram
```
[Viewer Eye View]
       |
       v
Layer 1 (Top)    : url("translucent-noise.png")  (Z-index equivalent: Top)
Layer 2 (Middle) : linear-gradient(...)          (Z-index equivalent: Mid)
Layer 3 (Bottom) : url("mountain-photo.jpg")     (Z-index equivalent: Low)
Base Fill        : background-color: #0f172a     (Rendered on canvas floor)
```

---

## 4. Syntax & Usage

### Baseline & Feature Support
- **Core Backgrounds & Linear/Radial Gradients**: Baseline Widely Available.
- **`conic-gradient()`**: Baseline 2020 (Chrome 69+, Safari 12.1+, Firefox 83+).
- **Gradient Color Interpolation Spaces (`in oklch`)**: Baseline 2023 (Chrome 111+, Safari 16.2+, Firefox 113+).
- **`background-clip: text`**: Baseline Widely Available (Chrome 4+, Safari 4+, Firefox 49+).

```css
/* Modern High-Vibrancy Linear Gradient in OKLCH */
.vibrant-banner {
  background: linear-gradient(
    in oklch to right,
    oklch(60% 0.25 240), /* Electric Blue */
    oklch(75% 0.22 140), /* Vivid Green */
    oklch(85% 0.20 95)   /* Radiant Yellow */
  );
}

/* Conic Gradient Pie Chart / Color Ring */
.conic-chart {
  background: conic-gradient(
    #2563eb 0% 40%,    /* Blue 40% */
    #10b981 40% 70%,   /* Green 30% */
    #f59e0b 70% 100%   /* Amber 30% */
  );
  border-radius: 50%;
}
```

---

## 5. Practical Examples

### Example 1: Multi-Layer Atmospheric Hero Header
```html
<header class="ambient-hero">
  <div class="hero-content">
    <h1>Planetary Scale Systems</h1>
    <p>Engineered for zero downtime across multi-cloud regions.</p>
  </div>
</header>
```

```css
.ambient-hero {
  position: relative;
  min-height: 480px;
  display: flex;
  align-items: center;
  justify-content: center;
  color: #ffffff;
  padding: 2rem;

  /* Multi-Layered Stack:
     1. Vignette gradient darkening corners
     2. Radial glowing beam from top center
     3. Subtle dark linear gradient
     4. Base deep navy color */
  background-image:
    radial-gradient(ellipse at 50% 0%, rgba(56, 189, 248, 0.25) 0%, transparent 70%),
    radial-gradient(circle at center, transparent 40%, rgba(15, 23, 42, 0.8) 100%),
    linear-gradient(to bottom, #0f172a, #020617);
  background-color: #020617;
}

.hero-content {
  max-width: 65ch;
  text-align: center;
}

.hero-content h1 {
  font-size: clamp(2.5rem, 5vw, 4rem);
  font-weight: 800;
  letter-spacing: -0.025em;
  line-height: 1.1;
  margin-block-end: 1rem;
}
```

### Example 2: Dynamic Animated Gradient Text via `background-clip: text`
```html
<h2 class="gradient-headline">Next-Generation Distributed Storage</h2>
```

```css
.gradient-headline {
  font-family: system-ui, sans-serif;
  font-size: 3rem;
  font-weight: 900;
  letter-spacing: -0.03em;
  
  /* Clips background gradient to the glyph outlines */
  background: linear-gradient(in oklch 135deg, #3b82f6, #ec4899, #f59e0b);
  -webkit-background-clip: text;
  background-clip: text;
  
  /* Makes text transparent so the background gradient shows through */
  -webkit-text-fill-color: transparent;
  color: transparent;
}
```

### Example 3: Pure CSS Blueprint Grid Pattern
```css
/* Creates a mathematical grid pattern without external SVG or PNG files */
.blueprint-grid {
  background-color: #0f172a;
  background-image:
    linear-gradient(rgba(255, 255, 255, 0.05) 1px, transparent 1px),
    linear-gradient(90deg, rgba(255, 255, 255, 0.05) 1px, transparent 1px);
  background-size: 24px 24px;
  background-position: -1px -1px;
}
```

### Example 4: Conic Gradient Circular Progress Meter
```html
<div class="progress-ring" role="progressbar" aria-valuenow="72" aria-valuemin="0" aria-valuemax="100">
  <div class="inner-circle">
    <span class="value">72%</span>
  </div>
</div>
```

```css
.progress-ring {
  width: 140px;
  height: 140px;
  border-radius: 50%;
  
  /* Sweep gradient: 72% active blue, 28% neutral gray */
  background: conic-gradient(#2563eb 0% 72%, #e2e8f0 72% 100%);
  display: flex;
  align-items: center;
  justify-content: center;
}

.inner-circle {
  width: 110px;
  height: 110px;
  border-radius: 50%;
  background-color: #ffffff;
  display: flex;
  align-items: center;
  justify-content: center;
  font-family: system-ui, sans-serif;
  font-weight: 700;
  font-size: 1.5rem;
  color: #0f172a;
}
```

### Example 5: High-Performance Mesh Gradient Approximation
```css
/* Multi-radial gradient simulation of complex organic mesh gradients */
.mesh-gradient-surface {
  background-color: #1e1b4b;
  background-image:
    radial-gradient(at 0% 0%, hsla(253, 75%, 65%, 0.8) 0px, transparent 50%),
    radial-gradient(at 100% 0%, hsla(339, 82%, 62%, 0.8) 0px, transparent 50%),
    radial-gradient(at 100% 100%, hsla(190, 95%, 50%, 0.8) 0px, transparent 50%),
    radial-gradient(at 0% 100%, hsla(260, 84%, 60%, 0.8) 0px, transparent 50%);
  background-size: 100% 100%;
}
```

---

## 6. Best Practices

1. **Always Specify a Solid Fallback `background-color`**: If an image fails to load or a gradient parser encounters an unsupported syntax, the solid `background-color` ensures text contrast remains WCAG compliant.
2. **Use OKLCH for Rich Color Gradients**: Always specify `in oklch` when building gradients between complementary or diverse hues to eliminate dull, muddy gray midpoint bands.
3. **Avoid `background-attachment: fixed` on Mobile Viewports**: On iOS Safari and Android Chrome, `fixed` disables GPU tile optimization, causing aggressive frame drops and jank during scrolling.
4. **Be Careful with `background-clip: text` Accessibility**: If CSS fails or background clipping is stripped in high-contrast mode, text set to `color: transparent` can become completely invisible. Always provide a fallback text color.
5. **Remember Layer Stacking Order**: First listed layer is on TOP; last listed layer is on BOTTOM.
6. **Use Percentages or Fixed Pixels for Pattern `background-size`**: When creating geometric patterns with repeating gradients, specify exact dimensions (`background-size: 20px 20px;`) to prevent rounding antialiasing blur.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Correction |
| :--- | :--- | :--- |
| **Omitting Fallback `background-color`** | White text over a failed background image renders as white-on-white (illegible). | Always pair `background-image` with an accessible `background-color`. |
| **Inverted Multiple Background Order** | Putting an opaque image first covers all gradients declared after it. | Put translucent gradients/overlays first; opaque image last. |
| **`color: transparent` without fallback in text gradients** | In browsers or print sheets where `background-clip: text` fails, text vanishes. | Use `@supports (background-clip: text)` to scope transparency. |
| **Using `background-attachment: fixed` on mobile** | Triggers CPU software repaints on every touch scroll frame, degrading performance. | Use `position: fixed` pseudo-elements instead. |
| **Muddy midpoints in wide-color gradients** | Legacy `sRGB` interpolation darkens colors at the halfway point. | Add `in oklch` to gradient declarations. |
| **Forgetting `-webkit-background-clip: text`** | Safari requires the vendor-prefixed property for text clipping to function. | Include both `-webkit-background-clip: text;` and `background-clip: text;`. |

---

## 8. Performance, Accessibility & Browser Support

### Accessibility Compliance (WCAG 2.2 AA)
- **Criterion 1.4.3 (Contrast Minimum)**: Text placed on gradients must maintain at least **4.5:1** contrast against *every* point of the gradient background. Test the darkest and lightest stops of the gradient.
- **Windows High Contrast Mode (WHCM)**: In `forced-colors: active`, the browser removes all CSS background images and gradients. Critical content or icons must never be delivered exclusively via background images.

---

## 9. Comparison / When to Use What

| Gradient Type | Geometry | Axis | Best Target Use Case |
| :--- | :--- | :--- | :--- |
| **`linear-gradient`** | Straight directional line | Angle (0–360deg) or direction (`to bottom`) | Buttons, hero banners, text clipping, subtle lighting |
| **`radial-gradient`** | Radiating circle or ellipse | Origin focal point (`center`, `top left`) | Spotlights, vignettes, glow effects, spheres |
| **`conic-gradient`** | Rotational sweep around center | Angle (`from 45deg at center`) | Pie charts, progress rings, radar sweeps, color pickers |
| **Multi-Radial Mesh** | Overlapping radial spots | Multi-point coordinate grid | Modern organic brand surfaces, glass textures |

---

## 10. Interview Questions

<details>
<summary>1. In what order are multiple background images layered in CSS, and which layer sits closest to the user?</summary>

In CSS, multiple background layers are declared in a comma-separated list. According to the W3C specification, the **first layer specified is rendered on the very top** (closest to the viewer), and subsequent layers are rendered underneath in descending order. The final declaration in the stack is rendered directly above the solid `background-color` layer, which sits at the very bottom of the canvas.
</details>

<details>
<summary>2. Why do linear gradients between complementary colors (e.g., blue and yellow) often appear dirty or gray in the middle, and how does modern CSS solve this?</summary>

Historically, gradients interpolated colors in the **sRGB** color space. In sRGB, mathematical interpolation calculates straight numerical averages of red, green, and blue coordinates. Because sRGB is not perceptually uniform, the midpoint between saturated blue and saturated yellow drops significantly in perceived luminance, creating a desaturated, muddy gray/brown band.

CSS Images Module Level 4 introduces **Color Interpolation Spaces**. Specifying `linear-gradient(in oklch to right, blue, yellow)` calculates interpolation in the perceptually uniform **OKLCH** color space, keeping perceived lightness and saturation constant throughout the curve and producing a vibrant green midpoint.
</details>

<details>
<summary>3. What is the difference between <code>background-attachment: scroll</code> and <code>background-attachment: local</code>?</summary>

- `background-attachment: scroll` (the default) binds the background image to the element itself. If the element contains an internal scrollbar (`overflow: auto`), the background remains fixed in place while the internal content scrolls past it.
- `background-attachment: local` binds the background image to the element's *content*. When internal content scrolls inside the container, the background image scrolls synchronously with the text.
</details>

<details>
<summary>4. Why is <code>background-attachment: fixed</code> notorious for causing performance and rendering bugs on mobile Safari (iOS)?</summary>

Mobile browsers render pages by dividing the DOM into independent compositor tiles uploaded to the GPU. With `background-attachment: fixed`, the background coordinates are bound to the mobile viewport rather than the document layout. Because the browser cannot predict where the background will sit during dynamic address bar expansion and momentum scrolling, it must continuously invalidate and repaint the entire viewport tile stack on the CPU on every single frame, causing severe frame drops (jank) and battery drain.
</details>

<details>
<summary>5. How does <code>background-clip: text</code> work, and what precautions are necessary to prevent invisible text?</summary>

`background-clip: text` instructs the layout engine to clip the element's background layers to the foreground glyph shapes of its text content. For the background to be visible through the text, the text itself must be made transparent (`color: transparent;` or `-webkit-text-fill-color: transparent;`).

**Precaution**: If a browser does not support the property or if the user is in High Contrast Mode (which strips background images), text set to `color: transparent` becomes completely invisible. Developers must wrap the rule in `@supports (background-clip: text)` and provide an opaque fallback text `color`.
</details>

<details>
<summary>6. What is the difference between <code>background-origin</code> and <code>background-clip</code>?</summary>

- `background-origin` determines the **starting position coordinates** ($0, 0$) of the background image (calculating where `background-position: 0 0` anchors: `border-box`, `padding-box`, or `content-box`).
- `background-clip` determines the **painting boundary** beyond which the background is clipped and made invisible (`border-box`, `padding-box`, `content-box`, or `text`).
</details>

<details>
<summary>7. How do you create hard color stops in a CSS gradient without blurry color blending?</summary>

By declaring the next color stop at the exact same percentage or length position as the preceding stop:
```css
background: linear-gradient(to right, #2563eb 50%, #10b981 50%);
```
Modern CSS also allows concise two-position syntax:
```css
background: linear-gradient(to right, #2563eb 0% 50%, #10b981 50% 100%);
```
This produces an instant, razor-sharp geometric boundary down the middle.
</details>

<details>
<summary>8. What happens to CSS backgrounds in Windows High Contrast Mode (WHCM)?</summary>

Under `@media (forced-colors: active)`, the browser automatically suppresses all `background-image` declarations (including all linear, radial, and conic gradients) and forces `background-color` to system `Canvas` or `ButtonFace`. Any critical visual information conveyed via background graphics disappears entirely, which is why essential icons and controls must be rendered using semantic HTML or inline SVG.
</details>

---

## 11. Practice Exercises

### Exercise 1: Build a Pure CSS Checkerboard Pattern
**Goal**: Create a repeating 20px checkerboard pattern using two overlapping linear gradients.

<details>
<summary>View Solution</summary>

```css
.checkerboard-pattern {
  background-color: #f1f5f9;
  background-image:
    linear-gradient(45deg, #cbd5e1 25%, transparent 25%),
    linear-gradient(-45deg, #cbd5e1 25%, transparent 25%),
    linear-gradient(45deg, transparent 75%, #cbd5e1 75%),
    linear-gradient(-45deg, transparent 75%, #cbd5e1 75%);
  background-size: 20px 20px;
  background-position:
    0 0,
    0 10px,
    10px -10px,
    -10px 0px;
}
```
</details>

### Exercise 2: Implement Modern OKLCH Vibrant Gradient
**Goal**: Construct a 3-stop linear gradient transitioning from purple to orange without midpoint desaturation.

<details>
<summary>View Solution</summary>

```css
.vibrant-gradient {
  background: linear-gradient(
    in oklch 120deg,
    oklch(55% 0.28 300), /* Vivid Purple */
    oklch(68% 0.24 20),  /* Vivid Coral */
    oklch(78% 0.20 60)   /* Radiant Orange */
  );
}
```
</details>

### Exercise 3: Accessible Gradient Text Heading
**Goal**: Create gradient text with progressive enhancement safeguards against invisible text failure.

<details>
<summary>View Solution</summary>

```css
.safe-gradient-text {
  font-size: 2.5rem;
  font-weight: 800;
  color: #2563eb; /* Accessible fallback color if clipping fails */
}

@supports (background-clip: text) or (-webkit-background-clip: text) {
  .safe-gradient-text {
    background: linear-gradient(to right, #2563eb, #9333ea);
    -webkit-background-clip: text;
    background-clip: text;
    -webkit-text-fill-color: transparent;
    color: transparent;
  }
}
```
</details>

### Exercise 4: Conic Gradient Radar Scanner
**Goal**: Build a rotating radar scan disk using a conic gradient and CSS animation.

<details>
<summary>View Solution</summary>

```css
@keyframes radarSweep {
  from { transform: rotate(0deg); }
  to { transform: rotate(360deg); }
}

.radar-dish {
  width: 160px;
  height: 160px;
  border-radius: 50%;
  background: conic-gradient(from 0deg, rgba(16, 185, 129, 0.4) 0deg, transparent 90deg);
  border: 2px solid #10b981;
  animation: radarSweep 2s linear infinite;
}

@media (prefers-reduced-motion: reduce) {
  .radar-dish {
    animation: none;
  }
}
```
</details>

### Exercise 5: Internal Scroll Sync with `background-attachment: local`
**Goal**: Build a code block whose watermark background scrolls synchronously with long code contents.

<details>
<summary>View Solution</summary>

```css
.code-scroll-pane {
  max-height: 250px;
  overflow: auto;
  padding: 1.5rem;
  background-color: #0f172a;
  background-image: linear-gradient(rgba(255, 255, 255, 0.03) 1px, transparent 1px);
  background-size: 100% 24px;
  background-attachment: local; /* Critical: scrolls with code */
}
```
</details>

### Exercise 6: Multi-Stop Pill Button Gradient
**Goal**: Build an action button with a subtle top-to-bottom inner lighting highlight.

<details>
<summary>View Solution</summary>

```css
.glossy-button {
  padding: 0.75rem 1.5rem;
  border-radius: 9999px;
  border: 1px solid rgba(255, 255, 255, 0.2);
  color: #ffffff;
  font-weight: 600;
  cursor: pointer;
  background:
    linear-gradient(to bottom, rgba(255, 255, 255, 0.2) 0%, rgba(255, 255, 255, 0) 50%),
    linear-gradient(to bottom, #2563eb, #1d4ed8);
  box-shadow: 0 2px 4px rgba(0, 0, 0, 0.1);
}

.glossy-button:focus-visible {
  outline: 2px solid #2563eb;
  outline-offset: 2px;
}
```
</details>

---

## 12. Summary / Cheat Sheet

| Property / Feature | Syntax Example | Key Role |
| :--- | :--- | :--- |
| `background-image` | `linear-gradient(...)` | Declares resolution-independent gradient layers |
| `linear-gradient` | `linear-gradient(to right, ...)` | Directional linear transition between colors |
| `radial-gradient` | `radial-gradient(circle at center, ...)`| Radiating circular or elliptical spotlight |
| `conic-gradient` | `conic-gradient(from 0deg, ...)` | 360-degree rotational color sweep |
| `in oklch` | `linear-gradient(in oklch to right, ...)`| Eliminates desaturated gray midpoint dead zones |
| `background-clip: text`| `background-clip: text;` | Clips background painting to glyph character shapes |
| `background-attachment`| `local` | Scrolls background with container content |

---

## 13. Further Reading
- [W3C CSS Backgrounds and Borders Module Level 3](https://www.w3.org/TR/css-backgrounds-3/)
- [W3C CSS Images Module Level 4 - Gradients in Color Spaces](https://www.w3.org/TR/css-images-4/#gradients)
- [MDN Web Docs: CSS Gradients](https://developer.mozilla.org/en-US/docs/Web/CSS/CSS_images/Using_CSS_gradients)
- [web.dev: Make beautiful gradients with OKLCH](https://web.dev/blog/css-color-4-gradient)
- [Lea Verou: CSS3 Patterns Gallery](https://lea.verou.me/css3patterns/)

---

[Previous: Module 06 README](file:///e:/Learning/engineering-notes/01-languages/css/06-typography-and-text/README.md) | [Module README](README.md) | [Next: 02-borders-radius-and-outlines.md](02-borders-radius-and-outlines.md)
