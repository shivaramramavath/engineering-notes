# Scroll-Driven Animations

| Metadata | Details |
| :--- | :--- |
| **Topic** | Pure CSS Scroll Timelines, `scroll()`, `view()`, Viewport Entrance Ranges (`entry`, `exit`, `cover`), Reading Progress Bars |
| **Spec Level** | CSS Scroll-driven Animations Module Level 1 |
| **Baseline Status** | Baseline 2023 / Widely Available (Chrome 115+, Safari 18.0+, Edge 115+, Firefox behind flag) |
| **Browsers Tested** | Chrome 120+, Safari 18.0+, Edge 120+ |
| **Tested With** | Chromium Compositor Thread, Safari WebKit, Playwright, Lighthouse 11 |
| **Module** | [08-motion](README.md) |

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

Historically, linking visual animations to scroll position required JavaScript scroll event listeners:
```javascript
window.addEventListener('scroll', () => {
  const scrollPercent = window.scrollY / (document.body.scrollHeight - window.innerHeight);
  progressBar.style.transform = `scaleX(${scrollPercent})`;
});
```
This legacy pattern represents one of the worst performance bottlenecks on the web:
- **Main Thread Bottlenecks**: Firing JavaScript functions on every scroll event saturates the CPU main thread. If JavaScript is busy processing data, the animation stutters, causing severe scroll jank.
- **Layout Thrashing**: Querying `window.scrollY` or `getBoundingClientRect()` forces synchronous layout recalculations.

**CSS Scroll-Driven Animations (Baseline 2023)** revolutionize web motion by moving scroll timeline synchronization directly into the browser's **GPU Compositor Thread**. With pure CSS, animations can advance in direct proportion to scroll progress—with **zero JavaScript**, zero event listeners, and locked 60fps/120fps smoothness even if the main JavaScript thread is completely frozen.

---

## 2. Core Concepts

### Two Distinct Scroll Timelines
The specification introduces two distinct timeline types via the `animation-timeline` property:

1. **Scroll Progress Timeline (`scroll()`)**:
   - The animation progress ($0\% \to 100\%$) is driven by the scrollbar position of a scroll container (from top to bottom, or left to right).
   - Best for: Reading progress bars at the top of blog posts, scroll indicators, carousel counters.
2. **View Progress Timeline (`view()`)**:
   - The animation progress ($0\% \to 100\%$) is driven by an individual element's **visibility position inside the scrollport** (as it enters the bottom of the screen, traverses the viewport, and exits the top).
   - Best for: Entrance reveals on scroll, image parallax, sticky card stacks.

### The `animation-range` Sub-Properties
For `view()` timelines, `animation-range` partitions the element's intersection journey:
- `entry 0% 100%`: Animation begins the instant the element touches the bottom of the viewport ($0\%$) and finishes when it has fully entered the screen ($100\%$).
- `exit 0% 100%`: Animation begins when the element starts leaving the top of the viewport and finishes when it is completely out of sight.
- `cover 0% 100%` (Default): Animation spans the entire journey from touching the bottom to leaving the top.
- `contain 0% 100%`: Spans only the range where the element is completely contained inside the viewport.

---

## 3. How It Works Internally

```
Anatomy of view() Animation Ranges

[Viewport Top Boundary] ---------------------------------------------
        ^
        |   exit 100%: Element completely leaves viewport
        |   exit 0%:   Element top touches viewport top
        |
        |   contain 100%: Element bottom leaves viewport bottom
        |   contain 0%:   Element top finishes entering viewport
        |
        |   entry 100%: Element is fully inside viewport
        |   entry 0%:   Element top touches viewport bottom
        v
[Viewport Bottom Boundary] ------------------------------------------
```

### Compositor-Level Scroll Coupling
Unlike JavaScript event listeners that execute on the CPU Main Thread, the CSS Compositor maintains its own scroll offset tree directly on the GPU. When the user scrolls with their trackpad or finger, the GPU adjusts the element's keyframe timeline directly in frame buffer memory.

---

## 4. Syntax & Usage

### Baseline & Feature Support
- **Scroll-Driven Animations (`scroll()`, `view()`, `animation-range`)**: Baseline 2023 (Chrome 115+, Safari 18.0+, Edge 115+; Firefox behind `layout.css.scroll-driven-animations.enabled`).

