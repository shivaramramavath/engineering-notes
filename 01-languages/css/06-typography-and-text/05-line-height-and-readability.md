# Line Height & Vertical Rhythm

| Metadata | Details |
| :--- | :--- |
| **Topic** | Unitless `line-height`, Half-Leading Mechanics, Inline Box Model Anatomy, Vertical Cadence, `ch` Measure & `text-box-trim` |
| **Spec Level** | CSS Inline Layout Module Level 3 / CSS Box Alignment Module Level 3 |
| **Baseline Status** | Widely Available (`line-height` Baseline 2015; `ch` unit Baseline 2017; `text-box-trim` Emerging / Chrome 133+) |
| **Browsers Tested** | Chrome 120+, Firefox 122+, Safari 17.2+, Edge 120+ |
| **Tested With** | Chromium LayoutNG, Safari WebKit, Playwright, WCAG Contrast & Spacing Tools |
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

In typography, "leading" (traditionally strips of lead placed between rows of metal type) establishes the vertical space between consecutive lines of text. In CSS, this is controlled by the `line-height` property. 

Line height is the single most critical factor in editorial reading comfort:
- **Preventing Eye Fatigue**: If `line-height` is too tight ($< 1.2$ for body copy), the eye accidentally skips or re-reads lines (double-tracking). If too loose ($> 2.0$), the eye struggles to find the start of the next line.
- **The Unitless Inheritance Trap**: Declaring `line-height` with units (`px`, `em`, `%`) on parent elements causes catastrophic typography overlap bugs in children with larger `font-size`. Only unitless numbers (`line-height: 1.5;`) inherit safely.
- **Line Length (The "Measure")**: Line height is physically bound to line length. Long lines require looser leading, while short columns need tighter leading. The CSS `ch` unit (width of the zero character `0`) provides the golden standard for bounding editorial text (`max-width: 65ch;`).
- **Eliminating Phantom Leading**: Font designers build internal leading margins directly into font metrics, creating persistent, invisible top and bottom gaps that prevent buttons, icons, and text from centering flush.

---

## 2. Core Concepts

### Unitless `line-height` vs Unit-Based Values
- **Unitless (Recommended)**: `line-height: 1.5;`. The factor itself is inherited down the DOM tree. When a child has `font-size: 32px`, its computed line-height scales dynamically to $32\text{px} \times 1.5 = 48\text{px}$.
- **Percentages / `em` (Hazardous)**: `line-height: 150%;` or `1.5em;`. The **computed pixel value** is calculated on the parent and inherited as a fixed length. If the parent has `font-size: 16px`, child elements inherit a static `24px` line-height regardless of how large their own font size is.
- **Pixels (Rigid)**: `line-height: 24px;`. Never scales with `font-size`.

### The Half-Leading Distribution
When an element's `line-height` exceeds its `font-size`:
$$\text{Leading} = \text{line-height} - \text{font-size}$$
The browser engine calculates half-leading:
$$\text{Half-Leading} = \frac{\text{Leading}}{2}$$
This half-leading space is placed equally above the font's ascent and below the font's descent, forming the inline line box.

### The Typographic Measure (`ch` Unit)
For sustained reading comfort, typographic research dictates lines should contain between **45 and 75 characters** (including spaces). 
- In CSS, `1ch` equals the advance measure of the `0` glyph in the current font.
- Setting `max-width: 65ch;` on article containers guarantees optimal reading line length across all viewport sizes.

### The Modern Frontier: `text-box-trim` (CSS Inline Layout Level 3)
Because fonts contain embedded vertical metrics (ascent and descent buffers), an element with `line-height: 1` still contains unwanted vertical whitespace above capital letters and below the baseline.
- `text-box-trim: both;` (formerly `leading-trim`): Strips away the font metric whitespace above capital letters and below the baseline.
- `text-box-edge: cap alphabetic;`: Trims from the cap-height of uppercase glyphs down to the alphabetic baseline, enabling pixel-perfect vertical centering of text inside buttons and cards.

---

