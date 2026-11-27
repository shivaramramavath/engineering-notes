# Image Processing & SVG in CSS

| Metadata | Details |
| :--- | :--- |
| **Topic** | Image Box Framing (`object-fit`, `object-position`), `aspect-ratio` Zero-CLS, `image-rendering: pixelated`, Coloring SVG Masks with CSS Variables |
| **Spec Level** | CSS Images Module Level 3 / CSS Images Module Level 4 / CSS Masking Module Level 1 |
| **Baseline Status** | Widely Available (`object-fit`, `object-position` Baseline 2015; `aspect-ratio` Baseline 2021; `image-rendering` Baseline 2020) |
| **Browsers Tested** | Chrome 120+, Firefox 122+, Safari 17.2+, Edge 120+ |
| **Tested With** | Chromium Blink Image Decoder, Safari WebKit, Playwright, Core Web Vitals (CLS) |
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

Images and vector assets on the web represent both the primary visual medium for user engagement and the primary culprit behind poor performance metrics (especially Cumulative Layout Shift [CLS] and Largest Contentful Paint [LCP]). Historically, fitting non-uniform photography into rigid UI card layouts required fragile CSS background hacks or server-side cropping pipelines.

Modern CSS provides comprehensive control over image rasterization, framing, and vector coloring:
- **`object-fit` & `object-position`**: Enforces precise bounding box framing (e.g., `cover`, `contain`) directly on native `<img>` and `<video>` tags without losing HTML semantics.
- **`aspect-ratio` (Baseline 2021)**: Reserves exact spatial dimensions for images *before* network bytes download, completely eliminating image-induced layout shift ($CLS = 0.00$).
- **`image-rendering`**: Switches between smooth bicubic interpolation and sharp nearest-neighbor interpolation (`pixelated`), essential for retro gaming art, pixel graphics, and scannable QR codes.
- **Coloring External SVGs with CSS Variables**: Resolving the long-standing limitation where external SVGs loaded via `<img>` cannot be recolored with CSS. By utilizing `mask-image` paired with `background-color`, external SVGs can be tinted to any CSS color variable.

---

## 2. Core Concepts

### `object-fit` Values
Controls how the contents of a replaced element (`<img>`, `<video>`) fit into its box:
- `fill` (Default): Stretches image to completely fill box, distorting aspect ratio.
- `cover`: Scales image to fill box while preserving intrinsic aspect ratio; excess dimensions are clipped.
- `contain`: Scales image to fit entirely within box while preserving aspect ratio; leaves empty letterbox gutters if proportions differ.
- `none`: Preserves image's natural dimensions without scaling; overflows or is clipped by the box.
- `scale-down`: Compares `none` and `contain` and selects the smaller resulting dimension.

### `object-position`
Dictates the alignment focal point of an image within its bounding box (identical to `background-position`):
```css
img.portrait-avatar {
  object-fit: cover;
  object-position: top center; /* Focuses on user face rather than torso */
}
```

### The Zero-CLS Rule: `aspect-ratio`
Prior to Baseline 2021, responsive images without fixed heights collapsed to `height: 0` until downloaded, causing content below to abruptly snap downward.
```css
.card-img {
  width: 100%;
  aspect-ratio: 16 / 9;
  object-fit: cover;
}
```
The browser instantly calculates the exact pixel height from the container's width, reserving the space on the page layout before a single network byte arrives.

### Coloring External SVGs with CSS Variables
An SVG loaded via `<img src="icon.svg">` cannot be styled with CSS `currentColor` or `fill`. However, using CSS Masking:
```css
.dynamic-tint-icon {
  width: 24px;
  height: 24px;
  background-color: var(--brand-color);
  -webkit-mask: url("/assets/icon.svg") no-repeat center / contain;
  mask: url("/assets/icon.svg") no-repeat center / contain;
}
```
The SVG acts as a transparency stencil over the CSS `background-color`, allowing external SVGs to be dynamically recolored using CSS variables and hover states!

---

## 3. How It Works Internally

```
object-fit: cover vs contain Geometry

Original 400x200 Image (2:1 ratio) inside 200x200 Box (1:1):

1. object-fit: cover
   +--------------------+
   | [Clipped]          |
   | +----------------+ |  ===> Fills entire 200x200 box!
   | | 200x200 Window | |       Preserves aspect ratio;
   | +----------------+ |       Clips 100px from left & right.
   | [Clipped]          |
   +--------------------+

2. object-fit: contain
   +--------------------+
   | Empty Letterbox    |
   | +----------------+ |  ===> Fits entirely within box!
   | | 200x100 Image  | |       Preserves aspect ratio;
   | +----------------+ |       Leaves empty letterbox bars.
   | Empty Letterbox    |
   +--------------------+
```

