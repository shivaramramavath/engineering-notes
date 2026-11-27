# Font Properties & Shorthand Engine

| Metadata | Details |
| :--- | :--- |
| **Topic** | Font Families, Font Sizing, Weight Scales, Stretch, Style, Shorthand Engine & Font Matching |
| **Spec Level** | CSS Fonts Module Level 4 / CSS Fonts Module Level 5 |
| **Baseline Status** | Widely Available (`font-family`, `font-size`, `font-weight`, `font` Baseline 2015; Variable font weights Baseline 2020) |
| **Browsers Tested** | Chrome 120+, Firefox 122+, Safari 17.2+, Edge 120+ |
| **Tested With** | W3C CSS Validator, Chromium LayoutNG, Core Web Vitals tooling |
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

Text rendering in CSS is driven by the browser's font selection and shaping engine. The properties controlling fonts (`font-family`, `font-size`, `font-weight`, `font-style`, `font-stretch`, `font-variant`) instruct the layout engine how to query system and web-loaded font faces, retrieve glyph contours, compute em-square scales, and map characters to visual representations.

Mastering CSS font properties is critical because:
- **Visual Hierarchy**: Typography establishes readability, tone, brand identity, and content hierarchy.
- **Rendering Performance**: Choosing between system font stacks and web fonts determines initial page render latency, Cumulative Layout Shift (CLS), and network bandwidth.
- **Shorthand Syntax Pitfalls**: The `font` shorthand property has strict parsing requirements (mandatory `font-size` and `font-family` in a specific order) and silently resets all other omitted font properties to their initial values, causing unexpected UI bugs if misunderstood.
- **Modern Baseline Features**: Font Level 4 introduces continuous numeric `font-weight` (1 to 1000) and `font-stretch` percentages for variable fonts, replacing legacy discrete keyword limits.

---

## 2. Core Concepts

### `font-family` & Generic Font Families
`font-family` accepts a comma-separated prioritized list of font names (the "font stack"). The browser iterates through the stack from left to right, rendering glyphs from the first font available on the host machine or downloaded via `@font-face`. Every font stack must conclude with a generic family keyword:
- `sans-serif`: Proportional typeface without serifs (e.g., Arial, Helvetica, system UI).
- `serif`: Proportional typeface with serifs (e.g., Georgia, Times New Roman).
- `monospace`: Monospaced typeface where all glyphs share identical advance width (e.g., Courier, Consolas, Fira Code).
- `system-ui`: The host operating system's native UI font (e.g., San Francisco on macOS/iOS, Segoe UI on Windows, Roboto on Android).
- Modern Level 4 generics: `ui-sans-serif`, `ui-serif`, `ui-monospace`, `ui-rounded`.

### `font-size` & Scaling Modes
`font-size` defines the height of the font's em-box (not the visual cap height of capital letters):
- **Absolute Keywords**: `xx-small`, `x-small`, `small`, `medium` (default, typically 16px), `large`, `x-large`, `xx-large`.
- **Length Units**: Absolute units (`px`, `pt`) or relative units (`rem`, `em`, `ch`, `ex`, `cap`, `ic`).
- **Percentage Units**: `%` relative to the parent element's computed `font-size`.
- **Fluid Math**: `clamp(min, preferred, max)` dynamically interpolating between screen sizes.

### `font-weight` (Discrete vs Continuous)
`font-weight` controls stroke thickness:
- **Keywords**: `normal` (400), `bold` (700), `lighter`, `bolder` (computed relative to parent).
- **Numeric Values**: Traditionally 100, 200, 300, 400, 500, 600, 700, 800, 900.
- **Continuous Range (Baseline 2020)**: Any real number from `1` to `1000` (e.g., `font-weight: 540;`), supported by variable fonts.

### `font-style` & Oblique Angles
- `normal`: Roman upright glyphs.
- `italic`: Specifically designed cursive glyphs designed for italic styling.
- `oblique`: Mechanically slanted upright glyphs. CSS Fonts Level 4 allows specifying angles: `font-style: oblique 14deg;`.

### `font-stretch` (Font Width)
Controls glyph expansion or condensation:
- Keywords: `normal`, `condensed`, `semi-condensed`, `expanded`, etc.
- Percentage values: `50%` to `200%` (e.g., `font-stretch: 125%;`).

