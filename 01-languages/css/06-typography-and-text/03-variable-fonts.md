# Variable Fonts & OpenType Axes

| Metadata | Details |
| :--- | :--- |
| **Topic** | OpenType Font Variations, Registered Axes (`wght`, `wdth`, `slnt`, `ital`, `opsz`), Custom Parametric Axes, `font-variation-settings` |
| **Spec Level** | CSS Fonts Module Level 4 / OpenType 1.8 Specification |
| **Baseline Status** | Widely Available (Variable Fonts Baseline 2020 across all major desktop and mobile browsers) |
| **Browsers Tested** | Chrome 120+, Firefox 122+, Safari 17.2+, Edge 120+ |
| **Tested With** | Playwright, W3C CSS Validator, Variable Fonts Browser DevTools |
| **Module** | [06-typography-and-text](README.md) |

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

Historically, web typography was constrained by network payload economics. Each combination of weight (e.g., Light, Regular, SemiBold, Bold, Black) and style (Upright, Italic) required a separate font binary (12 static font files at 25 KB each = 300 KB transferred over the network). To preserve performance, developers typically restricted designs to just two weights (400 and 700).

Introduced in OpenType 1.8 and standardized in CSS Fonts Level 4, **Variable Fonts** (OpenType Font Variations) revolutionize digital typography. A single compact font file contains the master glyph outlines along with mathematical delta vectors defining continuous design spaces across multiple axes:
- **Infinite Flexibility**: Instead of choosing between 400 and 700, designers can select any value: 432, 580, or 615.
- **Massive Performance Savings**: Replacing 8 to 12 separate static font files with a single variable file typically reduces overall font byte payload by 50% to 70% while consolidating network requests into one HTTP stream.
- **Micro-Interactions and Animation**: Font properties can be smoothly transitioned and animated in CSS without awkward stepped jumps or text jitter.

---

## 2. Core Concepts

### Registered Standard Axes (Lowercase)
The OpenType specification defines five standardized axes, designated by four-character lowercase tags. Modern CSS provides high-level properties mapped directly to these axes:

1. **`wght` (Weight)**: Controls stroke thickness.
   - Range: Typically `1` to `1000`.
   - CSS Property: `font-weight: 550;`.
2. **`wdth` (Width)**: Controls glyph horizontal proportions (condensation or expansion).
   - Range: Typically `50%` to `200%` (where `100%` is normal).
   - CSS Property: `font-stretch: 85%;` or `font-stretch: condensed;`.
3. **`slnt` (Slant)**: Controls mechanical angle of upright glyphs.
   - Range: Typically `-90deg` to `+90deg` (usually `0deg` to `-14deg`).
   - CSS Property: `font-style: oblique 12deg;`.
4. **`ital` (Italic)**: Binary or continuous transition from Roman to true cursive glyphs.
   - Range: `0` (off) to `1` (full italic).
   - CSS Property: `font-style: italic;`.
5. **`opsz` (Optical Size)**: Adjusts glyph stroke contrast, serifs, and proportions to optimize legibility at different physical display sizes.
   - Range: Typically `6` to `72` (correlating to point or pixel sizes).
   - CSS Property: `font-optical-sizing: auto;` or `font-optical-sizing: none;`.

### Custom Axes (Uppercase Tags)
Type designers can introduce proprietary parametric axes using four-character uppercase tags (or mixed case):
- `'GRAD'` (Grade): Modifies stroke weight slightly without altering character advance widths (preventing layout reflow on dark mode or hover).
- `'CASL'` (Casual): Transitions between formal and playful handwriting (e.g., in *Recursive*).
- `'CRSV'` (Cursive): Adjusts stroke continuity.
- `'XOPQ'` / `'YOPQ'`: Controls thick and thin stem parameters independently.
- CSS Property: Controlled via the low-level `font-variation-settings` property.

### High-Level Properties vs `font-variation-settings`
The W3C specification strictly dictates: **Always prefer high-level CSS properties** (`font-weight`, `font-stretch`, `font-style`, `font-optical-sizing`) over `font-variation-settings`.
- High-level properties cascade cleanly and independently.
- `font-variation-settings` behaves like `transform`: setting one axis overrides and resets all other axes previously declared on that element!