---

## 4. Syntax & Usage

### Baseline & Feature Support
- **`object-fit` & `object-position`**: Baseline Widely Available (Chrome 31+, Safari 7.1+, Firefox 36+, Edge 79+).
- **`aspect-ratio`**: Baseline 2021 (Chrome 88+, Safari 15+, Firefox 89+, Edge 88+).
- **`image-rendering: pixelated`**: Baseline 2020 (Chrome 41+, Safari 10+, Firefox 93+).

```css
/* Zero-CLS Responsive Video/Image Container */
.media-cover {
  width: 100%;
  aspect-ratio: 16 / 9;
  object-fit: cover;
  object-position: center;
  border-radius: 8px;
}

/* Crisp Retro Pixel Graphics */
.pixel-sprite {
  image-rendering: pixelated;
  width: 128px;
  height: 128px;
}
```

---

## 5. Practical Examples

### Example 1: High-Performance Zero-CLS Product Card
```html
<article class="product-card">
  <img src="sneaker.jpg"
       alt="Men's Velocity Running Sneaker"
       class="product-thumb"
       width="400"
       height="300"
       loading="lazy">
  <div class="product-details">
    <h3>Velocity Runner</h3>
    <p class="price">$140.00</p>
  </div>
</article>
```

```css
.product-card {
  max-width: 320px;
  border: 1px solid #e2e8f0;
  border-radius: 12px;
  overflow: hidden;
  background-color: #ffffff;
}

.product-thumb {
  width: 100%;
  aspect-ratio: 4 / 3; /* Reserves exact layout space */
  object-fit: cover;
  object-position: center;
  display: block;
  background-color: #f1f5f9; /* Skeleton placeholder color */
}

.product-details {
  padding: 1.25rem;
}
```

### Example 2: Dynamic Theming of External SVG Icons via CSS Mask
```html
<div class="dashboard-nav">
  <button type="button" class="nav-item">
    <span class="icon-mask icon-analytics"></span>
    <span>Analytics</span>
  </button>
  <button type="button" class="nav-item active">
    <span class="icon-mask icon-security"></span>
    <span>Security</span>
  </button>
</div>
```

```css
.nav-item {
  display: flex;
  align-items: center;
  gap: 0.75rem;
  padding: 0.75rem 1rem;
  border: none;
  background: none;
  color: #64748b;
  font-weight: 600;
  cursor: pointer;
  border-radius: 6px;
  transition: color 0.2s ease, background-color 0.2s ease;
}

.nav-item:hover,
.nav-item.active {
  color: #2563eb;
  background-color: #eff6ff;
}

/* Base Mask Icon Stencil */
.icon-mask {
  width: 20px;
  height: 20px;
  display: inline-block;
  background-color: currentColor; /* Adopts text color automatically! */
  -webkit-mask-repeat: no-repeat;
  mask-repeat: no-repeat;
  -webkit-mask-position: center;
  mask-position: center;
  -webkit-mask-size: contain;
  mask-size: contain;
}

.icon-analytics {
  -webkit-mask-image: url("/assets/icons/analytics.svg");
  mask-image: url("/assets/icons/analytics.svg");
}

.icon-security {
  -webkit-mask-image: url("/assets/icons/security.svg");
  mask-image: url("/assets/icons/security.svg");
}

.nav-item:focus-visible {
  outline: 2px solid #2563eb;
  outline-offset: 2px;
}
```

### Example 3: QR Code & Pixel Art Sharp Scaling
```css
/* Prevents bicubic antialiasing blur when blowing up small QR codes */
.qr-code-scanner {
  width: 256px;
  height: 256px;
  image-rendering: pixelated; /* Chrome, Edge, Safari */
  image-rendering: crisp-edges; /* Firefox fallback */
}
```

### Example 4: Circular User Avatar with Face-Tracking `object-position`
```css
.profile-avatar {
  width: 80px;
  height: 80px;
  border-radius: 50%;
  object-fit: cover;
  /* Positions crop focus on top 15% where faces are typically located */
  object-position: 50% 15%;
  border: 2px solid #ffffff;
  box-shadow: 0 2px 4px rgba(0, 0, 0, 0.1);
}
```

