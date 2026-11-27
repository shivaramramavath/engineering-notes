# Box Shadows & Text Shadows

| Metadata | Details |
| :--- | :--- |
| **Topic** | Physics of Elevation, Multi-Layer Ambient & Key Light Shadows, Inset Shadows, Neon Glows, Text Shadows & Performance Shaders |
| **Spec Level** | CSS Backgrounds and Borders Module Level 3 / CSS Text Decoration Module Level 3 |
| **Baseline Status** | Widely Available (`box-shadow`, `text-shadow`, inset shadows Baseline 2015 across all major browsers) |
| **Browsers Tested** | Chrome 120+, Firefox 122+, Safari 17.2+, Edge 120+ |
| **Tested With** | Chromium Blink Rasterizer, GPU Compositor Shaders, Safari WebKit |
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

In digital user interface design, shadows simulate real-world physics: lighting, elevation ($Z$-axis spatial distance), and surface contact. Shadows communicate interaction affordances—elevating a card on hover signals that it is clickable, while sinking a button with an inset shadow signals that it has been activated.

However, naive shadow engineering (such as declaring a harsh single-blur drop: `box-shadow: 0 10px 20px black;`) produces dirty, unnatural visual designs. In physical reality, light comes from both an **ambient environment** (producing soft, tight contact shadows) and a **key directional light** (producing diffuse, projected penumbras).

Mastering modern shadow engineering involves:
- **Layered Multi-Shadow Physics**: Stacking 3 to 6 subtle, progressive shadow layers to emulate true optical dispersion.
- **Inset Shadow Sculpting**: Creating tactile embossed, debossed, and sunken form inputs.
- **Neon Volumetric Glows**: Crafting luminous cybernetic glows using zero-offset, multi-stage color blurs.
- **Text Shadows for Legibility**: Ensuring white typography remains readable over complex, dynamic image backgrounds without dark scrims.
- **GPU Performance Management**: Avoiding massive Gaussian blur radii ($> 60\text{px}$) that trigger heavy pixel convolution shaders during scrolling.

---

## 2. Core Concepts

### `box-shadow` Parameter Anatomy
The `box-shadow` property accepts a comma-separated list of shadow definitions:
```css
box-shadow: [inset]? <offset-x> <offset-y> <blur-radius>? <spread-radius>? <color>?;
```
1. `inset` (Optional): Draws the shadow inside the frame, giving a sunken or bevelled appearance.
2. `offset-x`: Horizontal offset (positive moves right, negative moves left).
3. `offset-y`: Vertical offset (positive moves down, negative moves up).
4. `blur-radius` (Optional, defaults to 0): Gaussian blur standard deviation. A higher value produces a softer, more dispersed shadow.
5. `spread-radius` (Optional, defaults to 0): Positive values expand the shadow perimeter in all directions; negative values contract the perimeter.
6. `color`: CSS color, defaults to `currentColor`.

### Physics: Ambient Occlusion vs Key Light
Natural shadow systems combine two distinct physical light sources:
- **Ambient Occlusion**: A tight, dark, low-blur shadow directly under the object representing trapped light between close surfaces.
- **Direct Key Light**: A softer, larger, vertically offset shadow representing directional light cast from above.

```css
/* Natural Physics Elevation */
.natural-elevation-card {
  box-shadow:
    0 1px 2px rgba(0, 0, 0, 0.06),   /* 1. Ambient contact shadow */
    0 8px 16px -4px rgba(0, 0, 0, 0.1); /* 2. Key directional shadow */
}
```

### `text-shadow` Parameters
Unlike `box-shadow`, `text-shadow` **does not support spread-radius or the `inset` keyword**:
```css
text-shadow: <offset-x> <offset-y> <blur-radius>? <color>?;
```
`text-shadow` conforms strictly to the alpha contours of glyph letterforms.

---

## 3. How It Works Internally

```
Gaussian Blur GPU Shaders and Blur Radius

[Object Perimeter] ---> [Rasterize Mask] ---> [GPU 2-Pass Gaussian Convolution]
                                                              |
                                           Radius = 5px:  Fast 5-tap kernel
                                           Radius = 80px: Heavy 80-tap kernel
                                                          (Frame drops on mobile!)
```

