# Scrollbars & Scroll Behavior

| Metadata | Details |
| :--- | :--- |
| **Topic** | Standard CSS Scrollbars (`scrollbar-color`, `scrollbar-width`), Layout Shift Prevention (`scrollbar-gutter`), `scroll-behavior`, Sticky Anchor Offsets (`scroll-padding-top`) |
| **Spec Level** | CSS Scrollbars Styling Module Level 1 / CSS Overflow Module Level 3 / CSS Scroll Snap Module Level 1 |
| **Baseline Status** | Widely Available (`scroll-behavior` Baseline 2019; `scrollbar-gutter` Baseline 2022; `scrollbar-color / width` Baseline 2024) |
| **Browsers Tested** | Chrome 121+, Firefox 122+, Safari 17.2+, Edge 121+ |
| **Tested With** | Windows Overlay Scrollbars, macOS Dynamic Scrollbars, NVDA, Playwright |
| **Module** | [07-visual-styling](README.md) |

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

Scrollbars are essential system-level navigation controls for traversing overflowing content. For over two decades, styling scrollbars on the web was an inconsistent mess of proprietary, vendor-prefixed pseudo-elements (`::-webkit-scrollbar`, `::-webkit-scrollbar-thumb`), which were never standardized and were completely ignored by Firefox.

With the standardization of the **CSS Scrollbars Styling Module Level 1** (Baseline 2024 across all major engines), modern CSS provides elegant, cross-browser, declarative control over scrollbar appearance:
- **`scrollbar-color` & `scrollbar-width`**: The modern W3C standard properties controlling scrollbar thumb/track palettes and sizing.
- **`scrollbar-gutter: stable` (Baseline 2022)**: Eliminates the jarring Cumulative Layout Shift (CLS) that occurs when dynamic data or page transitions cause a scrollbar to appear or disappear, preventing the entire page from shaking horizontally.
- **Anchor Offset Defense (`scroll-padding-top`)**: Solves the classic web defect where clicking an anchor link (`#section-3`) causes the target heading to be hidden underneath a `position: sticky` header.
- **Accessible Smooth Scrolling**: Respecting user hardware and operating system motion settings via `@media (prefers-reduced-motion: reduce)`.

---

## 2. Core Concepts

### Modern Standard Scrollbars (Baseline 2024)
The modern W3C standard replaces dozens of lines of `-webkit-` rules with two clean properties:
1. **`scrollbar-color: <thumb-color> <track-color>`**:
   ```css
   html {
     scrollbar-color: #3b82f6 #0f172a; /* Thumb, then Track */
   }
   ```
2. **`scrollbar-width: auto | thin | none`**:
   - `auto`: Default platform thickness.
   - `thin`: Slim, refined modern thickness.
   - `none`: Completely hides the visual scrollbar while preserving scrollability via mouse wheel, touch swipe, and keyboard keys.

### Eliminating Layout Shift: `scrollbar-gutter`
When a page transitions between a short view (no scrollbar) and a long view (scrollbar appears), the browser adds a ~16px scrollbar on the right. This shifts the entire page content leftward by 16px (causing CLS).
- **`scrollbar-gutter: stable`**: Reserves space for the scrollbar gutter at all times, whether the content currently overflows or not.
- **`scrollbar-gutter: stable both-edges`**: Reserves matching gutters on both the start and end edges, keeping centered layouts perfectly symmetrical.

### Anchor Offsets: `scroll-padding`
When an anchor link (`<a href="#target">`) is clicked, the browser aligns the top of `#target` to the top edge of the viewport ($Y = 0$). If the page has a 64px sticky navigation bar, the top 64px of the heading is buried invisibly behind the header!
- Declaring `html { scroll-padding-top: 5rem; }` instructs the browser to offset all scroll-to-target positions by 5rem, keeping headings visible.

### Smooth Scrolling (`scroll-behavior`)
- `scroll-behavior: smooth`: Animates page transitions smoothly when clicking anchor links.
- **Strict Accessibility Requirement**: Must be disabled for users who prefer reduced motion.

---

## 3. How It Works Internally

```
The Sticky Header Anchor Collision vs scroll-padding Fix

1. Default Behavior (Heading occluded!):
+-----------------------------------------------------+
| [Sticky Header: position: sticky; height: 60px]     |
|   H1: Introduction to Distributed Systems  <--------|-- BURIED UNDER HEADER!
+-----------------------------------------------------+
| Rest of section content...                          |

2. With `html { scroll-padding-top: 60px; }`:
+-----------------------------------------------------+
| [Sticky Header: position: sticky; height: 60px]     |
+-----------------------------------------------------+
    <--- 60px scroll padding buffer --->
  H1: Introduction to Distributed Systems   <---------|-- CLEANLY VISIBLE!
  Rest of section content...
```

