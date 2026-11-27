# 04. Card Grids and Galleries

| Metadata | Details |
| :--- | :--- |
| **Spec Level & Status** | CSS Grid Layout Level 2 (Subgrid) / CSS Sizing Level 4 / W3C Recommendation |
| **Browsers Tested** | Chrome 120+, Firefox 125+, Safari 17.4+ (Universal Baseline) |
| **Tested With** | Modern DevTools, Responsive Viewport Stress Testing |
| **Prerequisites** | `04-grid/01-grid-fundamentals.md`, `04-grid/04-subgrid.md`, `13-layout-and-component-recipes/01-centering-everything.md` |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. Practical Examples
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

### The Problem
Card grids and image galleries are the primary content organization pattern across e-commerce catalogs, news portals, and SaaS dashboards. Historically, implementing responsive card grids required writing brittle, hardcoded media query breakpoints (`@media (min-width: 600px)`, `@media (min-width: 900px)`, `@media (min-width: 1200px)`). These breakpoints broke immediately when the grid was embedded inside a narrower layout sidebar or split-pane view.

Furthermore, cards with variable content lengths (e.g. one product has a 2-line title while an adjacent product has a 5-line title) caused severe internal alignment defects: card titles, description paragraphs, pricing tags, and "Add to Cart" buttons misaligned horizontally, creating an uneven, unprofessional visual rhythm.

### The Solution
Modern CSS provides two transformative layout primitives:
1. **The RAM Pattern (Repeat, Auto, Minmax)**: Creating responsive, multi-column card grids that automatically compute the optimal number of columns based on available space with **zero media queries**:
   ```css
   grid-template-columns: repeat(auto-fit, minmax(min(100%, 280px), 1fr));
   ```
2. **CSS Subgrid**: Allowing child elements inside individual cards (e.g. Card Title, Card Media, Card Body, Card Actions) to inherit and snap directly to the master row tracks of the parent grid, guaranteeing **flawless horizontal alignment** across all cards regardless of text length differences.

---

## 2. Core Concepts

### 1. The Misaligned Card Problem vs. CSS Subgrid Alignment

```
WITHOUT SUBGRID (Independent Card Flexbox / Block Layout):
┌───────────────────────────┐ ┌───────────────────────────┐
│ Image (16:9)              │ │ Image (16:9)              │
├───────────────────────────┤ ├───────────────────────────┤
│ Short Title               │ │ Long Multi-Line Title     │
│                           │ │ That Wraps Onto Three     │
│                           │ │ Entire Lines of Text      │
├───────────────────────────┤ ├───────────────────────────┤
│ Description...            │ │ Description... (MISALIGNED!)
├───────────────────────────┤ ├───────────────────────────┤
│ [BUTTON]                  │ │                           │
└───────────────────────────┘ │ [BUTTON] (MISALIGNED!)     │
                              └───────────────────────────┘

WITH CSS SUBGRID (Shared Parent Track Alignment):
┌───────────────────────────┐ ┌───────────────────────────┐
│ Image (Track 1)           │ │ Image (Track 1)           │
├───────────────────────────┤ ├───────────────────────────┤
│ Short Title               │ │ Long Multi-Line Title     │
│ (Track 2: Stretches auto) │ │ That Wraps Onto Three     │
├───────────────────────────┤ ├───────────────────────────┤
│ Description (Track 3: 1fr)│ │ Description (Track 3: 1fr)│ ◄── Perfectly Aligned!
├───────────────────────────┤ ├───────────────────────────┤
│ [BUTTON] (Track 4: auto)  │ │ [BUTTON] (Track 4: auto)  │ ◄── Perfectly Aligned!
└───────────────────────────┘ └───────────────────────────┘
```

### 2. `auto-fit` vs. `auto-fill`
- **`auto-fill`**: Fills the grid row with as many tracks as can physically fit, creating empty invisible tracks on the right if there aren't enough items.
- **`auto-fit`**: Fills the row with tracks, but **collapses empty tracks down to 0px**, allowing existing items to stretch and fill 100% of the available width. In 95% of UI card grids, `auto-fit` is the desired behavior.

---

## 3. How It Works Internally