### Inset vs Outset Coordinate Inversion
- **Outset Shadows**: Cast outside the border-box. Clipped automatically from the inside of the element's border box so transparent element backgrounds do not show the shadow underneath!
- **Inset Shadows**: Cast inside the padding box. Clipped automatically from the outside of the border-box.

---

## 4. Syntax & Usage

### Baseline & Feature Support
- **`box-shadow`**: Baseline Widely Available.
- **`text-shadow`**: Baseline Widely Available.
- **Multiple Shadows**: Baseline Widely Available.

```css
/* Multi-Layer High-End Elevation */
.elevation-high {
  box-shadow:
    0 1px 1px rgba(0, 0, 0, 0.05),
    0 2px 2px rgba(0, 0, 0, 0.05),
    0 4px 4px rgba(0, 0, 0, 0.05),
    0 8px 8px rgba(0, 0, 0, 0.05),
    0 16px 16px rgba(0, 0, 0, 0.05);
}

/* Inset Debossed Input Well */
.sunken-input {
  box-shadow: inset 0 2px 4px rgba(0, 0, 0, 0.08);
}

/* Legibility Text Shadow */
.hero-title-shadow {
  text-shadow: 0 2px 8px rgba(0, 0, 0, 0.6);
}
```

---

## 5. Practical Examples

### Example 1: Smooth 6-Layer Ambient Elevation System
```css
:root {
  /* Design System Elevation Scale */
  --shadow-sm:
    0 1px 2px rgba(0, 0, 0, 0.05);

  --shadow-md:
    0 2px 4px -1px rgba(0, 0, 0, 0.06),
    0 4px 6px -1px rgba(0, 0, 0, 0.08);

  --shadow-lg:
    0 4px 6px -2px rgba(0, 0, 0, 0.05),
    0 10px 15px -3px rgba(0, 0, 0, 0.08),
    0 20px 25px -5px rgba(0, 0, 0, 0.04);

  --shadow-xl:
    0 0 1px rgba(0, 0, 0, 0.08),
    0 2px 4px rgba(0, 0, 0, 0.04),
    0 8px 16px rgba(0, 0, 0, 0.04),
    0 16px 24px rgba(0, 0, 0, 0.04),
    0 24px 32px rgba(0, 0, 0, 0.04);
}

.interactive-card {
  background-color: #ffffff;
  border-radius: 12px;
  padding: 1.5rem;
  box-shadow: var(--shadow-md);
  transition: transform 0.25s cubic-bezier(0.16, 1, 0.3, 1),
              box-shadow 0.25s cubic-bezier(0.16, 1, 0.3, 1);
  border: 1px solid rgba(0, 0, 0, 0.04);
}

.interactive-card:hover {
  transform: translateY(-4px);
  box-shadow: var(--shadow-xl);
}
```

### Example 2: Cyberpunk Multi-Color Neon Glow
```html
<button type="button" class="neon-btn">Execute Protocol</button>
```

```css
.neon-btn {
  font-family: ui-monospace, monospace;
  font-size: 1rem;
  font-weight: 700;
  text-transform: uppercase;
  letter-spacing: 0.1em;
  padding: 0.875rem 2rem;
  background-color: #020617;
  color: #38bdf8;
  border: 2px solid #38bdf8;
  border-radius: 6px;
  cursor: pointer;

  /* Multi-stage layered neon luminescence */
  box-shadow:
    0 0 5px rgba(56, 189, 248, 0.5),
    0 0 20px rgba(56, 189, 248, 0.3),
    0 0 40px rgba(56, 189, 248, 0.15),
    inset 0 0 10px rgba(56, 189, 248, 0.2);

  text-shadow:
    0 0 4px rgba(56, 189, 248, 0.8),
    0 0 12px rgba(56, 189, 248, 0.4);

  transition: box-shadow 0.2s ease, text-shadow 0.2s ease, background-color 0.2s ease;
}

.neon-btn:hover {
  background-color: rgba(56, 189, 248, 0.1);
  box-shadow:
    0 0 8px rgba(56, 189, 248, 0.8),
    0 0 30px rgba(56, 189, 248, 0.5),
    0 0 60px rgba(56, 189, 248, 0.3),
    inset 0 0 15px rgba(56, 189, 248, 0.4);
}

.neon-btn:focus-visible {
  outline: 2px solid #ffffff;
  outline-offset: 4px;
}
```

