# Text Properties & Spacing

| Metadata | Details |
| :--- | :--- |
| **Topic** | Text Alignment, Text Decoration Sub-properties, Transformations, Letter/Word Spacing, OpenType Features (`tabular-nums`, Ligatures) |
| **Spec Level** | CSS Text Module Level 3 / CSS Text Decoration Module Level 4 / CSS Fonts Module Level 4 |
| **Baseline Status** | Widely Available (`text-align: start/end`, `text-decoration-*` Baseline 2020; `text-underline-offset` Baseline 2020; `font-variant-numeric` Baseline 2018) |
| **Browsers Tested** | Chrome 120+, Firefox 122+, Safari 17.2+, Edge 120+ |
| **Tested With** | W3C CSS Validator, Chromium LayoutNG, Safari WebKit |
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

While font properties (`font-size`, `font-family`, `font-weight`) configure the glyphs and typeface contours, **text properties** govern how those glyphs are positioned, decorated, spaced, transformed, and formatted along horizontal and vertical lines.

Text properties are foundational to professional interface design:
- **Optical Readability**: `letter-spacing` (tracking) and `word-spacing` prevent dense character crowding in headlines or loose sprawling in small uppercase labels.
- **Directional Fluidity**: Logical text alignment (`text-align: start` vs physical `text-align: left`) ensures seamless bidirectional support for RTL scripts (Arabic, Hebrew).
- **Modern Underlining**: Modern CSS eliminates harsh browser-default underline rendering with `text-underline-offset`, `text-decoration-thickness`, and `text-decoration-skip-ink`.
- **Financial & Data Alignment**: `font-variant-numeric: tabular-nums` enforces fixed digit advance widths, ensuring financial numbers align in columns without shifting during live updates.

---

## 2. Core Concepts

### Logical Text Alignment (`text-align`)
- `text-align: start`: Aligns text to the inline-start edge (left in LTR, right in RTL). **Always prefer `start` and `end` over physical `left` and `right`**.
- `text-align: end`: Aligns to the inline-end edge.
- `text-align: center`: Centers inline content along the line box.
- `text-align: justify`: Spaces text evenly across the line box (use cautiously; often introduces awkward typographic rivers).
- `text-align-last`: Specifies alignment for the final line of a block or paragraph before an explicit break.

### Modern Text Decoration Engine
`text-decoration` has evolved from a single property into a complete shorthand controlling four distinct sub-properties:
1. `text-decoration-line`: `none`, `underline`, `overline`, `line-through`.
2. `text-decoration-color`: Any CSS `<color>` (independent of text `color`).
3. `text-decoration-style`: `solid`, `double`, `dotted`, `dashed`, `wavy`.
4. `text-decoration-thickness`: Length or percentage (e.g., `2px`, `0.1em`, `from-font`).

Accompanying controls:
- `text-underline-offset`: Sets vertical distance between the text baseline and the underline (e.g., `4px`, `0.2em`).
- `text-decoration-skip-ink: auto`: Automatically interrupts underlines where glyph descenders (g, j, p, q, y) cross the line, preventing visual clashing.

### Spacing: Tracking and Kerning
- `letter-spacing`: Adjusts horizontal space between character glyphs.
  - Headings: Negative tracking (`-0.02em` to `-0.03em`) tightens large letterforms.
  - Uppercase tags: Positive tracking (`0.05em` to `0.1em`) improves all-caps legibility.
- `word-spacing`: Adjusts space between words (whitespace tokens).
- `text-indent`: Indents the first line of text in a block (traditional editorial style).

### OpenType Features: Ligatures & Numbers
Modern typefaces embed OpenType tables that can be unlocked via CSS:
- `font-variant-numeric: tabular-nums`: Monospaced numbers in proportional fonts.
- `font-variant-numeric: slashed-zero`: Renders zeros with an internal diagonal slash to distinguish `0` from capital `O`.
- `font-variant-ligatures: common-ligatures`: Combines character pairs (fi, fl, ff) into single aesthetic glyphs.
- `font-variant-caps: small-caps`: Uses true designed small capital glyphs rather than scaled-down uppercase letters.

---

## 3. How It Works Internally

