# CSS Scroll Snap and Touch Pagination

| Metadata | Details |
| :--- | :--- |
| **Spec Level** | CSS Scroll Snap Module Level 1 (W3C Candidate Recommendation) |
| **Baseline Status** | Widely Available (Supported across all modern evergreen engines since 2019) |
| **Browsers Tested** | Chrome 69+, Safari 11+, Firefox 68+, Edge 79+ |
| **Tested With** | W3C Web Platform Tests, Chromium Compositor Profiler, Playwright Mobile Emulation |

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

Creating touch-friendly carousels, image galleries, and full-screen presentation sections historically required heavy JavaScript libraries (such as Swiper.js, Slick Carousel, or Flickity). These libraries intercepted touch, pointer, and wheel events on the main JavaScript thread to calculate inertia physics and animate offsets manually.

This legacy approach introduced severe problems:
- **Scroll Jitter & Input Lag**: If the JavaScript main thread lagged, touch dragging felt rubbery and unresponsive.
- **Breaking Native Browser Physics**: Mobile operating systems (iOS and Android) feature hardware-accelerated momentum, rubber-banding, and flick gestures that JavaScript libraries struggle to mimic faithfully.
- **Accessibility & Focus Trapping Failures**: Intercepted scrolling frequently prevented keyboard users (`Tab`, arrow keys) from scrolling items into view.

**CSS Scroll Snap** solves this by embedding pagination physics directly into the browser's native scrolling engine. Operating directly on the GPU Compositor thread, CSS Scroll Snap guarantees fluid 120 FPS momentum snapping while preserving native hardware flick mechanics and complete keyboard accessibility.

---

## 2. Core Concepts

### The Container Property: `scroll-snap-type`
Declared on the scroll container. Defines the axis and strictness of the snap behavior:
- **Axis**: `x` (horizontal), `y` (vertical), or `both`.
- **Strictness**:
  - `mandatory`: The scroll container **must** snap to a snap point whenever scrolling rests. It is impossible for the viewport to remain resting between two elements.
  - `proximity`: The container snaps only if the scroll position comes within a close threshold of a snap point; otherwise, it allows free-form resting.

```css
.carousel-track {
  overflow-x: auto;
  scroll-snap-type: x mandatory;
}
```

### The Item Property: `scroll-snap-align`
Declared on child items inside the container. Specifies which edge of the item aligns with the container's snapport:
- `start`: Snaps the leading edge of the item to the leading edge of the container.
- `center`: Centers the item perfectly inside the container viewport.
- `end`: Snaps the trailing edge of the item to the trailing edge of the container.

```css
.carousel-slide {
  scroll-snap-align: center;
}
```

### Fast Flicks: `scroll-snap-stop`
Controls whether fast flick gestures are permitted to skip past multiple snap points:
- `normal` (Default): Fast flick gestures can glide across multiple items before coming to rest.
- `always`: Forces the scrolling container to snap to the very next immediate element, preventing users from skipping slides regardless of how vigorously they flick.

### Handling Offsets: `scroll-padding` and `scroll-margin`
- **`scroll-padding`** (Container): Offsets the snapping alignment area inside the container. Indispensable for accounting for fixed sticky headers or floating navigation arrows.
- **`scroll-margin`** (Item): Adds an outward offset boundary around individual items during snap calculations.

---

## 3. How It Works Internally

```
Native Compositor Scroll Snapping Pipeline:

  [User Swipes Screen / Rolls Trackpad]
                    │
                    ▼
  [Compositor Thread evaluates Inertia & Velocity]
                    │
                    ▼
  [Scroll Snap Engine projects terminal resting position]
                    │
                    ├── If mandatory: Identifies nearest 'scroll-snap-align' target
                    └── Calculates exact snap coordinate
                    │
                    ▼
  [Applies Hardware Spring / Deceleration Curve directly on GPU]
                    │
                    ▼
  [Resting Frame Snapped with Zero Main Thread Overhead!]
```

```
Scroll Padding vs. Scroll Margin Geometry:
┌────────────────────────────────────────────────────────┐
│ Container Snapport (Visible Viewport)                  │
│                                                        │
│ ┌──────────────────────┐  <── scroll-padding-top       │
│ │ Effective Snap Edge  │                               │
│ │                      │                               │
│ │   ┌──────────────┐   │                               │
│ │   │ Snap Item    │   │                               │
│ │   │              │   │                               │
│ │   └──────────────┘   │                               │
│ └──────────────────────┘                               │
└────────────────────────────────────────────────────────┘
```

