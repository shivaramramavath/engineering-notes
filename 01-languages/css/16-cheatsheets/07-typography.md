# Cheatsheet 07: Typography

| Spec Level | CSS Fonts Module Level 4 & CSS Text Module Level 4 | Baseline Status | Widely Available |
|---|---|---|---|
| Browsers Tested | Chrome 120+, Firefox 125+, Safari 17.4+ | Primary Usage | Font loading, typographic hierarchy, wrapping, text rendering |

---

## 1. Font Shorthand Syntax

```css
/* Order: style variant weight stretch size/line-height family */
body {
  font: italic small-caps 700 condensed 1rem/1.5 system-ui, -apple-system, sans-serif;
}
```

> [!IMPORTANT]
> When using `font` shorthand, `font-size` and `font-family` are **mandatory**. All omitted properties are automatically reset to their initial defaults (`normal`).

---

## 2. `@font-face` and `font-display` Matrix

```css
@font-face {
  font-family: 'Inter';
  src: url('/fonts/Inter-Variable.woff2') format('woff2-variations'),
       url('/fonts/Inter-Variable.woff2') format('woff2');
  font-weight: 100 900;
  font-style: normal;
  font-display: swap; /* Critical web performance property */
}
```

| `font-display` Value | Block Period (Hidden Text) | Swap Period (Fallback Visible) | Recommended Use Case |
|---|---|---|---|
| `swap` | Extremely short (0–100ms) | Infinite (swaps as soon as downloaded) | **Body text, articles, forms** (prevents FOIT) |
| `optional` | Extremely short (0–100ms) | 0ms (if not cached on 1st load, uses fallback; caches for 2nd page) | High-performance marketing sites, mobile networks |
| `block` | Medium (3000ms) | Infinite | Custom Icon fonts only (avoids ugly text glyph flash) |
| `fallback` | Short (100ms) | Short (approx 3000ms) | Non-critical headings |

---

## 3. Variable Fonts Axes Cheat Sheet

Standard Registered Axes (lowercase names):

```css
.variable-text {
  /* Weight: 100 to 900 */
  font-weight: 550;
  
  /* Width / Stretch: 75% to 125% */
  font-stretch: 95%;

  /* Slant: -10deg to 20deg */
  font-style: oblique 12deg;

  /* Optical Sizing: auto or none */
  font-optical-sizing: auto;
}

/* Custom Axes (4 uppercase letters via font-variation-settings) */
.custom-axis {
  font-variation-settings: "CASL" 0.8, "CRSV" 1;
}
```

---

## 4. Line Wrapping and Text Formatting

```css
/* 1. Balanced Headings (Baseline 2023) */
h1, h2, h3 {
  text-wrap: balance; /* Equalizes line lengths; prevents single-word orphans */
  max-inline-size: 25ch;
}

/* 2. Pretty Paragraphs (Baseline 2024) */
p {
  text-wrap: pretty; /* Adjusts hyphenation and last-line breaks to avoid orphans */
  max-inline-size: 65ch; /* Optimal reading line length (45–75 characters) */
}

/* 3. Automatic Hyphenation */
.prose {
  hyphens: auto; /* Requires <html lang="en"> attribute */
}

/* 4. Tabular Numbers for Data Tables */
.data-table td {
  font-variant-numeric: tabular-nums; /* Monospaces numbers in proportional font */
}
```

---

## 5. Text Truncation Cheat Sheet

### Single-Line Ellipsis
```css
.single-line-truncate {
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
  max-width: 100%;
}
```

### Multi-Line Clamping (Baseline Widely Available)
```css
.multi-line-truncate {
  display: -webkit-box;
  -webkit-box-orient: vertical;
  -webkit-line-clamp: 3; /* Truncates after 3 lines */
  overflow: hidden;
}
```

---

[← Previous: Cheatsheet 06 - Responsive Design](file:///e:/Learning/engineering-notes/01-languages/css/16-cheatsheets/06-responsive-and-media-queries.md) | [Next: Cheatsheet 08 - Colors and Gradients →](file:///e:/Learning/engineering-notes/01-languages/css/16-cheatsheets/08-colors-and-gradients.md)
