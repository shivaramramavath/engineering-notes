# Logical Properties and Writing Modes

| Metadata | Details |
| :--- | :--- |
| **Level** | Core / Modern Standard |
| **Prerequisites** | `02-box-model-and-layout-basics/01-the-box-model.md`, `02-box-model-and-layout-basics/03-margin-and-padding.md` |
| **Estimated Reading Time** | 22 minutes |
| **Status** | Complete |
| **Baseline Status** | Widely Available across all modern browsers (Baseline 2021) |
| **Browsers Tested** | Chrome 120+, Firefox 121+, Safari 17.2+ |
| **Tested With** | Automated CSS Validation Runner (`css_runner.js`) |

---

## Table of Contents
- [1 What & Why](#1-what--why)
- [2 Core Concepts](#2-core-concepts)
- [3 How It Works Internally](#3-how-it-works-internally)
- [4 Syntax & Usage](#4-syntax--usage)
- [5 Practical Examples](#5-practical-examples)
- [6 Best Practices](#6-best-practices)
- [7 Common Mistakes & Pitfalls](#7-common-mistakes--pitfalls)
- [8 Performance, Accessibility & Browser Support](#8-performance-accessibility--browser-support)
- [9 Comparison / When to Use What](#9-comparison--when-to-use-what)
- [10 Interview Questions](#10-interview-questions)
- [11 Practice Exercises](#11-practice-exercises)
- [12 Summary / Cheat Sheet](#12-summary--cheat-sheet)
- [13 Further Reading](#13-further-reading)

---

## 1 What & Why

Historically, CSS was built around physical screen geometry: `top`, `right`, `bottom`, and `left`. This physical paradigm works well when websites are rendered exclusively in Left-to-Right (LTR) horizontal scripts (such as English, French, or Spanish).

However, the modern web is global. Millions of users read languages in:
- **Right-to-Left (RTL)** scripts: Arabic, Hebrew, Persian, Urdu.
- **Vertical-to-Horizontal** scripts: Traditional Japanese, Chinese, or Mongolian.

In the past, internationalizing a website required cumbersome separate stylesheets or tedious class overrides (e.g., `body.rtl .button { margin-left: 0; margin-right: 12px; }`).

**CSS Logical Properties and Values** replace rigid physical directions with semantic, flow-relative terms: **Block** (the direction blocks stack) and **Inline** (the direction sentences flow). By adopting logical properties, your layouts automatically adapt to any language, direction, or writing mode with zero override CSS!

---

## 2 Core Concepts

### Physical vs. Logical Dimensions

Instead of fixed X and Y axes, modern CSS operates across two dynamic axes:

1. **Inline Axis**:
   - The direction text flows within a line.
   - In English (LTR horizontal): Left to Right.
   - In Arabic/Hebrew (RTL horizontal): Right to Left.
   - In Japanese (vertical-rl): Top to Bottom.
2. **Block Axis**:
   - The direction paragraphs or block containers stack.
   - In English/Arabic (horizontal-tb): Top to Bottom.
   - In Japanese (vertical-rl): Right to Left.

```
+-------------------------------------------------------------+
| Standard English (horizontal-tb, LTR)                       |
|                                                             |
|               block-start (Physical: TOP)                   |
|                        ▲                                    |
|                        │                                    |
|   inline-start ────────┼────────► inline-end                |
| (Physical: LEFT)       │          (Physical: RIGHT)         |
|                        ▼                                    |
|                block-end (Physical: BOTTOM)                 |
+-------------------------------------------------------------+
| Arabic / Hebrew (horizontal-tb, RTL)                        |
|                                                             |
|               block-start (Physical: TOP)                   |
|                        ▲                                    |
|                        │                                    |
|   inline-end   ◄───────┼────────  inline-start              |
| (Physical: LEFT)       │          (Physical: RIGHT)         |
|                        ▼                                    |
|                block-end (Physical: BOTTOM)                 |
+-------------------------------------------------------------+
```

### Complete Physical-to-Logical Mapping

| Physical Property | Modern Logical Equivalent | Meaning in LTR Horizontal | Meaning in RTL Horizontal |
| :--- | :--- | :--- | :--- |
| `width` | `inline-size` | Width | Width |
| `height` | `block-size` | Height | Height |
| `min-width` / `max-width` | `min-inline-size` / `max-inline-size` | Min/Max Width | Min/Max Width |
| `min-height` / `max-height` | `min-block-size` / `max-block-size` | Min/Max Height | Min/Max Height |
| `margin-top` | `margin-block-start` | Top Margin | Top Margin |
| `margin-bottom` | `margin-block-end` | Bottom Margin | Bottom Margin |
| `margin-left` | `margin-inline-start` | Left Margin | **Right Margin** |
| `margin-right` | `margin-inline-end` | Right Margin | **Left Margin** |
| `padding-left` | `padding-inline-start` | Left Padding | **Right Padding** |
| `padding-right` | `padding-inline-end` | Right Padding | **Left Padding** |
| `border-left` | `border-inline-start` | Left Border | **Right Border** |
| `border-right` | `border-inline-end` | Right Border | **Left Border** |
| `text-align: left` | `text-align: start` | Left | **Right** |
| `text-align: right` | `text-align: end` | Right | **Left** |
| `top`, `bottom` | `inset-block-start`, `inset-block-end` | Top / Bottom offset | Top / Bottom offset |
| `left`, `right` | `inset-inline-start`, `inset-inline-end` | Left / Right offset | **Right / Left offset** |

---

## 3 How It Works Internally

The browser layout engine calculates physical boundaries at computed-value time based on the active `writing-mode` and `direction` properties:

1. **`writing-mode`**: Controls whether lines run horizontally (`horizontal-tb`) or vertically (`vertical-rl`, `vertical-lr`).
2. **`direction`**: Controls inline text direction (`ltr` vs. `rtl`).

When the engine evaluates `margin-inline-start: 16px`:
- If `direction: ltr`, it maps internally to physical `margin-left: 16px`.
- If `direction: rtl`, it maps internally to physical `margin-right: 16px`.

This mapping occurs dynamically with zero layout recalibration or style re-injection cost.

---

## 4 Syntax & Usage

```css
.card {
  /* Dimensions */
  inline-size: min(100%, 480px); /* Replaces width: min(100%, 480px) */
  min-block-size: 200px;         /* Replaces min-height: 200px */

  /* Spacing */
  padding-block: 1.5rem;         /* Top and bottom padding */
  padding-inline: 1.25rem;       /* Left and right padding */
  margin-block-end: 2rem;        /* Bottom margin */

  /* Borders */
  border-inline-start: 4px solid #2563eb; /* Accent border on reading edge */

  /* Text Alignment */
  text-align: start;             /* Aligns left in LTR, right in RTL */
}
```

> [!NOTE]
> **Baseline Status: Widely Available**
> CSS Logical Properties and Values (`margin-inline`, `padding-block`, `inset-inline`, `inline-size`, etc.) are Baseline Widely Available across all modern browsers (Chrome 89+, Firefox 66+, Safari 15+).

---

## 5 Practical Examples

### Example 1: Icon Button with Direction-Aware Spacing

```css
.icon-button {
  display: inline-flex;
  align-items: center;
  padding-block: 8px;
  padding-inline: 16px;
  border-radius: 6px;
  background-color: #2563eb;
  color: #ffffff;
  font-weight: 500;
  border: none;
  cursor: pointer;
}

/* Icon margin is on the reading-end (right in LTR, left in RTL) */
.icon-button .button-icon {
  margin-inline-end: 8px;
  inline-size: 18px;
  block-size: 18px;
}
```

### Example 2: Blockquote with Accent Line on the Reading Edge

```css
.editorial-quote {
  /* Accent line on leading inline edge */
  border-inline-start: 4px solid #0284c7;
  padding-inline-start: 16px;
  padding-block: 8px;
  margin-inline: 0;
  margin-block: 24px;
  background-color: #f0f9ff;
  color: #0369a1;
  font-style: italic;
}
```

### Example 3: Edge Case — Absolute Badge on Avatar with Logical Offsets

```css
.user-avatar-wrapper {
  position: relative;
  display: inline-block;
  inline-size: 56px;
  block-size: 56px;
}

.avatar-img {
  inline-size: 100%;
  block-size: 100%;
  border-radius: 50%;
  object-fit: cover;
}

/* Pinned to top-end corner (top-right in LTR, top-left in RTL) */
.user-status-dot {
  position: absolute;
  inset-block-start: 2px;
  inset-inline-end: 2px;
  inline-size: 12px;
  block-size: 12px;
  border-radius: 50%;
  background-color: #22c55e;
  box-shadow: 0 0 0 2px #ffffff;
}
```

### Example 4: Failure Case — Mixing Physical and Logical Declarations

```css
/* FAILURE: Specificity & Cascade conflict */
.callout-box {
  padding-left: 20px;          /* Physical */
  padding-inline-start: 30px;  /* Logical */
  /* In CSS Cascade, physical and logical properties for the same edge
     compete at the same declaration level. The LAST declaration wins! */
}

/* FIXED: Standardize 100% on logical properties across the design system */
.callout-box-clean {
  padding-block: 16px;
  padding-inline: 24px;
}
```

### Example 5: Internationalized Centered Card Layout

```css
.centered-container {
  /* Margin-inline auto centers horizontally in both LTR and RTL */
  margin-inline: auto;
  inline-size: min(90vw, 720px);
  padding-block: 48px;
}

.form-group {
  margin-block-end: 20px;
}

.form-label {
  display: block;
  margin-block-end: 6px;
  font-weight: 600;
  text-align: start; /* Automatically aligns to appropriate reading edge */
}
```

---

## 6 Best Practices

1. **Adopt logical properties as your default writing style**: Use `padding-inline` and `margin-block` routinely, even for sites currently written only in English. It future-proofs the codebase.
2. **Use `text-align: start` instead of `text-align: left`**: This ensures headings and paragraphs automatically align properly when switched to an RTL language.
3. **Use `margin-inline: auto` for horizontal centering**: Replaces `margin: 0 auto;` without inadvertently resetting vertical block margins.
4. **Never mix physical and logical properties for the same element**: Avoid combining `margin-left` and `margin-inline-start`, as precedence depends strictly on source order.
5. **Set `<html dir="rtl">` or `<html dir="ltr">` in HTML**: Let CSS logical properties respond natively to the document `dir` attribute rather than toggling CSS classes.
6. **Use logical border-radius for asymmetric corners**: `border-start-start-radius` and `border-start-end-radius` flip automatically in RTL contexts.

---

## 7 Common Mistakes & Pitfalls

| Mistake | Consequence | Corrective Action |
| :--- | :--- | :--- |
| Hardcoding `margin-left: 12px` on icons | Icon sticks to wrong side of label in RTL | Use `margin-inline-end: 12px` |
| Using `text-align: left` for general text | Text stays left-aligned in Arabic/Hebrew | Use `text-align: start` |
| Mixing `padding-left` and `padding-inline-start` | Unpredictable override bugs based on CSS rule order | Stick strictly to logical properties |
| Using `left: 0` for flyout menus | Menu pops open on the wrong side in RTL | Use `inset-inline-start: 0` |
| Resetting vertical margin with `margin: 0 auto` | Unintentionally wipes existing block margins | Use `margin-inline: auto` |
| Asymmetric physical border radius on tabs | Rounded tab corners face wrong direction in RTL | Use `border-start-start-radius` |

---

## 8 Performance, Accessibility & Browser Support

### Layout Performance

- CSS Logical Properties introduce **zero performance overhead**. They are resolved during style computation alongside physical values.
- Modern CSS engines (Blink, Gecko, WebKit) compile logical tokens into internal layout offsets with zero additional reflow penalty compared to legacy physical properties.

### Accessibility & Internationalization (WCAG & W3C i18n)

- Proper handling of RTL reading order ensures that screen readers, visual focus rings, and cognitive expectations align with native speakers' natural eye-tracking patterns.
- By structuring layouts with logical properties and setting `<html lang="ar" dir="rtl">`, assistive technologies and browser auto-translation tools render flawless mirrored UI without clipped text.

---

## 9 Comparison / When to Use What

| Scenario | Physical Property | Modern Logical Replacement | Why Logical is Better |
| :--- | :--- | :--- | :--- |
| Center a block element | `margin: 0 auto;` | `margin-inline: auto;` | Preserves existing vertical block margins |
| Card horizontal padding | `padding: 0 16px;` | `padding-inline: 16px;` | Clean, self-documenting, preserves block padding |
| Leading list bullet accent | `border-left: 3px solid blue;` | `border-inline-start: 3px solid blue;` | Automatically flips to right border in RTL |
| Icon next to button text | `margin-right: 8px;` | `margin-inline-end: 8px;` | Maintains correct spacing regardless of script |
| Fullscreen modal pinned | `top: 0; left: 0; right: 0; bottom: 0;` | `inset: 0;` | 75% shorter, universally clean |

---

## 10 Interview Questions

<details>
<summary><strong>1. What are the two primary axes defined in CSS Logical Properties, and how do they differ from the physical X and Y axes?</strong></summary>

The two axes are the **Inline Axis** and the **Block Axis**. The inline axis corresponds to the direction words flow within a single line of text (left-to-right in English, right-to-left in Arabic, top-to-bottom in vertical Japanese). The block axis corresponds to the direction blocks and paragraphs stack (top-to-bottom in horizontal writing modes, right-to-left in vertical writing modes). Unlike fixed physical X/Y axes, logical axes dynamically adjust based on `writing-mode` and `direction`.
</details>

<details>
<summary><strong>2. What is the difference between <code>text-align: left</code> and <code>text-align: start</code>?</strong></summary>

`text-align: left` strictly forces text against the physical left screen boundary regardless of language. In contrast, `text-align: start` aligns text to the start edge of the inline axis: left in LTR languages (English) and right in RTL languages (Arabic, Hebrew).
</details>

<details>
<summary><strong>3. How does <code>margin-inline: auto</code> improve upon <code>margin: 0 auto</code>?</strong></summary>

`margin: 0 auto` sets both the top and bottom margins to `0`, which often unintentionally wipes out vertical spacing established by higher-level component rules. `margin-inline: auto` specifically centers the element along the inline axis while leaving block (vertical) margins completely untouched.
</details>

<details>
<summary><strong>4. What happens when an element has both <code>margin-left: 20px</code> and <code>margin-inline-start: 10px</code> declared?</strong></summary>

Because both properties resolve to the same physical edge in an LTR context, the standard CSS Cascade rules apply. Because both have identical selector specificity, whichever declaration appears **last in the source code** wins.
</details>

<details>
<summary><strong>5. What are the logical equivalents of <code>width</code> and <code>height</code>?</strong></summary>

`width` maps to `inline-size` (the dimension along the inline text axis), and `height` maps to `block-size` (the dimension along the block stacking axis).
</details>

<details>
<summary><strong>6. How do you create an asymmetric pill button whose leading edge is flat and trailing edge is round in both LTR and RTL?</strong></summary>

Using logical border radius properties:
```css
border-start-start-radius: 0;
border-end-start-radius: 0;
border-start-end-radius: 9999px;
border-end-end-radius: 9999px;
```
This automatically mirrors when switched to an RTL writing mode.
</details>

<details>
<summary><strong>7. How does the browser know which direction is "start" and which is "end"?</strong></summary>

It computes this from the element's effective `direction` (`ltr` or `rtl`, typically inherited from `<html dir="...">`) and `writing-mode` (`horizontal-tb`, `vertical-rl`, `vertical-lr`).
</details>

<details>
<summary><strong>8. Is there any performance penalty for using CSS logical properties over physical ones?</strong></summary>

No. Modern browser engines map logical properties directly to physical layout dimensions during the style computation phase with zero additional layout or reflow overhead.
</details>

---

## 11 Practice Exercises

### Exercise 1 (Easy): Modern Horizontal Centering
- **Task**: Center a container horizontally with a max width of 600px without resetting vertical margins.

<details>
<summary>View Solution</summary>

```css
.container {
  max-inline-size: 600px;
  margin-inline: auto;
}
```
</details>

### Exercise 2 (Easy): Direction-Aware Search Input
- **Task**: Style a search input with a magnifying glass icon on the start side and a clear button on the end side.

<details>
<summary>View Solution</summary>

```css
.search-input {
  inline-size: 100%;
  padding-block: 8px;
  padding-inline-start: 36px; /* Space for search icon */
  padding-inline-end: 36px;   /* Space for clear button */
  border: 1px solid #cbd5e1;
  border-radius: 6px;
  text-align: start;
}
```
</details>

### Exercise 3 (Medium): RTL-Ready Notification Toast
- **Task**: Style a notification toast with a colored border bar on the reading edge and a close button on the trailing edge.

<details>
<summary>View Solution</summary>

```css
.toast {
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding-block: 12px;
  padding-inline: 16px;
  background-color: #ffffff;
  border-inline-start: 4px solid #10b981;
  box-shadow: 0 4px 6px -1px rgba(0, 0, 0, 0.1);
  border-radius: 4px;
}

.toast-close-btn {
  margin-inline-start: 16px;
  background: transparent;
  border: none;
  cursor: pointer;
}
```
</details>

### Exercise 4 (Medium): Logical Floating Card with Inset
- **Task**: Anchor an absolute badge 12px from the top-end corner of an article card.

<details>
<summary>View Solution</summary>

```css
.card {
  position: relative;
  inline-size: 300px;
  border: 1px solid #e2e8f0;
  border-radius: 8px;
  padding: 16px;
}

.card-badge {
  position: absolute;
  inset-block-start: 12px;
  inset-inline-end: 12px;
  background-color: #f59e0b;
  color: #ffffff;
  padding: 2px 8px;
  border-radius: 4px;
  font-size: 12px;
}
```
</details>

### Exercise 5 (Hard): Mirrored Tab Navigation with Rounded Top Corners
- **Task**: Build tabs where each tab has rounded top corners (`start-start` and `start-end`) and proper inline spacing.

<details>
<summary>View Solution</summary>

```css
.tab-list {
  display: flex;
  border-block-end: 2px solid #e2e8f0;
  list-style: none;
  padding: 0;
  margin: 0;
}

.tab-item {
  padding-block: 10px;
  padding-inline: 20px;
  border-start-start-radius: 6px;
  border-start-end-radius: 6px;
  cursor: pointer;
  background: #f1f5f9;
  margin-inline-end: 4px;
}

.tab-item.active {
  background: #ffffff;
  border: 2px solid #e2e8f0;
  border-block-end-color: transparent;
  margin-block-end: -2px;
}
```
</details>

### Exercise 6 (Hard): Vertical Japanese Writing Mode Article
- **Task**: Format a poetry container in traditional vertical Japanese writing mode (`vertical-rl`).

<details>
<summary>View Solution</summary>

```css
.japanese-poetry-panel {
  writing-mode: vertical-rl;
  block-size: 400px;
  inline-size: 100%;
  padding-block: 24px;
  padding-inline: 32px;
  border-inline-start: 1px solid #cbd5e1;
  font-family: "Noto Serif JP", serif;
  line-height: 2;
}
```
</details>

---

## 12 Summary / Cheat Sheet

```css
/* Dimensions */
inline-size: 100%;         /* width */
block-size: 200px;          /* height */

/* Spacing */
margin-inline: auto;        /* margin-left & margin-right */
margin-block: 1rem 2rem;    /* margin-top & margin-bottom */
padding-inline: 16px;       /* padding-left & padding-right */
padding-block: 12px;        /* padding-top & padding-bottom */

/* Offsets */
inset-inline-start: 0;      /* left (LTR) / right (RTL) */
inset-inline-end: 0;        /* right (LTR) / left (RTL) */
inset-block-start: 0;       /* top */
inset-block-end: 0;         /* bottom */

/* Text Alignment */
text-align: start;          /* left (LTR) / right (RTL) */
text-align: end;            /* right (LTR) / left (RTL) */
```

---

## 13 Further Reading

- [MDN Web Docs: CSS Logical Properties and Values](https://developer.mozilla.org/en-US/docs/Web/CSS/CSS_Logical_Properties)
- [W3C CSS Logical Properties and Values Level 1](https://www.w3.org/TR/css-logical-1/)
- [web.dev: Learn CSS - Logical Properties](https://web.dev/learn/css/logical-properties/)
- [Smashing Magazine: Understanding Logical Properties And Values](https://www.smashingmagazine.com/2018/03/understanding-logical-properties-values/)

---

[Previous: 11-overflow-and-clipping.md](file:///e:/Learning/engineering-notes/01-languages/css/02-box-model-and-layout-basics/11-overflow-and-clipping.md) | [Section Index](file:///e:/Learning/engineering-notes/01-languages/css/02-box-model-and-layout-basics/README.md) | [Next: 13-box-alignment-basics.md](file:///e:/Learning/engineering-notes/01-languages/css/02-box-model-and-layout-basics/13-box-alignment-basics.md)