### The Mechanics of `minmax(min(100%, 280px), 1fr)`
Why is `min(100%, 280px)` necessary instead of plain `280px`?
1. On a desktop screen ($1200\text{px}$), $280\text{px} < 100\%$, so `min(100%, 280px)` evaluates to $280\text{px}$. The grid creates 4 columns.
2. On a narrow mobile screen ($320\text{px}$), $280\text{px}$ is smaller than 320px, so it fits cleanly.
3. However, on an ultra-narrow screen ($240\text{px}$, or a narrow split pane), $280\text{px}$ would be wider than the viewport, triggering an unwanted horizontal scrollbar!
4. By wrapping it in `min(100%, 280px)`, when the container width drops to 240px, the minimum track size smoothly contracts to $100\%$ ($240\text{px}$), eliminating horizontal overflow completely.

---

## 4. Syntax & Usage

### 1. The Universal Responsive RAM Grid
```css
.card-grid {
  display: grid;
  /* Automatically wraps down to 1 column on mobile, expands to 4+ columns on wide screens */
  grid-template-columns: repeat(auto-fit, minmax(min(100%, 280px), 1fr));
  gap: 1.5rem;
}
```

### 2. The Complete Subgrid Card Architecture
```css
/* Master Grid Container */
.subgrid-card-deck {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(min(100%, 300px), 1fr));
  /* Master rows: image, title, description, actions */
  grid-auto-rows: auto auto 1fr auto;
  gap: 1.5rem;
}

/* Individual Card */
.subgrid-card {
  grid-row: span 4; /* Spans 4 master rows */
  display: grid;
  grid-template-rows: subgrid; /* Snaps internal children to parent tracks */
  background: #ffffff;
  border-radius: 0.75rem;
  border: 1px solid #e2e8f0;
  overflow: clip;
}
```

---

## 5. Practical Examples

### Example 1: E-Commerce Product Deck with Subgrid Alignment
Notice how the price and buy buttons align across all cards, even when product titles vary wildly in length:

```html
<div class="product-deck">
  <!-- Card 1: Short title -->
  <article class="product-card">
    <div class="card-media">
      <img src="/img/mouse.webp" alt="Ergonomic Mouse" loading="lazy">
    </div>
    <h3 class="card-title">Ergo Mouse</h3>
    <p class="card-desc">Wireless precision optical mouse.</p>
    <div class="card-footer">
      <span class="price">$59.00</span>
      <button class="btn-buy">Add to Cart</button>
    </div>
  </article>

  <!-- Card 2: Extremely long title -->
  <article class="product-card">
    <div class="card-media">
      <img src="/img/keyboard.webp" alt="Mechanical Keyboard" loading="lazy">
    </div>
    <h3 class="card-title">Custom Hot-Swappable 75% Mechanical Keyboard with Gateron Switches</h3>
    <p class="card-desc">Premium CNC aluminum chassis with sound dampening foam and RGB backlighting.</p>
    <div class="card-footer">
      <span class="price">$189.00</span>
      <button class="btn-buy">Add to Cart</button>
    </div>
  </article>
</div>
```

```css
.product-deck {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(min(100%, 320px), 1fr));
  grid-auto-rows: auto auto 1fr auto; /* 4 shared tracks per card */
  gap: 1.5rem;
  max-width: 1200px;
  margin: 0 auto;
  padding: 2rem 1.5rem;
}

.product-card {
  grid-row: span 4;
  display: grid;
  grid-template-rows: subgrid;
  background: #ffffff;
  border-radius: 0.75rem;
  border: 1px solid #e2e8f0;
  overflow: clip;
}

.card-media {
  aspect-ratio: 16 / 10;
  width: 100%;
  background: #f1f5f9;
}

.card-media img {
  width: 100%;
  height: 100%;
  object-fit: cover;
  display: block;
}

.card-title {
  margin: 0;
  padding: 1.25rem 1.25rem 0.5rem;
  font-size: 1.25rem;
  font-weight: 700;
  color: #0f172a;
  line-height: 1.3;
}

.card-desc {
  margin: 0;
  padding: 0 1.25rem 1.25rem;
  color: #64748b;
  font-size: 0.9375rem;
  line-height: 1.5;
}

.card-footer {
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding: 1rem 1.25rem;
  background-color: #f8fafc;
  border-top: 1px solid #e2e8f0;
}

.price {
  font-size: 1.25rem;
  font-weight: 800;
  color: #0f172a;
}

.btn-buy {
  padding: 0.5rem 1rem;
  background-color: #2563eb;
  color: #ffffff;
  border: none;
  border-radius: 0.375rem;
  font-weight: 600;
  cursor: pointer;
}

.btn-buy:focus-visible {
  outline: 2px solid #2563eb;
  outline-offset: 2px;
}
```