---

## 3. How It Works Internally

```
+--------------------------------------------------------------------------+
| OpenType 1.8 Variation Mechanics                                         |
|                                                                          |
|  [Default Base Glyph Outline]                                            |
|  (e.g., Weight 400, Width 100%)                                          |
|         |                                                                |
|         +---> [Delta Table (gvar/cvar)] ---> +X / +Y Coordinate Offsets  |
|         |                                                                |
|         v                                                                |
|  User requests `font-weight: 620; font-stretch: 85%;`                    |
|  Font engine computes linear interpolation:                              |
|  Point(x, y) = Base(x, y) + (Delta_wght * 0.44) + (Delta_wdth * -0.15)  |
|         |                                                                |
|         v                                                                |
|  [Rasterized Dynamic Glyph Contour Rendered to Screen]                   |
+--------------------------------------------------------------------------+
```

### The Grade Axis (`GRAD`) Advantage
When animating `font-weight: 400` to `font-weight: 700` on a navigation link during hover, the text expands horizontally (advance widths increase), causing surrounding links to push outward and jitter.

The Grade (`'GRAD'`) axis thickens glyph stems by shifting internal counter contours without changing the external advance width of characters. Text becomes bolder visually while occupying the exact same pixel footprint, achieving zero layout shift during hover or theme toggling.

---

## 4. Syntax & Usage

### Baseline & Feature Support
- **Variable Font Rendering**: Baseline Widely Available (Chrome 62+, Safari 11+, Firefox 62+, Edge 79+).
- **High-Level Properties (`font-weight: 1-1000`, `font-stretch: %`)**: Baseline Widely Available.
- **`@font-face` range syntax (`font-weight: 100 900;`)**: Baseline Widely Available.

```css
/* 1. Registering Variable Font with Range Support */
@font-face {
  font-family: "InterVariable";
  font-style: normal;
  font-weight: 100 900;
  font-stretch: 75% 125%;
  font-display: swap;
  src: url("/fonts/inter-variable.woff2") format("woff2-variations");
}

/* 2. Applying High-Level Properties */
.dynamic-card h3 {
  font-family: "InterVariable", sans-serif;
  font-size: 1.5rem;
  font-weight: 640;      /* Maps to 'wght' 640 */
  font-stretch: 95%;     /* Maps to 'wdth' 95 */
  font-optical-sizing: auto; /* Enables 'opsz' auto-calculation */
}

/* 3. Custom Axis Syntax */
.custom-branded-text {
  font-family: "RecursiveVariable", sans-serif;
  font-weight: 500;
  /* Low-level access for custom axes ONLY */
  font-variation-settings: "CASL" 0.5, "CRSV" 1;
}
```

---

## 5. Practical Examples

### Example 1: Full-Spectrum Typography System (Single File)
```css
@font-face {
  font-family: "GeistVar";
  font-style: normal;
  font-weight: 100 900;
  font-display: swap;
  src: url("/fonts/geist-variable.woff2") format("woff2");
}

:root {
  --font-geist: "GeistVar", system-ui, sans-serif;
}

body {
  font-family: var(--font-geist);
  font-weight: 380; /* Refined body weight */
  font-size: 1rem;
  line-height: 1.6;
}

h1 {
  font-family: var(--font-geist);
  font-weight: 840; /* Ultra-punchy display weight */
  letter-spacing: -0.03em;
  line-height: 1.1;
}

strong, b {
  font-weight: 680; /* Elegant emphasis weight */
}
```

### Example 2: Grade Axis for Zero-Reflow Dark Mode
```css
/* Prevent dark-mode text blooming from triggering layout reflows */
:root {
  --text-grade: 0;
}

[data-theme="dark"] {
  /* On dark backgrounds, white text visually blooms, appearing heavier.
     A slight negative grade keeps perceived weight identical. */
  --text-grade: -20;
}

.grade-aware-text {
  font-family: "Roboto Flex", sans-serif;
  font-weight: 400;
  font-variation-settings: "GRAD" var(--text-grade);
}
```

