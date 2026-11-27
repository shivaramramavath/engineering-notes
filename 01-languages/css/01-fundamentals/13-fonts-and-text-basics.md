# Fonts & Text Basics

| Metadata | Details |
|:---|:---|
| **Level** | Foundational |
| **Prerequisites** | `01-fundamentals/01-syntax-rules-and-declarations.md`, `01-fundamentals/09-units.md` |
| **Estimated Reading Time** | 29 minutes |
| **Status** | Current |
| **Baseline Status** | Widely Available (Core fonts/text: Universal; text-wrap: balance: Baseline 2023) |
| **Browsers Tested** | Chrome 120+, Firefox 121+, Safari 17.2+, Edge 120+ |
| **Tested With** | W3C CSS Fonts Level 4, CSS Text Level 4, Playwright css_runner.js, stylelint |

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

### What is CSS Typography?
Typography is the visual delivery system of written human thought on the web. Over $90\%$ of web information is communicated through text. CSS provides fine-grained control over font selection, typographic scales, line spacing, text alignment, and word wrapping.

CSS partitions typography into two complementary modules:
1. **CSS Fonts Module**: Governs font resource selection (`font-family`), optical weight (`font-weight`), style (`font-style`), and size (`font-size`).
2. **CSS Text Module**: Governs the layout and spacing of text glyphs within their boxes (`line-height`, `letter-spacing`, `text-align`, `text-wrap`, `text-transform`).

### Why is Typographic Architecture Crucial?
Poor typographic CSS causes immediate friction:
- **Cumulative Layout Shift (CLS)**: Unoptimized fallback fonts cause text to reflow violently when webfonts download.
- **Cognitive Reading Fatigue**: Cramped `line-height` or excessively wide text blocks ($> 80\text{ch}$) strain human eye saccades.
- **Orphaned Words**: Headings ending with a single dangling word look unpolished. Modern CSS solves this natively via `text-wrap: balance`.

---

## 2. Core Concepts

### 1. Font Family Stacks & Generic Fallbacks
The `font-family` property specifies a prioritized list of font family names. The browser tests each font sequentially from left to right, using the first font installed locally or downloaded via `@font-face`:
```css
font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
```
- **Quoting Rule**: Font family names containing spaces, numbers, or punctuation **must be enclosed in quotation marks** (e.g., `"Open Sans"`, `"Comic Sans MS"`).
- **Mandatory Generic Fallback**: The stack **MUST always terminate with a generic family** (`sans-serif`, `serif`, `monospace`, `system-ui`) so the OS can supply a graceful fallback if prior fonts are unavailable.

### 2. The Golden Rule of `line-height`: Unitless Multipliers
`line-height` sets the height of an inline line box:
- **Always use a unitless number**: `line-height: 1.5;`
- *Never use percentages (`150%`) or pixels (`24px`) on parent containers*, because children inherit the computed fixed pixel length, causing headers with large font sizes to violently collide.

### 3. Modern Text Wrapping: `text-wrap: balance` & `pretty`
Standardized in CSS Text Level 4 (Baseline 2023):
- **`text-wrap: balance`**: Instructs the browser's line-breaking engine to balance the number of characters on each line across multi-line headings, preventing lonely "orphaned" single words.
- **`text-wrap: pretty`**: Optimizes paragraph endings to eliminate orphans without the heavier performance cost of balancing the entire block.

### 4. Granular Text Decoration Styling
Modern CSS provides independent control over link underlines:
- `text-decoration-line`: `underline`, `line-through`, `overline`.
- `text-decoration-color`: Changes underline color without affecting text color (`#0284c7`).
- `text-decoration-thickness`: Sets underline stroke width (`2px`, `0.1em`).
- `text-underline-offset`: Drops the underline away from the text baseline, preventing it from slicing through character descenders like `"p"`, `"g"`, and `"y"`.

---

## 3. How It Works Internally

### Typographic Anatomy & The Inline Formatting Box
When text is rasterized, the glyphs sit within an **Em Square** defined by the typeface designer:

