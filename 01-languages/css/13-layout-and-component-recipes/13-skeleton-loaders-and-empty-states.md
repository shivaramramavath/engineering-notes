# Skeleton Loaders and Empty States in Modern CSS

| Topic | Module | Level | Baseline Status | Browsers Tested | Date | Prerequisites |
|---|---|---|---|---|---|---|
| Skeletons & Empty States | 13: Layout & Component Recipes | Intermediate / Advanced | Widely Available | Chrome 120+, Firefox 120+, Safari 17.4+ | 2026-03-31 | Gradients, Animations, Accessibility, Layout |

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
9. [Comparison: Asynchronous Loading States](#9-comparison-asynchronous-loading-states)
10. [Interview Questions](#10-interview-questions)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary / Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

Modern web applications are asynchronous by design. Client-side single-page applications, server components, and edge rendering pipelines stream data into the browser in chunks. How an application behaves *while data is loading* or *when no data exists* dictates user perception of speed and product polish.

Historically, loading states and empty views suffered from severe technical and UX issues:
- **Catastrophic Cumulative Layout Shift (CLS)**: Rendering an isolated 20px spinning wheel in the center of the screen caused surrounding elements to abruptly jump hundreds of pixels downward when the actual 400px data card arrived.
- **CPU-Intensive Shimmer Jank**: Animating gradients using continuous `background-position` recalculations triggered repetitive paint cycles, draining mobile batteries and causing stutter during scroll.
- **Screen Reader Annoyance**: Skeletons without accessibility safeguards were read out as empty, unlabelled lists of meaningless shapes, or caused repetitive live region announcements.
- **Sterile Empty States**: Blank white pages with tiny text like "0 items found" left users confused, offering no guidance on what action to take next.

Modern CSS solves these challenges with mathematical precision:
- **Volume Reservation**: Skeletons that match the exact computed dimensions and grid constraints of incoming data structures, locking CLS to strictly 0.000.
- **Composited Mask Shimmers**: High-performance linear-gradient shimmers leveraging CSS masking or `transform: translateX()`, executing exclusively on the GPU compositor thread.
- **Accessible State Encapsulation**: Proper semantic use of `aria-busy="true"` and `aria-hidden="true"`.
- **Actionable Empty States**: Responsive, illustrated fallback layouts that guide users directly toward creation flows.

---

## 2. Core Concepts

### 1. Layout Pre-allocation (Zero CLS)
A skeleton screen is a low-fidelity wireframe placeholder that represents the exact geometry of incoming UI components. 
Instead of rendering a single generic loader:
- Headings are represented by rectangular blocks matching standard line-heights (`1.5rem`).
- Avatars are represented by fixed circles (`aspect-ratio: 1; border-radius: 50%`).
- Cards reserve their full `aspect-ratio` or `min-height`.

When dynamic data replaces the skeleton, zero layout recalculation occurs because the spatial volume was already allocated in the render tree.

### 2. High-Performance Shimmer Mechanics
Standard background-position animations (`background-position: 100% 0`) force the browser to repaint pixels continuously. 
The modern performant alternative uses an absolute pseudo-element (`::after`) with a fixed gradient that translates horizontally across the element:
```css
.skeleton {
  position: relative;
  overflow: hidden;
  background-color: #e2e8f0;
}

.skeleton::after {
  content: "";
  position: absolute;
  inset: 0;
  transform: translateX(-100%);
  background-image: linear-gradient(
    90deg,
    rgba(255, 255, 255, 0) 0%,
    rgba(255, 255, 255, 0.6) 50%,
    rgba(255, 255, 255, 0) 100%
  );
  animation: shimmer 1.5s infinite;
}

@keyframes shimmer {
  100% {
    transform: translateX(100%);
  }
}
```
Because `transform` is managed by the GPU compositor, the animation runs at a locked 60fps/120fps with zero layout or repaint overhead.

---

## 3. How It Works Internally

```
[Dynamic Data Request Initiated]
        │
        ▼
[Inject Skeleton Container into DOM: aria-busy="true"]
        │
        ├── Allocate Layout Box (width: 100%; aspect-ratio: 16 / 9)
        │     └── Browser geometry engine locks DOM coordinates (CLS = 0)
        │
        └── Paint Base Neutral Tone (background: #e2e8f0)
              │
              ▼
        [Compositor Layer: ::after Pseudo-element]
        ├── Upload Gradient Texture to GPU
        └── Animate transform: translateX(-100% -> 100%)
              │
              ▼
[Data Arrives from Network]
        │
        ▼
[Swap Inner Nodes, toggle aria-busy="false"]
        │
        ▼
[Data renders smoothly into identical reserved pixel box]
```

---

## 4. Syntax & Usage

### Baseline Browser Support
| Feature | Baseline Status | Chrome / Edge | Firefox | Safari |
|---|---|---|---|---|
| CSS `aspect-ratio` | Widely Available | 88+ | 89+ | 15.0+ |
| GPU Composited `transform` | Widely Available | 36+ | 16+ | 9.0+ |
| `prefers-reduced-motion` | Widely Available | 74+ | 63+ | 10.1+ |
| ARIA Busy (`aria-busy="true"`) | Widely Available | 1+ | 1+ | 1+ |

### Core Skeleton Boilerplate
```css
.skeleton {
  background-color: var(--skeleton-base, #e2e8f0);
  border-radius: var(--skeleton-radius, 4px);
  position: relative;
  overflow: hidden;
}

.skeleton::after {
  content: "";
  position: absolute;
  inset: 0;
  transform: translateX(-100%);
  background: linear-gradient(
    90deg,
    transparent 0%,
    rgba(255, 255, 255, 0.4) 50%,
    transparent 100%
  );
  animation: skeleton-shimmer 1.5s infinite;
}

@keyframes skeleton-shimmer {
  100% {
    transform: translateX(100%);
  }
}

@media (prefers-reduced-motion: reduce) {
  .skeleton::after {
    animation: none;
    display: none;
  }
  .skeleton {
    animation: skeleton-pulse 2s cubic-bezier(0.4, 0, 0.6, 1) infinite;
  }
}

@keyframes skeleton-pulse {
  0%, 100% { opacity: 1; }
  50% { opacity: 0.5; }
}
```

---

## 5. Practical Examples

### Example 1: Full Metric / Profile Card Skeleton Shell
```html
<div class="skeleton-card" aria-busy="true" aria-label="Loading user profile...">
  <div class="skeleton-header">
    <div class="skeleton skeleton-avatar" aria-hidden="true"></div>
    <div class="skeleton-meta">
      <div class="skeleton skeleton-line title" aria-hidden="true"></div>
      <div class="skeleton skeleton-line subtitle" aria-hidden="true"></div>
    </div>
  </div>

  <div class="skeleton-body">
    <div class="skeleton skeleton-line" aria-hidden="true"></div>
    <div class="skeleton skeleton-line" aria-hidden="true"></div>
    <div class="skeleton skeleton-line short" aria-hidden="true"></div>
  </div>

  <div class="skeleton-footer">
    <div class="skeleton skeleton-btn" aria-hidden="true"></div>
  </div>
</div>
```

```css
.skeleton-card {
  width: 100%;
  max-width: 420px;
  background: #ffffff;
  border: 1px solid #e2e8f0;
  border-radius: 12px;
  padding: 1.5rem;
  box-shadow: 0 1px 3px rgba(0, 0, 0, 0.05);
}

.skeleton {
  background-color: #f1f5f9;
  border-radius: 4px;
  position: relative;
  overflow: hidden;
}

.skeleton::after {
  content: "";
  position: absolute;
  inset: 0;
  transform: translateX(-100%);
  background: linear-gradient(
    90deg,
    rgba(241, 245, 249, 0) 0%,
    rgba(255, 255, 255, 0.8) 50%,
    rgba(241, 245, 249, 0) 100%
  );
  animation: shimmer 1.4s ease-in-out infinite;
}

@keyframes shimmer {
  100% { transform: translateX(100%); }
}

.skeleton-header {
  display: flex;
  align-items: center;
  gap: 1rem;
  margin-bottom: 1.25rem;
}

.skeleton-avatar {
  width: 48px;
  height: 48px;
  border-radius: 50%;
  flex-shrink: 0;
}

.skeleton-meta {
  flex: 1;
  display: flex;
  flex-direction: column;
  gap: 0.5rem;
}

.skeleton-line {
  height: 12px;
  width: 100%;
}

.skeleton-line.title {
  height: 16px;
  width: 60%;
}

.skeleton-line.subtitle {
  width: 40%;
}

.skeleton-body {
  display: flex;
  flex-direction: column;
  gap: 0.625rem;
  margin-bottom: 1.5rem;
}

.skeleton-line.short {
  width: 75%;
}

.skeleton-footer {
  display: flex;
  justify-content: flex-end;
}

.skeleton-btn {
  width: 110px;
  height: 36px;
  border-radius: 6px;
}
```

---

### Example 2: Data Table Rows Skeleton
```html
<table class="skeleton-table" aria-busy="true">
  <thead>
    <tr>
      <th>Cluster</th>
      <th>Region</th>
      <th>Status</th>
      <th>Throughput</th>
    </tr>
  </thead>
  <tbody>
    <tr>
      <td><div class="skeleton sk-cell w-80"></div></td>
      <td><div class="skeleton sk-cell w-60"></div></td>
      <td><div class="skeleton sk-badge"></div></td>
      <td><div class="skeleton sk-cell w-40"></div></td>
    </tr>
    <tr>
      <td><div class="skeleton sk-cell w-80"></div></td>
      <td><div class="skeleton sk-cell w-60"></div></td>
      <td><div class="skeleton sk-badge"></div></td>
      <td><div class="skeleton sk-cell w-40"></div></td>
    </tr>
  </tbody>
</table>
```

```css
.skeleton-table {
  width: 100%;
  border-collapse: collapse;
}

.skeleton-table th,
.skeleton-table td {
  padding: 0.875rem 1rem;
  border-bottom: 1px solid #f1f5f9;
  text-align: left;
}

.skeleton-table th {
  background: #f8fafc;
  color: #64748b;
  font-size: 0.8125rem;
}

.sk-cell {
  height: 14px;
}

.sk-cell.w-80 { width: 80%; }
.sk-cell.w-60 { width: 60%; }
.sk-cell.w-40 { width: 40%; }

.sk-badge {
  width: 64px;
  height: 20px;
  border-radius: 9999px;
}
```

---

### Example 3: Rich Empty State with Call to Action
```html
<div class="empty-state-canvas">
  <div class="empty-illustration" aria-hidden="true">
    <div class="icon-circle">📂</div>
  </div>
  
  <h3 class="empty-title">No Storage Buckets Configured</h3>
  <p class="empty-description">
    Create an S3-compatible encrypted object storage bucket to host assets, database backups, and telemetry archives.
  </p>

  <div class="empty-actions">
    <button type="button" class="btn btn-primary">Create New Bucket</button>
    <a href="#docs" class="btn btn-secondary">Read Storage Guide</a>
  </div>
</div>
```

```css
.empty-state-canvas {
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  text-align: center;
  padding: 4rem 2rem;
  background: #ffffff;
  border: 2px dashed #cbd5e1;
  border-radius: 12px;
  max-width: 600px;
  margin: 2rem auto;
}

.empty-illustration {
  margin-bottom: 1.5rem;
}

.icon-circle {
  width: 72px;
  height: 72px;
  border-radius: 50%;
  background: #eff6ff;
  border: 1px solid #bfdbfe;
  display: flex;
  align-items: center;
  justify-content: center;
  font-size: 2rem;
}

.empty-title {
  font-size: 1.25rem;
  font-weight: 700;
  color: #0f172a;
  margin: 0 0 0.5rem 0;
}

.empty-description {
  font-size: 0.95rem;
  color: #64748b;
  line-height: 1.5;
  max-width: 44ch;
  margin: 0 0 2rem 0;
}

.empty-actions {
  display: flex;
  gap: 1rem;
  flex-wrap: wrap;
  justify-content: center;
}

.btn {
  padding: 0.625rem 1.25rem;
  border-radius: 6px;
  font-weight: 600;
  cursor: pointer;
  text-decoration: none;
  font-size: 0.875rem;
}

.btn-primary {
  background: #2563eb;
  color: #ffffff;
  border: 1px solid #1d4ed8;
}

.btn-secondary {
  background: #f1f5f9;
  color: #334155;
  border: 1px solid #cbd5e1;
}
```

---

### Example 4: Dark-Mode Scoped Skeleton Wave
```html
<div class="card dark-mode-preview">
  <div class="skeleton dark-sk avatar"></div>
  <div class="skeleton dark-sk line"></div>
</div>
```

```css
.dark-mode-preview {
  background: #0f172a;
  padding: 1.5rem;
  border-radius: 8px;
}

.dark-sk {
  background-color: #1e293b;
}

.dark-sk::after {
  background: linear-gradient(
    90deg,
    rgba(30, 41, 59, 0) 0%,
    rgba(51, 65, 85, 0.6) 50%,
    rgba(30, 41, 59, 0) 100%
  );
}

.avatar {
  width: 40px;
  height: 40px;
  border-radius: 50%;
  margin-bottom: 0.75rem;
}

.line {
  height: 14px;
  width: 70%;
}
```

---

### Example 5: Search Query "No Results" Empty Card
```html
<div class="no-results-panel">
  <span class="search-bubble" aria-hidden="true">🔍</span>
  <h4>No transactions matching "INV-999"</h4>
  <p>Check the reference ID for typos or broaden your date filter window.</p>
  <button type="button" class="btn-clear">Reset All Filters</button>
</div>
```

```css
.no-results-panel {
  padding: 3rem 1rem;
  text-align: center;
  background: #f8fafc;
  border: 1px solid #e2e8f0;
  border-radius: 8px;
}

.search-bubble {
  font-size: 2.25rem;
  display: block;
  margin-bottom: 1rem;
}

.no-results-panel h4 {
  margin: 0 0 0.5rem 0;
  color: #1e293b;
}

.no-results-panel p {
  margin: 0 0 1.5rem 0;
  color: #64748b;
  font-size: 0.875rem;
}

.btn-clear {
  background: #ffffff;
  border: 1px solid #cbd5e1;
  padding: 0.5rem 1rem;
  border-radius: 6px;
  font-weight: 500;
  color: #334155;
  cursor: pointer;
}
```

---

## 6. Best Practices

1. **Pre-allocate Identical Geometry**: Skeletons must mimic the exact bounding box, grid columns, and line-heights of the real incoming data cards to keep Cumulative Layout Shift (CLS) at 0.
2. **Animate `transform: translateX()` Over `background-position`**: Shimmer animations using transforms run on the GPU compositor thread without triggering layout reflows or repaints.
3. **Hide Skeletons from Screen Readers**: Annotate individual skeleton placeholder blocks with `aria-hidden="true"`, and label the parent container with `aria-busy="true"` and an `aria-label="Loading..."` announcement.
4. **Respect `prefers-reduced-motion`**: For users with vestibular motion sensitivities, disable sweeping shimmers and fall back to subtle static opacity pulses (`animation: pulse 2s infinite`).
5. **Always Pair Empty States with Direct Actions**: Never display a dead-end message like "No data found". Provide a clear, actionable button (e.g. "Create new project", "Reset filters").
6. **Use Variable Line Widths for Text Blocks**: When rendering multi-line paragraph skeletons, vary the width of the final line (`width: 60%`) to look like natural typography.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Root Cause | Symptom | Correct Architecture |
|---|---|---|---|
| **High CLS on Load** | Skeleton height does not match final data card height | Entire page shifts down 200px when real content renders | Pre-allocate exact volume using `aspect-ratio` or `min-height` |
| **Battery Drain from Shimmer** | Animating `background-position` on 50 skeleton cells | CPU/GPU churns continuously; mobile frame rate drops | Use `transform: translateX()` on `::after` pseudo-elements |
| **Screen Reader Verbosity** | Skeletons lack `aria-hidden="true"` | Screen reader repeatedly announces empty shapes and meaningless spans | Apply `aria-hidden="true"` to placeholders |
| **Unresponsive Empty State** | Fixed pixel width (`width: 500px`) on empty state container | Empty card clips off-screen on mobile devices | Use `max-width: 500px; width: 100%;` |
| **Flash of Skeleton (FOUC)** | Displaying skeleton for ultra-fast <50ms cache hits | Screen flickers white/grey momentarily before showing data | Implement a 200ms debounce before rendering skeletons |
| **Missing Reduced Motion Guard**| Continuous bright shimmer on `prefers-reduced-motion` | Causes discomfort for users with vestibular disorders | Disable shimmer in `@media (prefers-reduced-motion: reduce)` |

---

## 8. Performance, Accessibility & Browser Support

### Performance Optimization
- **Layer Promotion**: When rendering skeleton grids containing dozens of cards, declare `contain: paint layout` on each card to isolate individual skeleton shimmer textures.
- **Debounced Rendering**: If server response latency is typically under 100ms, delaying the initial skeleton display via JavaScript by 150ms prevents an annoying visual flash for high-speed users.

### Accessibility Matrix
- **Parent Live Region**: Mark the data container with `aria-busy="true"`. Assistive technology will wait until `aria-busy` toggles to `false` before announcing the loaded dataset.
- **Empty State Landmarks**: Empty states should be enclosed within a semantic `<section>` or `<main>` landmark with appropriate headings (`<h3>`, `<h4>`) for screen reader navigability.

---

## 9. Comparison: Asynchronous Loading States

| Feature | CSS Shimmer Skeleton | Static Spinner Wheel | Progress Bar | Full-Screen Scrim |
|---|---|---|---|---|
| **CLS Prevention** | Perfect (Volume pre-allocated) | Terrible (0 volume reserved) | Moderate | Moderate |
| **Perceived Performance** | Fastest (Cognitive priming) | Slowest (Focus on waiting) | Moderate | Poor |
| **GPU Compositing** | Yes (via `transform`) | Yes (via `rotate`) | Yes | No |
| **Context Retention** | High (Layout known) | Low (Blank space) | Low | Low |
| **Baseline Status** | Widely Available | Widely Available | Widely Available | Widely Available |

---

## 10. Interview Questions

<details>
<summary><strong>1. Why is animating `transform: translateX()` on an `::after` pseudo-element superior to animating `background-position` for skeleton shimmers?</strong></summary>

**Answer:**
`background-position` is a property that affects the **Paint** stage of the browser rendering pipeline. Every single frame of the animation requires the browser's CPU/GPU to recalculate the rasterized pixels of the element. If an application displays 20 skeleton cards, each with multiple lines, the browser must repaint dozens of elements 60 times per second, leading to high CPU utilization, battery drain, and dropped frames.

In contrast, `transform` is processed exclusively during the **Composite** stage. The browser uploads the gradient texture once to the GPU. During animation, the GPU simply adjusts the coordinate offset of the layer texture without triggering layout recalculations or paint invalidations.
</details>

<details>
<summary><strong>2. How does a skeleton loader eliminate Cumulative Layout Shift (CLS)?</strong></summary>

**Answer:**
Cumulative Layout Shift occurs when visible DOM elements change their geometric coordinates between frames. 

When a skeleton loader is engineered, it reserves the exact layout volume (same grid tracks, aspect ratios, margins, paddings, and font line-heights) as the incoming content. When the API response resolves and the dynamic data is inserted into the DOM, the surrounding layout boundaries do not move by a single pixel, yielding a CLS score of 0.000.
</details>

<details>
<summary><strong>3. What accessibility markup should be applied to skeleton screens?</strong></summary>

**Answer:**
1. Skeletons are purely visual placeholders; individual placeholder boxes must declare `aria-hidden="true"` so screen readers ignore them.
2. The parent container holding the dynamic region should declare `aria-busy="true"` and an `aria-label="Loading [content name]..."` attribute.
3. When the network request completes and dynamic content is mounted, `aria-busy` is switched to `"false"`, signaling to assistive technology that the data is ready for reading.
</details>

<details>
<summary><strong>4. How should skeleton animations adapt to `@media (prefers-reduced-motion: reduce)`?</strong></summary>

**Answer:**
Sweeping, high-frequency linear gradient shimmers can trigger dizziness or migraines in users with vestibular disorders.

Under `@media (prefers-reduced-motion: reduce)`:
- Remove the horizontal shimmer animation: `animation: none;`.
- Hide the moving pseudo-element: `display: none;`.
- Replace the shimmer with a very subtle, low-frequency opacity pulse (e.g., 2-second ease-in-out breathing effect) or display a completely static neutral placeholder.
</details>

<details>
<summary><strong>5. What is the "skeleton flash" problem and how is it mitigated?</strong></summary>

**Answer:**
If an API response returns extremely quickly (e.g., in 30ms from local cache or edge worker), rendering a skeleton immediately causes a rapid, jarring flash of grey wireframes that vanishes instantly.

To mitigate this:
- Delay the visual appearance of the skeleton by 150–200ms using a CSS animation delay or JavaScript timer. If data returns within that initial window, the real content renders immediately without any skeleton ever flashing on screen.
</details>

<details>
<summary><strong>6. Why is a realistic typographic line-length variation important in text skeletons?</strong></summary>

**Answer:**
In natural typography, paragraphs rarely end on a full-width line. If a skeleton renders 4 lines of identical 100% width, it visually reads like a solid rectangular box rather than a paragraph of text.

By setting `.skeleton-line:last-child { width: 60%; }`, the placeholder mimics natural paragraph flow (*ragged right*), improving visual priming for sighted users.
</details>

<details>
<summary><strong>7. How does CSS `contain: paint` optimize complex skeleton screens?</strong></summary>

**Answer:**
`contain: paint` informs the browser layout engine that none of the element's descendants will paint outside its bounding box. For a dashboard rendering 40 skeleton cards, `contain: paint` guarantees that the shimmer animations inside each card are clipped strictly to their local box, preventing repaints from bubbling up to parent containers.
</details>

<details>
<summary><strong>8. How should high contrast mode (`forced-colors: active`) be styled on skeleton loaders?</strong></summary>

**Answer:**
In High Contrast Mode, background colors and subtle gradients are completely suppressed by the operating system. Skeletons styled only with `background-color: #f1f5f9;` will turn completely invisible.

To ensure skeletons remain visible:
- Apply explicit borders using system keywords: `border: 1px solid ButtonBorder;` or `outline: 1px solid GrayText;`.
- This ensures high-contrast users can clearly perceive the wireframe bounds of loading components.
</details>

---

## 11. Practice Exercises

### Exercise 1: Build a Pure CSS GPU Shimmer Skeleton
**Task**: Build a single 200px by 20px skeleton box with a GPU-accelerated horizontal shimmer running over 1.5 seconds.
<details>
<summary>View Verified Solution</summary>

```css
.sk-box {
  width: 200px;
  height: 20px;
  background-color: #e2e8f0;
  border-radius: 4px;
  position: relative;
  overflow: hidden;
}

.sk-box::after {
  content: "";
  position: absolute;
  inset: 0;
  transform: translateX(-100%);
  background: linear-gradient(
    90deg,
    transparent 0%,
    rgba(255, 255, 255, 0.6) 50%,
    transparent 100%
  );
  animation: shimmer 1.5s infinite;
}

@keyframes shimmer {
  to { transform: translateX(100%); }
}
```
</details>

---

### Exercise 2: Responsive Empty State Card
**Task**: Create an empty state card with an icon, title, description, and primary button. The card must center vertically and horizontally within its parent container.
<details>
<summary>View Verified Solution</summary>

```css
.empty-wrapper {
  display: grid;
  place-content: center;
  min-height: 400px;
  padding: 2rem;
}

.empty-card {
  display: flex;
  flex-direction: column;
  align-items: center;
  text-align: center;
  max-width: 440px;
  background: #ffffff;
  border: 1px dashed #cbd5e1;
  border-radius: 12px;
  padding: 2.5rem 1.5rem;
}
```
</details>

---

### Exercise 3: Reduced Motion Safe Skeleton
**Task**: Write the CSS to ensure a skeleton component disables its sliding shimmer and replaces it with a gentle opacity pulse when `prefers-reduced-motion: reduce` is active.
<details>
<summary>View Verified Solution</summary>

```css
@media (prefers-reduced-motion: reduce) {
  .skeleton::after {
    display: none;
  }
  .skeleton {
    animation: gentle-pulse 2s ease-in-out infinite;
  }
}

@keyframes gentle-pulse {
  0%, 100% { opacity: 1; }
  50% { opacity: 0.5; }
}
```
</details>

---

### Exercise 4: Circular Avatar Skeleton with `aspect-ratio`
**Task**: Style an avatar skeleton that maintains a 1:1 circle shape regardless of whether width is defined as a percentage or fixed pixel size.
<details>
<summary>View Verified Solution</summary>

```css
.avatar-sk {
  width: 56px;
  aspect-ratio: 1 / 1;
  border-radius: 50%;
  background-color: #cbd5e1;
  position: relative;
  overflow: hidden;
}
```
</details>

---

### Exercise 5: High Contrast Mode Border Defense
**Task**: Add a forced-colors media query ensuring that skeleton wireframe boxes display a visible outline when Windows High Contrast Mode is active.
<details>
<summary>View Verified Solution</summary>

```css
@media (forced-colors: active) {
  .skeleton {
    border: 1px solid GrayText;
  }
}
```
</details>

---

### Exercise 6: Multi-Line Paragraph Skeleton Stagger
**Task**: Style a 3-line paragraph skeleton where line 1 is 100% width, line 2 is 90% width, and line 3 is 60% width.
<details>
<summary>View Verified Solution</summary>

```html
<div class="para-sk">
  <div class="line line-1"></div>
  <div class="line line-2"></div>
  <div class="line line-3"></div>
</div>
```

```css
.para-sk {
  display: flex;
  flex-direction: column;
  gap: 0.5rem;
  width: 100%;
}

.para-sk .line {
  height: 12px;
  background: #e2e8f0;
  border-radius: 3px;
}

.para-sk .line-1 { width: 100%; }
.para-sk .line-2 { width: 90%; }
.para-sk .line-3 { width: 60%; }
```
</details>

---

## 12. Summary / Cheat Sheet

```
+------------------------------------------------------------------------------------+
|                      SKELETONS & EMPTY STATES CHEAT SHEET                          |
+------------------------------------------------------------------------------------+
| 1. GPU Composited Shimmer:                                                         |
|    .sk::after { position: absolute; inset: 0; transform: translateX(-100%);        |
|                 animation: shimmer 1.5s infinite; }                                |
|    @keyframes shimmer { to { transform: translateX(100%); } }                      |
+------------------------------------------------------------------------------------+
| 2. Accessibility Tagging:                                                          |
|    <div aria-busy="true" aria-label="Loading user feed...">                        |
|      <div class="sk-line" aria-hidden="true"></div>                                |
|    </div>                                                                          |
+------------------------------------------------------------------------------------+
| 3. Reduced Motion Guard:                                                           |
|    @media (prefers-reduced-motion: reduce) {                                       |
|      .sk::after { display: none; } .sk { animation: pulse 2s infinite; }           |
|    }                                                                               |
+------------------------------------------------------------------------------------+
| 4. Zero-CLS Volume Reservation:                                                    |
|    aspect-ratio: 16 / 9; width: 100%; /* Pre-allocates exact dimensions */         |
+------------------------------------------------------------------------------------+
| 5. Actionable Empty State:                                                         |
|    Always provide icon + title + description + primary action button               |
+------------------------------------------------------------------------------------+
| 6. High-Contrast Border Defense:                                                   |
|    @media (forced-colors: active) { .sk { border: 1px solid GrayText; } }          |
+------------------------------------------------------------------------------------+
```

---

## 13. Further Reading
- [W3C WAI-ARIA `aria-busy` Specification](https://www.w3.org/TR/wai-aria-1.2/#aria-busy)
- [web.dev: Optimize Cumulative Layout Shift (CLS)](https://web.dev/articles/cls)
- [MDN Web Docs: `@media (prefers-reduced-motion)`](https://developer.mozilla.org/en-US/docs/Web/CSS/@media/prefers-reduced-motion)
- [Google Material Design: Skeleton Screen Patterns](https://m3.material.io/)
- [CSS-Tricks: Building Skeleton Screens](https://css-tricks.com/building-skeleton-screens-css-javascript/)

---

## Navigation
- **Previous**: [Buttons, Badges, and Alerts](file:///e:/Learning/engineering-notes/01-languages/css/13-layout-and-component-recipes/12-buttons-badges-and-alerts.md)
- **Next**: [CSS-Only Components and Their Limits](file:///e:/Learning/engineering-notes/01-languages/css/13-layout-and-component-recipes/14-css-only-components-limits.md)