### Layout Shift Prevention Diagram
```
Without scrollbar-gutter:
Page 1 (Short):  [           Centered 800px Box           ]
Page 2 (Long):   [          Centered 800px Box         ] [S]  <-- Shakes left by 8px!

With scrollbar-gutter: stable:
Page 1 (Short):  [          Centered 800px Box         ] [ ]  <-- Reserved gutter!
Page 2 (Long):   [          Centered 800px Box         ] [S]  <-- Zero layout shift!
```

---

## 4. Syntax & Usage

### Baseline & Feature Support
- **`scroll-behavior: smooth`**: Baseline Widely Available (Chrome 61+, Safari 15.4+, Firefox 36+).
- **`scrollbar-gutter`**: Baseline 2022 (Chrome 94+, Firefox 97+, Safari 17+).
- **`scrollbar-color` & `scrollbar-width`**: Baseline 2024 (Chrome 121+, Safari 17.2+, Firefox 64+).

```css
/* Production Global Root Configuration */
html {
  /* 1. Reserve gutter to eliminate CLS */
  scrollbar-gutter: stable;

  /* 2. Modern cross-browser scrollbar styling */
  scrollbar-width: thin;
  scrollbar-color: #64748b #f1f5f9;

  /* 3. Offset anchor scrolling for sticky nav */
  scroll-padding-top: 4.5rem;

  /* 4. Smooth scrolling with reduced-motion protection */
  scroll-behavior: smooth;
}

@media (prefers-reduced-motion: reduce) {
  html {
    scroll-behavior: auto;
  }
}
```

---

## 5. Practical Examples

### Example 1: Dark Mode Code Block Scrollbar
```html
<pre class="dark-code-block"><code>
// Distributed Raft Consensus Heartbeat Dispatcher
async function broadcastHeartbeats(nodes) {
  return Promise.all(nodes.map(n => ping(n)));
}
</code></pre>
```

```css
.dark-code-block {
  max-height: 240px;
  overflow: auto;
  padding: 1.25rem;
  background-color: #0f172a;
  border-radius: 8px;
  color: #38bdf8;
  font-family: ui-monospace, SFMono-Regular, monospace;
  font-size: 0.875rem;

  /* Modern W3C Standard (Chrome 121+, Firefox, Safari 17.2+) */
  scrollbar-width: thin;
  scrollbar-color: #334155 #0f172a;
}

/* Backward compatibility for legacy WebKit engines */
.dark-code-block::-webkit-scrollbar {
  width: 8px;
  height: 8px;
}

.dark-code-block::-webkit-scrollbar-track {
  background: #0f172a;
}

.dark-code-block::-webkit-scrollbar-thumb {
  background-color: #334155;
  border-radius: 4px;
}
```

### Example 2: Zero-CLS Layouts via `scrollbar-gutter`
```html
<div class="dynamic-feed-container">
  <button type="button" class="load-more-btn">Fetch Additional Records</button>
  <div class="feed-list">
    <!-- Dynamic feed items loaded here -->
  </div>
</div>
```

```css
/* Guarantees the centered container never jerks when new items trigger a scrollbar */
.dynamic-feed-container {
  max-width: 720px;
  margin-inline: auto;
  padding: 2rem;
  scrollbar-gutter: stable both-edges;
}
```

### Example 3: Sticky Header with Anchor Alignment
```html
<header class="main-header">
  <nav>
    <a href="#architecture">Architecture</a>
    <a href="#benchmarks">Benchmarks</a>
  </nav>
</header>

<main>
  <section id="architecture">
    <h2>Cluster Architecture</h2>
    <p>Detailed topology documentation...</p>
  </section>
  <section id="benchmarks">
    <h2>Performance Benchmarks</h2>
    <p>Throughput graphs...</p>
  </section>
</main>
```

```css
.main-header {
  position: sticky;
  top: 0;
  height: 4rem; /* 64px */
  background-color: rgba(255, 255, 255, 0.8);
  backdrop-filter: blur(12px);
  z-index: 100;
  border-bottom: 1px solid #e2e8f0;
}

html {
  /* Offset scroll-to targets so headings are not covered by the 4rem header */
  scroll-padding-top: calc(4rem + 1rem); /* 4rem header + 1rem breathing room */
  scroll-behavior: smooth;
}

@media (prefers-reduced-motion: reduce) {
  html {
    scroll-behavior: auto;
  }
}
```