```
+--------------------------------------------------------------------+
| TYPOGRAPHIC ANATOMY OF A LINE BOX                                  |
+--------------------------------------------------------------------+
  [Top of Line Box]      ---------------------------------
                         ^                               ^
                         | Half-Leading                  |
  [Ascender Line]        |----------- d, h, k, l --------|
                         |                               |
  [Cap Height]           |----------- H, E, L, P --------| Line-Height
                         |                               |
  [Baseline]             |=========== a, c, e, n, x =====|
                         |                               |
  [Descender Line]       |----------- g, j, p, q, y -----|
                         | Half-Leading                  |
  [Bottom of Line Box]   ---------------------------------
```

- **Leading**: The difference between `line-height` and the font's intrinsic height.
- **Half-Leading**: The engine divides the remaining leading equally, placing half above the font ascender and half below the descender. This is why text sits vertically centered inside its line box.

---

## 4. Syntax & Usage

```
+-------------------------------------------------------------------------+
| Web Platform Baseline: Widely Available                                  |
| font-family, font-size, font-weight, line-height: Universal.             |
| text-underline-offset, text-decoration-thickness: Baseline 2021.        |
| text-wrap: balance: Widely Available (Baseline 2023 - All engines).     |
| text-wrap: pretty: Baseline 2024.                                       |
+-------------------------------------------------------------------------+
```

### Modern System Font Stack
Using native system fonts guarantees instant zero-download rendering (0ms First Contentful Paint):
```css
:root {
  /* High-performance modern system font stack */
  --font-sans: system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI",
               Roboto, Oxygen, Ubuntu, Cantarell, "Helvetica Neue", sans-serif;
  --font-mono: ui-monospace, "Cascadia Code", "Source Code Pro", Menlo,
               Consolas, "Courier New", monospace;
}

body {
  font-family: var(--font-sans);
  font-size: 1rem;
  line-height: 1.6;
  color: #1e293b;
}
```

### Balanced Headlines Syntax
```css
h1, h2, h3, h4 {
  font-family: var(--font-sans);
  font-weight: 700;
  line-height: 1.2;
  /* Balance headline text across lines */
  text-wrap: balance;
}
```

---

## 5. Practical Examples

### Example 1: Beautiful Accessible Hyperlink Styling
```css
.content a {
  color: #0284c7;
  text-decoration: underline;
  /* Shift underline 3px below baseline to clear descenders */
  text-underline-offset: 3px;
  /* Give underline subtle opacity so it doesn't distract from text */
  text-decoration-color: rgba(2, 132, 199, 0.4);
  text-decoration-thickness: 1.5px;
  transition: text-decoration-color 0.15s ease, color 0.15s ease;
}

.content a:hover {
  color: #0369a1;
  text-decoration-color: #0369a1;
}

.content a:focus-visible {
  outline: 2px solid #0284c7;
  outline-offset: 3px;
  border-radius: 2px;
}
```

### Example 2: Real-World Headline with `text-wrap: balance`
```html
<div class="card">
  <h2 class="card-title">
    Architecting Scalable Design Systems with Native Web Platform Features
  </h2>
  <p>Learn how modern CSS eliminates thousands of lines of legacy code.</p>
</div>
```
```css
.card {
  max-width: 420px;
  padding: 1.5rem;
  background: #ffffff;
  border: 1px solid #e2e8f0;
  border-radius: 8px;
}

.card-title {
  font-size: 1.5rem;
  font-weight: 800;
  line-height: 1.25;
  color: #0f172a;
  /* Eliminates awkward single-word wrapping */
  text-wrap: balance;
  margin-bottom: 0.75rem;
}
```