### Example 3: Inset Neumorphic / Skeuomorphic Switch
```css
.sunken-track {
  width: 60px;
  height: 32px;
  border-radius: 9999px;
  background-color: #e2e8f0;
  
  /* Inset shadows create a sunken physical trench */
  box-shadow:
    inset 0 2px 4px rgba(0, 0, 0, 0.15),
    inset 0 -1px 2px rgba(255, 255, 255, 0.8);
  position: relative;
  cursor: pointer;
}

.switch-thumb {
  position: absolute;
  top: 3px;
  left: 3px;
  width: 26px;
  height: 26px;
  border-radius: 50%;
  background-color: #ffffff;
  
  /* Outset tactile button shadow */
  box-shadow: 0 2px 4px rgba(0, 0, 0, 0.2);
}
```

### Example 4: Protective Text Shadows for Photo Overlays
```css
/* Ensures white text remains readable over unpredictable light and dark photos */
.photo-caption-title {
  color: #ffffff;
  font-size: 2.25rem;
  font-weight: 800;
  
  /* Layered protective scrim without full-container background overlay */
  text-shadow:
    0 1px 2px rgba(0, 0, 0, 0.8),
    0 2px 8px rgba(0, 0, 0, 0.5),
    0 4px 16px rgba(0, 0, 0, 0.3);
}
```

### Example 5: Embossed Editorial Lettering
```css
.embossed-heading {
  font-family: Georgia, serif;
  font-size: 3rem;
  font-weight: 700;
  color: #e2e8f0;
  background-color: #e2e8f0;
  
  /* Light from top-left, shadow to bottom-right */
  text-shadow:
    -1px -1px 1px #ffffff,
     1px  1px 1px rgba(0, 0, 0, 0.3);
}
```

---

## 6. Best Practices

1. **Use Multiple Shadow Layers for Realism**: Never use a single harsh shadow (`box-shadow: 0 10px 20px rgba(0,0,0,0.5)`). Combine a small, dark ambient shadow with a large, diffuse key shadow.
2. **Use Negative Spread Radii for Floating Cards**: Setting `spread-radius: -4px` pulls the shadow perimeter inward, preventing shadows from bleeding awkwardly out of the left and right card edges.
3. **Tint Shadows with Brand Hues Rather Than Pure Black**: Shading with a deep navy (`rgba(15, 23, 42, 0.1)`) instead of pure black (`rgba(0, 0, 0, 0.1)`) produces richer, more professional palettes.
4. **Limit Blur Radii on Scrollable Containers**: Avoid blur radii exceeding `40px` on elements that scroll or animate continuously. High blur radii require massive GPU shader passes.
5. **Always Pair Outset Shadows with Borders for WHCM**: Remember that Windows High Contrast Mode removes all `box-shadow` properties. Always include `border: 1px solid transparent;`.
6. **Keep `text-shadow` Subtle on Body Text**: Reserve pronounced text shadows for large display headlines. Heavy text shadows on small body text destroy letter readability.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Correction |
| :--- | :--- | :--- |
| **Harsh Single-Layer Black Shadow** | Looks dated, muddy, and visually jarring. | Layer 2–4 subtle, low-opacity shadows. |
| **Attempting `inset` on `text-shadow`** | Syntax error. `text-shadow` does not support `inset`. | Use `background-clip: text` gradient hacks for inner text shadows. |
| **Attempting `spread-radius` on `text-shadow`** | Syntax error. `text-shadow` only accepts offset-x, offset-y, blur, color. | `text-shadow` has no spread parameter. |
| **Animating `box-shadow` on 60fps scrolling** | Triggers expensive CPU/GPU repaints on every frame, causing scroll jank. | Animate `opacity` on a pseudo-element containing the shadow instead. |
| **Relying on `box-shadow` for card borders** | Vanishes completely in Windows High Contrast Mode. | Always add `border: 1px solid transparent;`. |
| **Excessive blur radii ($> 80\text{px}$)** | Severely slows down mobile rendering and battery life due to high convolution passes. | Keep maximum blur radius under `40px`. |