---

## 4. Syntax & Usage

```
Baseline Availability:
├── Chrome / Edge: 69+ (September 2018)
├── Safari: 11+ (September 2017)
└── Firefox: 68+ (July 2019)
* Widely available across all modern desktop and mobile browsers.
```

### Basic Syntax
```css
/* Container Setup */
.container {
  overflow-x: auto;
  scroll-snap-type: x mandatory;
  scroll-padding-inline: 1rem;
}

/* Child Item Setup */
.item {
  scroll-snap-align: start;
  scroll-snap-stop: always;
}
```

---

## 5. Practical Examples

### Example 1: High-Performance Touch Carousel
A smooth horizontal product carousel with peek preview of adjacent cards and no scrollbars.

```html
<div class="product-carousel" tabindex="0" role="region" aria-label="Featured Products">
  <article class="carousel-card">Product #1</article>
  <article class="carousel-card">Product #2</article>
  <article class="carousel-card">Product #3</article>
  <article class="carousel-card">Product #4</article>
</div>
```

```css
.product-carousel {
  display: flex;
  gap: 1.25rem;
  overflow-x: auto;
  scroll-snap-type: x mandatory;
  scroll-padding-inline: 1.5rem;
  padding: 1.5rem;

  /* Smooth keyboard scrolling */
  scroll-behavior: smooth;

  /* Hide scrollbars while preserving touch functionality */
  scrollbar-width: none; /* Firefox */
  &::-webkit-scrollbar {
    display: none; /* Safari & Chrome */
  }

  &:focus-visible {
    outline: 2px solid #2563eb;
    outline-offset: -2px;
  }
}

.carousel-card {
  flex: 0 0 18rem;
  scroll-snap-align: start;
  scroll-snap-stop: always;
  background-color: #ffffff;
  border: 1px solid #e2e8f0;
  border-radius: 0.75rem;
  padding: 2rem;
  box-shadow: 0 4px 6px -1px rgba(0, 0, 0, 0.1);
}
```

### Example 2: Full-Page Presentation Slides (Vertical Snapping)
A full-screen vertical presentation deck where scrolling cleanly snaps to each 100vh section.

```html
<main class="slides-deck">
  <section class="slide slide-1">
    <h1>01. Architecture</h1>
  </section>
  <section class="slide slide-2">
    <h1>02. Scalability</h1>
  </section>
  <section class="slide slide-3">
    <h1>03. Deployment</h1>
  </section>
</main>
```

```css
html, body {
  margin: 0;
  height: 100%;
}

.slides-deck {
  height: 100vh;
  overflow-y: auto;
  scroll-snap-type: y mandatory;
  scroll-behavior: smooth;
}

.slide {
  height: 100vh;
  width: 100vw;
  scroll-snap-align: start;
  scroll-snap-stop: always;
  display: flex;
  align-items: center;
  justify-content: center;
  font-family: system-ui, sans-serif;
  color: #ffffff;

  h1 {
    font-size: 3.5rem;
  }
}

.slide-1 { background-color: #1e3a8a; }
.slide-2 { background-color: #065f46; }
.slide-3 { background-color: #701a75; }
```

### Example 3: Center-Aligned Media Gallery with Fixed Overlay Header
Accounting for a sticky 4rem navigation bar using `scroll-padding`.

```html
<header class="sticky-nav">Navbar (4rem tall)</header>
<div class="gallery-feed">
  <div class="gallery-photo">Photo 1</div>
  <div class="gallery-photo">Photo 2</div>
</div>
```

```css
.sticky-nav {
  position: fixed;
  top: 0;
  left: 0;
  right: 0;
  height: 4rem;
  background-color: rgba(15, 23, 42, 0.85);
  backdrop-filter: blur(8px);
  z-index: 10;
}

.gallery-feed {
  height: 100vh;
  overflow-y: auto;
  scroll-snap-type: y mandatory;

  /* Crucial: offset snap alignment edge so photos aren't hidden behind header! */
  scroll-padding-top: 4.5rem;
}

.gallery-photo {
  height: 80vh;
  margin: 2rem 0;
  scroll-snap-align: start;
  background-color: #cbd5e1;
  border-radius: 0.5rem;
}
```

### Example 4: Proximity Snap for Subtle Card Rows
Using `scroll-snap-type: x proximity` so users can scroll freely, but items gently align if they stop nearby.

