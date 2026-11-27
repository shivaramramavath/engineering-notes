# Writing Modes & Internationalization

| Metadata | Details |
| :--- | :--- |
| **Topic** | Writing Modes (`horizontal-tb`, `vertical-rl`, `vertical-lr`), Bidirectional Text (`dir="rtl"`, `unicode-bidi`), Text Orientation & Tate-chu-yoko |
| **Spec Level** | CSS Writing Modes Level 3 / CSS Writing Modes Level 4 |
| **Baseline Status** | Widely Available (`writing-mode` Baseline 2017; `text-orientation` Baseline 2020; `text-combine-upright` Baseline 2020) |
| **Browsers Tested** | Chrome 120+, Firefox 122+, Safari 17.2+, Edge 120+ |
| **Tested With** | Japanese (Vertical Kanji/Kana), Arabic (RTL), Hebrew, Chromium LayoutNG |
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

The web is a global platform serving billions of users across hundreds of linguistic traditions. While Western languages flow horizontally from left to right, many major world scripts operate under completely different flow models:
- **Right-to-Left (RTL)**: Arabic, Hebrew, Persian, and Urdu flow horizontally from right to left.
- **Vertical-to-Horizontal**: Traditional Japanese, Chinese, and Korean (CJK) literature flows vertically from top to bottom, with columns progressing from right to left (`vertical-rl`).
- **Vertical-Left-to-Right**: Mongolian script flows vertically from top to bottom, with columns progressing from left to right (`vertical-lr`).

Mastering CSS writing modes and internationalization mechanics is essential for building resilient global architectures:
1. **Logical Box Dimensions**: When writing mode changes from horizontal to vertical, physical dimensions rotate. "Width" becomes block height, and "height" becomes inline width. Writing CSS with logical properties (`inline-size`, `block-size`, `margin-inline`) ensures layouts adapt automatically.
2. **True Bidirectional Rendering**: Handling mixed scripts (e.g., English brand names or numbers embedded inside an Arabic paragraph) requires understanding the Unicode Bidirectional Algorithm (BiDi) and `unicode-bidi: isolate`.
3. **East Asian Typographic Features**: Mastering `text-orientation: upright` and `text-combine-upright: all` (Tate-chu-yoko) allows elegant editorial book designs and vertical signage.

---

## 2. Core Concepts

### `writing-mode`
Governs whether lines of text flow horizontally or vertically, and the direction in which lines progress:
- `horizontal-tb` (Default): Horizontal text flow, top-to-bottom line progression (Latin, Cyrillic, Greek, Arabic, Hebrew, modern CJK).
- `vertical-rl`: Vertical text flow (top to bottom), right-to-left line progression (Traditional Japanese, Chinese poetry/novels).
- `vertical-lr`: Vertical text flow (top to bottom), left-to-right line progression (Mongolian script).

### Direction: HTML `dir` vs CSS `direction`
- **Rule of Law**: Direction should **always be declared in HTML** (`<html dir="rtl">` or `<div dir="rtl">`), **not** exclusively in CSS. If CSS fails to load, the browser must still parse and present the document in the correct reading direction.
- CSS `direction: rtl` or `direction: ltr` reflects the computed state.

### `text-orientation`
In vertical writing modes, determines the orientation of Latin and Western characters:
- `mixed` (Default): Rotates Latin and numeric characters 90 degrees clockwise while keeping CJK ideographs upright.
- `upright`: Keeps Latin characters standing upright in a vertical stack (ideal for vertical headings or acronyms).
- `sideways`: Rotates all characters 90 degrees clockwise (matches horizontal text rotated sideways).

### Tate-chu-yoko (`text-combine-upright`)
In vertical Japanese typography, short horizontal runs of Latin numbers (like "24" in "May 24") are rendered horizontally inside a single vertical glyph em-square.
- `text-combine-upright: all;`: Compresses up to 2–4 digits into the horizontal footprint of a single vertical character.