### Example 2: Dense Feature Gallery (Editorial Bento Grid)
A bento-style masonry gallery using `grid-auto-flow: dense` and spanning tracks:

```html
<div class="bento-gallery">
  <div class="bento-item bento-item--featured">
    <h3>Enterprise Architecture</h3>
    <p>Featured showcase spans 2 columns and 2 rows.</p>
  </div>
  <div class="bento-item">Widget A</div>
  <div class="bento-item">Widget B</div>
  <div class="bento-item bento-item--tall">Tall Telemetry Chart</div>
  <div class="bento-item">Widget C</div>
</div>
```

```css
.bento-gallery {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(240px, 1fr));
  grid-auto-rows: 200px;
  /* DENSE: Browser back-fills empty holes left by larger spanning items */
  grid-auto-flow: dense;
  gap: 1.25rem;
}

.bento-item {
  background: #ffffff;
  border: 1px solid #e2e8f0;
  border-radius: 0.75rem;
  padding: 1.5rem;
}

.bento-item--featured {
  grid-column: span 2;
  grid-row: span 2;
  background-color: #0f172a;
  color: #ffffff;
}

.bento-item--tall {
  grid-row: span 2;
}

@media (max-width: 600px) {
  .bento-item--featured,
  .bento-item--tall {
    grid-column: span 1;
    grid-row: span 1;
  }
}
```

### Example 3: Aspect-Ratio Photo Gallery with Hover Zoom
```css
.photo-gallery {
  display: grid;
  grid-template-columns: repeat(auto-fill, minmax(200px, 1fr));
  gap: 1rem;
}

.photo-card {
  position: relative;
  aspect-ratio: 1 / 1; /* Perfect square thumbnails */
  overflow: clip;
  border-radius: 0.5rem;
  background-color: #f1f5f9;
}

.photo-card img {
  width: 100%;
  height: 100%;
  object-fit: cover;
  transition: transform 0.3s cubic-bezier(0.16, 1, 0.3, 1);
  will-change: transform;
}

.photo-card:hover img {
  transform: scale(1.05); /* Hardware-accelerated zoom */
}
```

---

## 6. Best Practices

1. **Use `min(100%, 280px)` Inside `minmax()`**: Guarantee that grid tracks contract gracefully on screens narrower than the minimum pixel threshold without horizontal overflow.
2. **Favor `auto-fit` Over `auto-fill` for Cards**: Prevent single or few items from leaving empty visual voids on the right side of the container.
3. **Deploy CSS Subgrid for Multi-Row Cards**: Eliminate staggered, uneven description and button alignment across cards by spanning rows and applying `grid-template-rows: subgrid`.
4. **Always Define `aspect-ratio` on Card Media**: Reserve geometric space for card images to prevent Cumulative Layout Shift (CLS) as images load over the network.
5. **Use `grid-auto-flow: dense` for Asymmetric Bento Grids**: Instruct the layout engine to back-fill holes created by 2-column or 2-row feature cards.
6. **Apply `overflow: clip` to Card Containers**: Prevent scaling hover effects or child images from leaking past rounded card borders without breaking sticky ancestors.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Consequence | Correct Pattern |
| :--- | :--- | :--- |
| **Using `minmax(300px, 1fr)` without `min()`** | On mobile screens under 300px, grid triggers an unwanted horizontal scrollbar. | Use `minmax(min(100%, 300px), 1fr)`. |
| **Using `auto-fill` on a 2-item list in a 4-column space** | Items occupy only half the row, leaving awkward empty columns. | Use `auto-fit` so items stretch to fill available width. |
| **Forgetting `grid-row: span N` when using Subgrid** | Subgrid fails to inherit rows because it has no row span to map to. | Always declare `grid-row: span <number>` on subgrid items. |
| **Omitting `object-fit: cover` on card images** | Images with varying aspect ratios distort, stretch, or create uneven heights. | Combine `aspect-ratio: 16/9;` with `object-fit: cover;`. |
| **Using flexbox margin auto when cards have varying title heights** | Buttons align to the bottom, but description paragraphs remain misaligned. | Use CSS Subgrid to align all intermediate row elements. |
| **Hardcoding 12 media query breakpoints for column counts** | Brittle code that breaks when cards are placed inside narrow layout sidebars. | Use the RAM `repeat(auto-fit, minmax(...))` pattern. |