```css
.loose-carousel {
  display: flex;
  gap: 1rem;
  overflow-x: auto;
  scroll-snap-type: x proximity;
  padding: 1rem;
}

.loose-card {
  flex: 0 0 14rem;
  scroll-snap-align: center;
  background: white;
  border: 1px solid #e2e8f0;
  padding: 1rem;
}
```

### Example 5: 2D Grid Snapping (`scroll-snap-type: both`)
An interactive map or multi-directional canvas that snaps along both X and Y axes.

```css
.map-canvas {
  width: 100vw;
  height: 100vh;
  overflow: auto;
  scroll-snap-type: both mandatory;
  display: grid;
  grid-template-columns: repeat(4, 100vw);
  grid-template-rows: repeat(4, 100vh);
}

.map-quadrant {
  scroll-snap-align: center;
  border: 1px solid #cbd5e1;
}
```

---

## 6. Best Practices

1. **Always specify `scroll-behavior: smooth` on snap containers**: Ensures programmatic navigation (such as clicking next/prev arrow anchor links) animates smoothly rather than jumping instantly.
2. **Always use `scroll-padding` when sticky headers or floating buttons exist**: Without `scroll-padding`, snap points will align to the container's absolute boundary, placing the top or left of your cards behind sticky navigation elements.
3. **Use `scroll-snap-stop: always` for structured pagination**: If users are reading step-by-step onboarding cards or photo slides, prevent accidental over-scrolling with `scroll-snap-stop: always`.
4. **Make scroll containers keyboard-focusable**: Add `tabindex="0"` to the scroll container so keyboard-only users can focus the region and paginate using arrow keys.
5. **Never hide scrollbars without providing alternative visible indicators**: If hiding scrollbars via `scrollbar-width: none`, provide visible slide indicators (dots) or next/previous arrow buttons.
6. **Prefer `start` or `center` alignment over `end`**: `scroll-snap-align: start` is the most natural for horizontal reading orders (LTR); use `center` when highlighting a focused hero card among peeked neighbors.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Consequence | Correction |
| :--- | :--- | :--- |
| Forgetting `overflow-x: auto` on the container | Scroll snap has zero effect because container cannot scroll. | Always pair `scroll-snap-type` with `overflow: auto` or `scroll`. |
| Declaring `scroll-snap-type` on children instead of parent | Snapping fails completely; property is ignored on items. | Place `scroll-snap-type` on container; place `scroll-snap-align` on children. |
| Forgetting sticky header offset | Snapped cards get clipped under the sticky navigation bar. | Add `scroll-padding-top: <header-height>` to the scroll container. |
| Making items wider than the container with `mandatory` | Content is trapped; user cannot scroll to view the clipped portion of an oversized card. | Ensure item width does not exceed container width, or use `proximity`. |
| Missing keyboard focusability (`tabindex="0"`) | Keyboard users cannot tab to the carousel or use arrow keys to browse. | Add `tabindex="0"` and `:focus-visible` styling. |
| Forgetting `flex-shrink: 0` on flex items | Flexbox shrinks cards to fit in one screen instead of overflowing. | Set `flex: 0 0 <width>` on carousel items. |

---

## 8. Performance, Accessibility & Browser Support

### Performance
- **GPU Compositor Acceleration**: Snapping math runs completely inside the browser's hardware-accelerated compositor process.
- **Zero Script Thread Locks**: Operates without JavaScript event listeners, avoiding main-thread bottlenecks and preventing Interaction to Next Paint (INP) degradation.

### Accessibility (A11y)
- **WCAG 2.2 Compliance**: Native scroll snapping preserves standard keyboard scrolling (`ArrowLeft`, `ArrowRight`, `PageUp`, `PageDown`, `Home`, `End`).
- **Semantic ARIA Containers**:
```html
<div class="carousel" tabindex="0" role="region" aria-roledescription="carousel" aria-label="Customer Testimonials">
  <div class="slide" role="group" aria-roledescription="slide" aria-label="1 of 4">...</div>
</div>
```

### Browser Compatibility
- **Widely Available**: Full cross-browser support across all modern desktop and mobile browsers since 2019 (Chrome 69+, Safari 11+, Firefox 68+).
- **Feature Detection**:
```css
@supports (scroll-snap-type: x mandatory) {
  /* Supported */
}
```

---

## 9. Comparison / When to Use What

