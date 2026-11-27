# Contrast, Motion & User Preferences

| Metadata | Details |
|:---|:---|
| **Level** | Intermediate to Advanced |
| **Prerequisites** | `06-accessibility/01-accessibility-fundamentals-and-wcag.md`, `05-head-metadata-and-seo/02-meta-charset-viewport-and-color-scheme.md` |
| **Estimated Reading Time** | 29 minutes |
| **Status** | Current |
| **Baseline Status** | Widely Available (CSS Media Queries Level 5) |
| **Browsers Tested** | Chrome 120+, Firefox 121+, Safari 17.2+, Edge 120+ |
| **Tested With** | Windows High Contrast Mode, Chrome DevTools Rendering Emulation |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. [Practical Examples](#5-practical-examples)
6. [Best Practices](#6-best-practices)
7. [Common Mistakes & Pitfalls](#7-common-mistakes--pitfalls)
8. [Performance, Accessibility, SEO & Security Notes](#8-performance-accessibility-seo--security-notes)
9. [Comparison / When to Use What](#9-comparison--when-to-use-what)
10. [Interview Questions](#10-interview-questions)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary / Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

### The Spectrum of Visual and Neurological Diversity
Web accessibility extends far beyond screen readers for blind users. A massive portion of digital audiences experience visual, vestibular, and cognitive conditions that govern how they interact with screen displays:
- **Low-vision and aging users** require high contrast ratios between text and background colors to distinguish letterforms.
- **Color-blind users** (affecting ~8% of males and ~0.5% of females) cannot differentiate information conveyed purely through red and green hues.
- **Individuals with vestibular disorders** experience vertigo, nausea, migraines, and physical disorientation when exposed to parallax scrolling, large spinning loaders, or zoom animations.
- **Photophobia and migraine sufferers** rely on dark themes to minimize screen glare.
- **Windows High Contrast Mode (Forced Colors) users** force operating system color palettes to override custom website styling.

### Modern Responsive Design: Respecting User Agency
Modern CSS Media Queries Level 5 enables web applications to actively listen for and respect operating system preferences:
- `prefers-reduced-motion`
- `forced-colors`
- `prefers-color-scheme`
- `prefers-contrast`

Designing an accessible site means ensuring your HTML structure and CSS cascade adapt gracefully to these user signals rather than fighting them.

---

## 2. Core Concepts

### WCAG Color Contrast Thresholds
WCAG defines mathematical contrast ratios based on **Relative Luminance** ($1:1$ to $21:1$):

```
+--------------------------------------------------------------------------+
|                        WCAG CONTRAST BENCHMARKS                          |
+--------------------------------------------------------------------------+
| Category         | Level AA Standard        | Level AAA Standard         |
+------------------+--------------------------+----------------------------+
| Normal Text      | 4.5:1 Minimum            | 7.0:1 Enhanced             |
| (<24px / <19px B)| (WCAG 1.4.3)             | (WCAG 1.4.6)               |
+------------------+--------------------------+----------------------------+
| Large Text       | 3.0:1 Minimum            | 4.5:1 Enhanced             |
| (>=24px / >=19px)| (WCAG 1.4.3)             | (WCAG 1.4.6)               |
+------------------+--------------------------+----------------------------+
| Non-Text UI      | 3.0:1 Minimum            | No AAA requirement         |
| (Borders, Icons) | (WCAG 1.4.11)            |                            |
+------------------+--------------------------+----------------------------+
| Focus Indicators | 3.0:1 against adjacent   | Specific area & contrast   |
| (:focus-visible) | background (WCAG 2.4.7)  | (WCAG 2.4.13)              |
+------------------+--------------------------+----------------------------+
```

### Motion and Vestibular Disorders: `prefers-reduced-motion`
When a user enables "Reduce Motion" in their OS settings (macOS, iOS, Windows, Android), the browser exposes `@media (prefers-reduced-motion: reduce)`. 

Under **WCAG 2.3.3 (Animation from Interactions - Level AAA)** and **2.2.2 (Pause, Stop, Hide - Level A)**:
- Large visual movements (parallax scrolling, 3D flips, zooming hero banners) must be disabled or replaced with gentle cross-fades.
- Essential functional motion (e.g., instant state toggles) is permissible, but non-essential decorative animations must be suppressed.

### Windows High Contrast Mode (WHCM): `forced-colors`
When a user enables High Contrast in Windows, the browser enters **Forced Colors Mode**. 
- Custom `color`, `background-color`, and `border-color` declarations are **completely overridden** by system system color tokens (`Canvas`, `CanvasText`, `Highlight`, `ButtonFace`).
- CSS `box-shadow` and `background-image` are stripped!
- Borders that were invisible (`border: 1px solid transparent`) become visible.

---

## 3. How It Works Internally

### The Relative Luminance Formula
The W3C defines contrast ratio calculation based on the relative luminance of two colors:
$$\text{Contrast Ratio} = \frac{L_1 + 0.05}{L_2 + 0.05}$$
where $L_1$ is the relative luminance of the lighter color, and $L_2$ is the relative luminance of the darker color (ranging from $0.0$ for black to $1.0$ for white).

Relative luminance is computed from sRGB components converted to linear values:
$$L = 0.2126 R + 0.7152 G + 0.0722 B$$
Because the human eye is most sensitive to green wavelengths and least sensitive to blue, pure blue (`#0000FF`) has a luminance of only $0.0722$, while pure green (`#00FF00`) has a luminance of $0.7152$. This explains why light blue text on a white background is almost impossible to read.

---

## 4. Syntax & Usage

### Media Query Declarations Matrix

```css
/* 1. Respecting Motion Preference */
@media (prefers-reduced-motion: reduce) {
  *,
  *::before,
  *::after {
    animation-duration: 0.01ms !important;
    animation-iteration-count: 1 !important;
    transition-duration: 0.01ms !important;
    scroll-behavior: auto !important;
  }
}

/* 2. Forced Colors (High Contrast Mode) Adaptation */
@media (forced-colors: active) {
  /* Use system semantic colors instead of hardcoded hex values */
  .custom-badge {
    border: 1px solid ButtonText;
    color: CanvasText;
  }
  
  /* Ensure focus rings remain visible when box-shadow is stripped */
  button:focus-visible {
    outline: 2px solid Highlight;
  }
}

/* 3. Dark / Light Scheme */
@media (prefers-color-scheme: dark) {
  body {
    background-color: #121212;
    color: #e0e0e0;
  }
}
```

---

## 5. Practical Examples

### Example 1: Basic — Non-Text Contrast Compliance (WCAG 1.4.11)
Input borders and interactive icons must maintain at least 3:1 contrast against adjacent background colors:

```html
<style>
  /* FAILING: Border #e0e0e0 on white #ffffff has ratio of 1.25:1 (FAIL) */
  .broken-input {
    border: 1px solid #e0e0e0;
    padding: 8px;
  }

  /* PASSING: Border #767676 on white #ffffff has ratio of 4.54:1 (PASSES 1.4.11) */
  .accessible-input {
    border: 1.5px solid #767676;
    padding: 8px;
    border-radius: 4px;
    background-color: #ffffff;
    color: #222222;
  }

  .accessible-input:focus-visible {
    outline: 3px solid #005a9c; /* Ratio of 7.1:1 against white */
    outline-offset: 2px;
  }
</style>

<label for="user-code">Security Verification Code</label>
<input type="text" id="user-code" class="accessible-input" required>
```

### Example 2: Real-World — Universal Motion Reduction Utility
A production-grade CSS utility that neutralizes vestibular motion triggers without breaking functionality:

```css
/* Global animation neutralization for vestibular safety */
@media (prefers-reduced-motion: reduce) {
  html {
    scroll-behavior: auto;
  }

  /* Disable parallax transforms and smooth transitions */
  .parallax-hero,
  .carousel-slide,
  .floating-element {
    transform: none !important;
    transition: opacity 0.1s ease !important;
  }

  /* Replace continuous spinning loaders with pulse or static label */
  .spinner-icon {
    animation: none !important;
  }
  
  .spinner-icon::after {
    content: " (Loading...)";
    font-size: 0.9rem;
  }
}
```

### Example 3: Edge Case — Preserving Icons in Windows High Contrast Mode
In High Contrast Mode, SVGs using hardcoded colors or CSS `currentColor` can disappear if not assigned proper system keyword tokens:

```html
<style>
  .icon-button {
    display: inline-flex;
    align-items: center;
    gap: 8px;
    padding: 8px 16px;
    background-color: #005a9c;
    color: #ffffff;
    border: 1px solid transparent; /* Becomes visible border in WHCM! */
    border-radius: 4px;
    cursor: pointer;
  }

  @media (forced-colors: active) {
    .icon-button {
      /* In WHCM, border: transparent renders in the user's High Contrast border color */
      border: 1px solid ButtonText;
    }
    
    .icon-button svg {
      /* Forces SVG fill to match system button text color */
      fill: ButtonText;
      stroke: ButtonText;
    }
  }
</style>

<button type="button" class="icon-button">
  <svg width="16" height="16" viewBox="0 0 24 24" aria-hidden="true">
    <path d="M19 13h-6v6h-2v-6H5v-2h6V5h2v6h6v2z"/>
  </svg>
  <span>Add Resource</span>
</button>
```

### Example 4: Failure Case — Broken Contrast and Motion Anti-Patterns
Common mistakes that cause visual and physical harm:

```html
<!-- BROKEN: Low-contrast grey text (Fails WCAG 1.4.3 Level AA) -->
<!-- #999999 on white #ffffff has ratio of 2.84:1 (Below 4.5:1 minimum!) -->
<p style="color: #999999; background: #ffffff;">
  Terms and conditions apply. Contact support for details.
</p>

<!-- BROKEN: Using forced-color-adjust: none indiscriminately -->
<!-- Disables Windows High Contrast Mode, blinding users who depend on it! -->
<style>
  * {
    forced-color-adjust: none !important; /* SEVERE ANTI-PATTERN! */
  }
</style>

<!-- BROKEN: Endless rapid flashing banner (Violates WCAG 2.3.1 Seizures) -->
<div class="strobe-animation">FLASH SALE 50% OFF</div>
```

### Example 5: Accessible Production-Grade — Adaptable Theme Card Component
A card component that supports light mode, dark mode, high contrast mode, and reduced motion simultaneously:

```html
<style>
  :root {
    --card-bg: #ffffff;
    --card-border: #767676;
    --text-primary: #121212;
    --link-color: #005a9c;
  }

  @media (prefers-color-scheme: dark) {
    :root {
      --card-bg: #1e1e1e;
      --card-border: #888888;
      --text-primary: #f0f0f0;
      --link-color: #64b5f6;
    }
  }

  .adaptive-card {
    background-color: var(--card-bg);
    border: 1px solid var(--card-border);
    color: var(--text-primary);
    padding: 24px;
    border-radius: 8px;
    transition: transform 0.2s ease;
  }

  .adaptive-card:hover {
    transform: translateY(-4px);
  }

  @media (prefers-reduced-motion: reduce) {
    .adaptive-card {
      transition: none;
    }
    .adaptive-card:hover {
      transform: none; /* Strip physical motion */
    }
  }

  @media (forced-colors: active) {
    .adaptive-card {
      border: 1px solid CanvasText;
      background-color: Canvas;
      color: CanvasText;
    }
    .adaptive-card a {
      color: LinkText;
    }
  }
</style>

<article class="adaptive-card">
  <h2>Scalable Architecture</h2>
  <p>Learn how modern distributed systems handle multi-region failover.</p>
  <a href="/docs/architecture">Read Technical Blueprint</a>
</article>
```

---

## 6. Best Practices

1. **Verify Text Contrast with Automated Checkers**: Always test body text against background colors using DevTools to guarantee $\ge 4.5:1$ contrast.
2. **Ensure 3:1 Contrast for UI Boundaries (1.4.11)**: Input borders, buttons, checkboxes, and icons must have at least 3:1 contrast against adjacent background colors.
3. **Respect `prefers-reduced-motion` Globally**: Disable or simplify parallax, zooms, and autoplay video for users requesting reduced motion.
4. **Never Use `forced-color-adjust: none` as a Global Fix**: High Contrast users enable WHCM for visual survival; disabling it globally renders your site unreadable.
5. **Always Use 1px Transparent Borders on Custom Controls**: Transparent borders (`border: 1px solid transparent`) are invisible in normal view, but become clearly visible borders in Windows High Contrast Mode.
6. **Never Convey Information Solely by Color**: Always pair color indicators (red/green status) with text labels or distinct icons to support color-blind users.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Severity | WCAG Success Criterion | Correction |
|:---|:---|:---|:---|
| Grey body text below 4.5:1 ratio | Critical | 1.4.3 Contrast (Minimum) (AA) | Darken text to achieve $\ge 4.5:1$ ratio |
| Light grey input borders (< 3:1 ratio) | High | 1.4.11 Non-text Contrast (AA) | Darken input border to $\ge 3:1$ against background |
| Parallax scrolling ignoring reduced motion | High | 2.3.3 Animation from Interactions (AAA)| Disable transforms under `prefers-reduced-motion` |
| Disabling High Contrast with `forced-color-adjust: none` | High | 1.4.1 Use of Color & 1.4.3 | Use semantic system colors (`CanvasText`, `Highlight`)|
| Status indicated solely by color (green/red dot) | High | 1.4.1 Use of Color (A) | Add visible text label (*"Active"* / *"Failed"*) |
| Focus ring with < 3:1 contrast against background | High | 2.4.7 Focus Visible (AA) | Use high-contrast focus outline color |

---

## 8. Performance, Accessibility, SEO & Security Notes

### Zero Performance Cost of Modern CSS Queries
Media queries like `prefers-reduced-motion` and `prefers-color-scheme` are evaluated directly in C++ engine code during CSSOM compilation. They introduce zero JavaScript execution overhead and do not trigger layout recalculations.

### Accessibility Testing in DevTools
Modern browsers allow rapid emulation of all user preference queries without touching OS settings:
1. Open DevTools (`F12`).
2. Press `Ctrl+Shift+P` (or `Cmd+Shift+P`) to open the Command Menu.
3. Type `"Rendering"` and select **Show Rendering**.
4. In the Rendering panel, find:
   - **Emulate CSS media feature prefers-reduced-motion**
   - **Emulate CSS media feature forced-colors**
   - **Emulate CSS media feature prefers-color-scheme**
   - **Emulate vision deficiencies** (Protanopia, Deuteranopia, Tritanopia, Achromatopsia).

---

## 9. Comparison / When to Use What

### Contrast Standards Comparison

| Element Type | WCAG Minimum (AA) | WCAG Enhanced (AAA) | Sample Passing Combinations |
|:---|:---:|:---:|:---|
| **Body Text (<24px)** | 4.5:1 | 7.0:1 | `#595959` on `#ffffff` (4.54:1) |
| **Large Text (>=24px)**| 3.0:1 | 4.5:1 | `#767676` on `#ffffff` (4.54:1) |
| **Input Borders** | 3.0:1 | N/A | `#767676` on `#ffffff` (4.54:1) |
| **Active Focus Ring** | 3.0:1 | 3.0:1 | `#005a9c` on `#ffffff` (7.1:1) |
| **Disabled Control** | Exempt | Exempt | N/A (Exempt from 1.4.3) |

---

## 10. Interview Questions

### Q1: What are the minimum color contrast ratios required under WCAG 2.2 Level AA for normal text, large text, and non-text UI elements?
<details>
<summary>View Answer</summary>

Under WCAG 2.2 Level AA:
1. **Normal Text** (under 24px regular or under 19px bold): **4.5:1** minimum contrast against background (WCAG 1.4.3).
2. **Large Text** (24px or larger regular, or 19px or larger bold): **3.0:1** minimum contrast (WCAG 1.4.3).
3. **Non-Text UI Elements** (user interface components like input borders, checkboxes, button boundaries, and informative icons): **3.0:1** minimum contrast against adjacent colors (WCAG 1.4.11).
</details>

### Q2: What is the purpose of the `(prefers-reduced-motion: reduce)` media query, and what types of animations must be altered?
<details>
<summary>View Answer</summary>

`prefers-reduced-motion` detects if the user has requested the operating system to minimize non-essential motion.

It is designed to protect users with **vestibular disorders** (inner ear conditions) who experience vertigo, nausea, dizziness, or migraines from onscreen motion.
Animations that must be disabled or significantly softened include:
- Large viewport translations (parallax scrolling, 3D rotations, zoom effects).
- Endless spinning loaders.
- Smooth scrolling (`scroll-behavior: smooth` should become `auto`).
Subtle opacity fades or instant transitions are permitted.
</details>

### Q3: How does Windows High Contrast Mode (WHCM) affect website styling, and how should CSS authors adapt to it?
<details>
<summary>View Answer</summary>

In Windows High Contrast Mode (detected via `@media (forced-colors: active)`):
1. The browser engine overrides author CSS colors (`color`, `background-color`, `border-color`) with high-contrast system color tokens (e.g., `CanvasText`, `Canvas`, `Highlight`, `ButtonText`).
2. `box-shadow` and `background-image` are completely removed.
3. Subtle borders styled with `border: 1px solid transparent` automatically render using the system high-contrast border color, preserving component visual boundaries.

Authors should never disable WHCM globally with `forced-color-adjust: none`, and should ensure that icons and custom controls use system color keywords or SVG `fill: currentColor`.
</details>

### Q4: Why is conveying information solely through color a violation of WCAG 1.4.1 (Use of Color)?
<details>
<summary>View Answer</summary>

Color-blind individuals (such as those with red-green deuteranopia or protanopia) and users of monochrome or high-contrast displays cannot differentiate information encoded solely by color hues.

For example, displaying an error state by turning an input border red without providing an error text message or warning icon means a color-blind user cannot tell the input failed. Color must always be paired with a redundant visual cue (text label, icon, underline, or border pattern).
</details>

### Q5: What is Relative Luminance in the W3C contrast algorithm, and why does green have a higher luminance weight than blue?
<details>
<summary>View Answer</summary>

Relative Luminance is the relative brightness of any point in a colorspace, normalized to $0.0$ for the darkest black and $1.0$ for the lightest white.

In the formula:
$$L = 0.2126 R + 0.7152 G + 0.0722 B$$
Green has a weight of $0.7152$ (over 71% of total perceived luminance), while blue has only $0.0722$ (around 7%). This reflects human physiology: the human eye's cone photoreceptors are predominantly sensitive to green and yellow-green wavelengths.
</details>

### Q6: Are disabled form inputs required to satisfy the 4.5:1 contrast ratio?
<details>
<summary>View Answer</summary>

**No**. WCAG Success Criterion 1.4.3 explicitly exempts text or components that are part of an **inactive user interface component** (such as disabled buttons or inputs). 

However, making disabled elements illegible is still bad UX; authors should aim for reasonable legibility while clearly signaling inactive state.
</details>

### Q7: Why should designers use a `1px solid transparent` border on custom buttons and cards?
<details>
<summary>View Answer</summary>

In standard browser view, a `1px solid transparent` border is completely invisible, allowing box shadows or background colors to define the card's boundary.

However, in **Windows High Contrast Mode**, `box-shadow` is stripped and background colors are replaced with solid black or white. If an element had no border, it would blend into the surrounding background, losing its shape. WHCM automatically colors transparent borders with the user's high-contrast border color, preserving the component's visual boundary.
</details>

### Q8: What does the `@media (prefers-contrast: more)` query do?
<details>
<summary>View Answer</summary>

`prefers-contrast: more` detects if a user has enabled "Increase Contrast" in their OS accessibility settings. 

It allows authors to selectively increase borders, darken body text, remove translucent overlays, and intensify focus indicators specifically for users who need heightened contrast without affecting default site aesthetics.
</details>

---

## 11. Practice Exercises

### Exercise 1: Easy — Checking a Contrast Pair
**Problem**: A website uses dark grey text `#767676` on a pure white background `#ffffff`. Does this satisfy WCAG 2.2 AA for regular 16px body text?

<details>
<summary>View Solution</summary>

**Yes**. The contrast ratio between `#767676` and `#ffffff` is **4.54:1**, which exceeds the minimum 4.5:1 threshold mandated by WCAG 1.4.3 Level AA for normal text.
</details>

### Exercise 2: Easy — Disabling Smooth Scrolling for Reduced Motion
**Problem**: Write the CSS rule to disable smooth scrolling when a user prefers reduced motion.

<details>
<summary>View Solution</summary>

```css
@media (prefers-reduced-motion: reduce) {
  html {
    scroll-behavior: auto !important;
  }
}
```
</details>

### Exercise 3: Medium — Color-Independent Form Error Indication
**Problem**: A text field turns red on error. Update the HTML to comply with WCAG 1.4.1 (Use of Color).

<details>
<summary>View Solution</summary>

```html
<div class="field-error">
  <label for="zip-code">Postal / ZIP Code</label>
  <input 
    type="text" 
    id="zip-code" 
    aria-invalid="true" 
    aria-describedby="zip-err"
    style="border: 2px solid #d4351c;"
  >
  <!-- Redundant textual error message with icon satisfies WCAG 1.4.1 -->
  <p id="zip-err" style="color: #d4351c; font-weight: bold;">
    <span aria-hidden="true">⚠ </span>Error: Enter a valid 5-digit ZIP code.
  </p>
</div>
```
</details>

### Exercise 4: Medium — Preserving Focus Rings in Forced Colors Mode
**Problem**: An external stylesheet uses `box-shadow` for focus rings:
`button:focus-visible { outline: none; box-shadow: 0 0 0 3px blue; }`.
Why does this fail in Windows High Contrast Mode, and how do you fix it?

<details>
<summary>View Solution</summary>

**Why it fails**: Windows High Contrast Mode automatically strips all CSS `box-shadow` declarations to prevent visual clutter. Because `outline: none` was set, the button has **zero visible focus ring** in WHCM.

**The Fix**: Always use standard CSS `outline` with `Highlight` keyword for forced colors:
```css
button:focus-visible {
  outline: 3px solid #005a9c;
  outline-offset: 2px;
}

@media (forced-colors: active) {
  button:focus-visible {
    outline: 3px solid Highlight;
  }
}
```
</details>

### Exercise 5: Hard — Complete Accessible Theme Switcher Architecture
**Problem**: Author the CSS architecture supporting system dark mode, high contrast mode, and reduced motion for an enterprise button component.

<details>
<summary>View Solution</summary>

```css
/* Base Button Component */
.btn-primary {
  display: inline-flex;
  align-items: center;
  justify-content: center;
  padding: 10px 20px;
  background-color: #005a9c;
  color: #ffffff;
  border: 2px solid transparent;
  border-radius: 4px;
  font-weight: bold;
  cursor: pointer;
  transition: transform 0.15s ease, background-color 0.15s ease;
}

.btn-primary:hover {
  background-color: #003e6b;
  transform: translateY(-2px);
}

.btn-primary:focus-visible {
  outline: 3px solid #ffbf47;
  outline-offset: 2px;
}

/* 1. Reduced Motion Preference */
@media (prefers-reduced-motion: reduce) {
  .btn-primary {
    transition: none;
  }
  .btn-primary:hover {
    transform: none;
  }
}

/* 2. Dark Mode Preference */
@media (prefers-color-scheme: dark) {
  .btn-primary {
    background-color: #2196f3;
    color: #000000;
  }
  .btn-primary:hover {
    background-color: #1976d2;
  }
  .btn-primary:focus-visible {
    outline-color: #ffffff;
  }
}

/* 3. Forced Colors Mode (WHCM) */
@media (forced-colors: active) {
  .btn-primary {
    border: 2px solid ButtonText;
    background-color: ButtonFace;
    color: ButtonText;
  }
  .btn-primary:focus-visible {
    outline: 3px solid Highlight;
  }
}
```
</details>

### Exercise 6: Hard — Contrast Ratio Calculation Function
**Problem**: Write a vanilla JavaScript function that accepts two hex color strings (e.g., `"#ffffff"` and `"#005a9c"`) and returns their mathematical WCAG contrast ratio formatted to two decimal places.

<details>
<summary>View Solution</summary>

```javascript
function getLuminance(hex) {
  const rgb = parseInt(hex.replace('#', ''), 16);
  const r = (rgb >> 16) & 0xff;
  const g = (rgb >> 8) & 0xff;
  const b = (rgb >> 0) & 0xff;

  const a = [r, g, b].map(v => {
    v /= 255;
    return v <= 0.03928 ? v / 12.92 : Math.pow((v + 0.055) / 1.055, 2.4);
  });

  return a[0] * 0.2126 + a[1] * 0.7152 + a[2] * 0.0722;
}

function calculateWcagContrast(hex1, hex2) {
  const lum1 = getLuminance(hex1);
  const lum2 = getLuminance(hex2);
  const brighter = Math.max(lum1, lum2);
  const darker = Math.min(lum1, lum2);
  
  const ratio = (brighter + 0.05) / (darker + 0.05);
  return ratio.toFixed(2) + ':1';
}

console.log(calculateWcagContrast('#ffffff', '#005a9c')); // Returns "7.10:1"
```
</details>

---

## 12. Summary / Cheat Sheet

- **WCAG 1.4.3 Contrast (Normal Text)**: $\ge 4.5:1$ against background.
- **WCAG 1.4.3 Contrast (Large Text)**: $\ge 3.0:1$ ($\ge 24\text{px}$ regular or $\ge 19\text{px}$ bold).
- **WCAG 1.4.11 Non-Text Contrast**: $\ge 3.0:1$ for input borders, checkboxes, button edges, and icons.
- **WCAG 1.4.1 Use of Color**: Never use color as the sole indicator of status or error.
- **`prefers-reduced-motion`**: Suppress large movement, parallax, and spinning animations for vestibular safety.
- **`forced-colors`**: Windows High Contrast Mode strips `box-shadow` and forces system colors (`Canvas`, `ButtonText`). Use `border: 1px solid transparent` on cards/buttons.
- **DevTools Emulation**: Use Chrome/Firefox Rendering tab to emulate contrast, motion, and color blindness.

---

## 13. Further Reading

- [W3C WCAG 2.2: Understanding Success Criterion 1.4.3 Contrast (Minimum)](https://www.w3.org/WAI/WCAG22/Understanding/contrast-minimum.html)
- [W3C WCAG 2.2: Understanding Success Criterion 1.4.11 Non-text Contrast](https://www.w3.org/WAI/WCAG22/Understanding/non-text-contrast.html)
- [MDN Web Docs: prefers-reduced-motion](https://developer.mozilla.org/en-US/docs/Web/CSS/@media/prefers-reduced-motion)
- [Microsoft: Styling for Windows High Contrast Mode](https://blogs.windows.com/msedgedev/2020/09/17/styling-for-windows-high-contrast-with-new-standards-for-forced-colors/)

---

[Previous: 12 - Accessible Multimedia & Captions (12-accessible-multimedia-and-captions.md)](12-accessible-multimedia-and-captions.md) | [Section Index](README.md) | [Next: 14 - Testing with axe-core & Screen Readers ->](14-testing-with-axe-and-screen-readers.md)