### The `font` Shorthand Mechanics
The `font` shorthand combines multiple properties into one declaration. It has strict syntactic rules:
```css
/* Syntax: [font-style font-variant font-weight font-stretch]? font-size [/line-height]? font-family */
selector {
  font: italic small-caps 600 16px/1.5 "Helvetica Neue", sans-serif;
}
```
**Critical Rule**: `font-size` and `font-family` are **mandatory** and must appear in that specific order at the end. Any omitted sub-properties (`font-style`, `font-weight`, `line-height`, etc.) are automatically reset to their initial values.

---

## 3. How It Works Internally

```
+----------------------------------------------------------------------+
| Glyph Box Anatomy                                                    |
|                                                                      |
|  [Top of Em-Square] -----------------------------------------------  |
|                     ^                                                |
|                     | Ascent                                         |
|  [Cap-Height]       |   +---+                                        |
|                     |   | H |      +---+    (x-height)               |
|  [x-Height]         |   | H |      | x |                             |
|  [Baseline] --------v---+---+------+-+-+---------------------------- |
|                     ^                |                               |
|                     | Descent        | (descender)                   |
|  [Bottom of Em-Box]-v----------------+------------------------------ |
|                                                                      |
+----------------------------------------------------------------------+
  Total Em-Box Height = Ascent + Descent = Computed `font-size`
```

### The Font Matching Algorithm
When the browser encounters text styled with `font-family: A, B, sans-serif`:
1. It queries the local system and loaded `@font-face` cache for font `A`.
2. For every individual character in the text, it checks whether font `A` contains a matching glyph.
3. If font `A` is missing or lacks a glyph for a specific Unicode code point (e.g., an emoji or Japanese kanji in an English font), the browser falls back to font `B` for that specific glyph.
4. If font `B` lacks it, the browser queries the generic family (`sans-serif`), asking the operating system for a default fallback font (e.g., Apple Color Emoji, Segoe UI Emoji).

### Font Synthesis (`font-synthesis`)
If a font family only has a Regular 400 weight loaded, and CSS specifies `font-weight: bold` (700) or `font-style: italic`, the browser will automatically synthesize "faux bold" (by geometrically stroking glyph outlines) or "faux italic" (by shearing glyphs sideways). Faux fonts distort typographic proportions and introduce visual blurring. You can disable this behavior with:
```css
body {
  font-synthesis: none; /* Disables faux bold, faux italic, and faux small-caps */
}
```

---

## 4. Syntax & Usage

### Baseline & Feature Support
- **Core Font Properties (`family`, `size`, `weight`, `style`)**: Baseline Widely Available.
- **Continuous `font-weight: 1-1000`**: Baseline Widely Available (Chrome 62+, Safari 11+, Firefox 62+).
- **`system-ui` / Modern Generics**: Baseline Widely Available (Chrome 56+, Safari 11+, Firefox 92+).
- **`font-synthesis: none`**: Baseline Widely Available (Chrome 97+, Safari 9+, Firefox 34+).

```css
/* Standard Modern Declaration */
.heading-primary {
  font-family: "Inter", system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
  font-size: 2.25rem;
  font-weight: 650;
  line-height: 1.2;
  letter-spacing: -0.02em;
}

/* Equivalent Font Shorthand */
.heading-shorthand {
  font: 650 2.25rem/1.2 "Inter", system-ui, sans-serif;
  letter-spacing: -0.02em;
}
```

---

## 5. Practical Examples

### Example 1: The Modern System Font Stack (Zero Network Overhead)
```css
/* Native System UI Font Stack: Instant rendering, zero bytes transferred, zero CLS */
:root {
  --font-sans: system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI",
               Roboto, Oxygen-Sans, Ubuntu, Cantarell, "Helvetica Neue", sans-serif;
  --font-serif: Charter, "Bitstream Charter", "Sitka Text", Cambria, serif;
  --font-mono: ui-monospace, "Cascadia Code", "Source Code Pro", Menlo, Consolas,
               "DejaVu Sans Mono", monospace;
}

body {
  font-family: var(--font-sans);
  font-size: 1rem; /* 16px default */
  line-height: 1.5;
  color: #1e293b;
  -webkit-font-smoothing: antialiased;
  -moz-osx-font-smoothing: grayscale;
}

code, kbd, samp, pre {
  font-family: var(--font-mono);
  font-size: 0.875em;
}
```

