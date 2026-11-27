# CSS Resets and Normalisation

| Metadata | Details |
| :--- | :--- |
| **Spec Level** | Modern Reset Architectures, CSS Baseline Normalization & CSSWG Standards |
| **Baseline Status** | Universal CSS Engineering Standards |
| **Target Audience** | Frontend Engineers, UI Architects, Web Developers |
| **Tested With** | Blink, WebKit, Gecko, Modern Production Reset Suites |

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

Every web browser includes an internal **User Agent (UA) Stylesheet** that dictates default appearance before any custom CSS loads:
- WebKit (Safari), Blink (Chrome/Edge), and Gecko (Firefox) each define slightly different margins on `<h1>`–`<h6>`, different paddings on `<ul>` and `<ol>`, different default button borders, and different font sizes.
- Unset inputs, tables, and images exhibit quirky legacy behaviors (e.g. `img` behaves as an inline element leaving an accidental 4px gap underneath, text inputs don't inherit typography fonts).

Over the past two decades, frontend engineering transitioned through three distinct eras to tackle this inconsistency:
1. **The Scorched Earth Era (Eric Meyer Reset, 2007)**: Stripped *every* margin, padding, border, and font size down to zero. While predictable, it destroyed useful browser defaults (like strong text, list bullets, and emphasis).
2. **The Normalization Era (Normalize.css by Nicolas Gallagher, 2012)**: Rather than zeroing everything, it preserved useful defaults and corrected browser-specific quirks and bugs.
3. **The Modern Era (Modern Resets by Andy Bell & Josh Comeau, 2020–2026)**: Leverages modern platform features—`box-sizing: border-box`, responsive image behavior, logical properties, Cascade Layers (`@layer reset`), and zero-specificity `:where()`—to establish an ergonomic, sensible baseline without specificity side-effects.

---

## 2. Core Concepts

### Reset vs. Normalization vs. Modern Ergonomics
- **CSS Reset**: Zeroes out styles aggressively (e.g. `* { margin: 0; padding: 0; }`).
- **Normalize**: Retains browser defaults, harmonizing differences across browsers.
- **Modern Ergonomic Reset**: Fixes fundamental CSS design flaws (like `box-sizing: content-box` and inline images) while adopting modern standards (`text-wrap: balance`, `min-width: 0` for flex/grid containers).

### The Zero-Specificity Reset Architecture (`:where()`)
In traditional resets, writing:
```css
ul[class] {
  list-style: none;
}
```
introduces a specificity of $(0, 1, 1)$. If a component developer writes `ul { list-style: disc; }`, the reset selector wins because of the `[class]` attribute!

In modern CSS architecture, wrapping resets in **`:where()`** sets the specificity strictly to **$(0, 0, 0)$**:
```css
:where(ul[class]) {
  list-style: none; /* Specificity is (0, 0, 0)! Overridable by ANY selector! */
}
```

### The Cascade Layer Reset (`@layer reset`)
By placing the reset inside `@layer reset`, all reset rules are explicitly relegated to the lowest layer of the cascade:
```css
@layer reset, base, components, utilities;

@layer reset {
  *, *::before, *::after {
    box-sizing: border-box;
    margin: 0;
  }
}
```
Any rule authored in unlayered CSS or higher layers (`@layer base`, `@layer components`) effortlessly overrides reset declarations, completely eliminating specificity battles.

---

## 3. How It Works Internally

```
The 3 Eras of CSS Normalization:

1. Eric Meyer Reset (2007):
   html, body, div, span, h1, h2, p, ul... { margin: 0; padding: 0; border: 0; font-size: 100%; }
   * Scorched Earth: Deleted all margins, killed list bullets, destroyed semantic headings.

2. Normalize.css (2012):
   h1 { font-size: 2em; margin: 0.67em 0; }  <-- Preserved defaults across browsers
   button { font-family: inherit; }           <-- Fixed browser-specific form bugs

3. Modern Zero-Specificity @layer Reset (2024–2026):
   @layer reset {
     *, *::before, *::after { box-sizing: border-box; margin: 0; }
     :where(img, svg, video) { display: block; max-width: 100%; }
     :where(button, input, select, textarea) { font: inherit; }
     :where(h1, h2, h3) { text-wrap: balance; }
   }
   * Ergonomic defaults + zero-specificity + modern platform primitives!
```

```
Reset Rules Impact Breakdown:
┌──────────────────────────────┬────────────────────────────────────────────────────────┐
│ Modern Reset Rule            │ Problem Solved & Mechanical Benefit                   │
├──────────────────────────────┼────────────────────────────────────────────────────────┤
│ `box-sizing: border-box`     │ Includes padding and borders in element width/height.  │
│ `img { display: block; }`    │ Eliminates the annoying 4px inline baseline descender. │
│ `img { max-width: 100%; }`   │ Prevents high-resolution imagery from overflowing DOM. │
│ `button, input { font: inh;}`│ Forces form controls to use page typography fonts.     │
│ `min-width: 0`               │ Prevents flex and grid items from blowing out width.   │
│ `text-wrap: balance`         │ Eliminates typographic orphans in headlines.           │
└──────────────────────────────┴────────────────────────────────────────────────────────┘
```

---

## 4. Syntax & Usage

### The Authoritative 2026 Modern CSS Reset
A complete, battle-tested modern reset written for enterprise web standards:

```css
@layer reset {
  /* 1. Universal Box-Sizing & Margin Zeroing */
  *,
  *::before,
  *::after {
    box-sizing: border-box;
    margin: 0;
  }

  /* 2. Interactive Focus Root & Body Baseline */
  html:focus-within {
    scroll-behavior: smooth;
  }

  body {
    min-height: 100vh;
    min-height: 100dvh; /* Dynamic mobile viewport */
    line-height: 1.5;
    -webkit-font-smoothing: antialiased;
  }

  /* 3. Media & Embedded Content */
  :where(img, picture, video, canvas, svg) {
    display: block;
    max-width: 100%;
    height: auto;
  }

  /* 4. Form Controls Inheritance */
  :where(input, button, textarea, select) {
    font: inherit;
    color: inherit;
    letter-spacing: inherit;
  }

  /* 5. Text Area Resizing & Field Sizing */
  :where(textarea) {
    resize: vertical;
    field-sizing: content;
  }

  /* 6. Typographic Orphan Prevention */
  :where(h1, h2, h3, h4, h5, h6) {
    text-wrap: balance;
    line-height: 1.2;
  }

  :where(p, li, figcaption) {
    text-wrap: pretty;
  }

  /* 7. Unstyled Lists with Classes */
  :where(ul, ol)[class] {
    list-style: none;
    padding: 0;
  }

  /* 8. Accessible Reduced Motion Override */
  @media (prefers-reduced-motion: reduce) {
    html:focus-within {
      scroll-behavior: auto;
    }

    *,
    *::before,
    *::after {
      animation-duration: 0.01ms !important;
      animation-iteration-count: 1 !important;
      transition-duration: 0.01ms !important;
      scroll-behavior: auto !important;
    }
  }
}
```

---

## 5. Practical Examples

### Example 1: Solving the Infamous 4px Image Baseline Gap
By default, `<img>` is an inline element (`display: inline`). Because it sits on the typographic baseline, browsers leave space beneath it for descenders (like the tail on the letter `y` or `g`), creating an unexplained 3–4px gap underneath images inside cards.

```html
<!-- Problem: Gap visible underneath banner in default browser mode -->
<div class="card-media">
  <img src="banner.jpg" alt="Landscape">
</div>
```

```css
/* Modern Reset Fix: display: block eliminates inline baseline space */
:where(img) {
  display: block;
  max-width: 100%;
}
```

### Example 2: Form Controls Typography Inheritance
By default, `<input>`, `<button>`, and `<textarea>` do **not** inherit `font-family`, `font-size`, or `line-height` from `body`. They render in the browser's ugly default monospace or sans-serif system font.

```css
/* Reset makes form elements inherit page fonts naturally */
:where(input, button, textarea, select) {
  font: inherit;
}
```

### Example 3: The Unclassed List Pattern (`:where(ul:not([class]))`)
Preserve bullets on semantic lists in blog articles while automatically stripping bullets on navigational lists that have classes.

```html
<!-- Navigational list with a class: NO bullets -->
<ul class="nav-links">
  <li><a href="/">Home</a></li>
</ul>

<!-- Semantic editorial list without class: KEEPS bullets! -->
<article>
  <ul>
    <li>First point in research paper.</li>
    <li>Second point in research paper.</li>
  </ul>
</article>
```

```css
/* Only strip list styling if the list has a class assigned! */
:where(ul, ol)[class] {
  list-style: none;
  padding: 0;
}
```

### Example 4: Preventing Flexbox / Grid Blowouts
By default, flexbox and grid items have `min-width: auto`, which causes containers to blow out horizontally when encountering long text or wide tables.

```css
@layer reset {
  /* Prevent flex/grid items from exceeding parent width */
  :where(.flex-child, .grid-child) {
    min-width: 0;
  }
}
```

### Example 5: Typographic Orphan Prevention (`balance` and `pretty`)
Modern CSS resets automatically prevent single trailing words ("orphans") on headlines and paragraphs.

```css
@layer reset {
  /* Headings: Evenly balance line lengths */
  :where(h1, h2, h3, h4) {
    text-wrap: balance;
  }

  /* Body prose: Avoid single trailing orphan word at end of block */
  :where(p) {
    text-wrap: pretty;
  }
}
```

---

## 6. Best Practices

1. **Always wrap your reset inside `@layer reset`**: Guarantees that every reset declaration resides in the lowest layer of the cascade, allowing your design tokens and components to override it effortlessly.
2. **Use `:where()` to enforce $(0, 0, 0)$ specificity**: Never use high-specificity selectors in a reset. Wrapping rules in `:where(...)` ensures zero-specificity styling.
3. **Preserve list bullets on unclassed lists**: Writing `:where(ul, ol)[class] { list-style: none; }` strips bullets from navigation menus while preserving bullets in markdown-rendered articles.
4. **Always set `box-sizing: border-box` universally**: The single most important rule in web layout. Always include `*, *::before, *::after { box-sizing: border-box; }`.
5. **Never strip focus outlines in a reset without `:focus-visible`**: Writing `* { outline: none; }` is an accessibility disaster. If styling focus rings, always preserve native or custom `:focus-visible` rings.
6. **Include the accessible `0.01ms` reduced-motion pattern**: Ensures JavaScript components awaiting `transitionend` events still function when motion is disabled.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Consequence | Correction |
| :--- | :--- | :--- |
| `* { outline: none !important; }` | Completely blinds keyboard users; fails WCAG 2.4.7. | Never remove outline; style `:focus-visible` thoughtfully. |
| Resetting with high-specificity selectors | Component authors forced to use `!important` to override reset rules. | Wrap reset rules in `:where()` or `@layer reset`. |
| Stripping bullets from ALL lists: `ul { list-style: none; }` | Destroys bullet formatting in rich-text CMS blog articles. | Target classed lists only: `:where(ul)[class]`. |
| Setting `html { height: 100%; }` instead of `min-height` | Pages with long content clip or behave erratically. | Use `min-height: 100vh; min-height: 100dvh;`. |
| Forgetting `height: auto` on responsive images | Images with inline `height="400"` attributes distort aspect ratio. | Declare `height: auto;` alongside `max-width: 100%;`. |
| Over-aggressive input resets (`all: unset`) | Strips checkbox indicators and radio buttons completely, making them invisible. | Reset properties individually (`font`, `color`, `border`). |

---

## 8. Performance, Accessibility & Browser Support

### Performance
- **Zero Style Recalculation Overhead**: Wrapping resets in `@layer reset` allows modern browser style engines to compute baseline properties during initial style resolution without re-visiting the rule graph.
- **Tiny Payload**: Modern resets weigh less than $1\text{ KB}$ minified and gzipped.

### Accessibility (A11y)
- **Accessibility Tree Preservation**: Modern resets avoid deleting semantic HTML features. Unclassed lists preserve `role="list"` in VoiceOver and NVDA.
- **Vestibular Protection**: Incorporates `prefers-reduced-motion` to protect users with vestibular sensitivity.

### Browser Compatibility
- **Universal**: Supported across 100% of modern browsers. `@layer` and `:where()` enjoy full Baseline availability since early 2022.

---

## 9. Comparison / When to Use What

| Feature | Eric Meyer Reset (2007) | Normalize.css (2012) | Modern `@layer` Reset (2026) |
| :--- | :--- | :--- | :--- |
| **Philosophy** | Scorched Earth (Zero all) | Preserve & normalize | Ergonomic modern baseline |
| **Box-Sizing** | Not included | Not included | Universal `border-box` |
| **Specificity** | High / Tag-level | Tag-level | **Zero `(0, 0, 0)` via `:where()`**|
| **Cascade Layers** | No | No | **Full `@layer reset` support** |
| **Responsive Images**| No | No | Built-in (`max-width: 100%`) |

---

## 10. Interview Questions

### Q1: Why is `box-sizing: border-box` universally applied in modern CSS resets?
<details>
<summary>View Answer</summary>
The default W3C box model is `content-box`, where padding and borders are added *outside* the declared width. An element with `width: 100%` and `padding: 20px` expands to `calc(100% + 40px)`, causing horizontal overflow and broken grid layouts. Setting `box-sizing: border-box` forces padding and borders to be calculated *inside* the declared dimensions, making layout math intuitive and predictable.
</details>

### Q2: Why is `:where()` preferred over standard tag selectors when authoring modern resets?
<details>
<summary>View Answer</summary>
Tag selectors in a reset carry $(0, 0, 1)$ specificity, while attribute selectors like `ul[class]` carry $(0, 1, 1)$. This can inadvertently override author styles or lower-specificity rules. Wrapping reset selectors in `:where()` (e.g. `:where(h1, p)`) forces their specificity to $(0, 0, 0)$, guaranteeing that any author style can override the reset without specificity conflicts.
</details>

### Q3: What causes the mysterious 4px gap underneath images, and how does a modern reset fix it?
<details>
<summary>View Answer</summary>
By default, the `<img>` element is treated as an inline element (`display: inline`), meaning it aligns with the typographic baseline alongside text characters. The browser reserves space beneath the baseline for font descenders (like the tails of `p`, `y`, `g`), creating an empty 3–4px gap beneath images. A modern reset fixes this by declaring `:where(img) { display: block; max-width: 100%; height: auto; }`.
</details>

### Q4: Why don't form controls like `<button>` and `<input>` inherit typography from `body` by default?
<details>
<summary>View Answer</summary>
Historically, web browsers treated form controls as native operating system widgets rendered by the OS rather than the browser engine. The default User Agent stylesheet assigned OS-level system fonts to form elements and set `font: initial`. Modern resets fix this using `:where(input, button, textarea, select) { font: inherit; }`.
</details>

### Q5: Why should a reset target `ul[class]` rather than all `ul` elements?
<details>
<summary>View Answer</summary>
Web applications use `<ul>` and `<ol>` for two distinct purposes: navigation components (menus, tabs, breadcrumbs) where bullets are unwanted, and editorial prose (blog articles, markdown documentation) where bullets are required. By targeting `ul[class]`, the reset strips bullets from styled UI components while leaving standard unclassed article lists intact.
</details>

### Q6: How does `min-height: 100dvh` improve on `min-height: 100vh` in body resets?
<details>
<summary>View Answer</summary>
On mobile browsers (iOS Safari, Android Chrome), the classic `100vh` unit calculates against the full screen height without accounting for dynamic browser UI bars (the URL address bar and bottom toolbar). This causes bottom navigation elements or footers to be hidden underneath the browser chrome. The dynamic viewport unit `100dvh` dynamically recalculates as the browser bars expand and retract.
</details>

### Q7: Why is `* { transition: none !important; }` considered dangerous in an accessible motion reset?
<details>
<summary>View Answer</summary>
Setting `transition: none` stops the browser from firing the native `transitionend` event. Many JavaScript UI libraries, modals, and route transitions await the `transitionend` event to clean up DOM nodes or resolve promises. If `transition: none` is set, the event never fires, causing modals to freeze. Modern resets use `transition-duration: 0.01ms !important`, which collapses visual motion while still firing the required events.
</details>

### Q8: What is the benefit of placing a reset inside `@layer reset`?
<details>
<summary>View Answer</summary>
Placing the reset in an explicit Cascade Layer (`@layer reset`) ensures that all reset declarations reside in the lowest layer of the cascade. Regardless of how specific a reset selector is, any declaration in an unlayered stylesheet or higher layer (`@layer components`) will win automatically.
</details>

---

## 11. Practice Exercises

### Exercise 1: Universal Box-Sizing Setup
**Objective**: Write the standard box-sizing rule ensuring all pseudo-elements inherit border-box sizing.
<details>
<summary>View Solution</summary>

```css
*,
*::before,
*::after {
  box-sizing: border-box;
}
```
</details>

### Exercise 2: Zero-Specificity Form Inheritance
**Objective**: Ensure inputs, buttons, and textareas inherit typography fonts and colors with $(0, 0, 0)$ specificity.
<details>
<summary>View Solution</summary>

```css
:where(input, button, textarea, select) {
  font: inherit;
  color: inherit;
}
```
</details>

### Exercise 3: Responsive Media Reset
**Objective**: Write the reset rule for images and videos ensuring they never overflow their containers.
<details>
<summary>View Solution</summary>

```css
:where(img, picture, video, canvas, svg) {
  display: block;
  max-width: 100%;
  height: auto;
}
```
</details>

### Exercise 4: Editorial vs. Navigational List Handling
**Objective**: Write a list reset that removes list-style from classed lists while preserving it for unclassed lists.
<details>
<summary>View Solution</summary>

```css
:where(ul, ol)[class] {
  list-style: none;
  padding: 0;
  margin: 0;
}
```
</details>

### Exercise 5: Typographic Headline Balancing
**Objective**: Automatically prevent single-word trailing orphans in headings using `text-wrap: balance`.
<details>
<summary>View Solution</summary>

```css
:where(h1, h2, h3, h4, h5, h6) {
  text-wrap: balance;
  line-height: 1.25;
}
```
</details>

### Exercise 6: Complete Accessible Motion Reset Block
**Objective**: Author the accessible `prefers-reduced-motion` reset that collapses animations without breaking JS event listeners.
<details>
<summary>View Solution</summary>

```css
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
```
</details>

---

## 12. Summary / Cheat Sheet

```css
@layer reset {
  /* Box sizing & Margins */
  *, *::before, *::after { box-sizing: border-box; margin: 0; }

  /* Body baseline */
  body { min-height: 100dvh; line-height: 1.5; -webkit-font-smoothing: antialiased; }

  /* Images & Media */
  :where(img, video, svg) { display: block; max-width: 100%; height: auto; }

  /* Forms & Buttons */
  :where(input, button, textarea, select) { font: inherit; color: inherit; }

  /* Typography */
  :where(h1, h2, h3) { text-wrap: balance; }
  :where(ul, ol)[class] { list-style: none; padding: 0; }
}
```

- Universal `box-sizing: border-box` is mandatory.
- Use `@layer reset` and `:where()` for zero-specificity overridability.
- Eliminate 4px image baseline gaps with `display: block`.
- Support mobile viewports cleanly with `100dvh`.

---

## 13. Further Reading

- [Andy Bell: A Modern CSS Reset](https://piccalil.li/blog/a-more-modern-css-reset/)
- [Josh W. Comeau: The Custom CSS Reset](https://www.joshwcomeau.com/css/custom-css-reset/)
- [Eric Meyer: CSS Tools: Reset CSS](https://meyerweb.com/eric/tools/css/reset/)
- [MDN Web Docs: text-wrap: balance](https://developer.mozilla.org/en-US/docs/Web/CSS/text-wrap)

---

### Navigation
- **Previous**: [07-dark-mode-and-multi-theme.md](file:///e:/Learning/engineering-notes/01-languages/css/10-architecture-and-methodologies/07-dark-mode-and-multi-theme.md)
- **Next**: [09-css-modules-and-scoped-css.md](file:///e:/Learning/engineering-notes/01-languages/css/10-architecture-and-methodologies/09-css-modules-and-scoped-css.md)