---

## 8. Performance, Accessibility & Browser Support

### GPU Performance Optimization via Pseudo-Elements
Animating `box-shadow` directly forces the browser engine to re-rasterize the shadow filter on every tick of the animation curve.
**High-Performance Pattern**: Pre-render the shadow on a `::after` pseudo-element and animate its `opacity` instead:
```css
.card {
  position: relative;
  /* Base shadow */
  box-shadow: 0 2px 4px rgba(0, 0, 0, 0.05);
}

.card::after {
  content: "";
  position: absolute;
  inset: 0;
  border-radius: inherit;
  box-shadow: 0 20px 25px rgba(0, 0, 0, 0.15); /* Elevated shadow */
  opacity: 0;
  transition: opacity 0.25s ease;
  pointer-events: none;
}

.card:hover::after {
  opacity: 1; /* Hardware-accelerated GPU opacity fade! Zero repaints! */
}
```

---

## 9. Comparison / When to Use What

| Feature | Supported Properties | Clips to | Performance Cost |
| :--- | :--- | :--- | :--- |
| **`box-shadow` (Outset)** | Offset X/Y, Blur, Spread, Color | `border-radius` (Outside element) | Moderate (Scale with blur) |
| **`box-shadow` (Inset)** | `inset`, Offset X/Y, Blur, Spread, Color | Padding box (Inside element) | Low |
| **`text-shadow`** | Offset X/Y, Blur, Color (No spread/inset) | Glyph outlines | Low to Moderate |
| **`filter: drop-shadow`**| Offset X/Y, Blur, Color | Exact alpha transparency (PNG, SVG) | High |

---

## 10. Interview Questions

<details>
<summary>1. What are the differences between <code>box-shadow</code> and <code>text-shadow</code> in CSS?</summary>

1. **Parameters**: `box-shadow` supports six values: `inset`, `offset-x`, `offset-y`, `blur-radius`, `spread-radius`, and `color`. `text-shadow` only supports four values: `offset-x`, `offset-y`, `blur-radius`, and `color` (it lacks both `inset` and `spread-radius`).
2. **Clipping Geometry**: `box-shadow` conforms to the element's rectangular or curved `border-radius` box. `text-shadow` conforms strictly to the individual vector contours of character glyphs.
3. **Internal Occlusion**: `box-shadow` automatically clips out the area underneath the element's border box so transparent element backgrounds do not show the shadow behind them.
</details>

<details>
<summary>2. Why does animating <code>box-shadow</code> during hover states often cause frame drops, and how can it be optimized?</summary>

When `box-shadow` is transitioned (`transition: box-shadow 0.3s`), the browser cannot simply composite the element on the GPU. On every single frame, the rasterizer must recalculate the Gaussian blur convolution kernel across every pixel of the shadow perimeter, resulting in high CPU/GPU paint cycles.

**Optimization**: Render the target hover shadow on a `::after` pseudo-element with `opacity: 0`. During hover, animate `opacity: 1`. Because `opacity` is handled directly on dedicated GPU compositor layers without triggering layout or paint, the animation runs at a locked 60fps/120fps.
</details>

<details>
<summary>3. What is the role of the <code>spread-radius</code> in <code>box-shadow</code>, and why is a negative spread useful?</summary>

The `spread-radius` expands or contracts the shadow's geometry before the Gaussian blur is applied:
- A **positive spread** ($+4\text{px}$) expands the shadow boundary in all four directions, making the shadow larger than the element.
- A **negative spread** ($-4\text{px}$) contracts the shadow boundary. This is particularly valuable for realistic physical elevation: by applying a vertical offset (`offset-y: 12px`) and negative spread (`spread: -4px`), the shadow stays directly underneath the card without bleeding awkwardly past the left and right sides.
</details>