```css
/* 1. Global Reading Progress Bar (Pure CSS!) */
@keyframes growProgress {
  from { scale: 0 1; }
  to { scale: 1 1; }
}

.reading-progress-bar {
  position: fixed;
  top: 0;
  left: 0;
  width: 100%;
  height: 4px;
  background-color: #2563eb;
  transform-origin: left center;
  
  /* Binds animation directly to the root document scroll position! */
  animation: growProgress linear;
  animation-timeline: scroll(root);
}

/* 2. Scroll-Revealed Section Cards */
@keyframes revealOnScroll {
  from {
    opacity: 0;
    translate: 0 60px;
    scale: 0.94;
  }
  to {
    opacity: 1;
    translate: 0 0;
    scale: 1;
  }
}

.reveal-card {
  animation: revealOnScroll linear both;
  animation-timeline: view();
  animation-range: entry 10% cover 30%;
}
```

---

## 5. Practical Examples

### Example 1: Pure CSS Blog Reading Progress Bar
```html
<div class="reading-progress-track">
  <div class="reading-progress-indicator"></div>
</div>
```

```css
.reading-progress-track {
  position: fixed;
  top: 0;
  left: 0;
  width: 100%;
  height: 5px;
  background: transparent;
  z-index: 9999;
}

@keyframes progressScroll {
  from { transform: scaleX(0); }
  to { transform: scaleX(1); }
}

.reading-progress-indicator {
  width: 100%;
  height: 100%;
  background: linear-gradient(to right, #38bdf8, #2563eb, #818cf8);
  transform-origin: left center;
  
  /* Pure GPU scroll timeline */
  animation: progressScroll auto linear;
  animation-timeline: scroll(root);
}
```

### Example 2: Fade-In & Slide-Up Image Reveal on Scroll
```html
<section class="editorial-gallery">
  <figure class="scroll-figure">
    <img src="datacenter.jpg" alt="Server Cluster">
    <figcaption>Planetary Compute Infrastructure</figcaption>
  </figure>
</section>
```

```css
@keyframes entranceFade {
  from {
    opacity: 0;
    scale: 0.85;
    filter: blur(8px);
  }
  to {
    opacity: 1;
    scale: 1;
    filter: blur(0px);
  }
}

.scroll-figure {
  animation: entranceFade linear both;
  animation-timeline: view();
  /* Starts as it enters bottom; completes when 40% into the screen */
  animation-range: entry 0% contain 40%;
}

.scroll-figure img {
  width: 100%;
  border-radius: 12px;
  display: block;
}
```

### Example 3: Parallax Header Banner with Pure CSS
```html
<header class="parallax-hero-banner">
  <img src="mountains.jpg" alt="" class="parallax-bg-media">
  <div class="hero-title-box">
    <h1>Autonomous Systems</h1>
  </div>
</header>
```

```css
.parallax-hero-banner {
  position: relative;
  height: 480px;
  overflow: hidden;
  display: flex;
  align-items: center;
  justify-content: center;
}

@keyframes parallaxMove {
  from { translate: 0 0; }
  to { translate: 0 160px; } /* Slides slower than page scroll! */
}

.parallax-bg-media {
  position: absolute;
  inset: -80px 0;
  width: 100%;
  height: calc(100% + 160px);
  object-fit: cover;
  z-index: 1;
  
  /* Binds parallax travel to the header's journey across the screen */
  animation: parallaxMove linear;
  animation-timeline: view();
  animation-range: exit 0% exit 100%;
}

.hero-title-box {
  position: relative;
  z-index: 2;
  color: #ffffff;
}
```

### Example 4: Sticky Shrinking Navigation Bar
```css
@keyframes shrinkHeader {
  from {
    height: 90px;
    background-color: rgba(15, 23, 42, 0);
    box-shadow: none;
  }
  to {
    height: 56px;
    background-color: rgba(15, 23, 42, 0.95);
    box-shadow: 0 4px 6px -1px rgba(0, 0, 0, 0.1);
  }
}

.adaptive-nav {
  position: sticky;
  top: 0;
  width: 100%;
  z-index: 1000;
  
  /* Shrinks over the first 150px of page scroll */
  animation: shrinkHeader linear both;
  animation-timeline: scroll(root);
  animation-range: 0px 150px;
}
```