### Example 3: Continuous Smooth Hover Animation
```css
.interactive-nav-item {
  font-family: "InterVariable", sans-serif;
  font-size: 1.125rem;
  font-weight: 400;
  font-stretch: 100%;
  color: #1e293b;
  text-decoration: none;
  display: inline-block;
  padding: 0.5rem 1rem;
  transition: font-weight 0.25s cubic-bezier(0.16, 1, 0.3, 1),
              font-stretch 0.25s cubic-bezier(0.16, 1, 0.3, 1),
              color 0.25s ease;
}

.interactive-nav-item:hover {
  font-weight: 700;
  font-stretch: 110%;
  color: #2563eb;
}

.interactive-nav-item:focus-visible {
  outline: 2px solid #2563eb;
  outline-offset: 2px;
}
```

### Example 4: Responsive Fluid Sizing & Weight Modulation
```css
/* Thicken stroke weight slightly on small handheld screens to maintain legibility */
.fluid-reading-paragraph {
  font-family: "InterVariable", sans-serif;
  font-size: clamp(0.9375rem, 0.85rem + 0.4vw, 1.125rem);
  font-weight: clamp(380, 420 - 0.05vw, 400);
  line-height: 1.65;
}
```

### Example 5: Optical Sizing for Headlines vs Footnotes
```css
@font-face {
  font-family: "SourceSerifVar";
  src: url("/fonts/source-serif-variable.woff2") format("woff2");
  font-weight: 200 900;
}

/* Headlines: High contrast, thin delicate serifs, tighter tracking */
.magazine-headline {
  font-family: "SourceSerifVar", serif;
  font-size: 4rem;
  font-weight: 700;
  font-optical-sizing: auto; /* Browser automatically requests 'opsz' 64 */
  letter-spacing: -0.02em;
}

/* Captions: Sturdier serifs, wider counters, enhanced open shapes */
.magazine-caption {
  font-family: "SourceSerifVar", serif;
  font-size: 0.75rem;
  font-weight: 450;
  font-optical-sizing: auto; /* Browser automatically requests 'opsz' 12 */
  letter-spacing: 0.02em;
}
```

---

## 6. Best Practices

1. **Use High-Level Properties Whenever Possible**: Use `font-weight`, `font-stretch`, and `font-style` instead of `font-variation-settings`. High-level properties cascade modularly without clobbering each other.
2. **Specify Weight/Stretch Ranges in `@font-face`**: Declare `font-weight: 100 900;` and `font-stretch: 75% 125%;`. If you omit the range or set a single number, the browser may treat the file as a static font.
3. **Keep `font-optical-sizing: auto` Active**: Optical sizing ensures small text remains legible while large display text gains refined, delicate details.
4. **Use `'GRAD'` for Hover States to Prevent Shifting**: If animating bold text on hover, use the Grade axis if available in your font to eliminate advance width jitter.
5. **Always Supply Modern WOFF2**: Ensure variable fonts are compressed with WOFF2. Do not ship uncompressed TTF variable fonts (which can easily exceed 400 KB).
6. **Respect Reduced Motion on Variable Font Transitions**: Disable or shorten duration for users who prefer reduced motion.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Correction |
| :--- | :--- | :--- |
| **Overriding `font-variation-settings`** | Like `transform`, declaring `font-variation-settings: "wght" 700;` on a child destroys parent `"wdth" 90;`. | Use high-level `font-weight: 700;` or use CSS custom properties for axes. |
| **Omitting Range in `@font-face`** | Writing `font-weight: 400;` locks the variable font to 400; other weights trigger faux bold. | Write `font-weight: 100 900;` (space-separated range). |
| **Using `font-stretch: 80%` on static fonts** | Static fonts without width axes simply ignore percentage stretch values or render distorted. | Verify font has a `wdth` axis before specifying percentage widths. |
| **Case Sensitivity Errors in Tags** | Registered axes must be lowercase (`wght`), custom axes must be uppercase (`GRAD`). | Use exact 4-character case tags: `"wght" 600`, `"GRAD" 50`. |
| **Forgetting `format("woff2-variations")` or `format("woff2")`** | Older parsers may not detect variable font capabilities. | Declare `src: url(...) format("woff2");` or `format("woff2-variations")`. |
| **Animating `font-weight` on static fonts** | Triggers jarring discrete jumps (400 -> 700) rather than smooth interpolations. | Ensure the font is a true variable font with continuous `wght` axis. |