### Bidirectional Isolation (`unicode-bidi`)
When mixing LTR text inside RTL paragraphs:
- If a phone number or user handle (e.g., `@alex_99`) appears in an Arabic sentence, the BiDi algorithm can scramble the punctuation and numbers.
- Modern HTML provides `<bdi>` (Bidirectional Isolate).
- Modern CSS provides `unicode-bidi: isolate;` (or `isolate-override`), insulating the embedded string from the surrounding directionality.

---

## 3. How It Works Internally

```
Writing Mode & Coordinate System Rotation

1. horizontal-tb (Standard LTR):
    +------------------------------------> Inline Axis (X)
    |  Hello World
    |  Line 2
    v
   Block Axis (Y)

2. vertical-rl (Traditional Japanese):
   Block Progression <------------------+
                                        |
      L         L                       |
      i         i                       |
      n         n                       v Inline Axis (Y)
      e         e
      2         1

Logical Coordinate Equivalents:
- inline-size  = Width in horizontal  / Height in vertical
- block-size   = Height in horizontal / Width in vertical
```

---

## 4. Syntax & Usage

### Baseline & Feature Support
- **`writing-mode`**: Baseline Widely Available (Chrome 48+, Safari 10.1+, Firefox 41+).
- **`text-orientation`**: Baseline 2020 (Chrome 48+, Safari 14+, Firefox 41+).
- **`text-combine-upright`**: Baseline 2020 (Chrome 48+, Safari 15.4+, Firefox 48+).
- **Logical Properties**: Baseline Widely Available.

```css
/* Traditional Japanese Vertical Book Container */
.vertical-novel {
  writing-mode: vertical-rl;
  text-orientation: mixed;
  block-size: 500px;
  inline-size: 100%;
  overflow-x: auto;
  font-family: "Hiragino Mincho ProN", "Yu Mincho", serif;
  line-height: 1.8;
}

/* Tate-chu-yoko for numbers inside vertical text */
.tate-chu-yoko {
  text-combine-upright: all;
}

/* Logical spacing that adapts across all writing modes */
.component-card {
  inline-size: 100%;
  padding-block: 1.5rem;
  padding-inline: 2rem;
  margin-block-end: 1rem;
}
```

---

## 5. Practical Examples

### Example 1: Full Right-to-Left (RTL) Layout Adaptation
```html
<!DOCTYPE html>
<html lang="ar" dir="rtl">
<head>
  <meta charset="UTF-8">
  <title>RTL Dashboard</title>
  <link rel="stylesheet" href="styles.css">
</head>
<body>
  <div class="user-card">
    <img src="avatar.jpg" alt="" class="avatar">
    <div class="user-meta">
      <h3>طارق الشمري</h3>
      <p>مهندس أنظمة أول</p>
    </div>
  </div>
</body>
</html>
```

```css
/* Zero physical left/right rules needed; logical properties adapt automatically */
.user-card {
  display: flex;
  align-items: center;
  padding: 1rem;
  border: 1px solid #e2e8f0;
  border-radius: 8px;
  background-color: #ffffff;
}

.avatar {
  inline-size: 48px;
  block-size: 48px;
  border-radius: 50%;
  margin-inline-end: 1rem; /* Becomes margin-left in RTL automatically */
}

.user-meta h3 {
  margin: 0;
  font-size: 1.125rem;
  text-align: start; /* Flushes right in RTL automatically */
}

.user-meta p {
  margin: 0.25rem 0 0 0;
  color: #64748b;
  text-align: start;
}
```

### Example 2: Vertical Japanese Poetry Card with Tate-chu-yoko
```html
<div class="japanese-card" lang="ja">
  <div class="vertical-content">
    <h3>春の便り</h3>
    <p>
      令和<span class="num">06</span>年<span class="num">05</span>月<span class="num">24</span>日、
      東京の桜が満開を迎えました。
    </p>
  </div>
</div>
```