### Example 2: Modular Typographic Sizing Scale Using `rem`
```css
:root {
  /* Major Second (1.125) or Minor Third (1.200) Modular Scale */
  --font-size-xs: 0.75rem;    /* 12px */
  --font-size-sm: 0.875rem;   /* 14px */
  --font-size-base: 1rem;     /* 16px */
  --font-size-md: 1.125rem;   /* 18px */
  --font-size-lg: 1.25rem;    /* 20px */
  --font-size-xl: 1.5rem;     /* 24px */
  --font-size-2xl: 1.875rem;  /* 30px */
  --font-size-3xl: 2.25rem;   /* 36px */
  --font-size-4xl: 3rem;      /* 48px */
}

h1 { font-size: var(--font-size-4xl); font-weight: 800; line-height: 1.1; }
h2 { font-size: var(--font-size-3xl); font-weight: 700; line-height: 1.2; }
h3 { font-size: var(--font-size-2xl); font-weight: 600; line-height: 1.25; }
h4 { font-size: var(--font-size-xl);  font-weight: 600; line-height: 1.3; }
p  { font-size: var(--font-size-base); font-weight: 400; line-height: 1.6; }
small { font-size: var(--font-size-xs); line-height: 1.4; }
```

### Example 3: Variable Font Precision Weight and Stretch
```css
/* Utilizes variable font axes to fine-tune visual weight without stepped jumping */
@import url('https://fonts.googleapis.com/css2?family=Roboto+Flex:wdth,wght@75..100,100..900&display=swap');

.hero-title {
  font-family: "Roboto Flex", sans-serif;
  font-size: clamp(2rem, 5vw, 4.5rem);
  font-weight: 850;
  font-stretch: 90%;
  line-height: 1.05;
  letter-spacing: -0.03em;
}

.hero-subtitle {
  font-family: "Roboto Flex", sans-serif;
  font-size: 1.25rem;
  font-weight: 350;
  font-stretch: 100%;
  line-height: 1.5;
  color: #475569;
}
```

### Example 4: The `font` Shorthand Reset Trap
```css
/* Bug demonstration: Unintended property resets */
.button-base {
  font-weight: 700;
  font-style: italic;
  font-variant: small-caps;
}

/* DANGER: This shorthand resets font-style and font-variant to 'normal'! */
.button-primary {
  font: 1rem/1.5 sans-serif;
  /* font-weight becomes normal (400) */
  /* font-style becomes normal */
  /* font-variant becomes normal */
}

/* CORRECT FIX: Declare explicit longhands or supply all values to shorthand */
.button-primary-fixed {
  font: italic small-caps 700 1rem/1.5 sans-serif;
}
```

### Example 5: Disabling Faux Font Synthesis for Clean Typography
```css
/* Protect brand typography from browser faux-bold and faux-italic distortions */
body {
  font-family: "CustomBrandFont", system-ui, sans-serif;
  font-synthesis-weight: none;
  font-synthesis-style: none;
  font-synthesis-small-caps: none;
}

/* If a 700 weight is missing, the browser will NOT smear the 400 glyphs;
   it will display genuine 400 weight or fall back cleanly to a system font. */
strong, b {
  font-weight: 700;
}
```

---

## 6. Best Practices