---

## 8. Performance, Accessibility & Browser Support

### Universal Modern Baseline
- **CSS Grid & `minmax()`**: Universal baseline in all evergreen browsers since 2017.
- **CSS Subgrid**: Supported in all modern browsers (Firefox 71+, Safari 16.0+, Chrome 117+). Baseline Newly Available!
- **`aspect-ratio`**: Baseline Widely Available across Chrome 88+, Safari 15+, Firefox 89+.

---

## 9. Comparison / When to Use What

| Pattern | Complexity | Cross-Card Row Alignment | Auto-Wrapping | Best Suited For |
| :--- | :--- | :--- | :--- | :--- |
| **RAM Grid (Standard)** | Very Low | No (independent cards) | Yes (zero media queries) | Simple card lists, link directories |
| **Subgrid Card Deck** | Low-Medium | **Yes (Perfect horizontal alignment)**| Yes (zero media queries) | E-commerce product decks, pricing tables |
| **Bento Grid (`dense`)** | Medium | No (asymmetric blocks) | Semi-automatic | Landing page feature showcases |
| **Flexbox Column Wrap** | Very Low | No | Yes | Dynamic tags, badge lists |

---

## 10. Interview Questions

### Q1: Explain the RAM (Repeat, Auto, Minmax) pattern and why it eliminates the need for media queries in card grids.
<details>
<summary>Answer</summary>
The RAM pattern combines three CSS Grid features:
```css
grid-template-columns: repeat(auto-fit, minmax(min(100%, 280px), 1fr));
```
1. **`repeat()`**: Tells the engine to generate multiple column tracks.
2. **`auto-fit`**: Instructs the engine to calculate how many columns of at least 280px can fit in the container. Any empty tracks are collapsed to 0px, and existing tracks expand to fill remaining space.
3. **`minmax(..., 1fr)`**: Establishes a minimum column width ($280\text{px}$) and a maximum of $1\text{fr}$ (allowing tracks to grow equally).
4. **`min(100%, 280px)`**: Guarantees that if the container is narrower than 280px (e.g. mobile 240px), the track contracts to 100% width rather than overflowing.
The engine calculates columns dynamically based on container width. Whether the grid is on a 4K monitor, a mobile phone, or a narrow sidebar, it automatically displays 4, 3, 2, or 1 column without writing a single media query.
</details>

### Q2: What exact problem does CSS Subgrid solve in card deck layouts?
<details>
<summary>Answer</summary>
In a standard CSS Grid, children of a grid item do not participate in the parent grid's tracks. Each card manages its own internal layout independently (typically via Flexbox).
If Card A has a 1-line title and Card B has a 4-line title:
- Card B's title pushes its description and action button down.
- Card A's description sits much higher up.
The buttons and descriptions across neighboring cards are horizontally staggered and misaligned.
With **CSS Subgrid**:
```css
.card {
  grid-row: span 4;
  display: grid;
  grid-template-rows: subgrid;
}
```
The internal elements of all cards (Media, Title, Description, Button) snap to the **shared master row tracks of the parent grid**. All titles share Track 2 (which expands to fit the tallest title), and all buttons share Track 4, achieving **flawless horizontal alignment across all cards in the row**.
</details>

### Q3: What is the difference between `auto-fit` and `auto-fill` in `grid-template-columns`?
<details>
<summary>Answer</summary>
- **`auto-fill`**: Instructs the browser to create as many tracks as can physically fit in the container width, **even if some tracks contain no elements**. If a container can fit four 250px tracks but you only have two cards, `auto-fill` leaves two empty 250px tracks on the right.
- **`auto-fit`**: Creates as many tracks as fit, but **collapses any empty tracks with zero items down to 0px**. The remaining non-empty tracks expand (via `1fr`) to fill 100% of the available row width.
</details>

### Q4: Why does setting `grid-auto-flow: dense` sometimes cause accessibility issues for keyboard navigation?
<details>
<summary>Answer</summary>
`grid-auto-flow: dense` instructs the grid layout engine to back-fill empty holes in the grid. If an earlier grid slot was left empty because a 2-column item was too wide to fit, the engine will take a later, smaller 1-column item in source code and visually place it backward into that earlier hole.

