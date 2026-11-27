# Icon Fonts vs Inline & External SVG

| Metadata | Details |
| :--- | :--- |
| **Topic** | Evolution of Web Icons, Failure Modes of Icon Fonts, Inline SVG, External SVG Sprites, CSS `currentColor` & Accessibility Patterns |
| **Spec Level** | W3C Scalable Vector Graphics (SVG) 2 / CSS Fonts Module Level 4 / WAI-ARIA 1.2 |
| **Baseline Status** | Widely Available (`<svg>`, `<use>`, `currentColor`, `fill`, `stroke` Baseline 2015) |
| **Browsers Tested** | Chrome 120+, Firefox 122+, Safari 17.2+, Edge 120+ |
| **Tested With** | NVDA, VoiceOver, JAWS, Axe DevTools, Chromium SVG Renderer |
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

In the early responsive web era (2010–2016), **Icon Fonts** (such as Font Awesome, Glyphicons, and IcoMoon) became the dominant method for rendering icons. By encoding vector symbols as private-use Unicode glyphs inside a custom TTF/WOFF font file, developers could scale icons infinitely and recolor them via CSS `color`.

However, as browser rendering engines, screen readers, and web performance standards matured, icon fonts revealed severe architectural failure modes:
1. **Flash of Invisible / Unstyled Icons (FOIT / FOUT)**: On slow connections or network drops, icon fonts fail to download, leaving UI buttons completely blank or rendering bizarre unmapped Unicode replacement glyphs (empty boxes `` or random Chinese/Latin characters).
2. **Subpixel Antialiasing Blurring**: Fonts are rendered by the operating system's font rasterizer (ClearType, FreeType, CoreText), which applies hinting and antialiasing tailored for reading text. Icons mapped to font glyphs suffer from fuzzy edges and blurry alignments when scaled to non-integer sizes.
3. **Accessibility Disasters**: Screen readers frequently attempt to pronounce icon font characters or ligatures aloud, reading "magnifying glass" as "E-zero-two-four" or "privately mapped glyph".
4. **Monochrome Limitation**: Font glyphs are fundamentally single-color; creating multi-tone or dual-colored icons requires cumbersome layered hacks.

Today, **Scalable Vector Graphics (SVG)**—implemented via inline SVG elements or external SVG symbol sprites—has completely superseded icon fonts in professional frontend engineering.

---

## 2. Core Concepts

### The Modern SVG Architecture
SVG treats icons as first-class geometric DOM elements with dedicated coordinate systems (`viewBox`), paths, curves, and strokes:
- **Instant Rendering (Zero FOIT)**: Inline SVG renders immediately during DOM parsing without waiting for an asynchronous font network request.
- **Pixel-Crisp Vector Math**: SVG renders directly via the browser's 2D graphics engine (Blink Skia, Safari CoreGraphics), bypassing font antialiasing filters.
- **Multi-Tone & CSS Variables**: Individual paths inside an SVG can be styled independently with CSS custom properties, fills, strokes, and gradients.
- **The CSS Magic Keyword: `currentColor`**: Setting `fill: currentColor;` or `stroke: currentColor;` causes the SVG icon to automatically inherit the current text `color` of its parent element (matching button text, link hover states, and theme colors instantly).

### SVG Implementation Strategies
1. **Inline SVG**: Placing the raw `<svg>` markup directly in the HTML document.
   - Pros: Maximum styling flexibility via CSS; zero network latency; path-level animation.
   - Cons: Increases HTML payload size; cannot be cached independently by browser HTTP cache.
2. **External SVG Symbol Sprites (`<use>`)**: Grouping icons into a single `icons.svg` file containing `<symbol id="icon-name">` elements, referenced via `<svg><use href="/icons.svg#icon-name"></use></svg>`.
   - Pros: Single HTTP request; fully cached by browser; clean, lightweight HTML markup.
   - Cons: Cross-origin restrictions (must be served same-origin or with CORS).

---

## 3. How It Works Internally

```
Comparison of Icon Font vs SVG Rendering Pipeline

1. Icon Font Pipeline:
HTML Parsing -> CSSOM -> Font File Network Fetch (Blocked) -> OS Font Rasterizer
                                                                    |
                                                     (ClearType / Subpixel Hinting)
                                                                    v
                                                     [Blurry Icon or FOIT Box]

2. Inline SVG Pipeline:
HTML Parsing -> Direct 2D Vector Rendering (Skia / CoreGraphics)
                        |
                        v
         [Instant, Crisp Pixel-Aligned Icon]
```