1. **Always End Font Stacks with a Generic Family**: Every `font-family` declaration must end with `sans-serif`, `serif`, `monospace`, or `system-ui` to guarantee a predictable OS fallback.
2. **Quote Font Names Containing Spaces or Special Characters**: Fonts such as `"Helvetica Neue"`, `"Courier New"`, or `"Open Sans"` must be quoted. Standard keyword generics (`sans-serif`) must **never** be quoted.
3. **Use Relative `rem` for `font-size` Instead of `px`**: Respect the user's browser default font size settings (WCAG 1.4.4 Resize Text). A root `1rem` scales automatically when visually impaired users increase default browser font size.
4. **Be Vigilant with the `font` Shorthand**: Remember that omitted properties are reset to defaults. If you only want to change size and line-height, use `font-size` and `line-height` longhands.
5. **Disable Faux Synthesis with `font-synthesis: none`**: Prevent browsers from smearing upright fonts into pseudo-italics or rendering muddy faux bold.
6. **Pair `font-family` with Appropriate `letter-spacing`**: Tighter letter spacing (`-0.02em` to `-0.03em`) improves large headings; slightly looser tracking (`0.01em` to `0.02em`) enhances readability on small uppercase labels.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Correction |
| :--- | :--- | :--- |
| **Quoting generic families (`"sans-serif"`)** | Quotes cause the browser to search for an exact font named "sans-serif" instead of the generic family keyword. | Write `font-family: Arial, sans-serif;` without quotes around `sans-serif`. |
| **Omitting `font-family` in `font` shorthand** | Syntax error. The entire declaration is invalidated and discarded by the browser. | Both `font-size` and `font-family` are strictly mandatory in the shorthand. |
| **Setting `font-size: 16px` on `<html>`** | Overrides user browser accessibility preferences, locking font size against user zoom configurations. | Leave `html` font size at default or use `html { font-size: 100%; }`. |
| **Accidental property reset with `font` shorthand** | Calling `font: 14px sans-serif;` clears previously set `font-weight: bold;` back to `normal`. | Use longhand `font-size: 14px;` or include `font: bold 14px sans-serif;`. |
| **Using `font-weight: 600` on fonts without semi-bold** | Browser synthesizes faux bold or snaps down to 400 depending on browser engine algorithm. | Ensure the font file supports 600 or use a variable font. |
| **Relying on physical `px` for mobile typography** | Fails to adapt to different screen densities and ignores operating system text scaling. | Use `rem` and fluid `clamp()` functions. |

---

## 8. Performance, Accessibility & Browser Support

### Accessibility Compliance (WCAG 2.2 AA)
- **Criterion 1.4.4 (Resize Text)**: Text must be resizable up to 200% without loss of content or functionality. Using `rem` ensures that browser zoom adjustments propagate proportionally.
- **Criterion 1.4.12 (Text Spacing)**: Content must remain fully legible when users increase line height to 1.5, letter spacing to 0.12em, and word spacing to 0.16em. Never lock containers to fixed pixel heights that clip text when spacing expands.

### Rendering Engine Sub-pixel Antialiasing
- `-webkit-font-smoothing: antialiased;`: Switches macOS rendering from subpixel antialiasing to grayscale antialiasing. Produces crisper, thinner text on retina displays, especially with light text on dark backgrounds.
- `-moz-osx-font-smoothing: grayscale;`: Firefox equivalent on macOS.

---

## 9. Comparison / When to Use What

| Font Approach | Network Overhead | Visual Consistency | Performance / Speed | Best For |
| :--- | :--- | :--- | :--- | :--- |
| **System Font Stack** | **0 KB** (Zero downloads) | Varies per OS (San Francisco, Segoe UI, Roboto) | Instant (0ms FOUT/FOIT) | Dashboards, dev tools, high-speed apps |
| **Standard Web Font (WOFF2)** | 20–80 KB per weight | 100% consistent across all platforms | Requires network fetch (FOIT risk) | Unique brand identity, marketing sites |
| **Variable Web Font (WOFF2)** | 80–150 KB (single file) | 100% consistent, infinite weights/styles | Single request for all styles | Modern design systems with diverse typography |

---

## 10. Interview Questions

<details>
<summary>1. What are the mandatory sub-properties of the CSS <code>font</code> shorthand, and what happens to omitted properties?</summary>

In the `font` shorthand specification, `font-size` and `font-family` are strictly mandatory and must appear in that exact order at the end of the declaration (with an optional `/line-height` immediately following `font-size`). If either is omitted, the entire declaration is syntactically invalid and discarded. 

Any other font sub-properties not explicitly specified (`font-style`, `font-variant`, `font-weight`, `font-stretch`, `line-height`) are automatically reset to their initial default values (`normal`, `normal`, `normal`, `normal`, `normal`).
</details>

<details>
<summary>2. Why is setting <code>font-size: 62.5%</code> on the root <code>&lt;html&gt;</code> element considered a common pattern, and what is its trade-off?</summary>

Most desktop browsers default to a 16px base font size. Setting `html { font-size: 62.5%; }` calculates $16\text{px} \times 0.625 = 10\text{px}$. This enables developers to calculate `rem` values mentally: $1.6\text{rem} = 16\text{px}$, $2.4\text{rem} = 24\text{px}$, while still honoring user browser font size preferences (if a user changes default to 20px, $1\text{rem}$ becomes 12.5px). 