---

## 8. Performance, Accessibility & Browser Support

### Network Payload Economy
- **Static Multi-File Approach**: 6 weights $\times$ 25 KB = 150 KB across 6 separate HTTP requests.
- **Variable Font Approach**: 1 single file $\times$ 65 KB = 65 KB across 1 HTTP request.
- **Savings**: **~57% bandwidth reduction** and elimination of multiple round-trip requests.

### Reduced Motion Guard
```css
@media (prefers-reduced-motion: reduce) {
  .interactive-nav-item {
    transition: none;
  }
}
```

---

## 9. Comparison / When to Use What

| Feature | Static Web Fonts | Variable Web Fonts |
| :--- | :--- | :--- |
| **File Count** | 1 file per weight/style (e.g., 6–12 files) | **1 single file** for entire design spectrum |
| **Weight Flexibility** | Stepped integers (400, 700) | **Continuous float (1 to 1000)** |
| **Animation Fidelity** | Discrete instant jumps | **Silky 60fps smooth interpolation** |
| **Width & Slant Control** | Requires separate condensed/oblique binaries | Built-in continuous `wdth` and `slnt` axes |
| **Legacy Browser Fallback** | Supported since IE9 | Requires fallback `@font-face` for pre-2018 browsers |

---

## 10. Interview Questions

<details>
<summary>1. How does a variable font store multiple weights and widths in a single file without bloating the file size?</summary>

Rather than storing independent glyph outlines for every single weight and width combination, a variable font stores a **single master outline** (typically the Regular or Light style) along with a mathematical table of **deltas** (the `gvar` table for TrueType or `CFF2` for PostScript). 

These deltas specify the $X$ and $Y$ directional vector offsets for each glyph coordinate control point at the extreme limits of each axis. When an application requests an intermediate weight (such as 550), the font rendering engine computes a simple linear interpolation of those delta vectors, generating the exact intermediate contour in real time on the GPU/CPU.
</details>

<details>
<summary>2. What are the five standard registered OpenType variation axes, and what are their four-character tags?</summary>

The five standardized registered axes are:
1. Weight: `'wght'` (CSS: `font-weight`)
2. Width: `'wdth'` (CSS: `font-stretch`)
3. Slant: `'slnt'` (CSS: `font-style: oblique <angle>`)
4. Italic: `'ital'` (CSS: `font-style: italic`)
5. Optical Size: `'opsz'` (CSS: `font-optical-sizing`)

Standard registered axes are strictly identified by 4-letter **lowercase** tags. Custom axes created by type designers use **uppercase** or mixed-case tags (e.g., `'GRAD'`, `'CASL'`).
</details>

<details>
<summary>3. Why is using <code>font-variation-settings</code> considered an anti-pattern when high-level CSS properties exist?</summary>

`font-variation-settings` operates as a primitive, all-or-nothing property (identical to `transform`). If a parent element declares:
```css
.card { font-variation-settings: "wght" 500, "wdth" 90; }
```
and a child component declares:
```css
.card h2 { font-variation-settings: "wght" 800; }
```
the child completely overwrites the `"wdth" 90` declaration, resetting width to default 100. 

High-level properties (`font-weight`, `font-stretch`, `font-style`) cascade independently and cleanly:
```css
.card { font-weight: 500; font-stretch: 90%; }
.card h2 { font-weight: 800; } /* font-stretch: 90% is preserved! */
```
</details>

<details>
<summary>4. What is the Grade (<code>'GRAD'</code>) axis, and why is it uniquely valuable for UI states and dark mode?</summary>

The Grade axis adjusts the stroke weight of glyphs without changing their horizontal advance widths or bounding boxes. In standard typography, increasing `font-weight` from 400 to 700 widens characters, which shifts text horizontally and causes button widths or table columns to reflow. 