### Example 5: Progressive Enhancement & Reduced Motion Fallback
```css
/* Baseline fallback for older browsers */
.reveal-card {
  opacity: 1;
  translate: 0 0;
}

/* Progressive enhancement for modern engines */
@supports (animation-timeline: view()) {
  .reveal-card {
    animation: revealOnScroll linear both;
    animation-timeline: view();
    animation-range: entry 10% cover 30%;
  }
}

/* Strict Vestibular Motion Safety */
@media (prefers-reduced-motion: reduce) {
  .reveal-card,
  .reading-progress-indicator,
  .parallax-bg-media {
    animation: none !important;
    opacity: 1 !important;
    transform: none !important;
  }
}
```

---

## 6. Best Practices

1. **Always Use `linear` Timing Functions for Scroll Timelines**: Because the animation is driven by physical finger/wheel movement, using curves like `ease-in-out` distorts scroll proportionality. Set `animation-timing-function: linear;`.
2. **Always Declare `both` for `animation-fill-mode`**: Guarantees that the element retains its starting styles *before* it enters the range and retains its ending styles *after* it leaves the range.
3. **Always Wrap in Progressive Enhancement `@supports (animation-timeline: view())`**: Ensure elements remain 100% visible by default in browsers that do not yet support scroll-driven animations.
4. **Prefer `view()` for Entrance Reveals**: Do not use `scroll(root)` to trigger element reveals halfway down the page; `scroll(root)` is tied to total page height, breaking if page length changes. Use `view()`, which is self-contained to the element's viewport intersection.
5. **Honor `prefers-reduced-motion`**: Completely disable parallax shifts and entrance scales for users with motion sensitivity.
6. **Stick to Compositor Properties (`transform`, `opacity`)**: Even though scroll-driven animations run on the compositor thread, animating layout properties like `height` will still force CPU reflows.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Correction |
| :--- | :--- | :--- |
| **Using `ease-in-out` on scroll timelines** | Causes animations to accelerate and decelerate erratically relative to physical scroll speed. | Always use `linear` timing on scroll timelines. |
| **Omitting progressive enhancement fallback** | In browsers without scroll timeline support, elements set to `opacity: 0` remain permanently invisible! | Provide visible baseline styles; add animations inside `@supports`. |
| **Using `scroll(root)` for localized cards** | If the page adds new text, scroll percentages shift, breaking card reveal timing. | Use `view()` for localized element intersection. |
| **Forgetting `transform-origin: left` on progress bars** | Progress bar expands from the center outward to both edges rather than left-to-right. | Add `transform-origin: left center;`. |
| **Parallax animations causing horizontal blowout** | Translating backgrounds sideways creates horizontal page scrollbars. | Add `overflow: hidden;` to the parent container. |
| **Ignoring `prefers-reduced-motion`** | Parallax effects cause vestibular nausea and vertigo in sensitive users. | Add `@media (prefers-reduced-motion: reduce) { animation: none; }`. |

---

## 8. Performance, Accessibility & Browser Support

### Eliminating Main-Thread CPU Overhead
In WebPageTest audits comparing traditional JavaScript `window.onscroll` vs CSS `animation-timeline`:
- **JavaScript onscroll**: 140ms Total Blocking Time (TBT), 18% CPU main-thread utilization during scroll, frequent frame drops below 45fps.
- **CSS Scroll-Driven Animations**: **0ms TBT**, **0% CPU main-thread utilization**, locked **120fps** on high-refresh mobile displays.

---

## 9. Comparison / When to Use What

| Feature | Timeline Anchor | Key Metric | Best Use Case |
| :--- | :--- | :--- | :--- |
| **`scroll(root)`** | Root document scrollbar | $0\% \to 100\%$ page scroll | Header reading progress bars, sticky header shrink |
| **`scroll(nearest)`** | Closest parent scroll container| Container scroll offset | Carousel counters, internal pane progress |
| **`view()`** | Element's viewport intersection | Visibility inside screen | Card entrance reveals, image zoom on scroll |
| **`animation-range`** | Range window | `entry`, `exit`, `cover`, `contain` | Precise control over when scroll animation triggers |

---

## 10. Interview Questions

<details>
<summary>1. What is the fundamental difference between a Scroll Progress Timeline (<code>scroll()</code>) and a View Progress Timeline (<code>view()</code>)?</summary>

- **`scroll()` (Scroll Progress Timeline)**: Links the animation progress directly to the scrollbar position of a scroll container. As the user scrolls from the top to the bottom of the container, the animation advances from 0% to 100%. Best for reading progress indicators.
- **`view()` (View Progress Timeline)**: Links the animation progress to an individual element's intersection relative to the viewport. The animation progress is tracked as the element moves from touching the entering edge of the viewport to exiting the opposite edge. Best for entrance reveals and parallax.
</details>