<details>
<summary>4. What does the <code>inset</code> keyword do, and how are inset shadows clipped?</summary>

The `inset` keyword causes the shadow to be drawn *inside* the element's padding box, rather than outside the border-box. 
The shadow is clipped from the outside so it only renders within the interior of the element. Inset shadows create visual depth suggesting that the element is recessed, sunken, or carved into the page surface.
</details>

<details>
<summary>5. What happens to <code>box-shadow</code> in Windows High Contrast Mode (WHCM)?</summary>

Under `@media (forced-colors: active)`, the browser engine completely strips all `box-shadow` rules to eliminate visual distractions and guarantee high contrast. If a card or dialog relies entirely on `box-shadow` to separate itself from the canvas, the boundary disappears. Developers must always include `border: 1px solid transparent;` on elevated components.
</details>

<details>
<summary>6. How does the browser calculate Gaussian blur radius for shadows?</summary>

Per the CSS specification, the `blur-radius` represents the standard deviation ($\sigma$) of a Gaussian blur filter:
$$\sigma = \frac{\text{blur-radius}}{2}$$
The blur extends outward from the perimeter by approximately $2 \times \text{blur-radius}$, gradually tapering down in opacity according to the Gaussian bell curve.
</details>

<details>
<summary>7. How do you create a neon glow effect using <code>box-shadow</code>?</summary>

Set `offset-x: 0; offset-y: 0;` and layer multiple shadows with progressively larger blur radii and decreasing opacities using the desired neon color:
```css
box-shadow:
  0 0 5px #38bdf8,
  0 0 15px rgba(56, 189, 248, 0.6),
  0 0 30px rgba(56, 189, 248, 0.3);
```
</details>

<details>
<summary>8. Can <code>box-shadow</code> affect layout or cause scrollbars to appear?</summary>

No. `box-shadow` is drawn purely during the painting phase and does **not** alter the element's box model dimensions or document flow. However, if a large box shadow extends past the root `<html>` or `<body>` boundary, in some browser engines it can contribute to the visual overflow boundary, potentially causing horizontal or vertical scrollbars unless `overflow: hidden` is enforced on the container.
</details>

---

## 11. Practice Exercises

### Exercise 1: Build a Realistic 3-Layer Elevation System
**Goal**: Style a card component with layered contact, ambient, and directional shadows.

<details>
<summary>View Solution</summary>

```css
.realistic-card {
  background-color: #ffffff;
  border-radius: 8px;
  padding: 1.5rem;
  border: 1px solid rgba(0, 0, 0, 0.05); /* Base frame + WHCM safety */
  box-shadow:
    0 1px 2px rgba(0, 0, 0, 0.06),   /* Contact */
    0 4px 8px rgba(0, 0, 0, 0.04),   /* Ambient */
    0 12px 24px -4px rgba(0, 0, 0, 0.08); /* Key light */
}
```
</details>

### Exercise 2: High-Performance Shadow Hover Animation
**Goal**: Build a card hover elevation effect that animates `opacity` on `::after` instead of recalculating `box-shadow`.

<details>
<summary>View Solution</summary>

```css
.performant-card {
  position: relative;
  background-color: #ffffff;
  border-radius: 12px;
  padding: 1.5rem;
  box-shadow: 0 2px 4px rgba(0, 0, 0, 0.05);
  transition: transform 0.2s ease;
}

.performant-card::after {
  content: "";
  position: absolute;
  inset: 0;
  border-radius: inherit;
  box-shadow: 0 16px 32px rgba(0, 0, 0, 0.12);
  opacity: 0;
  transition: opacity 0.2s ease;
  pointer-events: none;
}

.performant-card:hover {
  transform: translateY(-2px);
}

.performant-card:hover::after {
  opacity: 1;
}
```
</details>

### Exercise 3: Recessed Form Input with Inset Shadow
**Goal**: Create a sunken input field that appears carved into the page surface.

<details>
<summary>View Solution</summary>