With `'GRAD'`, text becomes visually heavier while its character bounding boxes remain static to the sub-pixel, preventing layout reflow on `:hover`, `:active`, and dark mode transitions.
</details>

<details>
<summary>5. How must <code>@font-face</code> be written to properly declare a variable font's weight range?</summary>

In static font declarations, `font-weight` accepts a single value (e.g., `font-weight: 700;`). For variable fonts, `font-weight` must be declared as a **space-separated range**:
```css
@font-face {
  font-family: "MyVarFont";
  font-weight: 100 900;
  src: url("/font.woff2") format("woff2");
}
```
If you omit the range or declare a single number, the browser engine treats the font as a static font and may synthesize faux bold for weights outside that number.
</details>

<details>
<summary>6. How does <code>font-optical-sizing: auto</code> work internally in modern browsers?</summary>

When `font-optical-sizing: auto` is enabled (which is the default in modern user agents), the browser automatically samples the element's computed `font-size`. It then maps that pixel value directly to the variable font's `'opsz'` axis. 

At large sizes (e.g., 48px), the font engine renders high contrast and delicate serifs. At small sizes (e.g., 10px), the font engine automatically thickens thin strokes, widens counters, and opens letterforms for optimal readability.
</details>

<details>
<summary>7. Can you animate variable font axes with CSS keyframes and transitions?</summary>

Yes. Because variable font axes represent continuous numerical scales (unlike static font weights which are discrete files), CSS transitions and `@keyframes` can interpolate between values smoothly at 60fps/120fps without stepped flickering.
```css
.pulse-text {
  transition: font-weight 0.3s ease;
}
.pulse-text:hover {
  font-weight: 850;
}
```
</details>

<details>
<summary>8. How do you safely manage multiple custom axes with CSS Custom Properties to avoid clobbering?</summary>

Define independent custom properties that feed into a single `font-variation-settings` rule on the root or base element:
```css
:root {
  --font-casl: 0;
  --font-crsv: 0;
  --font-grad: 0;
}

.custom-font-element {
  font-variation-settings:
    "CASL" var(--font-casl),
    "CRSV" var(--font-crsv),
    "GRAD" var(--font-grad);
}

.playful-variant {
  --font-casl: 1; /* Only updates CASL; does not clobber CRSV or GRAD */
}
```
</details>

---

## 11. Practice Exercises

### Exercise 1: Register and Apply a Multi-Axis Variable Font
**Goal**: Register a variable font supporting weights 200–800 and widths 80%–120%, then apply a 620-weight, 90%-condensed style to an article heading.

<details>
<summary>View Solution</summary>

```css
@font-face {
  font-family: "FrauncesVar";
  font-weight: 200 800;
  font-stretch: 80% 120%;
  font-display: swap;
  src: url("/fonts/fraunces-variable.woff2") format("woff2");
}

.article-headline {
  font-family: "FrauncesVar", serif;
  font-size: 2.5rem;
  font-weight: 620;
  font-stretch: 90%;
  line-height: 1.15;
}
```
</details>

### Exercise 2: Build Reflow-Free Button Hover via Grade Axis
**Goal**: Thicken a button label on hover using the `'GRAD'` axis so the button dimensions do not shift by a single pixel.

<details>
<summary>View Solution</summary>

```css
.grade-button {
  font-family: "Roboto Flex", sans-serif;
  font-size: 1rem;
  font-weight: 500;
  font-variation-settings: "GRAD" 0;
  padding: 0.75rem 1.5rem;
  background-color: #0f172a;
  color: #ffffff;
  border: none;
  border-radius: 6px;
  cursor: pointer;
  transition: font-variation-settings 0.2s ease, background-color 0.2s ease;
}

.grade-button:hover {
  font-variation-settings: "GRAD" 100;
  background-color: #1e293b;
}

.grade-button:focus-visible {
  outline: 2px solid #2563eb;
  outline-offset: 2px;
}
```
</details>

### Exercise 3: Dynamic Slant Animation
**Goal**: Create an oblique hover effect that smoothly tilts text by 14 degrees on hover using `font-style: oblique`.