### Example 5: Video Background Cover
```html
<div class="video-hero">
  <video autoplay muted loop playsinline class="bg-video">
    <source src="background.mp4" type="video/mp4">
  </video>
  <div class="hero-overlay">
    <h1>Autonomous Cloud Intelligence</h1>
  </div>
</div>
```

```css
.video-hero {
  position: relative;
  height: 450px;
  overflow: hidden;
}

.bg-video {
  position: absolute;
  inset: 0;
  width: 100%;
  height: 100%;
  object-fit: cover; /* Fills container seamlessly like a background image */
  z-index: 1;
}

.hero-overlay {
  position: relative;
  z-index: 2;
  color: #ffffff;
  display: flex;
  align-items: center;
  justify-content: center;
  height: 100%;
}
```

---

## 6. Best Practices

1. **Always Set `aspect-ratio` on Responsive Images**: Eliminate Cumulative Layout Shift (CLS) by declaring `aspect-ratio: 16 / 9;` on media wrappers.
2. **Always Pair `aspect-ratio` with `object-fit: cover`**: Prevents images from distorting or squishing when their intrinsic pixel dimensions differ from the specified aspect ratio.
3. **Use the CSS Mask Trick to Color External SVGs**: Avoid inlining thousands of lines of identical SVG XML across cards; load the external SVG file as a CSS `mask-image` over `background-color: currentColor`.
4. **Tune `object-position` for Human Portraits**: Default `object-position: center` often cuts off the tops of human heads in portrait photography. Set `object-position: 50% 20%;` to anchor framing on faces.
5. **Set `image-rendering: pixelated` on Scannable Barcodes & QR Codes**: Blurry interpolation can render camera scanners unable to decode pixel barcodes.
6. **Include HTML `width` and `height` Attributes**: Even with CSS `aspect-ratio`, providing HTML dimension attributes (`<img width="800" height="600">`) allows browser layout engines to compute aspect ratios before CSS downloads.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Correction |
| :--- | :--- | :--- |
| **Omitting `object-fit: cover` when fixing heights** | Setting `width: 100%; height: 200px;` stretches and squashes image proportions. | Add `object-fit: cover;` to preserve intrinsic proportions. |
| **Omitting `-webkit-mask` on SVG tinting** | Safari and iOS devices fail to render the mask image, displaying a solid colored block. | Declare both `-webkit-mask` and standard `mask` properties. |
| **Relying on `width: 100%; height: auto;` without `aspect-ratio`** | Causes CLS. Height evaluates to 0px until image metadata arrives over the network. | Add `aspect-ratio: 16 / 9;` or HTML `width` and `height`. |
| **Using `image-rendering: pixelated` on smooth photos** | Turns smooth gradients and portraits into blocky, stair-stepped retro pixel blocks. | Reserve `pixelated` for pixel art, sprites, and QR codes. |
| **Using `object-fit` on non-replaced elements** | `object-fit` only applies to replaced elements (`<img>`, `<video>`, `<canvas>`). It has zero effect on `<div>`. | Use `background-size` on standard containers. |
| **Missing alt text when using CSS masks for icons** | CSS mask icons have no semantic representation in the DOM. | Ensure the parent `<button>` has `aria-label` or visible text. |

---

## 8. Performance, Accessibility & Browser Support

### Core Web Vitals (Zero CLS Guarantee)
When an `<img>` tag specifies:
```html
<img src="photo.jpg" alt="..." width="1600" height="900" style="width: 100%; height: auto;">
```
Modern browser engines (Chromium, WebKit, Gecko) automatically calculate an intrinsic aspect ratio ($1600/900 = 16:9$) from the HTML attributes and reserve that exact proportional box during initial HTML parsing, resulting in **$CLS = 0.00$**.

---

## 9. Comparison / When to Use What

| Technique | Semantic HTML | Browser Cached | CSS Variable Theming | Best Used For |
| :--- | :--- | :--- | :--- | :--- |
| **`<img>` with `object-fit`** | **Yes (`<img>`)** | **Yes (HTTP Cache)** | No | Content photos, articles, e-commerce products |
| **CSS `mask-image` over BG** | Yes (Container) | **Yes (Single SVG file)**| **Yes (`currentColor`)**| Navigation icons, toolbars, UI status badges |
| **Inline `<svg>`** | Yes (`<svg>`) | No (Inlined in DOM) | **Yes (`fill`)** | Complex multi-color illustrations, animations |
| **CSS `background-image`** | No (Purely decorative)| **Yes** | No | Textured hero backgrounds, decorative motifs |