## 3. How It Works Internally

```
+--------------------------------------------------------------------------+
| Anatomy of an Inline Line Box                                            |
|                                                                          |
|  +--------------------------------------------------------------------+  |
|  | [Top of Line Box]                                                  |  |
|  |   + Half-Leading (Space above)                                     |  |
|  |   +-------------------------------------------------------------+  |  |
|  |   | [Font Em-Box Ascent]                                        |  |  |
|  |   |  C a p i t a l                                              |  |  |
|  |   |----------------------------- [Baseline]                     |  |  |
|  |   | [Font Em-Box Descent]   (p, y descenders)                   |  |  |
|  |   +-------------------------------------------------------------+  |  |
|  |   + Half-Leading (Space below)                                     |  |
|  | [Bottom of Line Box]                                               |  |
|  +--------------------------------------------------------------------+  |
|                                                                          |
+--------------------------------------------------------------------------+
  Total Line Box Height = font-size + (2 * Half-Leading) = computed line-height
```

### The Inheritance Bug Diagram
```
Parent: font-size: 16px; line-height: 1.5em; (Computes to 24px)
  |
  +---> Child Heading: font-size: 40px;
        Inherits computed: line-height: 24px;  <--- DISASTER: Lines overlap!
        
Parent: font-size: 16px; line-height: 1.5;    (Computes factor 1.5)
  |
  +---> Child Heading: font-size: 40px;
        Inherits factor: line-height: 40px * 1.5 = 60px; <--- CLEAN & PERFECT!
```

---

## 4. Syntax & Usage

### Baseline & Feature Support
- **Unitless `line-height`**: Baseline Widely Available (supported in all browsers since CSS1).
- **`ch` Unit**: Baseline Widely Available (Chrome 27+, Safari 7+, Firefox 1+, Edge 12+).
- **`text-box-trim` & `text-box-edge`**: Baseline Emerging (Chrome 133+, Safari Technology Preview).

```css
/* Standard Body Text Leading */
body {
  font-family: system-ui, sans-serif;
  font-size: 1rem;      /* 16px */
  line-height: 1.6;     /* 25.6px computed */
}

/* Headings: Tighter Leading */
h1 {
  font-size: 2.5rem;    /* 40px */
  line-height: 1.15;    /* 46px computed */
}

/* Editorial Measure Constraint */
.article-body {
  max-width: 65ch;
  margin-inline: auto;
}
```

---

## 5. Practical Examples

### Example 1: Editorial Paragraph with Optimal Measure and Rhythm
```html
<article class="longform-story">
  <h2>The Architecture of Modern Typography</h2>
  <p>
    When rendering paragraphs across modern displays, line height must expand in
    proportion to column width. Narrow columns allow tight line tracking, but
    wide reading panes require open vertical breathing room to prevent double-reading.
  </p>
</article>
```

```css
.longform-story {
  max-width: 68ch; /* Enforces optimal 45–75 character measure */
  margin-inline: auto;
  padding: 2rem 1.5rem;
}

.longform-story h2 {
  font-size: clamp(1.75rem, 3vw, 2.5rem);
  line-height: 1.2;
  margin-block-end: 1rem;
  color: #0f172a;
}

.longform-story p {
  font-size: 1.125rem; /* 18px */
  line-height: 1.7;    /* 30.6px leading */
  margin-block-end: 1.5rem;
  color: #334155;
}
```

### Example 2: The Unitless Demonstration vs Inherited `em` Trap
```css
/* SAFE PATTERN: Unitless inheritance */
.safe-container {
  font-size: 16px;
  line-height: 1.5; /* Ratio inherits */
}

.safe-container h1 {
  font-size: 48px;
  /* Automatically computes: 48px * 1.5 = 72px line-height */
}

/* BROKEN PATTERN: Do NOT do this */
.broken-container {
  font-size: 16px;
  line-height: 1.5em; /* Computes to fixed 24px and locks! */
}

.broken-container h1 {
  font-size: 48px;
  /* Inherits fixed 24px line-height!
     48px letters collide and overlap inside 24px space! */
}
```