```css
.japanese-card {
  padding: 2rem;
  border: 2px solid #e5e7eb;
  border-radius: 12px;
  background-color: #fffaf0;
  display: inline-block;
}

.vertical-content {
  writing-mode: vertical-rl;
  text-orientation: mixed;
  font-family: "Yu Mincho", "Hiragino Mincho ProN", serif;
  font-size: 1.125rem;
  line-height: 2;
  block-size: 320px;
}

.vertical-content h3 {
  font-weight: 700;
  letter-spacing: 0.1em;
  margin-inline-end: 1.5rem;
}

.vertical-content .num {
  text-combine-upright: all; /* Renders '06', '05', '24' horizontally */
}
```

### Example 3: Bidirectional Isolation for User-Generated Handles
```html
<p class="notification" dir="rtl">
  قام المستخدم <bdi class="username">@john_doe_99</bdi> بالتعليق على منشورك.
</p>
```

```css
.notification {
  font-family: system-ui, sans-serif;
  font-size: 1rem;
  line-height: 1.5;
  text-align: start;
}

/* Explicit CSS BiDi Isolation if using generic tags */
.username-isolated {
  unicode-bidi: isolate;
  direction: ltr;
}
```

### Example 4: Stylistic Vertical Header in Western Architecture
```css
/* Creates a chic, modern vertical side-label on an editorial card */
.editorial-side-label {
  writing-mode: vertical-rl;
  text-orientation: mixed;
  transform: rotate(180deg); /* Reads cleanly from bottom-to-top */
  font-size: 0.75rem;
  font-weight: 700;
  text-transform: uppercase;
  letter-spacing: 0.15em;
  color: #94a3b8;
  padding-block: 1rem;
}
```

### Example 5: Upright Latin Acronyms in Vertical Columns
```css
/* Vertical navigation spine with upright letters */
.vertical-spine-nav {
  writing-mode: vertical-rl;
  text-orientation: upright; /* Keeps letters standing upright without 90deg tilt */
  font-family: "Inter", sans-serif;
  font-weight: 700;
  font-size: 0.875rem;
  letter-spacing: 0.1em;
  padding: 1rem;
  background-color: #0f172a;
  color: #38bdf8;
}
```

---

## 6. Best Practices

1. **Always Set `dir="rtl"` in the HTML Tag**: Never rely solely on CSS `direction: rtl`. Screen readers and browser fallback algorithms parse the HTML DOM before styles load.
2. **Use Logical Properties Universally**: Replace `margin-left`/`right`, `padding-left`/`right`, `left`/`right`, `width`/`height` with `margin-inline`, `padding-inline`, `inset-inline`, and `inline-size`.
3. **Use `<bdi>` for Dynamic User Content in BiDi Environments**: Wrap foreign usernames, emails, or phone numbers in `<bdi>` or apply `unicode-bidi: isolate;` to avoid punctuation scrambling.
4. **Use Tate-chu-yoko for 2-to-3 Digit Numbers in Vertical Text**: Set `text-combine-upright: all;` on small numeric runs in vertical Japanese/Chinese.
5. **Remember That Coordinate Axes Rotate**: In `vertical-rl`, horizontal scrolling becomes vertical scrolling and `overflow-x` scrolls vertically generated columns.
6. **Set `lang="..."` on Language Sections**: Always declare `lang="ja"`, `lang="ar"`, or `lang="zh"` so browsers select appropriate localized glyphs and punctuation rules.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Correction |
| :--- | :--- | :--- |
| **Declaring `direction: rtl` only in CSS** | If CSS is disabled or delayed, HTML renders in LTR, breaking reading flow for assistive tech. | Always set `<html dir="rtl">` directly in HTML markup. |
| **Using physical `left: 20px` in RTL layouts** | Hardcoded physical positions do not mirror, forcing manual overrides for every component. | Use logical `inset-inline-start: 20px;`. |
| **Forgetting `unicode-bidi: isolate` on mixed text** | Punctuation marks (parentheses, periods, colons) flip to the wrong end of English strings in RTL. | Wrap in `<bdi>` or declare `unicode-bidi: isolate;`. |
| **Setting `text-combine-upright: all` on long words** | Squashes 10-letter words into a grotesque, illegible horizontal smear inside a single em-square. | Restrict `text-combine-upright` to 2–4 digits maximum. |
| **Using `width: 100%` on a vertical writing mode block** | In vertical mode, `width` controls block size (column stacking), not line length. | Use `block-size` and `inline-size` for predictable layout. |
| **Assuming all vertical scripts are `vertical-rl`** | Mongolian script progresses from left to right. | Use `writing-mode: vertical-lr;` for Mongolian typography. |