### Example 4: Accessible Invisible Scrollbar
```css
/* Completely hides scrollbar track while maintaining keyboard & swipe navigation */
.stealth-scroll-tray {
  display: flex;
  gap: 1rem;
  overflow-x: auto;
  
  /* Modern Standard */
  scrollbar-width: none;
  
  /* Legacy WebKit fallback */
  &::-webkit-scrollbar {
    display: none;
  }
}
```

### Example 5: High-Contrast Mode Scrollbar Defense
```css
/* Windows High Contrast Mode automatically renders system scrollbars.
   Never force custom scrollbars with zero contrast in WHCM. */
@media (forced-colors: active) {
  html, .dark-code-block {
    scrollbar-color: auto;
  }
}
```

---

## 6. Best Practices

1. **Always Use Modern Standard `scrollbar-color` and `scrollbar-width`**: Transition away from proprietary `::-webkit-scrollbar` hacks as your primary solution. Modern Baseline browsers universally support the standard properties.
2. **Always Pair Sticky Headers with `scroll-padding-top`**: Prevent anchor links from burying section headings underneath sticky headers.
3. **Always Add `prefers-reduced-motion` Guards to `scroll-behavior: smooth`**: Smooth scrolling across large page heights triggers vestibular vertigo and nausea in sensitive users.
4. **Use `scrollbar-gutter: stable` on Root Documents**: Eliminate horizontal layout shifts between short and long pages.
5. **Ensure Scrollbar Thumb Contrasts Against Track**: Always provide at least **3:1** contrast between the thumb and track colors for low-vision accessibility.
6. **Retain Legacy WebKit Rules for Backward Compatibility**: Include `::-webkit-scrollbar` rules below standard properties to support older embedded webviews.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Correction |
| :--- | :--- | :--- |
| **Omitting `scroll-padding-top` with sticky headers** | Anchor links scroll directly underneath the sticky header, hiding section titles. | Add `html { scroll-padding-top: [header-height]; }`. |
| **`scroll-behavior: smooth` without reduced-motion check** | Causes severe vestibular distress for users with motion sensitivity. | Wrap in `@media (prefers-reduced-motion: reduce)`. |
| **Using `::-webkit-scrollbar` exclusively** | Completely ignored by Firefox; Firefox users see unstyled system scrollbars. | Use modern standard `scrollbar-color` and `scrollbar-width`. |
| **Page jumping when scrollbar appears** | Adding dynamic content causes layout shift (CLS) as 16px scrollbar pushes content. | Add `scrollbar-gutter: stable;`. |
| **Setting `scrollbar-width: none` without keyboard focus** | Keyboard users cannot see or scroll through overflowing content. | Ensure container is reachable via `tabindex="0"`. |
| **Overly thin scrollbars on touchscreen laptops** | Users cannot grab or drag the scrollbar thumb with precision. | Use `scrollbar-width: thin;` or `auto`. |

---

## 8. Performance, Accessibility & Browser Support

### Accessibility Compliance (WCAG 2.2 AA)
- **Criterion 2.3.3 (Animation from Interactions)**: Motion animation triggered by interaction can be disabled. Smooth scrolling must honor `prefers-reduced-motion`.
- **Criterion 2.1.1 (Keyboard)**: Any scrollable element (`overflow: auto`) must have `tabindex="0"` so keyboard-only users can scroll using arrow keys.

### Compositor Scroll Optimization
In modern browser engines (Chromium LayoutNG, Safari WebKit), scrolling is executed on a dedicated compositor thread. Customizing scrollbars using `scrollbar-color` maintains GPU-accelerated scrolling without dropping back to the main CPU thread.

---

## 9. Comparison / When to Use What

| Technique | Standards Status | Cross-Browser | CLS Impact | Best Used For |
| :--- | :--- | :--- | :--- | :--- |
| **`scrollbar-color / width`** | **W3C Standard (Baseline 2024)**| **100% Modern Engines** | None | All modern scrollbar theming |
| **`::-webkit-scrollbar`** | Proprietary / Deprecated | WebKit / Chromium only | None | Legacy fallback styling |
| **`scrollbar-gutter: stable`** | **W3C Standard (Baseline 2022)**| Chrome, Safari 17+, Firefox | **Eliminates CLS** | Root `<html>` and dynamic lists |
| **`scroll-padding-top`** | **W3C Standard** | All modern browsers | None | Sticky header anchor offset |