### Example 3: Vertical Rhythm Scale System
```css
:root {
  --base-size: 1rem;
  --line-unit: 1.5rem; /* 24px baseline grid unit */
}

body {
  font-size: var(--base-size);
  line-height: var(--line-unit);
}

/* Every block element snaps to multiples of 24px */
h1 {
  font-size: 2.25rem;
  line-height: calc(var(--line-unit) * 2); /* 48px */
  margin-block-end: var(--line-unit);      /* 24px */
}

h2 {
  font-size: 1.5rem;
  line-height: calc(var(--line-unit) * 1.5); /* 36px */
  margin-block-end: calc(var(--line-unit) * 0.75);
}

p {
  line-height: var(--line-unit); /* 24px */
  margin-block-end: var(--line-unit);
}
```

### Example 4: Precision Button Text Centering with Modern `text-box-trim`
```css
/* Progressive enhancement: Eliminates phantom leading inside buttons */
.clean-aligned-button {
  display: inline-flex;
  align-items: center;
  justify-content: center;
  padding: 0.75rem 1.5rem;
  font-family: system-ui, sans-serif;
  font-size: 1rem;
  font-weight: 600;
  line-height: 1;
  background-color: #2563eb;
  color: #ffffff;
  border: none;
  border-radius: 8px;
  cursor: pointer;

  /* Emerging baseline feature: trims font leading from cap to baseline */
  text-box-trim: both;
  text-box-edge: cap alphabetic;
}

.clean-aligned-button:focus-visible {
  outline: 2px solid #2563eb;
  outline-offset: 2px;
}
```

### Example 5: Responsive Leading (Dynamic Inverse Line-Height)
```css
/* As headlines grow larger on desktop, line-height must tighten */
.responsive-headline {
  font-size: clamp(2rem, 5vw, 4.5rem);
  /* At 2rem -> line-height: 1.25
     At 4.5rem -> line-height: 1.05 */
  line-height: clamp(1.05, 1.4 - 0.05 * 1vw, 1.25);
  letter-spacing: -0.03em;
}
```

---

## 6. Best Practices

1. **Always Use Unitless Values for `line-height`**: Set `line-height: 1.5;`, never `line-height: 1.5em;` or `150%`. Unitless values allow child headings to compute correct proportional line heights.
2. **Constrain Reading Columns to 45–75 Characters**: Use `max-width: 65ch;` on long-form articles. Full-width text on desktop 1920px screens severely degrades reading retention.
3. **Tighten Leading on Large Headlines**: Large display headings ($> 32\text{px}$) look disconnected with loose leading. Set heading line-height between `1.05` and `1.2`.
4. **Loosen Leading for Body Text**: Set body copy line-height between `1.5` and `1.7` to ensure clear separation between lines.
5. **Pair `line-height` with `font-size` in Typography Scales**: Always declare `line-height` alongside `font-size` in design system tokens.
6. **Honor WCAG 1.4.12 Text Spacing**: Ensure that increasing `line-height: 1.5;` via accessibility stylesheets does not clip text inside fixed-height containers.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Correction |
| :--- | :--- | :--- |
| **Using `line-height: 1.5em` on `<body>`** | Computes to a fixed pixel value (e.g., 24px) that is inherited statically by child `<h1>` and `<h2>`, causing massive text overlap. | Use unitless `line-height: 1.5;`. |
| **Fixed pixel container heights on text blocks** | Setting `height: 100px;` causes text to overflow or clip when users resize fonts. | Use `min-height` or let content determine box height. |
| **Loose leading on multiline headings** | An `h1` at 48px with `line-height: 1.6` leaves huge 28px gaps between lines, breaking visual grouping. | Set heading `line-height` between `1.1` and `1.25`. |
| **Ignoring the Measure on desktop screens** | Unconstrained paragraphs stretch across 1800px monitors (180+ characters per line), straining reader eyes. | Apply `max-width: 65ch;`. |
| **Using `line-height: 1` on body paragraphs** | Descenders (g, y) collide with ascenders (h, d) on the next line. | Reserve `line-height: 1` for single-line badges or buttons. |
| **Using negative `margin-top` to fix font alignment** | Fragile hack that breaks across different operating systems and font fallbacks. | Use flex/grid alignment or modern `text-box-trim`. |