---

## 8. Performance, Accessibility & Browser Support

### Accessibility Compliance (WCAG 2.2 AA)
- **Criterion 3.1.1 (Language of Page)**: The default human language of each web page must be programmatically determined via `<html lang="...">`.
- **Criterion 3.1.2 (Language of Parts)**: Sections in a foreign language (e.g., a Japanese quote in an English article) must have localized `lang` attributes (`<blockquote lang="ja">`).

### Bidirectional Neutral Characters
Punctuation marks (periods, parentheses, exclamation points) and spaces have **neutral directionality** in Unicode. They inherit the directionality of surrounding characters. When an English URL sits at the end of an Arabic sentence, the terminating period often jumps to the far right unless properly isolated with `unicode-bidi: isolate`.

---

## 9. Comparison / When to Use What

| Mode | Direction | Line Progression | Primary Scripts / Use Cases |
| :--- | :--- | :--- | :--- |
| `horizontal-tb` | LTR | Top to Bottom | Latin, Cyrillic, Greek, Modern CJK |
| `horizontal-tb` | RTL | Top to Bottom | Arabic, Hebrew, Urdu, Persian |
| `vertical-rl` | TTB | Right to Left | Traditional Japanese, Chinese literature, vertical UI spines |
| `vertical-lr` | TTB | Left to Right | Traditional Mongolian script |

---

## 10. Interview Questions

<details>
<summary>1. Why must the text direction of an RTL web application be declared on the HTML element via <code>dir="rtl"</code> rather than solely in CSS?</summary>

Text directionality is a core semantic property of content, not merely a stylistic decoration. 
1. The HTML parser and accessibility tree process the `dir` attribute immediately, allowing screen readers to interpret punctuation and character order correctly before external stylesheets finish downloading.
2. Form input controls (`<input>`, `<textarea>`) and browser UI dialogs (alerts, prompts) infer directional defaults directly from the HTML document's `dir` attribute.
3. If an external stylesheet fails to load or is blocked by network latency, content declared with `<html dir="rtl">` continues to render in correct bidirectional order.
</details>

<details>
<summary>2. How do CSS Logical Properties relate to <code>writing-mode</code>?</summary>

Physical properties (`width`, `height`, `margin-top`, `padding-left`) are tied to physical screen coordinates. CSS Logical Properties (`inline-size`, `block-size`, `margin-block-start`, `padding-inline-end`) are mapped dynamically relative to the flow of content:
- In `horizontal-tb`: `inline-size` is width; `block-size` is height.
- In `vertical-rl`: `inline-size` is **height** (the length of the vertical line); `block-size` is **width** (the column progression).
Using logical properties guarantees components maintain correct geometric relationships regardless of writing mode or direction.
</details>

<details>
<summary>3. What is the Unicode Bidirectional Algorithm (BiDi), and how does <code>unicode-bidi: isolate</code> interact with it?</summary>

The Unicode BiDi algorithm determines the visual display order of characters with varying intrinsic directionality (e.g., strong RTL Arabic characters mixed with strong LTR Latin numbers or neutral punctuation). 