---

## 10. Interview Questions

<details>
<summary>1. What are the modern W3C standard properties for styling scrollbars, and how do they replace legacy WebKit pseudo-elements?</summary>

The W3C CSS Scrollbars Styling Module Level 1 introduced two standardized properties:
1. `scrollbar-color: <thumb-color> <track-color>;`
2. `scrollbar-width: auto | thin | none;`

These replace proprietary vendor-prefixed pseudo-elements (`::-webkit-scrollbar`, `::-webkit-scrollbar-thumb`, `::-webkit-scrollbar-track`). While WebKit pseudo-elements allowed arbitrary CSS (shadows, borders, gradients) on scrollbars, they were never standardized, caused significant engine bloat, and were rejected by Firefox. The new properties provide a clean, declarative, high-performance cross-browser standard.
</details>

<details>
<summary>2. How does <code>scrollbar-gutter: stable</code> prevent Cumulative Layout Shift (CLS)?</summary>

When a web page transitions from a short view (content fits entirely in the viewport, no scrollbar needed) to a long view (content overflows, scrollbar appears), the browser introduces a vertical scrollbar (typically 15px–17px wide on desktop). 

Because the viewport available for layout shrinks by the scrollbar's width, the entire page content snaps to the left by 8px–16px. `scrollbar-gutter: stable` instructs the browser layout engine to reserve the scrollbar gutter space at all times, preventing horizontal layout reflow and eliminating CLS.
</details>

<details>
<summary>3. What bug occurs when clicking anchor links on a page with a <code>position: sticky</code> header, and how does CSS fix it?</summary>

When a user navigates to an anchor link (`#section-2`), the browser aligns the top border of `#section-2` with the top edge of the viewport ($Y = 0$). If the page has a sticky header (e.g., 60px tall), the header renders directly on top of the first 60px of the target section, occluding the heading text.

**Fix**: Declare `scroll-padding-top: 60px;` (or greater) on the `<html>` root element. This instructs the scroll snapping and anchor alignment engine to offset all scroll destinations by 60px below the viewport top edge.
</details>

<details>
<summary>4. Why must <code>scroll-behavior: smooth</code> always be paired with a <code>prefers-reduced-motion</code> media query?</summary>

Smooth scrolling causes the browser viewport to rapidly pan across the screen over several seconds. For users suffering from vestibular disorders, inner-ear balance impairments, or motion sickness, continuous unrequested camera panning triggers dizziness, nausea, and disorientation. Pairing `scroll-behavior: smooth` with:
```css
@media (prefers-reduced-motion: reduce) {
  html { scroll-behavior: auto; }
}
```
ensures that the browser immediately jumps to the target position for users requesting reduced motion.
</details>

<details>
<summary>5. What is the difference between <code>scrollbar-gutter: stable</code> and <code>scrollbar-gutter: stable both-edges</code>?</summary>

- `stable`: Reserves a scrollbar gutter only along the inline-end edge (the right edge in LTR). While this eliminates layout shifts when scrollbars appear, it slightly off-centers centered content (`margin: auto`).
- `stable both-edges`: Reserves identical gutter widths on **both** the inline-start and inline-end edges. This keeps centered layouts perfectly symmetrical on the screen.
</details>

<details>
<summary>6. How can you completely hide a scrollbar while keeping the element scrollable?</summary>

By declaring `scrollbar-width: none;` (for modern standard browsers) alongside `::-webkit-scrollbar { display: none; }` (for legacy WebKit engines). The element remains fully scrollable via mouse wheel, trackpad gestures, keyboard arrow keys, and touch dragging.
</details>

<details>
<summary>7. What accessibility requirement is mandatory when creating an internally scrollable container?</summary>

The container must be given `tabindex="0"`, an explicit accessible label (`aria-label="..."`), and a visible `:focus-visible` indicator. Without `tabindex="0"`, keyboard-only users have no way to focus the container and cannot scroll its content using arrow or Page Up/Page Down keys.
</details>

<details>
<summary>8. How do overlay scrollbars (macOS / mobile) interact with <code>scrollbar-gutter</code>?</summary>

On operating systems using floating overlay scrollbars (macOS with trackpads, iOS, Android), scrollbars float transparently *on top* of content without occupying physical box model space. On these platforms, `scrollbar-gutter` automatically recognizes that the platform does not consume layout space and does not create an unnecessary empty gap unless classic scrollbars are plugged in.
</details>

---

## 11. Practice Exercises