```
+----------------------------------------------------------------------+
| Modern Underline Anatomy                                             |
|                                                                      |
|  T y p o g r a p h y                                                 |
|    |       |   |                                                     |
|  --+-------+---+----------------------------- [Baseline]             |
|    |  (g)  |   | (p) (descenders cross below)                        |
|  ==+=======+===+============================= [text-underline-offset]|
|    ^       ^   ^                                                     |
|    |       |   |                                                     |
|  [skip-ink: auto interrupts underline to leave breathing room]       |
+----------------------------------------------------------------------+
```

### Proportional vs Tabular Numbers
- **Proportional Numbers** (`font-variant-numeric: normal`): The digit `1` is narrow (e.g., 8px wide); the digit `8` is wide (e.g., 14px wide). In data tables, numbers constantly jitter horizontally as values change.
- **Tabular Numbers** (`font-variant-numeric: tabular-nums`): Every digit from `0` to `9` occupies the exact same horizontal advance width (e.g., 12px), aligning decimals vertically like monospaced code.

---

## 4. Syntax & Usage

### Baseline & Feature Support
- **`text-align: start / end`**: Baseline Widely Available.
- **`text-decoration-thickness` & `text-underline-offset`**: Baseline 2020.
- **`text-decoration-skip-ink`**: Baseline 2020.
- **`font-variant-numeric`**: Baseline Widely Available.

```css
/* Modern Accessible Hyperlink Styling */
a.editorial-link {
  color: #2563eb;
  text-decoration-line: underline;
  text-decoration-color: #93c5fd;
  text-decoration-style: solid;
  text-decoration-thickness: 2px;
  text-underline-offset: 4px;
  text-decoration-skip-ink: auto;
  transition: text-decoration-color 0.2s ease, color 0.2s ease;
}

a.editorial-link:hover {
  color: #1d4ed8;
  text-decoration-color: #1d4ed8;
}

/* Precision Financial Data */
.currency-cell {
  font-family: system-ui, sans-serif;
  font-variant-numeric: tabular-nums;
  text-align: end;
}
```

---

## 5. Practical Examples

### Example 1: Financial Ledger with Tabular Numbers and Slashed Zeros
```html
<table class="financial-ledger">
  <thead>
    <tr>
      <th scope="col">Account Code</th>
      <th scope="col">Description</th>
      <th scope="col" class="num-col">Balance (USD)</th>
    </tr>
  </thead>
  <tbody>
    <tr>
      <td class="code">ACC-0902</td>
      <td>Operating Reserves</td>
      <td class="num-col">$1,480,210.00</td>
    </tr>
    <tr>
      <td class="code">ACC-0114</td>
      <td>Payroll Escrow</td>
      <td class="num-col">$311,114.50</td>
    </tr>
  </tbody>
</table>
```

```css
.financial-ledger {
  width: 100%;
  border-collapse: collapse;
  font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
}

.financial-ledger th,
.financial-ledger td {
  padding: 0.625rem 1rem;
  border-bottom: 1px solid #e2e8f0;
}

.financial-ledger .code {
  font-variant-numeric: slashed-zero tabular-nums;
  letter-spacing: 0.05em;
}

.financial-ledger .num-col {
  text-align: end;
  font-variant-numeric: tabular-nums;
}
```

### Example 2: Uppercase Badge Tracking (Letter Spacing)
```css
/* Badges and tags: Uppercase text requires loose tracking for legibility */
.badge-tag {
  display: inline-flex;
  align-items: center;
  padding: 0.25rem 0.5rem;
  font-size: 0.6875rem;
  font-weight: 700;
  text-transform: uppercase;
  letter-spacing: 0.08em; /* Opens up character spacing */
  border-radius: 4px;
  background-color: #f1f5f9;
  color: #334155;
}
```

### Example 3: Expressive Headline with Negative Tracking
```css
/* Large hero headlines require tightened tracking to prevent loose visual density */
.hero-headline {
  font-family: "Inter", system-ui, sans-serif;
  font-size: clamp(2.5rem, 6vw, 4.5rem);
  font-weight: 800;
  line-height: 1.08;
  letter-spacing: -0.035em; /* Tighter character spacing */
  color: #0f172a;
}
```

### Example 4: Modern Wavy Strikethrough & Decorative Underlines
```css
.sale-price-container {
  display: flex;
  align-items: baseline;
  gap: 0.75rem;
}

.original-price {
  font-size: 1rem;
  color: #94a3b8;
  text-decoration: line-through;
  text-decoration-color: #ef4444;
  text-decoration-thickness: 2px;
}

.discounted-price {
  font-size: 1.5rem;
  font-weight: 700;
  color: #0f172a;
  text-decoration: underline wavy #3b82f6 2px;
  text-underline-offset: 6px;
}
```