By default, an inline LTR fragment inside an RTL sentence interacts with neighboring characters, causing neutral punctuation like colons or brackets to render on the wrong side. `unicode-bidi: isolate` instructs the layout engine to treat the element as an independent, self-contained directional island, preventing its internal directionality from leaking out and corrupting surrounding text.
</details>

<details>
<summary>4. What is Tate-chu-yoko and which CSS property enables it?</summary>

Tate-chu-yoko (縦中横, "horizontal in vertical") is an East Asian typographic convention where short horizontal Latin characters or numbers (typically 2 to 4 digits, like dates or chapter numbers) are set horizontally inside a single vertical glyph em-square in a vertical text flow. 

In CSS, this is enabled using `text-combine-upright: all;`.
</details>

<details>
<summary>5. What is the difference between <code>text-orientation: mixed</code> and <code>text-orientation: upright</code> in vertical writing modes?</summary>

- `text-orientation: mixed` (the default) keeps full-width CJK ideographs upright, but rotates Latin and Western alphanumeric glyphs 90 degrees clockwise (flowing down the line like sideways text).
- `text-orientation: upright` forces all characters—including Latin letters and numbers—to stand straight up, stacking vertically one on top of the other.
</details>

<details>
<summary>6. How does <code>overflow-x</code> behave in a container styled with <code>writing-mode: vertical-rl</code>?</summary>

In `vertical-rl`, the block progression axis flows from right to left horizontally. Because lines stack horizontally along the X-axis, overflow occurs along the X-axis. Consequently, `overflow-x: auto;` creates a horizontal scroll container where new columns appear on the left, and the initial scroll position starts at the extreme right edge.
</details>

<details>
<summary>7. What does the HTML <code>&lt;bdi&gt;</code> element do?</summary>

`<bdi>` (Bidirectional Isolate) isolates a span of text that might be formatted in a different direction from the surrounding text without knowing its exact direction in advance (such as dynamic usernames or database values). It acts as a semantic HTML equivalent of applying `unicode-bidi: isolate;` in CSS.
</details>

<details>
<summary>8. Why should you avoid <code>text-align: left</code> when authoring multi-lingual UI components?</summary>

`text-align: left` locks text to the physical left boundary. In an RTL language like Arabic or Hebrew, natural reading starts at the right boundary. Hardcoding `left` forces developers to write manual overrides (`[dir="rtl"] .element { text-align: right; }`). Using `text-align: start;` automatically computes to `left` in LTR and `right` in RTL with zero overrides.
</details>

---

## 11. Practice Exercises

### Exercise 1: Build a Direction-Agnostic Media Card
**Goal**: Create a media object with an image and text description that mirrors automatically in RTL without writing extra CSS classes.

<details>
<summary>View Solution</summary>

```css
.media-card {
  display: flex;
  align-items: center;
  padding: 1rem;
  border: 1px solid #cbd5e1;
  border-radius: 8px;
}

.media-thumbnail {
  inline-size: 64px;
  block-size: 64px;
  border-radius: 6px;
  margin-inline-end: 1rem; /* Automatically mirrors in RTL */
  flex-shrink: 0;
}

.media-content {
  text-align: start; /* Automatically mirrors in RTL */
}
```
</details>

### Exercise 2: Vertical Japanese Book Excerpt with Tate-chu-yoko
**Goal**: Lay out a vertical Japanese poetry block with 2-digit dates embedded horizontally.

<details>
<summary>View Solution</summary>

```html
<div class="japanese-novel" lang="ja">
  <p>昭和<span class="tate">59</span>年、東京にて執筆。</p>
</div>
```

```css
.japanese-novel {
  writing-mode: vertical-rl;
  text-orientation: mixed;
  font-family: "Yu Mincho", serif;
  font-size: 1.125rem;
  line-height: 2;
  block-size: 250px;
}

.tate {
  text-combine-upright: all;
}
```
</details>