```css
.sunken-input {
  width: 100%;
  padding: 0.625rem 1rem;
  border: 1px solid #cbd5e1;
  border-radius: 6px;
  background-color: #f8fafc;
  box-shadow: inset 0 2px 4px rgba(0, 0, 0, 0.06);
  font-size: 0.875rem;
  color: #0f172a;
}

.sunken-input:focus-visible {
  outline: 2px solid #2563eb;
  outline-offset: 2px;
  background-color: #ffffff;
  box-shadow: none;
}
```
</details>

### Exercise 4: Protective Hero Title Text Shadow
**Goal**: Add multi-layer text shadows to a hero title to guarantee readability over light, dark, and noisy images.

<details>
<summary>View Solution</summary>

```css
.hero-headline {
  font-family: system-ui, sans-serif;
  font-size: 3rem;
  font-weight: 800;
  color: #ffffff;
  text-shadow:
    0 1px 2px rgba(0, 0, 0, 0.8),
    0 4px 12px rgba(0, 0, 0, 0.6);
}
```
</details>

### Exercise 5: Glowing Neon Pill Tag
**Goal**: Create an emerald status tag with an internal and external luminescence glow.

<details>
<summary>View Solution</summary>

```css
.neon-tag {
  display: inline-block;
  padding: 0.25rem 0.75rem;
  border-radius: 9999px;
  font-size: 0.75rem;
  font-weight: 700;
  color: #10b981;
  background-color: rgba(16, 185, 129, 0.1);
  border: 1px solid #10b981;
  box-shadow:
    0 0 10px rgba(16, 185, 129, 0.4),
    inset 0 0 5px rgba(16, 185, 129, 0.2);
}
```
</details>

### Exercise 6: Subtle Under-Lip Tab Shadow
**Goal**: Create an active navigation tab that casts a delicate bottom shadow over the content panel below.

<details>
<summary>View Solution</summary>

```css
.active-tab {
  position: relative;
  z-index: 10;
  background-color: #ffffff;
  border-top-left-radius: 8px;
  border-top-right-radius: 8px;
  padding: 0.75rem 1.5rem;
  box-shadow: 0 -2px 6px rgba(0, 0, 0, 0.05);
}
```
</details>

---

## 12. Summary / Cheat Sheet

| Feature | Syntax Pattern | Key Behavior |
| :--- | :--- | :--- |
| **`box-shadow` Outset** | `0 4px 8px -2px rgba(0,0,0,0.1)` | Casts external shadow; negative spread tightens edges |
| **`box-shadow` Inset** | `inset 0 2px 4px rgba(0,0,0,0.08)` | Casts internal shadow inside padding box |
| **`text-shadow`** | `0 2px 4px rgba(0,0,0,0.5)` | Follows glyph contours (no spread, no inset) |
| **Multi-Shadow Physics**| Ambient (0 1px 2px) + Key (0 8px 16px)| Eliminates muddy, unnatural single-blur drops |
| **GPU Optimization** | Animate `opacity` on `::after` | Avoids expensive CPU/GPU Gaussian blur recalculations |

---

## 13. Further Reading
- [W3C CSS Backgrounds and Borders Module Level 3 - Box Shadow](https://www.w3.org/TR/css-backgrounds-3/#box-shadow)
- [MDN Web Docs: box-shadow](https://developer.mozilla.org/en-US/docs/Web/CSS/box-shadow)
- [MDN Web Docs: text-shadow](https://developer.mozilla.org/en-US/docs/Web/CSS/text-shadow)
- [Josh W. Comeau: Designing Beautiful Shadows in CSS](https://www.joshwcomeau.com/css/designing-shadows/)
- [Tobias Ahlin: How to animate box-shadow with silky 60fps](https://tobiasahlin.com/blog/how-to-animate-box-shadow-with-silky-smooth-performance/)

---

[Previous: 02-borders-radius-and-outlines.md](02-borders-radius-and-outlines.md) | [Module README](README.md) | [Next: 04-filters-and-backdrop-filter.md](04-filters-and-backdrop-filter.md)