### Example 5: Drop Caps via `::first-letter`
```css
.editorial-article p.intro::first-letter {
  font-size: 3.5rem;
  font-weight: 800;
  float: inline-start;
  line-height: 0.85;
  margin-inline-end: 0.5rem;
  margin-block-start: 0.15rem;
  color: #1e3a8a;
  text-transform: uppercase;
}
```

---

## 6. Best Practices

1. **Always Use `text-align: start` and `end`**: Replace `text-align: left` and `text-align: right` to guarantee automatic RTL compatibility without writing extra override classes.
2. **Always Enable `tabular-nums` for Numeric Tables and Timers**: Prevent numbers from jumping and shifting horizontally in clocks, counters, and data tables.
3. **Use Relative Units (`em`) for `letter-spacing`**: Setting tracking in `em` ensures that letter spacing scales proportionally whenever `font-size` changes.
4. **Increase `letter-spacing` on `text-transform: uppercase`**: Small uppercase text naturally crowds together; adding `0.05em` to `0.1em` tracking dramatically enhances legibility.
5. **Set `text-underline-offset` on Links**: A subtle offset (e.g., `3px` to `5px`) separates the underline from letter baselines, dramatically improving readability.
6. **Avoid `text-align: justify` on the Web**: Without advanced hyphenation engines, CSS justified text generates uneven white-space gaps ("rivers of white") that impair dyslexic readers.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Correction |
| :--- | :--- | :--- |
| **Using `border-bottom` to fake underlines** | Does not break around descenders (`skip-ink`), does not scale with font metrics, can trigger layout reflow. | Use `text-decoration` with `text-underline-offset` and `thickness`. |
| **Setting `letter-spacing` in fixed `px`** | Does not scale when the component's `font-size` increases on desktop viewports. | Use relative units: `letter-spacing: -0.02em;`. |
| **Hardcoding `text-align: left` in multi-lingual apps** | In RTL languages (Arabic, Hebrew), left-aligned text breaks natural reading flow. | Use logical `text-align: start;`. |
| **Omitting `tabular-nums` on animated countdowns** | Differing digit widths cause the countdown container to rapidly shake and jitter. | Add `font-variant-numeric: tabular-nums;`. |
| **Using `text-transform: uppercase` in source HTML** | Screen readers may spell out uppercase text letter-by-letter (e.g., announcing "C-A-R-T"). | Keep normal casing in HTML; apply `text-transform: uppercase;` in CSS. |
| **Excessive negative `letter-spacing`** | Glyphs overlap, causing serious readability and accessibility violations. | Keep negative tracking subtle (rarely exceed `-0.04em`). |

---

## 8. Performance, Accessibility & Browser Support

### Accessibility Compliance (WCAG 2.2 AA)
- **Criterion 1.4.12 (Text Spacing)**: Ensuring layouts accommodate:
  - Line height: at least 1.5 times the font size.
  - Spacing following paragraphs: at least 2 times font size.
  - Letter spacing: at least 0.12 times font size.
  - Word spacing: at least 0.16 times font size.
- **Link Discrimination (Criterion 1.4.1)**: Color alone cannot be used to distinguish links from surrounding text. Underlines (`text-decoration: underline`) are the universal, accessible visual indicator.

---

## 9. Comparison / When to Use What

| Feature | Property | Primary Use Case |
| :--- | :--- | :--- |
| **Inline Alignment** | `text-align: start / end` | Bidirectional body and heading alignment |
| **Underline Distance** | `text-underline-offset` | Creating breathing room between text and link lines |
| **Number Monospacing**| `font-variant-numeric: tabular-nums` | Spreadsheets, stats dashboards, live timers |
| **Character Tracking** | `letter-spacing: -0.02em` | Tightening large display headlines |
| **Visual Case** | `text-transform: uppercase` | Labels, badges, navigation headers |

---

## 10. Interview Questions

<details>
<summary>1. What is the difference between <code>text-align: start</code> and <code>text-align: left</code>?</summary>

`text-align: left` is a physical alignment property that forces content to the physical left boundary of the container, regardless of writing direction. 

`text-align: start` is a CSS Logical Property that evaluates the directional context of the element (`direction: ltr` vs `direction: rtl`). In Left-to-Right contexts (English, Spanish), `start` computes to `left`. In Right-to-Left contexts (Arabic, Hebrew), `start` automatically computes to `right`, eliminating the need to write direction-specific overrides.
</details>