### Exercise 3: Isolate Dynamic Usernames in RTL Chat
**Goal**: Safely render foreign LTR usernames inside an Arabic chat bubble without scrambling the trailing colon.

<details>
<summary>View Solution</summary>

```html
<div class="chat-bubble" dir="rtl">
  <span class="user-handle" style="unicode-bidi: isolate;">@alex_dev_99</span>: أهلاً بك في الفريق!
</div>
```

```css
.chat-bubble {
  font-family: system-ui, sans-serif;
  text-align: start;
  padding: 0.75rem 1rem;
  background-color: #f1f5f9;
  border-radius: 8px;
}
```
</details>

### Exercise 4: Construct an Upright Vertical Side-Banner
**Goal**: Build a vertical banner with upright Latin letters for a modern web app sidebar.

<details>
<summary>View Solution</summary>

```css
.vertical-badge {
  writing-mode: vertical-rl;
  text-orientation: upright;
  font-family: system-ui, sans-serif;
  font-weight: 800;
  font-size: 0.8125rem;
  letter-spacing: 0.1em;
  padding: 1rem 0.5rem;
  background-color: #2563eb;
  color: #ffffff;
  border-radius: 4px;
}
```
</details>

### Exercise 5: RTL-Safe Absolute Positioning
**Goal**: Position a close button in the top-inline-end corner of a modal dialog so it adapts across both LTR and RTL.

<details>
<summary>View Solution</summary>

```css
.modal-dialog {
  position: relative;
  inline-size: 100%;
  max-inline-size: 500px;
  padding: 2rem;
  border-radius: 12px;
}

.modal-close-btn {
  position: absolute;
  inset-block-start: 1rem; /* Top in both */
  inset-inline-end: 1rem;   /* Right in LTR, Left in RTL */
  background: none;
  border: none;
  cursor: pointer;
}
```
</details>

### Exercise 6: Multi-Lingual Font Fallback Chain
**Goal**: Create a font stack that seamlessly falls back to appropriate native fonts for English, Arabic, and Japanese.

<details>
<summary>View Solution</summary>

```css
body {
  font-family:
    "Inter",                    /* Latin primary */
    "Geeza Pro", "Arial",       /* Arabic fallback */
    "Hiragino Sans", "Yu Gothic",/* Japanese fallback */
    sans-serif;
}
```
</details>

---

## 12. Summary / Cheat Sheet

| Property | Values | Purpose |
| :--- | :--- | :--- |
| `writing-mode` | `horizontal-tb \| vertical-rl \| vertical-lr` | Sets horizontal vs vertical text flow |
| `dir` (HTML) | `ltr \| rtl \| auto` | Semantic bidirectional root declaration |
| `text-orientation` | `mixed \| upright \| sideways` | Western character angle in vertical mode |
| `text-combine-upright` | `all \| digits [2-4]` | Tate-chu-yoko (horizontal numbers in vertical text) |
| `unicode-bidi` | `isolate \| embed \| bidi-override` | Isolates mixed-direction character strings |
| `text-align` | `start \| end` | Direction-aware inline text alignment |

---

## 13. Further Reading
- [W3C CSS Writing Modes Level 3](https://www.w3.org/TR/css-writing-modes-3/)
- [MDN Web Docs: writing-mode](https://developer.mozilla.org/en-US/docs/Web/CSS/writing-mode)
- [W3C Internationalization: Structural markup and right-to-left text in HTML](https://www.w3.org/International/questions/qa-html-dir)
- [web.dev: CSS Logical Properties](https://web.dev/learn/css/logical-properties/)
- [W3C: Requirements for Japanese Text Layout (JLReq)](https://www.w3.org/TR/jlreq/)

---

[Previous: 06-text-wrap-and-hyphenation.md](06-text-wrap-and-hyphenation.md) | [Module README](README.md) | [Next: 08-text-overflow-and-clamping.md](08-text-overflow-and-clamping.md)