---

## 4. Syntax & Usage

### Baseline & Feature Support
- **Inline `<svg>` & `<path>`**: Baseline Widely Available.
- **`<use href="...">` external sprites**: Baseline Widely Available (Chrome 50+, Safari 12.1+, Firefox 51+, Edge 79+).
- **CSS `currentColor`**: Baseline Widely Available.

```css
/* Standard Production SVG Icon Styling */
.icon {
  display: inline-block;
  width: 1.25rem;  /* 20px default */
  height: 1.25rem;
  flex-shrink: 0;
  vertical-align: middle;
  fill: currentColor; /* Inherits text color */
}

/* For stroked icon systems (e.g., Lucide, Feather) */
.icon-stroked {
  display: inline-block;
  width: 1.25rem;
  height: 1.25rem;
  fill: none;
  stroke: currentColor;
  stroke-width: 2;
  stroke-linecap: round;
  stroke-linejoin: round;
}
```

---

## 5. Practical Examples

### Example 1: Accessible Inline SVG Button Pattern
```html
<!-- Pattern A: Icon accompanying visible text (Decorative Icon) -->
<button type="button" class="btn-primary">
  <svg class="icon-stroked" viewBox="0 0 24 24" aria-hidden="true" focusable="false">
    <path d="M5 12h14M12 5l7 7-7 7" />
  </svg>
  <span>Continue to Checkout</span>
</button>

<!-- Pattern B: Icon-only button (Standalone Functional Icon) -->
<button type="button" class="icon-only-btn" aria-label="Close dialog">
  <svg class="icon-stroked" viewBox="0 0 24 24" aria-hidden="true" focusable="false">
    <path d="M18 6L6 18M6 6l12 12" />
  </svg>
</button>
```

```css
.btn-primary {
  display: inline-flex;
  align-items: center;
  gap: 0.5rem;
  padding: 0.625rem 1.25rem;
  font-size: 0.9375rem;
  font-weight: 600;
  background-color: #2563eb;
  color: #ffffff;
  border: none;
  border-radius: 6px;
  cursor: pointer;
  transition: background-color 0.2s ease;
}

.btn-primary:hover {
  background-color: #1d4ed8;
}

.btn-primary:focus-visible {
  outline: 2px solid #2563eb;
  outline-offset: 2px;
}

.icon-only-btn {
  display: inline-flex;
  align-items: center;
  justify-content: center;
  width: 2.25rem;
  height: 2.25rem;
  border-radius: 50%;
  border: 1px solid #cbd5e1;
  background-color: #ffffff;
  color: #64748b;
  cursor: pointer;
  transition: color 0.2s ease, border-color 0.2s ease;
}

.icon-only-btn:hover {
  color: #0f172a;
  border-color: #94a3b8;
}

.icon-only-btn:focus-visible {
  outline: 2px solid #2563eb;
  outline-offset: 2px;
}
```

### Example 2: External SVG Sprite System with `<use>`
```html
<!-- Inside HTML: Clean, semantic, highly performant -->
<svg class="icon" aria-hidden="true">
  <use href="/assets/icons/sprite.svg#icon-search"></use>
</svg>
```

```xml
<!-- Inside /assets/icons/sprite.svg (Cached once by HTTP cache) -->
<svg xmlns="http://www.w3.org/2000/svg" style="display: none;">
  <symbol id="icon-search" viewBox="0 0 24 24">
    <circle cx="11" cy="11" r="8" fill="none" stroke="currentColor" stroke-width="2" />
    <line x1="21" y1="21" x2="16.65" y2="16.65" stroke="currentColor" stroke-width="2" stroke-linecap="round" />
  </symbol>
  
  <symbol id="icon-trash" viewBox="0 0 24 24">
    <polyline points="3 6 5 6 21 6" fill="none" stroke="currentColor" stroke-width="2" />
    <path d="M19 6v14a2 2 0 0 1-2 2H7a2 2 0 0 1-2-2V6" fill="none" stroke="currentColor" stroke-width="2" />
  </symbol>
</svg>
```