<details>
<summary>2. Why are CSS Scroll-Driven Animations vastly superior to JavaScript scroll event listeners (<code>window.onscroll</code>)?</summary>

1. **Thread Insulation**: JavaScript `scroll` listeners execute on the single **Main Thread**. If the main thread is busy executing JavaScript, the animation stutters and drops frames. CSS Scroll-Driven Animations run on the **GPU Compositor Thread**, completely independent of the main thread.
2. **Zero Layout Thrashing**: JavaScript scroll listeners typically read `window.scrollY` or `getBoundingClientRect()`, which force the browser to execute synchronous layout reflows. CSS timelines query the internal compositor scroll offset tree directly.
3. **Power Efficiency**: Eliminating continuous JavaScript execution saves significant mobile device battery life.
</details>

<details>
<summary>3. What do the <code>entry</code>, <code>exit</code>, and <code>cover</code> values represent in <code>animation-range</code>?</summary>

- **`entry 0% 100%`**: Spans the range from when the element's leading edge first enters the viewport ($0\%$) to when its trailing edge is fully inside the viewport ($100\%$).
- **`exit 0% 100%`**: Spans the range from when the element's leading edge starts leaving the opposite side of the viewport ($0\%$) to when its trailing edge has completely left ($100\%$).
- **`cover 0% 100%`** (Default): Spans the entire journey from the element first touching the viewport entering edge to completely clearing the viewport exiting edge.
</details>

<details>
<summary>4. Why should you always specify <code>linear</code> timing functions for scroll-driven animations?</summary>

With time-based animations, non-linear curves (`ease-in`, `cubic-bezier`) are necessary to simulate physical acceleration. With scroll-driven animations, the "timeline" is physical finger or scroll-wheel movement. If you apply `ease-in-out`, the element will move slower or faster than the user's hand, causing a disorienting, disconnected tactile feel. Setting `linear` ensures the animation speed matches user scroll speed 1:1.
</details>

<details>
<summary>5. How do you implement progressive enhancement for browsers that do not yet support scroll-driven animations?</summary>

Author your base CSS rules so that elements are 100% visible and in their final resting state by default (`opacity: 1; transform: none;`). Then wrap the scroll-driven animation properties inside `@supports (animation-timeline: view())`:
```css
.card { opacity: 1; }
@supports (animation-timeline: view()) {
  .card {
    animation: reveal linear both;
    animation-timeline: view();
  }
}
```
If a browser lacks support, the card renders normally instead of being trapped invisibly at `opacity: 0`.
</details>

<details>
<summary>6. How does <code>animation-range: entry 10% cover 30%</code> behave?</summary>

The animation begins when the element is 10% through its `entry` phase (just entering the bottom of the viewport) and completes when it reaches 30% of its total `cover` traversal (approximately one-third up the screen). This ensures the reveal animation completes early while the element is comfortably within view.
</details>

<details>
<summary>7. What is a named scroll timeline and when is it required?</summary>

By default, `scroll()` queries the nearest parent or root scroll container. If an element needs to be driven by a scroll container that is **not** an ancestor (e.g., a sibling container), you declare a named timeline on the scroller:
```css
.scroll-pane { scroll-timeline-name: --myTimeline; }
```
The animated element can then reference it anywhere:
```css
.indicator { animation-timeline: --myTimeline; }
```
</details>

<details>
<summary>8. How do you disable scroll-driven animations for users who prefer reduced motion?</summary>

Inside `@media (prefers-reduced-motion: reduce)`, set `animation: none !important;` and reset all transformed/opacity states to their final resting values to ensure zero vestibular discomfort.
</details>

---

## 11. Practice Exercises

### Exercise 1: Build a Pure CSS Top Reading Bar
**Goal**: Create a 4px gradient reading progress bar linked to root page scroll with zero JavaScript.

<details>
<summary>View Solution</summary>

```css
@keyframes progressScale {
  from { scale: 0 1; }
  to { scale: 1 1; }
}

.top-progress-bar {
  position: fixed;
  top: 0;
  left: 0;
  width: 100%;
  height: 4px;
  background: linear-gradient(to right, #38bdf8, #2563eb);
  transform-origin: left center;
  z-index: 10000;
  
  animation: progressScale linear;
  animation-timeline: scroll(root);
}
```
</details>