### Exercise 1: Build a Production Root Scrollbar Configuration
**Goal**: Configure the `<html>` root with stable gutters, anchor offsets for a 70px header, and accessible smooth scrolling.

<details>
<summary>View Solution</summary>

```css
html {
  scrollbar-gutter: stable;
  scrollbar-width: thin;
  scrollbar-color: #64748b #f8fafc;
  scroll-padding-top: calc(70px + 1.5rem);
  scroll-behavior: smooth;
}

@media (prefers-reduced-motion: reduce) {
  html {
    scroll-behavior: auto;
  }
}
```
</details>

### Exercise 2: Dark Theme Code Editor Scrollbar
**Goal**: Style a code block with a navy track and bright cyan thumb.

<details>
<summary>View Solution</summary>

```css
.code-editor-tray {
  max-height: 300px;
  overflow: auto;
  background-color: #020617;
  color: #38bdf8;
  padding: 1rem;
  scrollbar-width: thin;
  scrollbar-color: #0284c7 #0f172a;
}
```
</details>

### Exercise 3: Stealth Horizontal Category Bar
**Goal**: Build a horizontal tag navigation bar that scrolls horizontally with hidden scrollbars.

<details>
<summary>View Solution</summary>

```css
.tag-tray {
  display: flex;
  gap: 0.5rem;
  overflow-x: auto;
  white-space: nowrap;
  padding: 0.5rem 1rem;
  scrollbar-width: none;
}

.tag-tray::-webkit-scrollbar {
  display: none;
}
```
</details>

### Exercise 4: Symmetrical Centered Layout with `both-edges`
**Goal**: Ensure a centered article container remains mathematically centered on the screen regardless of scrollbar state.

<details>
<summary>View Solution</summary>

```css
.centered-reading-container {
  max-width: 65ch;
  margin-inline: auto;
  scrollbar-gutter: stable both-edges;
}
```
</details>

### Exercise 5: Keyboard-Accessible Scrollable Widget
**Goal**: Create an accessible scrollable terms-of-service box that keyboard users can navigate.

<details>
<summary>View Solution</summary>

```html
<div class="terms-box" tabindex="0" role="region" aria-label="Terms of Service Agreement">
  <p>Contractual terms and privacy guidelines...</p>
</div>
```

```css
.terms-box {
  max-height: 200px;
  overflow-y: auto;
  padding: 1rem;
  border: 1px solid #cbd5e1;
  border-radius: 6px;
  scrollbar-width: thin;
}

.terms-box:focus-visible {
  outline: 2px solid #2563eb;
  outline-offset: 2px;
}
```
</details>

### Exercise 6: Anchor Jump Verification Test
**Goal**: Write CSS that offsets anchor scrolling for both top sticky navbar and left fixed sidebar.

<details>
<summary>View Solution</summary>

```css
html {
  scroll-padding-top: 5rem;
  scroll-padding-left: 16rem;
}
```
</details>

---

## 12. Summary / Cheat Sheet

| Property | Values | Purpose |
| :--- | :--- | :--- |
| `scrollbar-color` | `<thumb> <track>` | Modern W3C standard scrollbar palette |
| `scrollbar-width` | `auto \| thin \| none` | Modern W3C standard scrollbar thickness |
| `scrollbar-gutter` | `stable \| both-edges` | Reserves gutter space to eliminate layout shift (CLS) |
| `scroll-padding-top` | `<length>` (e.g., `5rem`) | Offsets anchor link scrolling below sticky headers |
| `scroll-behavior` | `smooth \| auto` | Smooth scrolling; must honor `prefers-reduced-motion` |

---

## 13. Further Reading
- [W3C CSS Scrollbars Styling Module Level 1](https://www.w3.org/TR/css-scrollbars-1/)
- [W3C CSS Overflow Module Level 3 - scrollbar-gutter](https://www.w3.org/TR/css-overflow-3/#scrollbar-gutter-property)
- [MDN Web Docs: scrollbar-color](https://developer.mozilla.org/en-US/docs/Web/CSS/scrollbar-color)
- [MDN Web Docs: scrollbar-gutter](https://developer.mozilla.org/en-US/docs/Web/CSS/scrollbar-gutter)
- [web.dev: Prevent layout shifting with scrollbar-gutter](https://web.dev/blog/scrollbar-gutter)

---

[Previous: 09-styling-forms-and-inputs.md](09-styling-forms-and-inputs.md) | [Module README](README.md) | [Next: 11-cursors-and-pointer-events.md](11-cursors-and-pointer-events.md)