### Example 3: Dual-Tone SVG Icon with CSS Custom Properties
```html
<svg class="duotone-icon" viewBox="0 0 24 24" aria-hidden="true">
  <path class="primary-shape" d="M12 2L2 7l10 5 10-5-10-5z" />
  <path class="secondary-shape" d="M2 17l10 5 10-5M2 12l10 5 10-5" />
</svg>
```

```css
.duotone-icon {
  width: 2rem;
  height: 2rem;
  --icon-primary: #2563eb;
  --icon-secondary: #93c5fd;
}

.duotone-icon:hover {
  --icon-primary: #1d4ed8;
  --icon-secondary: #60a5fa;
}

.duotone-icon .primary-shape {
  fill: var(--icon-primary);
  transition: fill 0.2s ease;
}

.duotone-icon .secondary-shape {
  fill: none;
  stroke: var(--icon-secondary);
  stroke-width: 2;
  transition: stroke 0.2s ease;
}
```

### Example 4: Interactive Micro-Animation on SVG Paths
```css
/* Animated Hamburger to Close X Toggle */
.menu-toggle-icon line {
  stroke: currentColor;
  stroke-width: 2;
  stroke-linecap: round;
  transition: transform 0.3s cubic-bezier(0.16, 1, 0.3, 1),
              opacity 0.2s ease;
  transform-origin: center;
}

.menu-btn[aria-expanded="true"] .line-top {
  transform: translateY(6px) rotate(45deg);
}

.menu-btn[aria-expanded="true"] .line-middle {
  opacity: 0;
}

.menu-btn[aria-expanded="true"] .line-bottom {
  transform: translateY(-6px) rotate(-45deg);
}
```

### Example 5: High-Contrast Mode Support for SVG
```css
/* Under Windows High Contrast Mode, SVGs styled with custom hex colors vanish!
   Using currentColor ensures SVGs adopt the system ButtonText or CanvasText. */
.system-icon {
  width: 1.25rem;
  height: 1.25rem;
  stroke: currentColor;
  fill: none;
}

@media (forced-colors: active) {
  .system-icon {
    stroke: CanvasText;
  }
}
```

---

## 6. Best Practices

1. **Always Set `aria-hidden="true"` and `focusable="false"` on Decorative SVGs**: Decorative icons must not be focusable or announced to screen readers.
2. **Always Provide `aria-label` on Icon-Only Buttons**: If a button contains only an SVG icon (e.g., a trash can or magnifying glass), the `<button>` must have `aria-label="Delete item"` or `aria-label="Search"`.
3. **Use `currentColor` for Fills and Strokes**: Setting `stroke: currentColor;` makes icons effortlessly adapt to text colors, theme changes, and hover states.
4. **Always Define a `viewBox` on SVGs**: Without a `viewBox="0 0 24 24"`, SVGs will not scale responsively when sized via CSS `width` and `height`.
5. **Always Set `flex-shrink: 0` on Icons Inside Flex Containers**: In flexbox layouts with expanding text, icons will shrink into squished slivers unless protected by `flex-shrink: 0;`.
6. **Prefer External SVG Sprites for Large Icon Sets**: If an application uses 50+ icons, external SVG symbol sprites keep HTML payloads small while caching the icons globally.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Correction |
| :--- | :--- | :--- |
| **Omitting `aria-hidden="true"` on decorative icons** | Screen readers stumble on unlabelled graphics, announcing "image" or reading internal `<path>` data. | Add `aria-hidden="true" focusable="false"` to the `<svg>`. |
| **Using icon fonts for critical UI actions** | If the font file fails to load or drops connection, users see blank buttons or missing symbols. | Use inline SVG or SVG sprites. |
| **Omitting `viewBox` in SVG files** | CSS `width` and `height` simply crop or stretch the canvas unpredictably. | Always ensure `<svg viewBox="0 0 24 24">` is present. |
| **Squished icons in Flexbox rows** | Flexbox compresses non-shrink-protected items when text wraps or grows. | Add `flex-shrink: 0;` to all `.icon` rules. |
| **Hardcoding `#000000` fill inside SVG paths** | Prevents CSS hover recoloring and dark mode theme switching. | Replace hardcoded colors with `currentColor` or CSS variables. |
| **Cross-origin failures with `<use href="...">`** | Browsers block external SVG sprites loaded from a different CDN origin due to CORS restrictions. | Host the SVG sprite on the same origin or proxy it. |

---