| Feature | CSS Scroll Snap | JavaScript Slider Library |
| :--- | :--- | :--- |
| **Execution Thread** | Compositor Thread (GPU) | JavaScript Main Thread (V8) |
| **Bundle Size** | **0 KB** | ~15–50 KB minified |
| **Mobile Momentum Physics**| 100% Native OS feel | Emulated (can feel sluggish) |
| **Keyboard Accessibility** | Built-in native browser support| Requires manual keyboard listeners |
| **Complex Callback Events** | Limited (requires IntersectionObserver) | Extensive (`onSlideChange`, etc.) |

---

## 10. Interview Questions

### Q1: What is the architectural difference between `scroll-snap-type: mandatory` and `proximity`?
<details>
<summary>View Answer</summary>
`mandatory` enforces that the scroll container **must** come to rest at a valid snap point. If the user stops dragging midway between two items, the engine automatically animates the container to the closest snap target. `proximity` only snaps if the user's natural scroll momentum brings them within a close distance to a snap point; otherwise, the container is permitted to come to rest at any free-form coordinate.
</details>

### Q2: What is the difference between `scroll-padding` and `scroll-margin` in the Scroll Snap specification?
<details>
<summary>View Answer</summary>
`scroll-padding` is declared on the **scroll container**; it defines an inset boundary inside the container's snapport (ideal for offsetting sticky top headers or navigation controls). `scroll-margin` is declared on the **child snap items**; it expands or contracts the item's individual snap target box relative to the container.
</details>

### Q3: What problem does `scroll-snap-stop: always` solve?
<details>
<summary>View Answer</summary>
By default (`scroll-snap-stop: normal`), a vigorous flick gesture can skip past multiple snap targets before momentum expires. Setting `scroll-snap-stop: always` forces the scrolling engine to trap and snap to the very next adjacent element, preventing fast gestures from skipping items in onboarding flows, slideshows, or product tours.
</details>

### Q4: Why does a flexbox container often fail to scroll snap horizontally unless `flex-shrink: 0` is set?
<details>
<summary>View Answer</summary>
By default, flexbox items have `flex-shrink: 1`. If several items are placed inside a flex container without wrapping, the browser will shrink each item so they all fit inside the container's single-screen width. Because the items don't overflow, no scroll container is created. Setting `flex: 0 0 100%` or `flex-shrink: 0` ensures items preserve their full width, forcing horizontal overflow and enabling scroll snapping.
</details>

### Q5: Can CSS Scroll Snap be combined with smooth anchor link navigation?
<details>
<summary>View Answer</summary>
Yes. Setting `scroll-behavior: smooth` on the container ensures that clicking in-page anchor links (e.g. `<a href="#slide-3">`) smoothly animates the container directly to the targeted element's snap point.
</details>

### Q6: How do you detect the actively snapped item in JavaScript?
<details>
<summary>View Answer</summary>
Rather than listening to `scroll` events, the modern standards-compliant approach is to use the **`IntersectionObserver` API**. By creating an observer with a threshold of $0.6$ to $1.0$, the observer fires cleanly whenever an item snaps into the center viewport, allowing UI dots or counters to update without performance penalties.
</details>

### Q7: What is the "Oversized Item Trap" in `scroll-snap-type: mandatory`?
<details>
<summary>View Answer</summary>
If a child item is taller or wider than the scroll container and `scroll-snap-align: start` is set with `mandatory`, the browser will force the container to snap to the top/start of the item every time scrolling stops. The user will be unable to scroll down to view the bottom of that item because the snap engine will repeatedly jerk the view back to the start.
</details>

### Q8: Does CSS Scroll Snap work on the root `<html>` element?
<details>
<summary>View Answer</summary>
Yes. Declaring `html { scroll-snap-type: y mandatory; }` and section elements with `scroll-snap-align: start; min-height: 100vh;` creates a full-page snapping layout that works across the entire browser viewport.
</details>

---

## 11. Practice Exercises

### Exercise 1: Three-Card Product Peek Carousel
**Objective**: Build a horizontal carousel where cards are 80% of viewport width and snap to the center, showing peeking edges of adjacent cards.
<details>
<summary>View Solution</summary>

```css
.peek-carousel {
  display: flex;
  gap: 1rem;
  overflow-x: auto;
  scroll-snap-type: x mandatory;
  scroll-padding-inline: 10%;
  padding: 1rem 0;
  scroll-behavior: smooth;
}

.peek-card {
  flex: 0 0 80%;
  scroll-snap-align: center;
  background: white;
  border-radius: 0.5rem;
  border: 1px solid #cbd5e1;
  padding: 1.5rem;
}
```
</details>