<details>
<summary>2. Why is <code>text-decoration</code> now preferred over <code>border-bottom</code> for styling link underlines?</summary>

Historically, developers used `border-bottom` because legacy CSS underlines were stuck to the baseline and couldn't be customized in color or thickness. However:
1. `border-bottom` is part of the box model; adding or removing it can cause layout shifts unless balanced with transparent borders.
2. `border-bottom` cuts rigidly through descending letters (g, j, p, q, y).
3. Modern CSS provides `text-decoration-thickness`, `text-decoration-color`, `text-underline-offset`, and `text-decoration-skip-ink: auto`, offering superior typographic beauty with zero layout shift.
</details>

<details>
<summary>3. What does <code>font-variant-numeric: tabular-nums</code> do, and why is it crucial in financial dashboards?</summary>

In standard proportional typefaces, digits have variable advance widths (e.g., the digit `1` is much narrower than the digit `8`). When numbers are rendered in data tables or animated live counters, this creates two problems:
1. In tables, decimal points and digit columns do not align vertically.
2. In counters or clocks, the bounding box constantly vibrates as numbers change.
`tabular-nums` activates the OpenType `tnum` feature, which forces all numerical digits to occupy identical horizontal advance widths, ensuring vertical alignment and eliminating jitter.
</details>

<details>
<summary>4. Why should developers transform text casing using CSS <code>text-transform</code> rather than typing all-caps in HTML?</summary>

Typing text directly in uppercase in HTML (`<button>SUBMIT ORDER</button>`) can cause screen readers (such as JAWS or NVDA) to interpret the text as an acronym or initialism, reading it aloud letter-by-letter ("S-U-B-M-I-T O-R-D-E-R"). Writing semantic casing in HTML (`<button>Submit Order</button>`) and applying `text-transform: uppercase;` in CSS guarantees screen readers pronounce words correctly while achieving the intended visual styling.
</details>

<details>
<summary>5. How does <code>text-decoration-skip-ink: auto</code> work?</summary>

`text-decoration-skip-ink` controls whether underlines and overlines are drawn continuously or clipped when they intersect with glyph contours. With `auto` (the default in modern browsers), the browser layout engine detects where glyph descenders (such as the tails of 'g', 'p', and 'y') cross the underline path and automatically introduces a small whitespace gap, preventing the line from visually colliding with the glyph.
</details>

<details>
<summary>6. What causes "typographic rivers" in <code>text-align: justify</code> and how can it be mitigated?</summary>

When text is justified, the browser stretches spaces between words to ensure both left and right margins align flush. On narrow columns or screens, this leads to large, irregular gaps of whitespace between words that visually align into vertical streams or "rivers" across consecutive lines. This can be mitigated by pairing justification with hyphenation (`hyphens: auto;`) and setting `text-justify: inter-word;`.
</details>

<details>
<summary>7. What is the difference between tracking (<code>letter-spacing</code>) and kerning?</summary>

- **Kerning**: The adjustment of space between specific adjacent pairs of characters (e.g., tucking 'A' closer under the arm of 'V'). Kerning is built into the font file's OpenType `kern` table.
- **Tracking (`letter-spacing`)**: The uniform expansion or tightening of space across all characters in a text selection.
</details>

<details>
<summary>8. How do you implement a true fraction in CSS using OpenType features?</summary>

By applying `font-variant-numeric: diagonal-fractions;` to numbers formatted with a slash (e.g., `1/2` or `3/4`). If the font contains OpenType fraction tables (`frac`), the browser automatically renders real numerator and denominator glyphs separated by an angled fraction bar ($½$, $¾$) instead of full-sized integers.
</details>

---

## 11. Practice Exercises

### Exercise 1: Build an Accessible Hover-Animated Link Underline
**Goal**: Style an inline link with a 2px offset, skip-ink enabled, and a color transition on hover.

<details>
<summary>View Solution</summary>

```css
.accessible-link {
  color: #1d4ed8;
  text-decoration: underline;
  text-decoration-color: #60a5fa;
  text-decoration-thickness: 2px;
  text-underline-offset: 3px;
  text-decoration-skip-ink: auto;
  transition: text-decoration-color 0.2s ease, color 0.2s ease;
}

.accessible-link:hover {
  color: #1e3a8a;
  text-decoration-color: #1e3a8a;
}

.accessible-link:focus-visible {
  outline: 2px solid #1d4ed8;
  outline-offset: 4px;
}
```
</details>

