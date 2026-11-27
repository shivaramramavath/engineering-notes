# Colours & Color Spaces

| Metadata | Details |
|:---|:---|
| **Level** | Intermediate |
| **Prerequisites** | `01-fundamentals/01-syntax-rules-and-declarations.md`, `01-fundamentals/09-units.md` |
| **Estimated Reading Time** | 30 minutes |
| **Status** | Current |
| **Baseline Status** | Widely Available (OKLCH, color-mix(), P3: Baseline 2023; light-dark(): Baseline 2024) |
| **Browsers Tested** | Chrome 120+, Firefox 121+, Safari 17.2+, Edge 120+ |
| **Tested With** | W3C CSS Color Module Level 4 & 5, Playwright css_runner.js, stylelint |

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

### What are Modern CSS Color Spaces?
For over 25 years, web colors were confined to the **sRGB color space** via hex codes (`#0284c7`), `rgb()`, and `hsl()`. While simple, sRGB is mathematically flawed:
- **sRGB is Gamut-Constrained**: Standard sRGB can display only about **$35\%$ of the visible colors** perceivable by the human eye. Modern smartphone displays, Apple Retina screens, and OLED monitors can display the much wider **Display-P3 gamut** ($50\%$ more visible colors).
- **HSL is Perceptually Non-Uniform**: In HSL, both yellow (`hsl(60, 100%, 50%)`) and blue (`hsl(240, 100%, 50%)`) claim a "lightness" of $50\%$. Yet yellow is blindingly bright while blue is intensely dark. This makes automated color generation in HSL mathematically unreliable for accessibility contrast.

### Why OKLCH is the New Industry Standard
Standardized in **CSS Color Module Level 4 & 5**, **OKLCH** represents colors in a perceptually uniform color space designed around human biological vision:
- **$L$ (Perceived Lightness)**: $0\%$ (pitch black) to $100\%$ (pure white). $70\%$ lightness looks identically bright whether hue is blue, green, yellow, or pink!
- **$C$ (Chroma / Saturation)**: $0$ (neutral monochrome gray) up to $\approx 0.37$ (maximum vivid saturation supported by your monitor).
- **$H$ (Hue Angle)**: $0^\circ$ to $360^\circ$ on the perceptual color wheel.

---

## 2. Core Concepts

### 1. The Perceptually Uniform Space: OKLCH
```css
/* Lightness: 0.6 (60%) | Chroma: 0.22 | Hue: 250deg (Blue) */
--primary-color: oklch(0.6 0.22 250);
```
- Because lightness is physically uniform, changing the hue angle ($H$) rotates through colors **while keeping the exact same contrast ratio against black or white**!

### 2. Wide Gamut Display-P3
Allows web developers to tap into rich, vibrant greens, neons, and deep reds previously impossible in standard sRGB:
```css
/* Ultra-vibrant neon green in Display-P3 */
.neon-accent {
  color: color(display-p3 0 1 0.2);
}
```

### 3. Dynamic Mixing: `color-mix()`
Enables runtime color blending directly in CSS without requiring preprocessors (like Sass `darken()` or `mix()`):
```css
/* Mix 80% primary brand color with 20% black in the OKLCH space */
.btn:hover {
  background-color: color-mix(in oklch, var(--brand-primary) 80%, black);
}
```

### 4. Relative Color Syntax (RCS)
Derives a new color from an existing color by deconstructing its channels, modifying them with math, and emitting a new color:
```css
/* Create a 50% translucent tint of ANY brand color */
.surface-overlay {
  background-color: oklch(from var(--brand-primary) l c h / 0.5);
}

/* Create the exact complementary color (rotate hue by 180 degrees) */
.complementary-tag {
  background-color: oklch(from var(--brand-primary) l c calc(h + 180));
}
```

### 5. Native Scheme Switching: `light-dark()`
Eliminates media query boilerplate by declaring both light and dark mode colors in a single property:
```css
:root {
  color-scheme: light dark;
}

.card {
  /* light-dark(lightValue, darkValue) */
  background-color: light-dark(#ffffff, #0f172a);
  color: light-dark(#0f172a, #f8fafc);
}
```

---

## 3. How It Works Internally