### Example 3: Edge Case — Unitless Line-Height vs Fixed Pixels
```html
<div class="banner">
  <span class="eyebrow">Enterprise Solutions</span>
  <h1 class="main-title">Building Global Scale</h1>
</div>
```
```css
/* Good practice: Unitless line height scales across disparate child font sizes */
.banner {
  line-height: 1.3;
}

.eyebrow {
  font-size: 0.875rem; /* line-height computes to 14px * 1.3 = 18.2px */
  text-transform: uppercase;
  letter-spacing: 0.05em;
  color: #64748b;
}

.main-title {
  font-size: 3rem; /* line-height computes to 48px * 1.3 = 62.4px */
  color: #0f172a;
  margin-top: 0.5rem;
}
```

### Example 4: Failure Case — Missing Quotes and Missing Generic Fallback
```css
/* Anti-Pattern: Unquoted font name with spaces and no generic fallback */
.article {
  font-family: Open Sans; /* Fails parsing on strict engines! */
}
```
*Failure Analysis*:
1. If the user doesn't have "Open Sans" installed locally and network font download fails, the browser has no generic fallback (`sans-serif`), defaulting unpredictably to `Times New Roman` (serif).
2. Fonts with spaces must be enclosed in quotes (`"Open Sans"`).
*Correct Pattern*:
```css
.article {
  font-family: "Open Sans", system-ui, sans-serif;
}
```

### Example 5: Clean Code Block Styling with Monospace System Stack
```html
<pre class="code-block"><code>function helloWorld() {
  console.log("Modern CSS Typography");
}</code></pre>
```
```css
.code-block {
  font-family: var(--font-mono, ui-monospace, monospace);
  font-size: 0.875rem;
  line-height: 1.5;
  background-color: #0f172a;
  color: #f8fafc;
  padding: 1rem;
  border-radius: 6px;
  overflow-x: auto;
  tab-size: 2;
}
```
*Playwright Test Runner Verification*:
```javascript
// Test computed line-height and font-family
const title = document.querySelector('.card-title');
const computed = window.getComputedStyle(title);
console.log('Computed Font Size:', computed.fontSize); // "24px"
console.log('Computed Line Height:', computed.lineHeight); // "30px" (24 * 1.25)
```

---

## 6. Best Practices

1. **Always End `font-family` with a Generic Family**: Terminate every font stack with `sans-serif`, `serif`, `monospace`, or `system-ui`.
2. **Always Use Unitless `line-height`**: Set `line-height: 1.5;` or `1.6;` on body text to ensure proportional scaling.
3. **Apply `text-wrap: balance` to Headings**: Add `text-wrap: balance` to `h1–h4` elements to eliminate orphaned words on responsive wraps.
4. **Cap Paragraph Line Length at 65ch**: Use `max-width: 65ch` on article prose to maintain optimal reading ergonomics.
5. **Use `text-underline-offset` on Links**: Prevent underlines from slicing through lowercase letters like `"g"`, `"p"`, and `"y"` by setting `text-underline-offset: 2px` or `3px`.
6. **Quote Multi-Word Font Names**: Always wrap names like `"Helvetica Neue"` or `"Fira Code"` in single or double quotes.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Root Cause | Symptom / Failure | Correct Pattern |
|:---|:---|:---|:---|
| Using fixed pixel `line-height` on root | Inheriting fixed pixel lengths | Headings overlap lines when font-size increases. | Use unitless `line-height: 1.5;`. |
| Omitting generic font fallback | Laziness or assuming font loads | Page defaults to Times New Roman if webfont fails. | Always end with `sans-serif` or `serif`. |
| Unquoted font names with spaces | Syntax misunderstanding | Some browsers fail to parse `"Open Sans"` without quotes. | Enclose in quotes: `"Open Sans"`. |
| Applying `text-wrap: balance` to long body paragraphs | Performance engine budget | Browser ignores balance because it only applies to $< 10$ lines. | Use `balance` on headings; use `pretty` on body. |
| Using `text-transform: uppercase` in HTML source text | Mixing content with presentation | Screen readers spell out acronyms letter-by-letter. | Write normal text in HTML; style with CSS `text-transform`. |
| Justifying text with `text-align: justify` on web | Lack of hyphenation | Creates unsightly "rivers of white space" between words. | Use `text-align: left` or pair with `hyphens: auto`. |