### Exercise 2: Vertical Story Feed (TikTok / Reels Style)
**Objective**: Create a 100vh vertical snapping container where users flip through full-screen story cards one by one.
<details>
<summary>View Solution</summary>

```css
.story-feed {
  height: 100vh;
  overflow-y: auto;
  scroll-snap-type: y mandatory;
  scrollbar-width: none;

  &::-webkit-scrollbar {
    display: none;
  }
}

.story-card {
  height: 100vh;
  scroll-snap-align: start;
  scroll-snap-stop: always;
  display: flex;
  align-items: center;
  justify-content: center;
}
```
</details>

### Exercise 3: Sticky Header Offset Compensation
**Objective**: Configure a vertical snap list that accounts for a 5rem sticky header using `scroll-padding-top`.
<details>
<summary>View Solution</summary>

```css
.feed-container {
  height: 100vh;
  overflow-y: auto;
  scroll-snap-type: y mandatory;
  scroll-padding-top: 5rem;
}

.feed-item {
  height: 70vh;
  scroll-snap-align: start;
  margin: 1.5rem;
  background: #f1f5f9;
}
```
</details>

### Exercise 4: Step-by-Step Wizard Slider
**Objective**: Build a multi-step form slider that forces users to view every single step without skipping slides on fast gestures.
<details>
<summary>View Solution</summary>

```css
.wizard-track {
  display: flex;
  overflow-x: auto;
  scroll-snap-type: x mandatory;
  scroll-behavior: smooth;
}

.wizard-step {
  flex: 0 0 100%;
  scroll-snap-align: start;
  scroll-snap-stop: always;
  padding: 2rem;
}
```
</details>

### Exercise 5: Accessible Keyboard Carousel Wrapper
**Objective**: Ensure a scroll snap carousel provides high-contrast keyboard focus indicators and smooth tabbing.
<details>
<summary>View Solution</summary>

```css
.keyboard-carousel {
  display: flex;
  gap: 1rem;
  overflow-x: auto;
  scroll-snap-type: x mandatory;
  padding: 1rem;

  &:focus-visible {
    outline: 2px solid #2563eb;
    outline-offset: -2px;
  }
}

.keyboard-carousel > * {
  scroll-snap-align: start;
}
```
</details>

### Exercise 6: Loose Proximity Product Shelf
**Objective**: Build an e-commerce shelf where users can scroll freely, but items snap to start if released within close range.
<details>
<summary>View Solution</summary>

```css
.shelf-track {
  display: flex;
  gap: 1.5rem;
  overflow-x: auto;
  scroll-snap-type: x proximity;
  padding: 1rem;
}

.shelf-item {
  flex: 0 0 16rem;
  scroll-snap-align: start;
  background: white;
  border: 1px solid #e2e8f0;
}
```
</details>

---

## 12. Summary / Cheat Sheet

```css
/* Container */
.carousel {
  display: flex;
  overflow-x: auto;
  scroll-snap-type: x mandatory; /* Axis & Mode */
  scroll-padding: 0 2rem;         /* Snapport insets */
  scroll-behavior: smooth;
}

/* Children */
.slide {
  flex: 0 0 100%;
  scroll-snap-align: start;      /* Alignment: start | center | end */
  scroll-snap-stop: always;       /* Prevent multi-item skipping */
}
```

- `scroll-snap-type`: `mandatory` forces snapping; `proximity` gently assists.
- `scroll-snap-align`: `start`, `center`, or `end`.
- `scroll-snap-stop: always` prevents over-flicking.
- `scroll-padding` offsets fixed/sticky navigation overlays.
- Widely available across all modern browsers.

---

## 13. Further Reading

- [MDN Web Docs: CSS Scroll Snap](https://developer.mozilla.org/en-US/docs/Web/CSS/CSS_scroll_snap)
- [W3C CSS Scroll Snap Module Level 1](https://www.w3.org/TR/css-scroll-snap-1/)
- [web.dev: Well-controlled scrolling with CSS Scroll Snap](https://web.dev/css-scroll-snap/)
- [CSS-Tricks: Practical CSS Scroll Snapping](https://css-tricks.com/practical-css-scroll-snapping/)

---

### Navigation
- **Previous**: [11-field-sizing-and-form-controls.md](file:///e:/Learning/engineering-notes/01-languages/css/09-modern-css/11-field-sizing-and-form-controls.md)
- **Next**: [13-content-visibility-and-containment.md](file:///e:/Learning/engineering-notes/01-languages/css/09-modern-css/13-content-visibility-and-containment.md)