## 8. Performance, Accessibility & Browser Support

### Accessibility Compliance Matrix (WCAG 2.2 AA)
- **Criterion 1.1.1 (Non-text Content)**:
  - If the icon is decorative alongside visible text: Hide it with `aria-hidden="true"`.
  - If the icon is the sole content of an interactive control: The control must have an accessible name (`aria-label="..."`).
  - If the icon conveys standalone information in content (e.g., a status indicator): Provide `<svg role="img" aria-label="Status: Healthy">`.

### Performance Comparison
- **Icon Font (Font Awesome 6 Pro)**: ~150 KB font binary download, blocking render tree, font parsing latency.
- **SVG Sprite (30 icons)**: ~12 KB gzip payload, zero render blocking, cached instantly across sessions.

---

## 9. Comparison / When to Use What

| Dimension | Icon Fonts | Inline SVG | External SVG Sprite (`<use>`) |
| :--- | :--- | :--- | :--- |
| **Network Latency** | High (Requires separate font fetch) | **Zero (Embedded in HTML)** | Single cached request (`sprite.svg`) |
| **Rendering Crispness** | Blurry at non-standard sizes | **Flawless mathematical precision** | **Flawless mathematical precision** |
| **Color Control** | Monochrome only | Multi-color & CSS variables | Single/multi-color |
| **Failure Mode** | FOIT blank square or wrong glyph | **Zero failure (Renders with DOM)** | Fallback if asset fails |
| **Accessibility** | Prone to screen reader defects | Clean ARIA attributes | Clean ARIA attributes |
| **Animation Fidelity** | Whole glyph only | Individual path control | Whole symbol |

---

## 10. Interview Questions

<details>
<summary>1. Why have SVG icons fundamentally replaced Icon Fonts in modern web development?</summary>

Icon fonts suffer from several critical architectural flaws:
1. **Network & Rendering Reliability**: Icon fonts are external binaries loaded via `@font-face`. On slow or interrupted connections, icons suffer from FOIT (remaining blank) or FOUT (displaying bizarre fallback characters). SVG renders immediately with the DOM.
2. **Visual Fidelity**: Fonts are rendered by the OS font rasterizer using subpixel text antialiasing and hinting, which distorts icons at non-integer pixel sizes. SVGs are rasterized by the browser's dedicated 2D graphics engine (Blink Skia, WebKit CoreGraphics), producing crisp vector boundaries at any resolution.
3. **Accessibility**: Screen readers often pronounce unmapped Unicode font glyphs as random characters or hexadecimal codes. SVGs support robust ARIA labeling (`aria-hidden="true"`, `role="img"`, `<title>`).
4. **Color & Styling**: Icon fonts are strictly monochrome. SVGs support multi-colored paths, CSS variables, independent fills, and strokes.
</details>

<details>
<summary>2. How does the CSS <code>currentColor</code> keyword work, and why is it essential for SVG icon systems?</summary>

`currentColor` evaluates to the computed value of the CSS `color` property on the current element (or inherited from its closest ancestor). By assigning `fill: currentColor;` or `stroke: currentColor;` to SVG paths, the icon automatically matches the text color of whatever container it resides in. 

If a parent button changes its text color on `:hover` or theme switch (`color: #2563eb`), the SVG icon immediately updates its color without needing separate stroke/fill override selectors.
</details>

<details>
<summary>3. Why must you set <code>flex-shrink: 0</code> on SVG icons when used inside Flexbox layouts?</summary>

By default in CSS Flexbox, all items have `flex-shrink: 1`. When a flex row contains an icon alongside a text label (such as in a button or navigation link), if the container becomes narrow or the text wraps, the flex layout algorithm will compress any shrinkable items. 

Because SVG icons typically have fixed pixel dimensions (e.g., `width: 20px; height: 20px`), allowing them to shrink causes the icon to distort into an oval or crush into a 0px sliver. Setting `flex-shrink: 0;` locks the icon to its defined dimensions.
</details>

<details>
<summary>4. What are the two distinct accessibility patterns for SVG icons depending on their visual context?</summary>

1. **Decorative Context** (Icon accompanies visible text, e.g., `<button><svg>...</svg> Checkout</button>`):
   - The icon must be completely hidden from assistive technologies using `aria-hidden="true"` and `focusable="false"`. The screen reader reads only the visible text.