### Human Vision & The Color Gamut Comparison
Human retinas process color using three types of cone cells (L, M, S). The **CIE 1931 Chromaticity Diagram** maps the complete horseshoe of human vision:

```
+--------------------------------------------------------------------+
| THE COLOR GAMUT HORSESHOE (Human Vision)                          |
|                                                                    |
|              ^  Green                                              |
|             / \                                                    |
|            /   \       [Display-P3: ~50% of Human Spectrum]        |
|           /  *--\---+                                              |
|          /  / *--\--+--+   [sRGB: ~35% of Human Spectrum]          |
|         /  / /    \  \  \                                          |
|        /  / /  D65 \  \  \                                         |
|       /  +-+--------+--+  \                                        |
|      /    /          \     \                                       |
|     /----+------------+-----\                                      |
|    < Blue              Red   >                                     |
+--------------------------------------------------------------------+
```

1. **sRGB Clipping**: When you declare a vivid color in sRGB, vibrant wavelengths are clipped to the small inner triangle.
2. **Display-P3 Expansion**: P3 encompasses a much larger area of vibrant greens, fiery oranges, and deep reds.
3. **OKLCH Uniformity**: OKLCH maps this physical spectrum into polar coordinates ($L, C, H$) using non-linear transformation matrices calibrated against human perceptual contrast data (developed by Björn Ottosson in 2020).

---

## 4. Syntax & Usage

```
+-------------------------------------------------------------------------+
| Web Platform Baseline: Widely Available                                  |
| - OKLCH, Display-P3, color-mix(): Widely Available (Baseline 2023)       |
| - Relative Color Syntax (RCS): Baseline 2024                             |
| - light-dark() function: Baseline 2024                                   |
+-------------------------------------------------------------------------+
```

### Modern Functional Color Syntax
Modern CSS abandons commas in color functions, using whitespace-separated values and a forward slash (`/`) for alpha opacity:
```css
/* Modern space-separated format */
.color-demo {
  /* OKLCH */
  color: oklch(0.65 0.18 140 / 0.9);

  /* Modern rgb() */
  background-color: rgb(2 132 199 / 80%);

  /* Modern hsl() */
  border-color: hsl(200deg 98% 39% / 0.5);
}
```

### High Contrast System Colors
CSS defines semantic system colors that automatically adapt to the user's operating system high-contrast theme:
```css
@media (forced-colors: active) {
  .btn-border {
    /* Guaranteed high contrast from OS palette */
    border: 2px solid ButtonText;
    background-color: ButtonFace;
    color: ButtonText;
  }
}
```

---

## 5. Practical Examples

### Example 1: Basic Modern Color System Tokens
```css
:root {
  /* Define core brand tokens in OKLCH */
  --brand-primary: oklch(0.6 0.22 250);
  --brand-success: oklch(0.65 0.2 145);
  --brand-warning: oklch(0.75 0.18 75);
  --brand-danger:  oklch(0.6 0.24 25);

  /* Background surfaces */
  --surface-base:  oklch(0.99 0.005 250);
  --surface-card:  oklch(1.0 0 0);
  --text-main:     oklch(0.2 0.02 250);
}

body {
  background-color: var(--surface-base);
  color: var(--text-main);
  font-family: system-ui, sans-serif;
}
```

### Example 2: Real-World Dynamic State Generation with `color-mix()`
Eliminate hardcoded hover, active, and focus colors by deriving them dynamically:
```css
.btn-primary {
  background-color: var(--brand-primary);
  color: #ffffff;
  padding: 0.625rem 1.25rem;
  border-radius: 6px;
  border: none;
  font-weight: 600;
  cursor: pointer;
  transition: background-color 0.15s ease;
}

/* Hover: Automatically mix in 12% black */
.btn-primary:hover {
  background-color: color-mix(in oklch, var(--brand-primary) 88%, black);
}

/* Active: Automatically mix in 25% black */
.btn-primary:active {
  background-color: color-mix(in oklch, var(--brand-primary) 75%, black);
}

/* Focus outline with 40% translucent primary color */
.btn-primary:focus-visible {
  outline: 3px solid color-mix(in oklch, var(--brand-primary) 50%, transparent);
  outline-offset: 2px;
}
```

