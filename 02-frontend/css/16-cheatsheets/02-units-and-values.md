# Cheatsheet 02: Units and Values

| Spec Level | CSS Values and Units Module Level 4 | Baseline Status | Widely Available |
|---|---|---|---|
| Browsers Tested | Chrome 120+, Firefox 125+, Safari 17.4+ | Primary Usage | Sizing, spacing, responsive fluid scaling |

---

## 1. Absolute Units Reference

| Unit | Name | Standard Equivalence | Screen Use Case |
|---|---|---|---|
| `px` | CSS Pixel | 1/96th of an inch (visual angle reference) | Borders, fine box shadows, hairline dividers |
| `pt` | Point | 1/72 inch (1.333 CSS px) | `@media print` style sheets only |
| `in` | Inch | 96 CSS pixels | Physical print output layout |
| `cm` / `mm` | Metric | 1cm = 37.8px / 1mm = 3.78px | Physical print output layout |

> [!CAUTION]
> Never use `px` for `font-size` or layout typography in production web applications. Fixed `px` font sizes prevent browser zoom and user-defined minimum font accessibility settings from scaling text properly.

---

## 2. Relative & Typographic Units Reference

| Unit | Relative To | Typical Use Case | Critical Caveat |
|---|---|---|---|
| `rem` | Root element (`<html>`) `font-size` (default 16px) | Font sizes, global layout spacing, margins, padding | Always predictable; unaffected by parent font sizing |
| `em` | Current element's own `font-size` | Component-relative padding, icons, border-radius | Compounds exponentially when nested (`1.2em` in `1.2em` = 1.44×) |
| `ch` | Width of the zero (`0`) glyph in current font | Restricting maximum paragraph line length | Varies drastically between proportional and monospace fonts |
| `ex` | x-height of current font (lowercase 'x') | Fine vertical text alignment, superscripts | Font-dependent |
| `lh` | Line-height of the current element | Aligning icons or borders to exact line height | Baseline 2023 |
| `%` | Parent container's computed metric | Fluid layout column widths, translation offsets | In `padding` and `margin`, `%` is calculated against container **width**, even for top/bottom! |

---

## 3. Viewport Units: Small (`s`), Large (`l`), Dynamic (`d`)

Modern mobile browsers render UI chrome (dynamic address bar, bottom navigation) that expands and retracts during scroll gestures.

```
+------------------------------------+
|  [ URL / Search Bar Visible ]      | <--- svh (Small Viewport Height)
|                                    |      Safe area when address bar is fully OPEN
|                                    |
|                                    |
|                                    | <--- lvh (Large Viewport Height)
|  [ URL Bar Collapsed on Scroll ]   |      Full screen when address bar is HIDDEN
+------------------------------------+
  dvh (Dynamic Viewport Height) tracks the address bar frame-by-frame during scroll
```

| Viewport Unit | Description & Target Geometry | Recommended Application |
|---|---|---|
| `100vw` / `100vh` | Classic viewport width and height | Desktop layouts (caution: `100vw` ignores scrollbar width!) |
| `100svh` | Small viewport height (browser chrome open) | Hero sections where bottom CTA **must never** be cropped |
| `100lvh` | Large viewport height (browser chrome hidden) | Full-screen background imagery |
| `100dvh` | Dynamic viewport height (adapts during gesture) | Full-screen sliding drawer menus, native-feel modals |
| `100vi` / `100vb` | Viewport Inline / Viewport Block | Writing-mode agnostic responsive layouts |

---

## 4. Container Query Units

Require a container ancestor defined via `container-type: inline-size` or `size`.

| Unit | Relative To | Calculation |
|---|---|---|
| `cqi` | Container Query Inline size | 1% of the query container's inline size (width in horizontal mode) |
| `cqb` | Container Query Block size | 1% of the query container's block size (height in horizontal mode) |
| `cqw` / `cqh` | Container Query Width / Height | 1% of the query container's physical width or height |
| `cqmin` / `cqmax`| Container Query Min / Max | Smaller or larger of `cqi` or `cqb` |

---

## 5. Math Functions Quick Reference

### `calc()`, `min()`, `max()`, and `clamp()`

```css
/* 1. calc(): Dynamic mixed unit arithmetic */
/* Operators +, - MUST have whitespace around them! */
.container {
  width: calc(100% - 2rem);
  height: calc(100vh - 64px);
}

/* 2. min(): Resolves to the SMALLEST value (sets an upper boundary) */
.content {
  width: min(100%, 1200px); /* Never exceeds 1200px, shrinks on mobile */
}

/* 3. max(): Resolves to the LARGEST value (sets a lower boundary) */
.banner {
  padding-block: max(2rem, 5vh); /* Never drops below 2rem */
}

/* 4. clamp(MIN, VAL, MAX): Ideal responsive fluid scaling */
/* Formula: clamp(minimum-allowed, preferred-scalable, maximum-allowed) */
h1 {
  font-size: clamp(2rem, 1.5rem + 2.5vw, 4rem);
}
```

### Production Fluid Scaling Cheat Formula

$$\text{Ideal Slope} = \frac{\text{Max Size} - \text{Min Size}}{\text{Max Viewport} - \text{Min Viewport}}$$

```css
/* Fluid Type: 16px (1rem) at 320px viewport to 24px (1.5rem) at 1280px viewport */
:root {
  --fluid-body: clamp(1rem, 0.833rem + 0.833vw, 1.5rem);
}
```

---

[← Previous: Cheatsheet 01 - Selectors](file:///e:/Learning/engineering-notes/01-languages/css/16-cheatsheets/01-selectors-and-specificity.md) | [Next: Cheatsheet 03 - Box Model and Positioning →](file:///e:/Learning/engineering-notes/01-languages/css/16-cheatsheets/03-box-model-and-positioning.md)