---

## 10. Interview Questions

<details>
<summary>1. How does <code>object-fit: cover</code> differ from <code>object-fit: contain</code>?</summary>

- `object-fit: cover`: Scales the image so that both its width and height completely fill the element's bounding box while maintaining the image's original aspect ratio. If the image proportions do not match the box proportions, the image is cropped/clipped along the overflowing dimension.
- `object-fit: contain`: Scales the image so that its entire content fits within the bounding box while maintaining aspect ratio. If the proportions do not match, the image will not fill the box completely, leaving empty letterbox gutters.
</details>

<details>
<summary>2. How does the CSS <code>aspect-ratio</code> property eliminate Cumulative Layout Shift (CLS)?</summary>

When a responsive image is styled with `width: 100%; height: auto;`, the browser cannot know what height to allocate until the image headers or file bytes arrive over the network. The element starts with `height: 0px` and abruptly expands once loaded, shifting all surrounding page content downward.

`aspect-ratio: 16 / 9;` instructs the browser's layout engine to immediately calculate the height based on the available width ($H = W \times \frac{9}{16}$) during the very first layout pass. The spatial footprint is reserved instantly, resulting in zero layout shift.
</details>

<details>
<summary>3. Why can't you recolor an external SVG loaded via <code>&lt;img src="icon.svg"&gt;</code> using CSS <code>color</code> or <code>fill</code>, and how does the CSS Mask technique bypass this?</summary>

An SVG loaded via `<img>` is treated as an isolated, sandboxed external document. The parent page's CSSOM cannot penetrate the boundary of the `<img>` tag to alter internal SVG XML properties like `fill` or `stroke`.

**The CSS Mask Bypass**: Instead of using `<img>`, apply the SVG as a `mask-image` on an element with a CSS `background-color: currentColor;`. The SVG acts as an alpha stencil, carving out the background color into the shape of the icon, allowing the icon to be dynamically recolored via CSS variables and hover states.
</details>

<details>
<summary>4. What does <code>image-rendering: pixelated</code> do, and when should it be used?</summary>

By default, when an image is scaled up beyond its intrinsic dimensions, the browser applies bilinear or bicubic filtering to smooth out transitions between pixels, resulting in a blurry image. 

`image-rendering: pixelated` forces the browser's image decoder to use **nearest-neighbor interpolation**. When scaled up, pixels retain their crisp, sharp, square edges. It is essential for pixel art, retro video game graphics, and high-density QR codes that must remain camera-readable.
</details>

<details>
<summary>5. What elements are considered "replaced elements" in CSS?</summary>

A replaced element is an element whose appearance and intrinsic dimensions are defined by an external resource rather than CSS formatting. Examples include:
- `<img>`
- `<video>`
- `<canvas>`
- `<iframe>`
- `<embed>`
Properties like `object-fit` and `object-position` only operate on replaced elements.
</details>

<details>
<summary>6. How does <code>object-position</code> calculate percentage values?</summary>

Similar to `background-position`, percentage values in `object-position` align the specified percentage point of the image with the corresponding percentage point of the bounding box. 
`object-position: 50% 50%` aligns the exact center of the image with the exact center of the box. `object-position: 100% 0%` aligns the top-right corner of the image with the top-right corner of the box.
</details>

<details>
<summary>7. What is the difference between <code>object-fit: none</code> and <code>object-fit: scale-down</code>?</summary>

- `none`: Completely ignores the size of the container; the image is rendered at its intrinsic natural pixel dimensions, clipping or overflowing as necessary.
- `scale-down`: Evaluates both `none` and `contain` and selects whichever produces the smaller concrete object size, preventing small icons from being blown up unnecessarily.
</details>

<details>
<summary>8. Why is setting HTML <code>width</code> and <code>height</code> attributes still recommended even when using modern CSS?</summary>

Modern browser layout engines read the HTML `width="800" height="600"` attributes to calculate an internal default aspect ratio ($800/600$) before external CSS stylesheets finish downloading over the network. This ensures layout reservations begin during initial DOM parsing before external styles are applied.
</details>