---

## 8. Performance, Accessibility & Browser Support

### Rendering Engine Performance of `text-wrap: balance`
- Balancing text requires the browser engine to perform an **iterative binary search** over line-break permutations to find the configuration with minimal line variance.
- To prevent performance degradation, the W3C limits `text-wrap: balance` to blocks with **fewer than 10 lines of text**.
- Engine execution takes under **$0.1\text{ms}$** for headings, making it completely safe for production headlines.

### Accessibility Safeguards
- **WCAG 1.4.12 (Text Spacing)**: Requires interfaces to support `line-height: 1.5` and paragraph spacing $2\times$ font size without text clipping.
- **Dyslexia Considerations**: Avoid `text-align: justify` without hyphenation; irregular word gaps disrupt reading flow for neurodivergent users.

---

## 9. Comparison / When to Use What

| Feature | Primary Purpose | Scope | Recommended Setting |
|:---|:---|:---|:---|
| **`font-family`** | Typeface selection | Global / Component | Modern System Stack |
| **`font-size`** | Text scale | Component | `rem` or `clamp()` |
| **`line-height`** | Vertical line spacing | Global / Component | Unitless `1.4` to `1.6` |
| **`text-wrap: balance`**| Eliminates heading orphans | Headlines ($< 10$ lines)| `h1, h2, h3` |
| **`text-wrap: pretty`** | Eliminates paragraph orphans| Long body prose | Articles, blogs |
| **`text-underline-offset`**| Cleans up link underlines | Hyperlinks | `2px` to `4px` |

---

## 10. Interview Questions

<details>
<summary><strong>1. Why does the W3C specify that `line-height` should ideally be declared as a unitless number rather than with `px`, `em`, or `%`?</strong></summary>

Inheritance in CSS propagates the **Computed Value**, not the raw declaration.
- If you declare `line-height: 150%` or `1.5em` on `body` with `font-size: 16px`, the browser computes $16\text{px} \times 1.5 = 24\text{px}$. The fixed length **`24px`** is then inherited by all descendants. When an `<h1>` has `font-size: 40px`, its line height remains locked at `24px`, causing lines to collide.
- If you declare unitless `line-height: 1.5`, the child element inherits the **proportional multiplier `1.5`**. The `<h1>` computes its line height as $40\text{px} \times 1.5 = 60\text{px}$, maintaining proportional readability.
</details>

<details>
<summary><strong>2. How does `text-wrap: balance` work, and why does the browser engine limit it to 10 lines?</strong></summary>

`text-wrap: balance` uses a line-breaking algorithm that runs a binary search to find the visual width that produces the most equal line lengths across all lines of the element, preventing single-word orphans. Because testing line-break permutations has an $O(N \log N)$ complexity, running it on massive 500-word paragraphs would cause noticeable layout reflow latency. The CSS specification caps the balancing algorithm at **10 lines**; elements exceeding 10 lines automatically fall back to standard line-breaking.
</details>

<details>
<summary><strong>3. What is the difference between `text-wrap: balance` and `text-wrap: pretty`?</strong></summary>

- `text-wrap: balance` aims for **total line length equality** across the entire text block, ideal for titles and headlines.
- `text-wrap: pretty` focuses strictly on the **final two lines of text** to ensure that the last line does not end with an orphaned single word, without altering the line wrapping of earlier lines. It is significantly faster computationally and is designed for multi-paragraph body text.
</details>

<details>
<summary><strong>4. Why should you always include a generic font family at the end of a `font-family` stack?</strong></summary>