### Example 3: Edge Case — Relative Color Syntax for Dynamic Badges
Derive tinted backgrounds and dark text from an unknown primary brand color:
```html
<span class="badge" style="--badge-color: oklch(0.55 0.25 150);">
  Status: Operational
</span>
```
```css
.badge {
  display: inline-flex;
  align-items: center;
  padding: 0.25rem 0.75rem;
  border-radius: 9999px;
  font-size: 0.875rem;
  font-weight: 600;

  /* Use RCS: Set lightness to 92% and lower chroma for subtle background */
  background-color: oklch(from var(--badge-color) 0.92 0.04 h);

  /* Use RCS: Set lightness to 30% and higher chroma for legible text */
  color: oklch(from var(--badge-color) 0.3 0.15 h);

  border: 1px solid oklch(from var(--badge-color) 0.8 0.08 h);
}
```

### Example 4: Failure Case — The Dirty Gray Interpolation Trap
When transitioning between colors in legacy sRGB (like blue to yellow):
```css
/* Anti-Pattern: sRGB gradient passes through a muddy desaturated gray */
.muddy-gradient {
  background: linear-gradient(to right, #0000ff, #ffff00);
}

/* Modern Solution: OKLCH preserves rich chroma throughout the transition */
.vibrant-gradient {
  background: linear-gradient(to right in oklch, oklch(0.5 0.25 260), oklch(0.9 0.2 100));
}
```
*Failure Analysis*: In standard sRGB, interpolating between complementary colors passes directly through the dark gray center of the color cube. In `in oklch`, the interpolation traverses along the vivid outer perimeter, maintaining vibrant saturation.

### Example 5: Production Dark Mode with `light-dark()`
```css
:root {
  color-scheme: light dark;
}

.theme-card {
  /* Automatically shifts based on user's OS preference */
  background-color: light-dark(#ffffff, #1e293b);
  color: light-dark(#0f172a, #f8fafc);
  border: 1px solid light-dark(#e2e8f0, #334155);
  box-shadow: 0 4px 6px light-dark(rgba(0, 0, 0, 0.05), rgba(0, 0, 0, 0.3));
  padding: 1.5rem;
  border-radius: 8px;
}
```
*Playwright Test Runner Verification*:
```javascript
// Test color-mix support
const supportsColorMix = CSS.supports('color', 'color-mix(in oklch, red, blue)');
console.log('Engine supports color-mix:', supportsColorMix); // true
```

---

## 6. Best Practices

1. **Standardize Design System Tokens on OKLCH**: Adopt OKLCH for design systems to ensure mathematical predictability when creating accessible color palettes.
2. **Use `color-mix()` Instead of Preprocessors**: Replace Sass `darken()` and `lighten()` with native `color-mix(in oklch, var(--color) 85%, black)`.
3. **Always Declare Interpolation Space in Gradients**: Use `linear-gradient(to right in oklch, ...)` to prevent muddy gray midpoints.
4. **Leverage `light-dark()` for Simple Theme Switching**: Replace verbose `@media (prefers-color-scheme: dark)` blocks with inline `light-dark()` definitions.
5. **Always Verify WCAG Contrast with Color Pickers**: While OKLCH provides perceptually uniform lightness, always test text against background colors using DevTools to confirm a minimum $4.5:1$ ratio.
6. **Supply Progressive Fallbacks for Wide Gamut P3**: Wrap Display-P3 colors in `@supports (color: color(display-p3 0 0 0))` or let Lightning CSS compile fallbacks for older devices.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Root Cause | Symptom / Failure | Correct Pattern |
|:---|:---|:---|:---|
| Trusting HSL lightness for accessibility | HSL lightness is mathematically non-uniform | Light yellow text fails WCAG contrast despite having $50\%$ lightness. | Use OKLCH lightness ($L$). |
| Gradients passing through muddy gray | Defaulting to sRGB gradient interpolation | Ugly brown/gray desaturated bands in the center of gradients. | Add `in oklch` to `linear-gradient()`. |
| Using `light-dark()` without `color-scheme` | Missing root context declaration | `light-dark()` always outputs the light value, ignoring OS dark mode. | Add `color-scheme: light dark;` to `:root`. |
| Setting OKLCH Chroma ($C$) higher than 0.4 | Exceeding visible display limits | Colors clip to gamut boundary, resulting in clipping distortions. | Keep chroma typically between $0.02$ and $0.25$. |
| Mixing colors in `srgb` space | Defaulting to legacy space | `color-mix(in srgb, ...)` produces dull, muddy intermediate tones. | Always mix in `oklch`: `color-mix(in oklch, ...)`. |
| Comma delimiters in modern color functions | Legacy syntax habits | `oklch(0.6, 0.2, 150)` fails parsing in strict modern parsers. | Use space separation: `oklch(0.6 0.2 150)`. |