---

## 8. Performance, Accessibility & Browser Support

### Accessibility Compliance (WCAG 2.2 AA)
- **Criterion 1.4.12 (Text Spacing)**: Requires that content can adjust to the following parameters without loss of content or functionality:
  - Line height: at least 1.5 times the font size.
  - Spacing following paragraphs: at least 2 times the font size.
  - Letter spacing: at least 0.12 times the font size.
  - Word spacing: at least 0.16 times the font size.
- **Criterion 1.4.8 (Visual Presentation - AAA)**: Line length must not exceed 80 characters or glyphs (34 for CJK).

---

## 9. Comparison / When to Use What

| Element Type | Recommended `line-height` | Reason |
| :--- | :--- | :--- |
| **Body Paragraphs** | `1.5` – `1.7` | Provides optimal breathing room for sustained multi-line reading |
| **Subheadings (h2–h4)** | `1.25` – `1.35` | Balances hierarchy with multi-line phrase coherence |
| **Display Headings (h1)** | `1.05` – `1.2` | Keeps massive words visually unified as a single typographic unit |
| **Single-Line Buttons / Badges** | `1` or `1.1` | Simplifies vertical centering in flex/grid containers |
| **Code Blocks (`<pre>`)** | `1.4` – `1.5` | Balances vertical scanning efficiency in monospaced code |

---

## 10. Interview Questions

<details>
<summary>1. Why is declaring a unitless <code>line-height: 1.5</code> fundamentally different from declaring <code>line-height: 1.5em</code> or <code>line-height: 150%</code>?</summary>

The critical difference lies in **how values are computed and inherited** down the DOM tree:
- When using **unitless** numbers (`1.5`), the raw ratio factor is inherited by child elements. Each child calculates its line height by multiplying its *own* computed `font-size` by 1.5 ($32\text{px} \times 1.5 = 48\text{px}$).
- When using **units** (`1.5em`, `150%`, `24px`), the browser computes the absolute pixel value on the parent element immediately ($16\text{px} \times 1.5 = 24\text{px}$). That static 24px pixel value is what inherits down to children. If a child element has `font-size: 40px`, it still inherits `line-height: 24px`, causing lines of text to collide and overlap.
</details>

<details>
<summary>2. Explain the concept of "half-leading" in the CSS inline formatting context.</summary>

Half-leading is the mechanism browsers use to vertically center a line of glyphs within its line box. When `line-height` is larger than the font's em-box size (`font-size`), the difference is called the "leading":
$$\text{Leading} = \text{line-height} - \text{font-size}$$
The browser divides this leading in half. One half is added to the top of the font's ascent metric, and the other half is added to the bottom of the descent metric. The sum of the ascent, descent, and both half-leadings equals the total height of the generated line box.
</details>

<details>
<summary>3. What is the typographic "measure", and why is the CSS <code>ch</code> unit the ideal unit for setting it?</summary>

In typography, the "measure" is the length of a line of text. Reading psychology demonstrates that optimal reading comprehension occurs when lines contain between 45 and 75 characters. If lines are too long, readers lose their place when scanning back to the left edge; if too short, constant eye jumping disrupts rhythm.

The `ch` unit represents the advance width of the character `0` (zero) in the current font. Because digits in most typefaces approximate the average width of Latin alphabet characters, setting `max-width: 65ch;` directly enforces an optimal character count independent of viewport size or screen resolution.
</details>

<details>
<summary>4. What problem does the modern <code>text-box-trim</code> (formerly <code>leading-trim</code>) specification solve?</summary>

Font files have internal metrics (ascent, descent, capital height) that include built-in whitespace above uppercase letters and below the baseline. Because of this baked-in whitespace, elements like buttons, badges, and card titles never align flush against containers when centered with CSS padding or flexbox. 