The trade-off is that third-party UI libraries or web components assuming a default 16px root font size will render significantly smaller (10px) unless explicitly isolated or adjusted.
</details>

<details>
<summary>3. What is the difference between <code>font-style: italic</code> and <code>font-style: oblique</code>?</summary>

`font-style: italic` requests a true cursive italic typeface designed by the type designer, featuring distinct glyph anatomy (e.g., a single-story 'a' instead of a double-story 'a'). `font-style: oblique` requests a slanted Roman typeface where the standard upright glyphs are mechanically angled. In CSS Fonts Level 4, `oblique` accepts an optional angle parameter (e.g., `font-style: oblique 12deg;`). If a font family lacks an italic face, browsers traditionally synthesize faux-oblique for both declarations.
</details>

<details>
<summary>4. How does the browser font-matching algorithm handle text containing characters from multiple language scripts?</summary>

The browser executes font matching on a per-glyph (per-code point) basis. When evaluating `font-family: "Inter", "Noto Sans JP", sans-serif;`, the browser queries "Inter" for Latin characters. When it encounters Japanese Kanji or Hiragana characters not present in Inter's glyph table, it traverses down the font stack to "Noto Sans JP", rendering Japanese characters seamlessly without dropping Latin styling.
</details>

<details>
<summary>5. What is "faux bold" and "faux italic", and how can developers prevent them?</summary>

When CSS requests `font-weight: 700` or `font-style: italic` but the loaded font file only contains a Regular 400 upright face, the browser layout engine synthesizes the missing style by artificially expanding glyph outlines (faux bold) or mathematically shearing glyphs sideways (faux italic). This ruins typographic proportions, degrades kerning, and creates visual blurriness. It can be prevented using `font-synthesis: none;` or `font-synthesis-weight: none; font-synthesis-style: none;`.
</details>

<details>
<summary>6. Why is quoting generic font family names like <code>"monospace"</code> a syntax error?</summary>

Generic font family keywords (`serif`, `sans-serif`, `monospace`, `system-ui`) are reserved CSS identifiers. When placed inside quotes (`"monospace"`), the CSS parser treats the value as a literal font name rather than a generic keyword. The browser will look for an installed font specifically named "monospace" instead of querying the operating system's default monospaced engine, breaking the fallback chain.
</details>

<details>
<summary>7. What is the difference between <code>em</code> and <code>rem</code> when applied to <code>font-size</code> versus when applied to <code>padding</code>?</summary>

For `font-size`:
- `1rem` equals the computed `font-size` of the root element (`<html>`).
- `1em` equals the computed `font-size` of the **parent** element (which can compound recursively).

For box model properties (`padding`, `margin`, `width`):
- `1rem` always equals the computed root `font-size`.
- `1em` equals the computed `font-size` of the **current element itself**.
</details>

<details>
<summary>8. How does CSS Fonts Level 4 expand numeric <code>font-weight</code> support?</summary>

In Fonts Level 3, `font-weight` only accepted discrete increments of 100 ($100, 200, \dots, 900$). Fonts Level 4 allows any floating-point number between `1` and `1000` (e.g., `font-weight: 425;`). When used with OpenType Variable Fonts containing a `wght` axis, the browser renders the exact interpolated stroke weight without quantization.
</details>

---

## 11. Practice Exercises

### Exercise 1: Build a Bulletproof System Font Stack
**Goal**: Configure a typography stylesheet using modern system fonts that loads instantly with zero network requests.

<details>
<summary>View Solution</summary>

```css
:root {
  --system-ui: system-ui, -apple-system, BlinkMacSystemFont, 'Segoe UI',
               Roboto, Helvetica, Arial, sans-serif, 'Apple Color Emoji',
               'Segoe UI Emoji', 'Segoe UI Symbol';
  --system-mono: ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas,
                 'Liberation Mono', 'Courier New', monospace;
}

body {
  font-family: var(--system-ui);
  font-size: 1rem;
  line-height: 1.5;
  color: #0f172a;
}

pre, code {
  font-family: var(--system-mono);
  font-size: 0.875em;
}
```
</details>

### Exercise 2: Master the `font` Shorthand Syntax
**Goal**: Write a single `font` shorthand rule that sets bold weight, italic style, 18px size, 1.4 line-height, and Georgia font fallback.

<details>
<summary>View Solution</summary>