### Exercise 2: Create a Tabular Metrics HUD Display
**Goal**: Build a digital timer display that maintains constant width as milliseconds count up.

<details>
<summary>View Solution</summary>

```html
<div class="hud-timer">
  <span class="time-segment">02</span>:<span class="time-segment">45</span>.<span class="time-ms">892</span>
</div>
```

```css
.hud-timer {
  font-family: ui-monospace, SFMono-Regular, Menlo, monospace;
  font-size: 2rem;
  font-weight: 700;
  font-variant-numeric: tabular-nums slashed-zero;
  color: #10b981;
  background-color: #064e3b;
  padding: 0.5rem 1rem;
  border-radius: 6px;
  display: inline-block;
}
```
</details>

### Exercise 3: Uppercase Micro-Tag Tracking
**Goal**: Design an uppercase status tag with proper letter spacing and vertical alignment.

<details>
<summary>View Solution</summary>

```css
.status-pill {
  display: inline-block;
  font-size: 0.75rem;
  font-weight: 700;
  text-transform: uppercase;
  letter-spacing: 0.08em;
  padding: 0.25rem 0.625rem;
  border-radius: 9999px;
  background-color: #fef3c7;
  color: #92400e;
}
```
</details>

### Exercise 4: Diagonal Fractions Formatting
**Goal**: Render recipe ingredient fractions using OpenType diagonal fractions.

<details>
<summary>View Solution</summary>

```html
<p class="recipe-ingredient">Add <span class="fraction">1/2</span> cup olive oil and <span class="fraction">3/4</span> tsp salt.</p>
```

```css
.fraction {
  font-variant-numeric: diagonal-fractions;
  font-family: Georgia, serif;
}
```
</details>

### Exercise 5: Direction-Resilient RTL/LTR Header
**Goal**: Build a card header with title and action button that swaps seamlessly between English and Arabic.

<details>
<summary>View Solution</summary>

```css
.card-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  text-align: start;
  padding: 1rem;
  border-bottom: 1px solid #e2e8f0;
}

.card-title {
  margin: 0;
  font-size: 1.25rem;
  font-weight: 600;
}
```
</details>

### Exercise 6: Decorative Multi-Style Heading Underline
**Goal**: Create a playful section heading underlined with a double coral stroke with 6px offset.

<details>
<summary>View Solution</summary>

```css
.playful-title {
  font-family: system-ui, sans-serif;
  font-size: 2.25rem;
  font-weight: 800;
  text-decoration-line: underline;
  text-decoration-style: double;
  text-decoration-color: #f43f5e;
  text-decoration-thickness: 3px;
  text-underline-offset: 6px;
}
```
</details>

---

## 12. Summary / Cheat Sheet

| Property | Values | Purpose |
| :--- | :--- | :--- |
| `text-align` | `start \| end \| center \| justify` | Logical text alignment along inline axis |
| `text-decoration-line` | `none \| underline \| line-through` | Type of line decoration |
| `text-decoration-color`| `<color>` | Color of line (independent of text color) |
| `text-decoration-thickness` | `<length> \| from-font` | Stroke width of decoration line |
| `text-underline-offset`| `<length> \| auto` | Vertical gap between baseline and underline |
| `text-decoration-skip-ink` | `auto \| none` | Clips underline around glyph descenders |
| `font-variant-numeric` | `tabular-nums \| slashed-zero` | OpenType number metrics and glyph variants |
| `letter-spacing` | `<length>` (e.g., `0.05em`) | Uniform character tracking |

---

## 13. Further Reading
- [W3C CSS Text Module Level 3](https://www.w3.org/TR/css-text-3/)
- [W3C CSS Text Decoration Module Level 4](https://www.w3.org/TR/css-text-decor-4/)
- [MDN Web Docs: text-decoration](https://developer.mozilla.org/en-US/docs/Web/CSS/text-decoration)
- [MDN Web Docs: font-variant-numeric](https://developer.mozilla.org/en-US/docs/Web/CSS/font-variant-numeric)
- [Modern CSS: Modern Link Underlines](https://moderncss.dev/)

---

[Previous: 03-variable-fonts.md](03-variable-fonts.md) | [Module README](README.md) | [Next: 05-line-height-and-readability.md](05-line-height-and-readability.md)