`text-box-trim: both; text-box-edge: cap alphabetic;` instructs the browser layout engine to strip the font's internal leading above the capital letter height and below the baseline, allowing typography to behave like true geometric rectangles.
</details>

<details>
<summary>5. How does `line-height` affect the height of block-level containers versus inline elements?</summary>

- On **inline elements** (`<span>`, `<a>`), `line-height` determines the height of the inline line box, but does **not** change the element's actual background or border clipping box (which strictly wraps the font's em-box/content area).
- On **block-level containers** (`<p>`, `<div>`), `line-height` determines the minimum height of every line box inside the block, directly establishing the calculated total height of the block container.
</details>

<details>
<summary>6. Why do headlines generally require smaller unitless `line-height` values than body copy?</summary>

At small font sizes (16px), characters are compact and lines require generous proportional leading (1.5 to 1.7) so the eye can easily track across sentences without accidental jumping. 

At large font sizes (36px–64px), characters are massive and the absolute physical gap between lines becomes enormous if a 1.5 ratio is maintained ($48\text{px} \times 1.5 = 72\text{px}$, creating a 24px void). A tight leading of 1.1 to 1.2 keeps multi-line headlines unified as a cohesive visual group.
</details>

<details>
<summary>7. What happens when `line-height` is smaller than `font-size` (e.g., `line-height: 0.8`)?</summary>

The calculated leading becomes **negative**. The browser calculates negative half-leading, effectively subtracting space from above the ascent and below the descent. As a result, consecutive lines of text will visually overlap each other, with ascenders colliding with descenders of the line above.
</details>

<details>
<summary>8. How do you satisfy WCAG 2.2 Criterion 1.4.12 regarding text spacing in design systems?</summary>

Designers and developers must ensure that layout containers do not use fixed, rigid pixel heights (`height: 40px; overflow: hidden;`) that clip text when a user activates custom user-agent stylesheets (which increase `line-height` to 1.5, paragraph spacing to 2x, and letter spacing to 0.12em). Always use `min-height` or auto-expanding flex/grid layouts.
</details>

---

## 11. Practice Exercises

### Exercise 1: Build an Editorial Article with Optimal Measure and Leading
**Goal**: Style a longform blog post with 65ch measure, 1.65 body leading, and a tightly tracked 1.15 headline.

<details>
<summary>View Solution</summary>

```css
.editorial-post {
  max-width: 65ch;
  margin-inline: auto;
  padding: 3rem 1.5rem;
}

.editorial-post h1 {
  font-family: system-ui, sans-serif;
  font-size: clamp(2.25rem, 5vw, 3.5rem);
  line-height: 1.15;
  letter-spacing: -0.025em;
  margin-block-end: 1.5rem;
}

.editorial-post p {
  font-family: Georgia, serif;
  font-size: 1.125rem;
  line-height: 1.65;
  margin-block-end: 1.5rem;
  color: #1e293b;
}
```
</details>

### Exercise 2: Fix the Unit-Based Line-Height Bug
**Goal**: Refactor a broken stylesheet where headings inside a banner overlap because of unit-based inheritance.

<details>
<summary>View Solution</summary>

```css
/* BEFORE (BROKEN):
.banner { font-size: 14px; line-height: 20px; }
.banner h1 { font-size: 40px; } -> Overlaps!
*/

/* AFTER (FIXED): */
.banner {
  font-size: 0.875rem;
  line-height: 1.5; /* Unitless ratio inherits properly */
}

.banner h1 {
  font-size: 2.5rem;
  line-height: 1.2; /* Explicit heading leading */
}
```
</details>

### Exercise 3: Modern Button with Trimmed Vertical Metrics
**Goal**: Create a button that centers text visually without phantom leading offsets.

<details>
<summary>View Solution</summary>