---

## 8. Performance, Accessibility & Browser Support

### Color Math Performance
All modern browser engines (Blink, Gecko, WebKit) implement color space conversions in hardware-accelerated SIMD vector instructions inside the GPU rendering stack. Evaluating `color-mix()` and `oklch()` incurs **zero measurable performance overhead** compared to standard hex codes.

### Accessibility Architecture
- OKLCH is the first CSS color space where **delta lightness ($\Delta L$) correlates directly to perceived contrast**.
- If a background has $L = 0.95$ and text has $L = 0.25$, the contrast difference of $0.70$ guarantees WCAG AA compliance across **every hue angle ($H$) on the color wheel**!

---

## 9. Comparison / When to Use What

| Format | Color Gamut | Perceptually Uniform? | Dynamic Mixing? | Primary Use Case |
|:---|:---|:---:|:---:|:---|
| **Hex (`#0284c7`)** | sRGB only | No | No | Legacy codebases, static graphics |
| **`hsl()`** | sRGB only | No | No | Legacy manual hue tweaks |
| **`oklch()`** | **Wide Gamut / Universal**| **YES** | Yes | Modern design systems & tokens |
| **`color-mix()`** | Any space (OKLCH recommended)| N/A | **YES** | Dynamic hover/active/tint derivation |
| **`light-dark()`** | Evaluates any color format | N/A | No | Effortless dual-theme systems |
| **Display-P3** | Wide Gamut | No | No | Ultra-saturated photographic accents |

---

## 10. Interview Questions

<details>
<summary><strong>1. Why is HSL considered perceptually broken for building accessible web palettes?</strong></summary>

HSL (Hue, Saturation, Lightness) is a simple cylindrical transformation of the sRGB color cube. It treats all hues equally, ignoring human ocular biology. The human eye has far more receptors for green and red light than for blue light. In HSL:
- Pure yellow (`hsl(60, 100%, 50%)`) and pure blue (`hsl(240, 100%, 50%)`) have the exact same lightness value ($50\%$).
- However, yellow has a perceived luminance of $\approx 0.93$, while blue has a perceived luminance of $\approx 0.07$.
- Attempting to build an automated contrast algorithm using HSL lightness fails because an HSL $50\%$ color can be blindingly bright or pitch dark depending on hue.
</details>

<details>
<summary><strong>2. What are the three parameters of `oklch(L C H)` and what are their valid ranges?</strong></summary>

1. **$L$ (Lightness)**: Measures perceived brightness from $0.0$ (or $0\%$, pure black) to $1.0$ (or $100\%$, pure white).
2. **$C$ (Chroma)**: Measures color saturation/purity starting from $0.0$ (neutral gray). While technically unbound, standard displays reach maximum saturation between $0.2$ and $0.37$.
3. **$H$ (Hue)**: The angle on the color circle from $0^\circ$ to $360^\circ$ ($0^\circ$ is magenta-pink, $90^\circ$ is yellow, $145^\circ$ is green, $250^\circ$ is blue).
</details>

<details>
<summary><strong>3. How does Relative Color Syntax (RCS) work?</strong></summary>

Relative Color Syntax allows you to derive a new color from an existing color using the `from` keyword:
```css
background-color: oklch(from var(--primary) l c calc(h + 180));
```
The browser takes the input color, converts it into the specified color space (OKLCH), unpacks its channels as local variables (`l`, `c`, `h`, and `alpha`), allows you to perform math on them via `calc()`, and emits the newly transformed color.
</details>

<details>
<summary><strong>4. What is the "dirty gray" gradient effect, and how does specifying `in oklch` eliminate it?</strong></summary>