---

## 11. Practice Exercises

### Exercise 1: Build a Responsive Zero-CLS Media Card
**Goal**: Style a media thumbnail that maintains a 16:9 ratio, covers its frame without distortion, and reserves layout space.

<details>
<summary>View Solution</summary>

```css
.card-media-wrapper {
  width: 100%;
  max-width: 400px;
  overflow: hidden;
  border-radius: 8px;
}

.card-media-img {
  width: 100%;
  aspect-ratio: 16 / 9;
  object-fit: cover;
  object-position: center;
  display: block;
}
```
</details>

### Exercise 2: Dynamic SVG Mask Theming
**Goal**: Create an icon class that loads an external SVG mask and recolors dynamically to brand blue on hover.

<details>
<summary>View Solution</summary>

```css
.brand-icon-mask {
  display: inline-block;
  width: 24px;
  height: 24px;
  background-color: #64748b;
  -webkit-mask: url("/assets/cloud.svg") no-repeat center / contain;
  mask: url("/assets/cloud.svg") no-repeat center / contain;
  transition: background-color 0.2s ease;
}

.brand-icon-mask:hover {
  background-color: #2563eb;
}
```
</details>

### Exercise 3: Crisp Scaled QR Code
**Goal**: Render a small 64x64 QR code graphic at 256x256 without antialiasing blur.

<details>
<summary>View Solution</summary>

```css
.crisp-qr {
  width: 256px;
  height: 256px;
  image-rendering: pixelated;
  image-rendering: crisp-edges;
  display: block;
}
```
</details>

### Exercise 4: Face-Focused Avatar Crop
**Goal**: Ensure an employee directory thumbnail always anchors its crop to the top 20% of the image.

<details>
<summary>View Solution</summary>

```css
.team-avatar {
  width: 72px;
  height: 72px;
  border-radius: 50%;
  object-fit: cover;
  object-position: 50% 20%;
  border: 2px solid #e2e8f0;
}
```
</details>

### Exercise 5: Video Background Letterbox Elimination
**Goal**: Force an HTML5 video element to fill an entire hero container without black letterbox bars.

<details>
<summary>View Solution</summary>

```css
.hero-video-bg {
  position: absolute;
  inset: 0;
  width: 100%;
  height: 100%;
  object-fit: cover;
}
```
</details>

### Exercise 6: Scale-Down Safety for Brand Logos
**Goal**: Style a partner logo container where large logos shrink to fit, but small logos are never scaled up and blurred.

<details>
<summary>View Solution</summary>

```css
.partner-logo-cell {
  width: 180px;
  height: 80px;
  display: flex;
  align-items: center;
  justify-content: center;
}

.partner-logo-cell img {
  max-width: 100%;
  max-height: 100%;
  object-fit: scale-down;
}
```
</details>

---

## 12. Summary / Cheat Sheet

| Property | Values / Example | Role |
| :--- | :--- | :--- |
| `object-fit` | `cover \| contain \| fill` | Controls framing of replaced elements (`<img>`, `<video>`) |
| `object-position` | `50% 20%` | Focal alignment point within the bounding box |
| `aspect-ratio` | `16 / 9` | Reserves spatial layout dimensions; eliminates CLS |
| `image-rendering` | `pixelated \| auto` | Enables sharp nearest-neighbor interpolation |
| CSS SVG Mask | `mask: url(icon.svg)` | Tints external SVG assets via CSS `background-color` |

---

## 13. Further Reading
- [W3C CSS Images Module Level 3 - object-fit](https://www.w3.org/TR/css-images-3/#the-object-fit)
- [W3C CSS Box Sizing Module Level 4 - aspect-ratio](https://www.w3.org/TR/css-sizing-4/#aspect-ratio)
- [MDN Web Docs: object-fit](https://developer.mozilla.org/en-US/docs/Web/CSS/object-fit)
- [web.dev: Optimize Cumulative Layout Shift with aspect-ratio](https://web.dev/learn/css/aspect-ratio/)
- [CSS-Tricks: Tinting SVG with CSS Masks](https://css-tricks.com/almanac/properties/m/mask-image/)

---

[Previous: 11-cursors-and-pointer-events.md](11-cursors-and-pointer-events.md) | [Module README](README.md) | [Next Module: 08-motion](file:///e:/Learning/engineering-notes/01-languages/css/08-motion/README.md)