```css
.card-quote {
  /* [style] [weight] [size]/[line-height] [family] */
  font: italic 700 1.125rem/1.4 Georgia, serif;
}
```
</details>

### Exercise 3: Construct a Fluid Typography Heading with `clamp()`
**Goal**: Create an `h1` that scales smoothly from 32px (at 360px viewport) to 64px (at 1200px viewport).

<details>
<summary>View Solution</summary>

```css
/*
  Slope = (64 - 32) / (1200 - 360) = 32 / 840 = 0.038095 (3.81vw)
  Intercept = 32 - (360 * 0.038095) = 32 - 13.71 = 18.29px (1.143rem)
*/
h1.fluid-hero {
  font-size: clamp(2rem, 1.143rem + 3.81vw, 4rem);
  font-weight: 800;
  line-height: 1.1;
  letter-spacing: -0.025em;
}
```
</details>

### Exercise 4: Disable Faux Bold and Faux Italic
**Goal**: Enforce clean typography by forbidding browser-synthesized bold and italic styles across all headings and body text.

<details>
<summary>View Solution</summary>

```css
html {
  font-synthesis-weight: none;
  font-synthesis-style: none;
  font-synthesis-small-caps: none;
}
```
</details>

### Exercise 5: Continuous Variable Font Weight Transitions
**Goal**: Animate a button's `font-weight` continuously from 400 to 700 on hover without stepped visual jumps.

<details>
<summary>View Solution</summary>

```css
.variable-btn {
  font-family: "Roboto Flex", system-ui, sans-serif;
  font-size: 1rem;
  font-weight: 400;
  padding: 0.75rem 1.5rem;
  background-color: #2563eb;
  color: #ffffff;
  border: none;
  border-radius: 6px;
  cursor: pointer;
  transition: font-weight 0.2s ease, background-color 0.2s ease;
}

.variable-btn:hover {
  font-weight: 650;
  background-color: #1d4ed8;
}

.variable-btn:focus-visible {
  outline: 2px solid #2563eb;
  outline-offset: 2px;
}
```
</details>

### Exercise 6: Optical Sizing Integration
**Goal**: Configure `font-optical-sizing: auto` to automatically optimize glyph proportions between large headings and micro-copy.

<details>
<summary>View Solution</summary>

```css
.dynamic-text-container {
  font-family: "Fraunces", serif;
  font-optical-sizing: auto;
}

.dynamic-text-container .lead-title {
  font-size: 3.5rem; /* Browser automatically selects display optical size */
}

.dynamic-text-container .footnote {
  font-size: 0.75rem; /* Browser automatically selects caption optical size */
}
```
</details>

---

## 12. Summary / Cheat Sheet

| Property | Values | Example |
| :--- | :--- | :--- |
| `font-family` | `<family-name> \| <generic-name>` | `font-family: "Inter", system-ui, sans-serif;` |
| `font-size` | `<length> \| <percentage> \| <keyword>` | `font-size: clamp(1rem, 2vw, 1.5rem);` |
| `font-weight` | `1-1000 \| normal \| bold \| bolder` | `font-weight: 600;` |
| `font-style` | `normal \| italic \| oblique [<angle>]` | `font-style: oblique 10deg;` |
| `font-stretch` | `<percentage> \| normal \| condensed` | `font-stretch: 110%;` |
| `font-synthesis` | `none \| weight \| style \| small-caps` | `font-synthesis: none;` |
| `font` (shorthand) | `[style weight]? size[/line-height] family` | `font: 600 1rem/1.5 system-ui;` |

---

## 13. Further Reading
- [W3C CSS Fonts Module Level 4](https://www.w3.org/TR/css-fonts-4/)
- [MDN Web Docs: font-family](https://developer.mozilla.org/en-US/docs/Web/CSS/font-family)
- [MDN Web Docs: font Shorthand](https://developer.mozilla.org/en-US/docs/Web/CSS/font)
- [web.dev: Modern Font Best Practices](https://web.dev/learn/css/typography/)
- [Modern Font Stacks](https://modernfontstacks.com/)

---

[Previous: Module 05 README](file:///e:/Learning/engineering-notes/01-languages/css/05-responsive-design/README.md) | [Module README](README.md) | [Next: 02-web-fonts-and-font-display.md](02-web-fonts-and-font-display.md)