If all named fonts in a stack are unavailable (e.g., custom webfonts fail to download and local system fonts aren't installed), the browser requires an instruction on what fundamental typographic style to fall back to (`sans-serif`, `serif`, `monospace`). Without a generic fallback, the browser defaults to its global User Agent font, which may be a completely mismatched serif typeface that breaks the visual layout.
</details>

<details>
<summary><strong>5. What is the typographic "half-leading" concept in CSS line layout?</strong></summary>

In the CSS inline formatting model, "leading" is the difference between the `line-height` and the font's intrinsic height. The engine calculates $\text{half-leading} = (\text{line-height} - \text{font-size}) / 2$, and places half above the font's ascender line and half below the descender line. This automatically centers glyphs vertically within each inline line box.
</details>

<details>
<summary><strong>6. How does `text-underline-offset` improve typographic aesthetics and accessibility?</strong></summary>

Traditionally, browser underlines sliced directly through character descenders (the descending tails of `"g"`, `"j"`, `"p"`, `"q"`, and `"y"`), causing visual clutter and impairing reading speed for users with dyslexia. `text-underline-offset` allows authors to shift the underline downward away from the baseline, providing clean clearance for character descenders while preserving clear link affordances.
</details>

<details>
<summary><strong>7. Why should you avoid using `text-transform: uppercase` in raw HTML source code?</strong></summary>

Writing text in all-caps directly in HTML markup (`<button>SUBMIT ORDER</button>`) can cause screen readers to announce the text as an acronym letter-by-letter (e.g., *"S-U-B-M-I-T O-R-D-E-R"*). The accessible pattern is to write standard sentence or title case in HTML (`<button>Submit Order</button>`) and apply `text-transform: uppercase;` in CSS.
</details>

<details>
<summary><strong>8. What does `tab-size` control in CSS?</strong></summary>

The `tab-size` property controls the width of the tab character (`U+0009`) when rendered inside preformatted text containers (`<pre>` or elements with `white-space: pre`). By default, browsers render tabs with an excessively wide 8-space width. Setting `tab-size: 2;` or `tab-size: 4;` formats code blocks with modern indent widths.
</details>

---

## 11. Practice Exercises

### Exercise 1: Build a Modern System Font Stack (Easy)
**Task**: Write the CSS variable declaration on `:root` for `--font-system` that uses modern platform system UI fonts across Apple, Windows, Android, and Linux, ending with the appropriate generic family.

<details>
<summary><strong>View Solution</strong></summary>

```css
:root {
  --font-system: system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI",
                 Roboto, Oxygen, Ubuntu, Cantarell, sans-serif;
}
```
*Explanation*: `system-ui` is the modern standard, with legacy engine fallbacks followed by `sans-serif`.
</details>

---

### Exercise 2: Prevent Orphaned Headline Text (Easy)
**Task**: Apply modern CSS text-wrapping to all `h1`, `h2`, and `h3` elements to balance multi-line headings across screens.

<details>
<summary><strong>View Solution</strong></summary>

```css
h1, h2, h3 {
  text-wrap: balance;
}
```
*Explanation*: `text-wrap: balance` balances line lengths across headings automatically.
</details>

---

### Exercise 3: Clear Descenders with Link Underline Offset (Medium)
**Task**: Style all links inside an `.editorial` class with a 2px royal blue underline shifted 4px below the baseline so it never collides with letter descenders.

<details>
<summary><strong>View Solution</strong></summary>

```css
.editorial a {
  color: #1d4ed8;
  text-decoration: underline;
  text-decoration-color: #3b82f6;
  text-decoration-thickness: 2px;
  text-underline-offset: 4px;
}
```
*Explanation*: `text-underline-offset: 4px` drops the underline cleanly below descenders.
</details>

---

### Exercise 4: Clean Code Snippet Typography (Medium)
**Task**: Style `<pre class="code">` to use native monospace fonts, a readable 1.5 line height, and a 2-space tab indent.

<details>
<summary><strong>View Solution</strong></summary>

```css
pre.code {
  font-family: ui-monospace, "Cascadia Code", "Source Code Pro", Menlo, Consolas, monospace;
  font-size: 0.875rem;
  line-height: 1.5;
  tab-size: 2;
}
```
*Explanation*: Combines modern monospace system fonts with 2-space tab sizing.
</details>

---

### Exercise 5: Accessible Uppercase Badges (Hard)
**Task**: Create a badge class `.status-badge` that displays text in uppercase with slightly expanded letter-spacing, while keeping HTML source text in accessible title case.

<details>
<summary><strong>View Solution</strong></summary>

```html
<!-- Accessible HTML text -->
<span class="status-badge">Payment Complete</span>
```
```css
.status-badge {
  display: inline-flex;
  align-items: center;
  padding: 0.25rem 0.5rem;
  font-size: 0.75rem;
  font-weight: 700;
  /* Visual uppercase without breaking screen readers */
  text-transform: uppercase;
  letter-spacing: 0.05em;
  background-color: #dcfce7;
  color: #15803d;
  border-radius: 4px;
}
```
*Explanation*: `text-transform: uppercase` handles visual styling without causing screen readers to spell words aloud as acronyms.
</details>

---

### Exercise 6: Programmatic Line-Height Verification in JavaScript (Hard)
**Task**: Write a JavaScript utility function `isProportionalLineHeight(element)` that verifies whether an element's computed line-height is at least $1.4\times$ its computed font-size to confirm reading accessibility.

<details>
<summary><strong>View Solution</strong></summary>

```javascript
/**
 * Verifies that an element satisfies WCAG proportional line-height recommendations.
 * @param {HTMLElement} element
 * @returns {boolean}
 */
function isProportionalLineHeight(element) {
  const styles = window.getComputedStyle(element);
  const fontSize = parseFloat(styles.fontSize);
  const lineHeight = parseFloat(styles.lineHeight);

  if (isNaN(lineHeight)) return false; // handles 'normal' if uncomputed
  const ratio = lineHeight / fontSize;
  return ratio >= 1.4;
}

const paragraph = document.querySelector('p');
console.log('Satisfies Line-Height Requirement:', isProportionalLineHeight(paragraph));
```
*Explanation*: Computes the mathematical ratio between line-height and font-size.
</details>

---

## 12. Summary / Cheat Sheet

```
+-------------------------------------------------------------------------------------------------+
| CSS FONTS & TEXT BASICS MASTER CHEAT SHEET                                                      |
+--------------------------+-------------------------------------+--------------------------------+
| Property                 | Example Syntax                      | Core Best Practice             |
+--------------------------+-------------------------------------+--------------------------------+
| font-family              | system-ui, sans-serif               | Always end with generic family |
| line-height              | line-height: 1.5;                   | ALWAYS use unitless multiplier |
| text-wrap: balance       | text-wrap: balance;                 | Use on headings (< 10 lines)   |
| text-wrap: pretty        | text-wrap: pretty;                  | Use on body paragraphs         |
| text-underline-offset    | text-underline-offset: 3px;         | Clear character descenders     |
| text-transform           | text-transform: uppercase;          | Keep HTML in title/sentence case|
| font-weight              | font-weight: 700;                   | 400 normal, 700 bold           |
| tab-size                 | tab-size: 2;                        | Format code tabs to 2 spaces   |
+--------------------------+-------------------------------------+--------------------------------+
```

---

## 13. Further Reading

- [W3C CSS Fonts Module Level 4](https://www.w3.org/TR/css-fonts-4/)
- [W3C CSS Text Module Level 4](https://www.w3.org/TR/css-text-4/)
- [MDN Web Docs: text-wrap](https://developer.mozilla.org/en-US/docs/Web/CSS/text-wrap)
- [web.dev: CSS text-wrap: balance](https://web.dev/articles/css-text-wrap-balance)
- [Butterick's Practical Typography: Line Length & Leading](https://practicaltypography.com/)

---

[Previous: 12 - CSS Values & Math Functions](12-values-and-functions.md) | [Section Index](README.md) | [Next: 14 - Shorthand vs Longhand Properties](14-shorthand-and-longhand-properties.md)