This causes a **visual order vs. DOM source order divergence** (WCAG 1.3.2 / 2.4.3). A keyboard user pressing `Tab` will navigate through the items in HTML source order, causing the focus ring to jump backward and forward erratically across the screen, disorienting keyboard users.
</details>

### Q5: How does `aspect-ratio` combined with `object-fit: cover` prevent layout distortion in responsive galleries?
<details>
<summary>Answer</summary>
- **`aspect-ratio: 16 / 9`**: Forces the container to reserve the exact geometric proportions (16:9, 1:1 square, etc.) regardless of viewport width, completely eliminating Cumulative Layout Shift (CLS) as images download.
- **`object-fit: cover`**: Instructs the browser to scale the image proportionally so that it fills the entire 16:9 box without stretching or squishing, cropping excess edges cleanly while preserving native image aspect ratios.
</details>

### Q6: What happens if an image inside a grid card is missing `min-width: 0` or `max-width: 100%`?
<details>
<summary>Answer</summary>
An `<img>` tag has an intrinsic natural width based on its raster file dimensions (e.g. 2000px wide).
Because a grid item's minimum size along `1fr` defaults to `auto` (the content's intrinsic size), an unconstrained image will force the grid column track to expand to 2000px, blowing out the grid and triggering an unwanted horizontal scrollbar.
Adding `max-width: 100%; height: auto;` or `min-width: 0;` constrains the image within its allocated grid track.
</details>

### Q7: Can CSS Subgrid inherit column tracks and row tracks simultaneously?
<details>
<summary>Answer</summary>
Yes. An item can declare:
```css
.complex-tile {
  grid-column: span 3;
  grid-row: span 2;
  display: grid;
  grid-template-columns: subgrid;
  grid-template-rows: subgrid;
}
```
The child element inherits both horizontal column lines and vertical row lines from the parent grid simultaneously, allowing two-dimensional alignment with external grid items across both axes.
</details>

### Q8: How does Container Query support enhance card grids compared to viewport media queries?
<details>
<summary>Answer</summary>
Viewport media queries evaluate the entire screen width. If a card is styled to switch to a horizontal layout at `768px`, it will always render horizontally on a desktop monitor, even if placed inside a narrow 300px sidebar.
By establishing a container context:
```css
.card-wrapper { container-type: inline-size; }
@container (min-width: 450px) { .card { flex-direction: row; } }
```
The card adapts based on its **own available container width**. If placed in a wide main grid, it renders horizontally; if placed in a narrow sidebar column, it automatically reflows to a compact vertical stack, regardless of screen resolution.
</details>

---

## 11. Practice Exercises

### Exercise 1: Build a Subgrid-Aligned Pricing Table Deck
**Objective**: Build a 3-tier pricing table using CSS Subgrid where tier names, monthly prices, feature lists, and action buttons align horizontally across all cards.

<details>
<summary>Solution</summary>

```html
<div class="pricing-deck">
  <!-- Tier 1 -->
  <article class="pricing-card">
    <h3 class="tier-name">Starter</h3>
    <div class="tier-price"><span class="amount">$29</span>/mo</div>
    <ul class="tier-features">
      <li>Up to 5 Cluster Nodes</li>
      <li>Standard Telemetry</li>
    </ul>
    <button class="btn-tier">Start Free Trial</button>
  </article>

  <!-- Tier 2 -->
  <article class="pricing-card pricing-card--featured">
    <h3 class="tier-name">Enterprise Distributed Edition</h3>
    <div class="tier-price"><span class="amount">$199</span>/mo</div>
    <ul class="tier-features">
      <li>Unlimited Multi-Region Nodes</li>
      <li>Zero-Latency Edge Routing</li>
      <li>24/7 Dedicated Support SRE</li>
    </ul>
    <button class="btn-tier btn-tier--primary">Contact Sales</button>
  </article>
</div>
```