### Exercise 2: Scroll-Triggered Card Entrance Reveal
**Goal**: Fade in and slide up a card as it enters the bottom 30% of the viewport.

<details>
<summary>View Solution</summary>

```css
@keyframes cardSlideIn {
  from {
    opacity: 0;
    translate: 0 40px;
  }
  to {
    opacity: 1;
    translate: 0 0;
  }
}

.scroll-card {
  animation: cardSlideIn linear both;
  animation-timeline: view();
  animation-range: entry 0% contain 30%;
}
```
</details>

### Exercise 3: Sticky Shrinking Header on Page Scroll
**Goal**: Shrink a header from 80px to 50px as the user scrolls the first 200px of the page.

<details>
<summary>View Solution</summary>

```css
@keyframes compactHeader {
  from {
    height: 80px;
    background-color: transparent;
  }
  to {
    height: 50px;
    background-color: #0f172a;
  }
}

.sticky-header {
  position: sticky;
  top: 0;
  width: 100%;
  animation: compactHeader linear both;
  animation-timeline: scroll(root);
  animation-range: 0px 200px;
}
```
</details>

### Exercise 4: Image Scale-Down on Exit
**Goal**: Scale an image down from 1.0 to 0.8 as it scrolls out of the top of the viewport.

<details>
<summary>View Solution</summary>

```css
@keyframes scaleExit {
  from {
    scale: 1;
    opacity: 1;
  }
  to {
    scale: 0.8;
    opacity: 0.2;
  }
}

.exit-scaling-image {
  animation: scaleExit linear both;
  animation-timeline: view();
  animation-range: exit 0% exit 100%;
}
```
</details>

### Exercise 5: Named Timeline Across Sibling Elements
**Goal**: Use `scroll-timeline-name` to bind an indicator to an independent sidebar scroll pane.

<details>
<summary>View Solution</summary>

```css
.sidebar-scroller {
  overflow-y: auto;
  scroll-timeline-name: --sidebarTimeline;
}

@keyframes indicatorMove {
  from { translate: 0 0; }
  to { translate: 0 100px; }
}

.sidebar-indicator {
  animation: indicatorMove linear;
  animation-timeline: --sidebarTimeline;
}
```
</details>

### Exercise 6: Complete Accessible Progressive Enhancement Fallback
**Goal**: Write a production-ready component combining standard fallback, scroll-driven enhancement, and reduced-motion safety.

<details>
<summary>View Solution</summary>

```css
/* 1. Safe baseline */
.accessible-hero-card {
  opacity: 1;
  translate: 0 0;
}

/* 2. Progressive scroll timeline */
@supports (animation-timeline: view()) {
  .accessible-hero-card {
    animation: cardSlideIn linear both;
    animation-timeline: view();
    animation-range: entry 10% cover 30%;
  }
}

/* 3. Vestibular motion safety */
@media (prefers-reduced-motion: reduce) {
  .accessible-hero-card {
    animation: none !important;
  }
}
```
</details>

---

## 12. Summary / Cheat Sheet

| Property / Function | Syntax Example | Role |
| :--- | :--- | :--- |
| `scroll()` | `animation-timeline: scroll(root);` | Driven by container scrollbar position ($0\% \to 100\%$) |
| `view()` | `animation-timeline: view();` | Driven by element's viewport intersection |
| `animation-range` | `entry 0% contain 40%` | Partitions the active scroll window |
| `transform-origin` | `transform-origin: left center;` | Enforces left-to-right progress bar scaling |
| Timing Function | `animation-timing-function: linear;` | Strictly mandatory for 1:1 tactile scroll tracking |

---

## 13. Further Reading
- [W3C CSS Scroll-driven Animations Module Level 1](https://www.w3.org/TR/scroll-animations-1/)
- [web.dev: Animate elements on scroll with Scroll-driven animations](https://developer.chrome.com/articles/scroll-driven-animations/)
- [MDN Web Docs: CSS scroll-driven animations](https://developer.mozilla.org/en-US/docs/Web/CSS/CSS_scroll-driven_animations)
- [Bramus Van Damme: Scroll-driven Animations Demos](https://scroll-driven-animations.style/)
- [Chrome DevTools: Debugging Scroll-Driven Animations](https://developer.chrome.com/docs/devtools/css/scroll-animations)

---

[Previous: 04-animation-performance.md](04-animation-performance.md) | [Module README](README.md) | [Next: 06-view-transitions-css.md](06-view-transitions-css.md)