```css
.flush-button {
  display: inline-flex;
  align-items: center;
  justify-content: center;
  padding: 0.625rem 1.25rem;
  font-family: system-ui, sans-serif;
  font-size: 0.9375rem;
  font-weight: 600;
  line-height: 1;
  background-color: #0f172a;
  color: #ffffff;
  border-radius: 6px;
  border: none;
  cursor: pointer;

  /* Progressive enhancement for browsers supporting text-box-trim */
  text-box-trim: both;
  text-box-edge: cap alphabetic;
}

.flush-button:focus-visible {
  outline: 2px solid #2563eb;
  outline-offset: 2px;
}
```
</details>

### Exercise 4: Construct a Baseline Rhythm Utility Class
**Goal**: Build a utility system based on an 8px grid (24px base line unit) ensuring elements remain on the vertical grid.

<details>
<summary>View Solution</summary>

```css
:root {
  --grid-unit: 8px;
  --line-height-base: calc(var(--grid-unit) * 3); /* 24px */
}

.rhythm-p {
  font-size: 1rem;
  line-height: var(--line-height-base);
  margin-block-end: var(--line-height-base);
}

.rhythm-h2 {
  font-size: 1.5rem;
  line-height: calc(var(--line-height-base) * 1.5); /* 36px */
  margin-block-end: calc(var(--grid-unit) * 2);     /* 16px */
}
```
</details>

### Exercise 5: Inverse Fluid Line-Height
**Goal**: Write a CSS rule where line-height decreases automatically as the viewport widens and the headline grows.

<details>
<summary>View Solution</summary>

```css
.fluid-hero-title {
  font-size: clamp(2rem, 6vw, 4.5rem);
  /* Scales line-height inversely from 1.3 on mobile to 1.1 on desktop */
  line-height: clamp(1.1, 1.35 - 0.2 * (1vw), 1.3);
  letter-spacing: -0.03em;
}
```
</details>

### Exercise 6: Verify Layout Resilience under WCAG 1.4.12
**Goal**: Design a card layout that expands vertically when custom text-spacing styles are applied without clipping.

<details>
<summary>View Solution</summary>

```css
.resilient-card {
  display: flex;
  flex-direction: column;
  min-height: 200px; /* Never lock to fixed height */
  padding: 1.5rem;
  border: 1px solid #cbd5e1;
  border-radius: 8px;
  background-color: #ffffff;
}

.resilient-card p {
  line-height: 1.5;
  margin-block-end: 1rem;
}
```
</details>

---

## 12. Summary / Cheat Sheet

| Property / Concept | Syntax / Value | Description |
| :--- | :--- | :--- |
| **Unitless `line-height`** | `line-height: 1.5;` | Safe inheritance; child multiplies its own `font-size` |
| **Editorial Measure** | `max-width: 65ch;` | Constrains line length to optimal 45–75 characters |
| **Headline Leading** | `line-height: 1.1 – 1.25;` | Keeps large display words visually unified |
| **Body Copy Leading** | `line-height: 1.5 – 1.7;` | Prevents eye fatigue and accidental line-skipping |
| **Half-Leading** | `(line-height - font-size) / 2` | Browser space added above and below em-box |
| **`text-box-trim`** | `text-box-trim: both;` | Strips font metric whitespace for flush centering |

---

## 13. Further Reading
- [W3C CSS Inline Layout Module Level 3](https://www.w3.org/TR/css-inline-3/)
- [MDN Web Docs: line-height](https://developer.mozilla.org/en-US/docs/Web/CSS/line-height)
- [web.dev: CSS text-box-trim](https://web.dev/blog/text-box-trim)
- [Butterick's Practical Typography: Line Length](https://practicaltypography.com/line-length.html)
- [W3C WCAG 2.2: Understanding Success Criterion 1.4.12 (Text Spacing)](https://www.w3.org/WAI/WCAG22/Understanding/text-spacing.html)

---

[Previous: 04-text-properties-and-spacing.md](04-text-properties-and-spacing.md) | [Module README](README.md) | [Next: 06-text-wrap-and-hyphenation.md](06-text-wrap-and-hyphenation.md)