When a browser renders a gradient between two complementary colors in sRGB (e.g., from blue to yellow), it interpolates coordinates through the middle of the sRGB color space. Because the center of the color cube is desaturated gray, the middle of the gradient renders as a muddy, brown-gray band. Specifying `in oklch` (`linear-gradient(to right in oklch, blue, yellow)`) instructs the engine to interpolate in the polar OKLCH space, sweeping around the color wheel and preserving vibrant chroma throughout.
</details>

<details>
<summary><strong>5. What requirement must be met before `light-dark()` can function properly?</strong></summary>

The document or ancestor container must explicitly define the **`color-scheme`** property (e.g., `:root { color-scheme: light dark; }`). Without `color-scheme: light dark`, the browser does not activate dynamic scheme switching for that tree, and `light-dark()` will perpetually evaluate only the first (light) color argument.
</details>

<details>
<summary><strong>6. What is the Display-P3 color gamut, and why does it matter on modern hardware?</strong></summary>

Display-P3 is a wide-gamut RGB color space originally introduced by the digital cinema industry and adopted by Apple for iPhone/Mac Retina displays and modern OLED screens. It covers approximately **$50\%$ of visible colors**, compared to sRGB which covers only $35\%$. P3 enables significantly more vibrant, saturated greens, intense oranges, and deep reds that cannot be represented in standard hex or sRGB codes.
</details>

<details>
<summary><strong>7. How does `color-mix()` resolve percentages when they don't add up to 100%?</strong></summary>

If the specified percentages in `color-mix()` do not sum to $100\%$ (e.g., `color-mix(in oklch, red 20%, blue 30%)`), the browser engine normalizes them:
- The ratio between the colors is scaled proportionally to $100\%$ ($20\% + 30\% = 50\%$, so red becomes $40\%$ and blue becomes $60\%$).
- An alpha transparency modifier ($50\%$) is then applied to the resulting mixture.
</details>

<details>
<summary><strong>8. What are CSS System Colors like `CanvasText` and `ButtonFace`?</strong></summary>

System Colors are keywords that map directly to the operating system's platform color palette (e.g., `Canvas` for background, `CanvasText` for text, `Highlight` for selected text, `ButtonFace` for buttons). They are especially crucial for accessibility when a user activates **Windows High Contrast Mode** or `@media (forced-colors: active)`, ensuring that components seamlessly adopt user-mandated colors.
</details>

---

## 11. Practice Exercises

### Exercise 1: Create Accessible Complementary Accent via RCS (Easy)
**Task**: Given a brand token `--brand-color: oklch(0.65 0.22 240);`, write the CSS rule for `.accent-box` using Relative Color Syntax that rotates the hue by $180^\circ$ while keeping the exact same lightness and chroma.

<details>
<summary><strong>View Solution</strong></summary>

```css
.accent-box {
  background-color: oklch(from var(--brand-color) l c calc(h + 180));
}
```
*Explanation*: `calc(h + 180)` rotates to the exact complementary hue while keeping lightness and chroma identical.
</details>

---

### Exercise 2: Derive Hover State with `color-mix()` (Easy)
**Task**: Write the hover state rule for a button `.btn-accent` that blends $85\%$ of `var(--accent-color)` with $15\%$ `black` using `color-mix()` in the OKLCH space.

<details>
<summary><strong>View Solution</strong></summary>

```css
.btn-accent:hover {
  background-color: color-mix(in oklch, var(--accent-color) 85%, black);
}
```
*Explanation*: Blending with black in OKLCH creates a clean, predictable darker shade without shifting hue.
</details>

---

### Exercise 3: Vibrant Polar Gradient (Medium)
**Task**: Build a horizontal linear gradient from cyan (`oklch(0.7 0.2 200)`) to magenta (`oklch(0.65 0.25 340)`) that interpolates through the OKLCH color space to prevent muddy gray midtones.

<details>
<summary><strong>View Solution</strong></summary>

```css
.vibrant-banner {
  height: 200px;
  background-image: linear-gradient(
    to right in oklch,
    oklch(0.7 0.2 200),
    oklch(0.65 0.25 340)
  );
}
```
*Explanation*: Adding `in oklch` ensures interpolation follows the polar chroma perimeter.
</details>