2. **Standalone / Interactive Context** (Icon is the only item in a button or link, e.g., a search button with only a magnifying glass):
   - The icon must be hidden with `aria-hidden="true"`, and the parent control must be given an accessible name via `aria-label="Search"` on the `<button>`.
   - Alternatively, if the SVG is a standalone content illustration, use `<svg role="img" aria-label="Descriptive text">`.
</details>

<details>
<summary>5. How does the external SVG sprite pattern with <code>&lt;use href="..."&gt;</code> work?</summary>

In an external sprite system, all icons are consolidated into a single static file (e.g., `/icons.svg`) wrapped in an `<svg>` tag. Each icon is encapsulated in a `<symbol>` tag with an `id` and `viewBox`:
```xml
<symbol id="trash" viewBox="0 0 24 24"><path d="..."/></symbol>
```
In web pages, icons are referenced via:
```html
<svg class="icon"><use href="/icons.svg#trash"></use></svg>
```
The browser downloads and caches the sprite file once via HTTP caching, keeping the HTML DOM clean and lightweight while preventing duplicated SVG markup across pages.
</details>

<details>
<summary>6. What happens if an SVG does not have a <code>viewBox</code> attribute?</summary>

The `viewBox` attribute (`min-x, min-y, width, height`) establishes the internal coordinate system of the vector canvas and enables aspect ratio scaling. Without a `viewBox`, resizing the SVG with CSS `width` and `height` does not scale the internal paths; instead, the browser simply expands or shrinks the outer viewport box, clipping the vector drawing or leaving huge empty gutters.
</details>

<details>
<summary>7. Why do SVG icons styled with hardcoded hex colors break in Windows High Contrast Mode (WHCM)?</summary>

Under `@media (forced-colors: active)`, the operating system strips author colors, forcing text to system colors (`CanvasText`, `Highlight`). If an SVG contains hardcoded inline fills (`fill="#2563eb"`), the browser will either override it or fail to contrast against the high-contrast background. Setting `stroke: currentColor; fill: none;` guarantees the icon adopts the high-contrast system text color.
</details>

<details>
<summary>8. What is the difference between inline SVG and embedding an SVG via <code>&lt;img src="icon.svg"&gt;</code>?</summary>

- An SVG loaded via `<img>` is treated as a separate, sandboxed image document. CSS in the parent HTML document cannot target, style, or recolor internal SVG paths (e.g., `currentColor` does not penetrate the `<img>` boundary).
- Inline SVG becomes part of the document DOM tree, allowing full CSS inheritance, path styling, CSS variable integration, and keyframe animations.
</details>

---

## 11. Practice Exercises

### Exercise 1: Build a Reusable Accessible SVG Icon Class
**Goal**: Author a CSS rule set for stroked vector icons that enforces square dimensions, prevents flex squishing, and inherits text colors.

<details>
<summary>View Solution</summary>

```css
.ui-icon {
  display: inline-block;
  width: 1.25rem;
  height: 1.25rem;
  flex-shrink: 0;
  vertical-align: middle;
  fill: none;
  stroke: currentColor;
  stroke-width: 2;
  stroke-linecap: round;
  stroke-linejoin: round;
}
```
</details>

### Exercise 2: Accessible Icon-Only Floating Action Button (FAB)
**Goal**: Create a round floating button with a plus icon that meets WCAG 2.2 AA accessibility standards.

<details>
<summary>View Solution</summary>

```html
<button type="button" class="fab-btn" aria-label="Create new document">
  <svg class="ui-icon" viewBox="0 0 24 24" aria-hidden="true" focusable="false">
    <line x1="12" y1="5" x2="12" y2="19"></line>
    <line x1="5" y1="12" x2="19" y2="12"></line>
  </svg>
</button>
```

```css
.fab-btn {
  display: inline-flex;
  align-items: center;
  justify-content: center;
  width: 3.5rem;
  height: 3.5rem;
  border-radius: 50%;
  background-color: #2563eb;
  color: #ffffff;
  border: none;
  box-shadow: 0 4px 6px -1px rgba(0, 0, 0, 0.1);
  cursor: pointer;
  transition: transform 0.2s ease, background-color 0.2s ease;
}

.fab-btn:hover {
  background-color: #1d4ed8;
  transform: scale(1.05);
}

.fab-btn:focus-visible {
  outline: 3px solid #2563eb;
  outline-offset: 3px;
}
```
</details>