<details>
<summary>View Solution</summary>

```css
.dynamic-slant-link {
  font-family: "InterVariable", sans-serif;
  font-size: 1.25rem;
  font-style: normal;
  color: #2563eb;
  text-decoration: none;
  display: inline-block;
  transition: font-style 0.3s ease;
}

.dynamic-slant-link:hover {
  font-style: oblique -14deg;
}

.dynamic-slant-link:focus-visible {
  outline: 2px solid #2563eb;
  outline-offset: 2px;
}
```
</details>

### Exercise 4: Modular CSS Custom Property Management for Variable Axes
**Goal**: Architect a CSS architecture that allows setting `'CASL'` and `'CRSV'` without overwriting each other.

<details>
<summary>View Solution</summary>

```css
.recursive-text {
  font-family: "Recursive", sans-serif;
  --casl: 0;
  --crsv: 0;
  --slnt: 0;
  font-variation-settings:
    "CASL" var(--casl),
    "CRSV" var(--crsv),
    "slnt" var(--slnt);
}

.recursive-text.casual {
  --casl: 1;
}

.recursive-text.cursive {
  --crsv: 1;
}
```
</details>

### Exercise 5: Continuous Pulse Keyframe Animation
**Goal**: Write a `@keyframes` animation that pulses a display font's weight smoothly between 300 and 800.

<details>
<summary>View Solution</summary>

```css
@keyframes weightPulse {
  0%, 100% {
    font-weight: 300;
    font-stretch: 90%;
  }
  50% {
    font-weight: 800;
    font-stretch: 110%;
  }
}

.breathing-header {
  font-family: "InterVariable", sans-serif;
  font-size: 3rem;
  animation: weightPulse 4s ease-in-out infinite;
}

@media (prefers-reduced-motion: reduce) {
  .breathing-header {
    animation: none;
    font-weight: 700;
  }
}
```
</details>

### Exercise 6: Optical Sizing Verification Matrix
**Goal**: Explicitly disable optical sizing on a decorative headline where high-contrast serifs are undesired at large scale.

<details>
<summary>View Solution</summary>

```css
.chunky-retro-display {
  font-family: "FrauncesVar", serif;
  font-size: 4rem;
  font-weight: 900;
  /* Disable automatic optical size adaptation to maintain heavy slab look */
  font-optical-sizing: none;
  font-variation-settings: "opsz" 9;
}
```
</details>

---

## 12. Summary / Cheat Sheet

| Axis Tag | CSS High-Level Property | Value Example | Description |
| :--- | :--- | :--- | :--- |
| `wght` | `font-weight` | `font-weight: 560;` | Continuous stroke weight (1–1000) |
| `wdth` | `font-stretch` | `font-stretch: 85%;` | Glyph width condensation/expansion |
| `slnt` | `font-style` | `font-style: oblique 12deg;` | Mechanical slant angle |
| `ital` | `font-style` | `font-style: italic;` | True cursive italic toggle |
| `opsz` | `font-optical-sizing` | `font-optical-sizing: auto;` | Automatic optimization for font size |
| Custom | `font-variation-settings` | `"GRAD" 50, "CASL" 0.8` | Low-level proprietary parametric axes |

---

## 13. Further Reading
- [W3C CSS Fonts Module Level 4 - Variable Fonts](https://www.w3.org/TR/css-fonts-4/#font-variation-props)
- [MDN Web Docs: Variable Fonts Guide](https://developer.mozilla.org/en-US/docs/Web/CSS/CSS_Fonts/Variable_Fonts_Guide)
- [V-Fonts: A Showcase of Beautiful Variable Fonts](https://v-fonts.com/)
- [Google Fonts: Variable Fonts Knowledge Guide](https://fonts.google.com/knowledge)
- [Microsoft OpenType 1.8 Specification](https://learn.microsoft.com/en-us/typography/opentype/spec/otff#font-variations)

---

[Previous: 02-web-fonts-and-font-display.md](02-web-fonts-and-font-display.md) | [Module README](README.md) | [Next: 04-text-properties-and-spacing.md](04-text-properties-and-spacing.md)