---

### Exercise 4: Clean Theme Switching via `light-dark()` (Medium)
**Task**: Configure a `.surface` class that automatically renders with background `#ffffff` and text `#1e293b` in light mode, and background `#0f172a` with text `#f8fafc` in dark mode, without using `@media (prefers-color-scheme)` blocks.

<details>
<summary><strong>View Solution</strong></summary>

```css
:root {
  color-scheme: light dark;
}

.surface {
  background-color: light-dark(#ffffff, #0f172a);
  color: light-dark(#1e293b, #f8fafc);
  padding: 1.5rem;
  border-radius: 8px;
}
```
*Explanation*: With `color-scheme: light dark` set, `light-dark()` dynamically handles OS theme shifts.
</details>

---

### Exercise 5: Relative Alpha Transparency Generation (Hard)
**Task**: An application receives arbitrary custom brand colors. Write a CSS utility `.translucent-surface` using Relative Color Syntax that extracts the input color `--brand` and applies a $20\%$ alpha transparency while preserving all original color channels.

<details>
<summary><strong>View Solution</strong></summary>

```css
.translucent-surface {
  /* Retain exact L, C, H coordinates; override alpha channel with 0.2 */
  background-color: oklch(from var(--brand) l c h / 0.2);
  backdrop-filter: blur(8px);
}
```
*Explanation*: Specifying `/ 0.2` in RCS overrides the alpha channel while leaving color coordinates unchanged.
</details>

---

### Exercise 6: Programmatic OKLCH Gamut Detection via JavaScript (Hard)
**Task**: Write a JavaScript function `isWideGamutSupported()` that checks whether the user's physical monitor supports the Display-P3 wide color gamut using `window.matchMedia()`.

<details>
<summary><strong>View Solution</strong></summary>

```javascript
/**
 * Detects whether the user's screen hardware supports Wide Gamut Display-P3.
 * @returns {boolean}
 */
function isWideGamutSupported() {
  if (typeof window === 'undefined' || !window.matchMedia) return false;
  return window.matchMedia('(color-gamut: p3)').matches;
}

console.log('Display supports P3 Gamut:', isWideGamutSupported());
```
*Explanation*: `(color-gamut: p3)` inspects display hardware capabilities directly through media queries.
</details>

---

## 12. Summary / Cheat Sheet

```
+-------------------------------------------------------------------------------------------------+
| CSS COLOURS & COLOR SPACES MASTER CHEAT SHEET                                                   |
+--------------------------+-------------------------------------+--------------------------------+
| Syntax                   | Example                             | Primary Benefit                |
+--------------------------+-------------------------------------+--------------------------------+
| OKLCH                    | oklch(0.65 0.22 250 / 0.9)          | Perceptually uniform lightness |
| color-mix()              | color-mix(in oklch, red 80%, black) | Native runtime color blending  |
| Relative Color Syntax    | oklch(from var(--c) 0.9 c h)        | Derive tints/shades from tokens|
| light-dark()             | light-dark(#ffffff, #0f172a)        | Single-property dark mode      |
| Display-P3               | color(display-p3 0 1 0.2)           | Ultra-vibrant wide gamut       |
| Gradient Interpolation   | linear-gradient(in oklch, ...)      | Eliminates dirty gray banding  |
| System Colors            | Canvas, CanvasText, ButtonFace      | High-contrast OS accessibility |
+--------------------------+-------------------------------------+--------------------------------+
```

---

## 13. Further Reading

- [W3C CSS Color Module Level 4](https://www.w3.org/TR/css-color-4/)
- [W3C CSS Color Module Level 5](https://www.w3.org/TR/css-color-5/)
- [MDN Web Docs: oklch()](https://developer.mozilla.org/en-US/docs/Web/CSS/color_value/oklch)
- [web.dev: High definition CSS color guide](https://web.dev/articles/hd-color)
- [Evil Martians: OKLCH in CSS: why we moved from RGB and HSL](https://evilmartians.com/chronicles/oklch-in-css-why-quit-rgb-hsl)

---

[Previous: 09 - CSS Units: Absolute vs Relative](09-units.md) | [Section Index](README.md) | [Next: 11 - Custom Properties (CSS Variables)](11-custom-properties.md)