```css
.pricing-deck {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(min(100%, 300px), 1fr));
  grid-auto-rows: auto auto 1fr auto; /* Name, Price, Features, Button */
  gap: 1.5rem;
  max-width: 900px;
  margin: 2rem auto;
  padding: 1rem;
}

.pricing-card {
  grid-row: span 4;
  display: grid;
  grid-template-rows: subgrid;
  background: #ffffff;
  border: 1px solid #e2e8f0;
  border-radius: 0.75rem;
  padding: 1.5rem;
}

.pricing-card--featured {
  border-color: #2563eb;
  box-shadow: 0 10px 15px -3px rgba(37, 99, 235, 0.1);
}

.tier-name {
  margin: 0;
  font-size: 1.25rem;
  color: #0f172a;
}

.tier-price {
  font-size: 1.125rem;
  color: #64748b;
  margin-block: 0.5rem 1rem;
}

.amount {
  font-size: 2.25rem;
  font-weight: 800;
  color: #0f172a;
}

.tier-features {
  list-style: none;
  padding: 0;
  margin: 0 0 1.5rem;
  display: flex;
  flex-direction: column;
  gap: 0.5rem;
  color: #475569;
}

.btn-tier {
  padding: 0.75rem 1.25rem;
  border-radius: 0.5rem;
  font-weight: 600;
  cursor: pointer;
  background: #f1f5f9;
  border: 1px solid #cbd5e1;
  color: #0f172a;
}

.btn-tier--primary {
  background: #2563eb;
  color: #ffffff;
  border: none;
}

.btn-tier:focus-visible {
  outline: 2px solid #2563eb;
  outline-offset: 2px;
}
```
</details>

### Exercise 2: Responsive Photo Mosaic Gallery
**Objective**: Build a responsive photo mosaic where the 1st and 4th images span 2 columns/rows with high-performance hover scaling.

<details>
<summary>Solution</summary>

```html
<div class="mosaic-gallery">
  <div class="mosaic-item mosaic-item--large"><img src="/img/1.webp" alt="Nature"></div>
  <div class="mosaic-item"><img src="/img/2.webp" alt="City"></div>
  <div class="mosaic-item"><img src="/img/3.webp" alt="Architecture"></div>
  <div class="mosaic-item mosaic-item--wide"><img src="/img/4.webp" alt="Aerial"></div>
</div>
```

```css
.mosaic-gallery {
  display: grid;
  grid-template-columns: repeat(auto-fill, minmax(220px, 1fr));
  grid-auto-rows: 180px;
  grid-auto-flow: dense;
  gap: 1rem;
}

.mosaic-item {
  overflow: clip;
  border-radius: 0.5rem;
  background: #f1f5f9;
}

.mosaic-item--large {
  grid-column: span 2;
  grid-row: span 2;
}

.mosaic-item--wide {
  grid-column: span 2;
}

.mosaic-item img {
  width: 100%;
  height: 100%;
  object-fit: cover;
  display: block;
  transition: transform 0.25s ease;
  will-change: transform;
}

.mosaic-item:hover img {
  transform: scale(1.04);
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **RAM Pattern**: `repeat(auto-fit, minmax(min(100%, 280px), 1fr))` creates responsive grids with zero media queries.
- **CSS Subgrid**: `grid-row: span 4; display: grid; grid-template-rows: subgrid;` aligns card titles, descriptions, and buttons perfectly.
- **`auto-fit`**: Fills rows and collapses empty columns to 0px (recommended for cards).
- **`auto-fill`**: Fills rows and preserves empty column slots.
- **Media Preservation**: Combine `aspect-ratio: 16 / 9;` with `object-fit: cover;` to prevent layout shift.
- **Dense Packing**: Use `grid-auto-flow: dense;` to eliminate holes in bento galleries.

---

## 13. Further Reading
- [W3C CSS Grid Layout Module Level 2 (Subgrid)](https://www.w3.org/TR/css-grid-2/)
- [MDN Web Docs: CSS Subgrid](https://developer.mozilla.org/en-US/docs/Web/CSS/CSS_grid_layout/Subgrid)
- [Rachel Andrew: CSS Grid and Subgrid Complete Guide](https://www.smashingmagazine.com/2022/05/subgrid-css-grid-layouts/)
- [web.dev: Responsive Layouts Without Media Queries](https://web.dev/responsive-web-design-basics/#responsive-images)
- [Ahmad Shadeed: Alignment in CSS Subgrid](https://ishadeed.com/article/learn-css-subgrid/)

---

| Previous Note | Next Note |
| :--- | :--- |
| [03. Sticky Header and Footer Patterns](03-sticky-header-and-footer-patterns.md) | [05. Dashboard Layouts](05-dashboard-layouts.md) |