### Exercise 3: Implement Dual-Tone Icon Theming
**Goal**: Create an SVG alert icon that uses two CSS custom properties for body fill and exclamation mark.

<details>
<summary>View Solution</summary>

```html
<svg class="duotone-alert" viewBox="0 0 24 24" aria-hidden="true">
  <circle class="alert-bg" cx="12" cy="12" r="10" />
  <line class="alert-mark" x1="12" y1="8" x2="12" y2="12" />
  <circle class="alert-mark" cx="12" cy="16" r="1" />
</svg>
```

```css
.duotone-alert {
  width: 2rem;
  height: 2rem;
  --bg-color: #fef3c7;
  --mark-color: #d97706;
}

.duotone-alert .alert-bg {
  fill: var(--bg-color);
}

.duotone-alert .alert-mark {
  stroke: var(--mark-color);
  stroke-width: 2;
  stroke-linecap: round;
}
```
</details>

### Exercise 4: SVG Spin Animation for Loading States
**Goal**: Write a smooth infinite rotation keyframe animation for an SVG spinner.

<details>
<summary>View Solution</summary>

```css
@keyframes iconSpin {
  from {
    transform: rotate(0deg);
  }
  to {
    transform: rotate(360deg);
  }
}

.spinner-icon {
  width: 1.5rem;
  height: 1.5rem;
  animation: iconSpin 0.8s linear infinite;
  transform-origin: center;
}

@media (prefers-reduced-motion: reduce) {
  .spinner-icon {
    animation-duration: 2s; /* Slower, non-disorienting rotation */
  }
}
```
</details>

### Exercise 5: External SVG Sprite Reference
**Goal**: Write the complete markup to reference an external SVG symbol with proper fallback dimensions.

<details>
<summary>View Solution</summary>

```html
<svg class="ui-icon" aria-hidden="true" focusable="false">
  <use href="/assets/icons/navigation.svg#icon-arrow-left"></use>
</svg>
```
</details>

### Exercise 6: WHCM Compatibility Layer
**Goal**: Enforce high contrast compliance on an icon button so it renders cleanly in Windows High Contrast Mode.

<details>
<summary>View Solution</summary>

```css
.contrast-icon-btn {
  display: inline-flex;
  align-items: center;
  gap: 0.5rem;
  border: 1px solid transparent; /* Becomes system border in WHCM */
  padding: 0.5rem 1rem;
  color: #1e293b;
}

.contrast-icon-btn svg {
  stroke: currentColor;
}

@media (forced-colors: active) {
  .contrast-icon-btn {
    border-color: ButtonBorder;
    color: ButtonText;
  }
}
```
</details>

---

## 12. Summary / Cheat Sheet

| Feature | Icon Fonts (Legacy) | Modern SVG (Standard) |
| :--- | :--- | :--- |
| **Element** | `<i class="fa fa-icon"></i>` | `<svg class="icon">...</svg>` |
| **Color Coupling** | `color: #hex;` | `fill: currentColor;` or `stroke: currentColor;` |
| **Network Loading** | Asynchronous font binary (FOIT risk) | Instant (Inline) or cached sprite |
| **Pixel Rendering** | Subpixel font antialiasing (blurry) | Crisp 2D vector coordinates |
| **Decorative A11y** | Often misread by screen readers | `aria-hidden="true" focusable="false"` |
| **Flexbox Safety** | Variable width | Requires `flex-shrink: 0;` |

---

## 13. Further Reading
- [W3C Scalable Vector Graphics (SVG) 2 Specification](https://www.w3.org/TR/SVG2/)
- [MDN Web Docs: SVG in HTML](https://developer.mozilla.org/en-US/docs/Learn/HTML/Multimedia_and_embedding/Adding_vector_graphics_to_the_Web)
- [CSS-Tricks: Seriously, Don't Use Icon Fonts](https://css-tricks.com/icon-fonts-vs-svg/)
- [Lucide Icons: Modern Accessible SVG Icons](https://lucide.dev/)
- [Sara Soueidan: Accessible SVGs](https://www.sarasoueidan.com/blog/accessible-svgs/)

---

[Previous: 08-text-overflow-and-clamping.md](08-text-overflow-and-clamping.md) | [Module README](README.md) | [Next: 10-performance-of-fonts.md](10-performance-of-fonts.md)
