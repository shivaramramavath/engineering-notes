# Cursors & Pointer Events

| Metadata | Details |
| :--- | :--- |
| **Topic** | Cursor Types, Custom SVG Cursors & Hotspots, Hit-Testing Pass-Throughs (`pointer-events: none`), Mobile Touch Action (`touch-action`) |
| **Spec Level** | CSS Basic User Interface Module Level 4 / Pointer Events Level 3 |
| **Baseline Status** | Widely Available (`cursor`, `pointer-events` Baseline 2015; `touch-action` Baseline 2017) |
| **Browsers Tested** | Chrome 120+, Firefox 122+, Safari 17.2+, Edge 120+ |
| **Tested With** | Windows Pointer Engine, macOS Cocoa, iOS WebKit Touch Gestures |
| **Module** | [07-visual-styling](README.md) |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. [Practical Examples](#5-practical-examples)
6. [Best Practices](#6-best-practices)
7. [Common Mistakes & Pitfalls](#7-Common-mistakes--pitfalls)
8. [Performance, Accessibility & Browser Support](#8-performance-accessibility--browser-support)
9. [Comparison / When to Use What](#9-comparison--when-to-use-what)
10. [Interview Questions](#10-interview-questions)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary / Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

In graphical user interfaces, the mouse pointer is the primary visual bridge between the user's hand and digital interface elements. The shape of the cursor instantly communicates interaction affordances:
- A pointing hand indicates navigation.
- An I-beam indicates selectable text.
- An open hand (`grab`) signals a draggable surface.
- A slashed circle (`not-allowed`) conveys a disabled action.

Equally critical is controlling the browser's **Hit-Testing Engine** via `pointer-events`:
- **Click-Through Overlays**: Floating decorative graphics, SVG gradient overlays, and custom dropdown chevron arrows frequently cover interactive buttons. Setting `pointer-events: none` makes the overlay completely invisible to the hit-testing engine, allowing mouse clicks, touch taps, and hover states to pass directly to the underlying controls.
- **Mobile Touch Latency**: `touch-action: manipulation` eliminates the legacy 300ms double-tap-to-zoom delay on mobile touchscreen browsers.

---

## 2. Core Concepts

### Standard System Cursors
- **General**: `default` (arrow), `none` (invisible cursor), `auto`.
- **Links & Actions**: `pointer` (pointing hand).
- **Text Selection**: `text` (I-beam), `vertical-text`.
- **Draggables**: `grab` (open hand), `grabbing` (closed fist).
- **Movement & Resizing**: `move`, `crosshair`, `col-resize`, `row-resize`, `n-resize`, `e-resize`, `nwse-resize`.
- **Status / Disabled**: `not-allowed`, `wait` (hourglass/spinner), `progress`.

### Custom Cursors with Hotspot Coordinates
Modern CSS allows custom SVG or PNG images to serve as cursors.
```css
/* Syntax: url(<image-path>) <hotspot-x> <hotspot-y>, <fallback-keyword> */
.custom-brush {
  cursor: url("/assets/brush.svg") 4 20, crosshair;
}
```
- **Hotspot Coordinates**: The exact pixel offset within the image that acts as the active click point (e.g., the precise tip of a pencil icon).
- **Mandatory Fallback**: A generic keyword fallback (like `auto` or `crosshair`) is **strictly mandatory** per the W3C specification; omitting the fallback keyword invalidates the declaration.

### The Power of `pointer-events: none`
- When `pointer-events: none;` is declared on an element:
  1. The element is ignored during mouse hover, click, drag, and touch hit-testing.
  2. Events pass through to whatever element lies visually beneath it in the $Z$-order.
  3. Child elements can selectively re-enable hit-testing by declaring `pointer-events: auto;`.

### Mobile Touch Ergonomics: `touch-action`
`touch-action` dictates how touchscreen gestures (pinch, scroll, double-tap) are handled:
- `touch-action: auto`: Standard browser scrolling, pinching, and gestures.
- `touch-action: none`: Disables all browser touch gestures (required for HTML5 canvas games, signature drawing pads, or map panning).
- `touch-action: pan-y`: Allows vertical scrolling, but disables horizontal panning and swipe gestures.
- `touch-action: manipulation`: Permits standard scrolling and continuous pinch-zoom, but disables double-tap zoom gestures (eliminating tap delay on mobile).

---

## 3. How It Works Internally

```
Hit-Testing Dispatch Pipeline with pointer-events: none

User clicks at coordinate (X: 150, Y: 220):
       |
       v
[Hit-Testing Engine Traverses Render Tree (Top to Bottom)]
       |
       v
Layer 1 (Top):    .svg-watermark-overlay (pointer-events: none)
                  ===> SKIPPED! Ignored by hit-testing!
       |
       v
Layer 2 (Middle): button.checkout-action (pointer-events: auto)
                  ===> MATCH! Click event dispatched to button!
```

### Hotspot Coordinate Geometry
```
Custom Cursor SVG (32x32px Pencil):
+-------------------------------+ (0, 0)
|       \                       |
|        \                      |
|         \                     |
|          \                    |
|           * (Tip at 28, 28)   | <=== Hotspot declared: url(...) 28 28
+-------------------------------+ (32, 32)
```

---

## 4. Syntax & Usage

### Baseline & Feature Support
- **`cursor` keywords**: Baseline Widely Available.
- **Custom Cursor Hotspot syntax**: Baseline Widely Available.
- **`pointer-events: none`**: Baseline Widely Available (Chrome 1+, Safari 4+, Firefox 3.6+, Edge 12+).
- **`touch-action`**: Baseline Widely Available (Chrome 36+, Safari 13+, Firefox 52+).

```css
/* Click-Through Decorative Watermark */
.decorative-watermark {
  position: absolute;
  inset: 0;
  pointer-events: none; /* Never intercepts clicks */
  background: radial-gradient(circle at center, transparent, rgba(0, 0, 0, 0.4));
}

/* Draggable Kanban Card */
.kanban-task {
  cursor: grab;
}

.kanban-task:active {
  cursor: grabbing;
}
```

---

## 5. Practical Examples

### Example 1: Click-Through Custom Chevron on `<select>`
```html
<div class="select-wrapper">
  <select class="form-select">
    <option>Production Cluster A</option>
    <option>Staging Cluster B</option>
  </select>
  <svg class="chevron-icon" viewBox="0 0 24 24">
    <polyline points="6 9 12 15 18 9"></polyline>
  </svg>
</div>
```

```css
.select-wrapper {
  position: relative;
  display: inline-block;
  width: 240px;
}

.form-select {
  width: 100%;
  padding: 0.625rem 2.5rem 0.625rem 1rem;
  appearance: none;
  border: 1px solid #cbd5e1;
  border-radius: 6px;
  background-color: #ffffff;
  cursor: pointer;
}

/* Critical: Clicks on the chevron icon pass through to the <select> */
.chevron-icon {
  position: absolute;
  right: 0.75rem;
  top: 50%;
  translate: 0 -50%;
  width: 16px;
  height: 16px;
  fill: none;
  stroke: #64748b;
  stroke-width: 2;
  pointer-events: none; /* Clicks pass into select */
}

.form-select:focus-visible {
  outline: 2px solid #2563eb;
  outline-offset: 2px;
}
```

### Example 2: Interactive Signature Pad (Touch Action)
```html
<canvas id="signature-pad" class="drawing-canvas"></canvas>
```

```css
.drawing-canvas {
  width: 100%;
  height: 200px;
  border: 2px dashed #cbd5e1;
  border-radius: 8px;
  background-color: #ffffff;
  cursor: crosshair;

  /* Critical: Prevents mobile browser from intercepting finger touches as page scrolling! */
  touch-action: none;
}
```

### Example 3: Drag-and-Drop Kanban Task Card
```css
.task-item {
  padding: 1rem;
  background-color: #ffffff;
  border: 1px solid #e2e8f0;
  border-radius: 8px;
  box-shadow: 0 1px 3px rgba(0, 0, 0, 0.05);
  cursor: grab;
  user-select: none;
  transition: box-shadow 0.2s ease, transform 0.2s ease;
}

.task-item:active {
  cursor: grabbing;
  transform: scale(1.02);
  box-shadow: 0 10px 15px -3px rgba(0, 0, 0, 0.15);
}
```

### Example 4: Custom SVG Crosshair Cursor with Hotspot
```css
/* Custom sniper-precision crosshair cursor with hotspot exactly at (16, 16) */
.precision-map-viewer {
  cursor: url('data:image/svg+xml;utf8,<svg xmlns="http://www.w3.org/2000/svg" width="32" height="32" viewBox="0 0 32 32"><circle cx="16" cy="16" r="10" stroke="%232563eb" stroke-width="2" fill="none"/><line x1="16" y1="2" x2="16" y2="30" stroke="%232563eb" stroke-width="2"/><line x1="2" y1="16" x2="30" y2="16" stroke="%232563eb" stroke-width="2"/></svg>') 16 16, crosshair;
}
```

### Example 5: Non-Interactive Disabled Button State
```css
.btn-disabled {
  opacity: 0.6;
  cursor: not-allowed;
  /* WARNING: Setting pointer-events: none on buttons removes tooltips and hover titles!
     Prefer using native HTML disabled attribute where possible. */
}
```

---

## 6. Best Practices

1. **Always Set `pointer-events: none` on Decorative Overlays**: Any background gradient overlay, decorative watermark, or floating SVG icon sitting atop clickable elements must have `pointer-events: none`.
2. **Always Provide a Keyword Fallback for Custom Cursors**: `cursor: url("pen.png") 0 0, auto;`. If the image fails to download or the file is corrupted, the browser falls back safely.
3. **Use Hotspot Coordinates Accurately**: Never omit coordinates on directional tools (arrows, pencils, crosshairs); without coordinates, the click point defaults to the top-left $(0, 0)$ corner of the image.
4. **Use `touch-action: manipulation` for Instant Mobile Taps**: Prevents the 300ms mobile double-tap delay on interactive web apps.
5. **Use `touch-action: pan-y` on Horizontal Carousels**: Allows users to scroll the page vertically with natural momentum while capturing horizontal swipes inside the carousel.
6. **Avoid `cursor: pointer` on Non-Clickable Elements**: Using `cursor: pointer` on plain static headings or text confuses users into thinking content is hyperlinked.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Correction |
| :--- | :--- | :--- |
| **Omitting keyword fallback on custom cursor** | Invalid CSS syntax; browser discards the entire `cursor` declaration. | Always append a keyword: `, auto;` or `, crosshair;`. |
| **Setting `pointer-events: none` on disabled buttons** | Prevents hover tooltips (like "Please complete step 1 first") from ever triggering. | Keep pointer events active and style with `cursor: not-allowed`. |
| **Omitting `touch-action: none` on drawing canvas** | Drawing on touchscreens scrolls the browser page instead of drawing strokes. | Add `touch-action: none;` to the canvas. |
| **Oversized custom cursor images ($> 128\text{px}$)** | Browsers enforce security limits (typically $32\times32$ or $128\times128$ max) to prevent cursor spoofing. | Keep custom cursor dimensions under $32\times32$ or $64\times64$. |
| **Wrong hotspot coordinate on custom pointer** | User clicks with the tip of a custom pencil, but the click registers 20px away. | Specify exact coordinates: `url(pencil.svg) 2 28, pointer;`. |
| **Using `cursor: pointer` everywhere** | Platform conventions dictate pointer hand is for hyperlinks; using it everywhere dilutes affordance. | Use platform-appropriate cursors (`default`, `pointer`). |

---

## 8. Performance, Accessibility & Browser Support

### Accessibility Compliance (WCAG 2.2 AA)
- **Disabled Controls**: If a button is styled with `pointer-events: none`, screen readers may still read the button, but keyboard users cannot focus it. It is always superior to use the native HTML `<button disabled>` attribute.
- **Cursor Spoofing Security**: For security reasons (preventing malicious websites from disguising cursor icons to trick users into clicking phishing buttons), modern browsers enforce a strict maximum cursor size of **128x128 pixels** (often clamped to 32x32) and automatically restrict custom cursors when hovering outside the browser viewport.

---

## 9. Comparison / When to Use What

| Property | Values | Affects Mouse Events | Affects Touch Gestures | Best Used For |
| :--- | :--- | :--- | :--- | :--- |
| **`pointer-events`** | `none \| auto` | **Yes (Complete pass-through)**| **Yes** | Decorative overlays, custom select chevrons |
| **`cursor`** | `pointer \| grab \| url(...)` | Visual icon only | No | Conveying interaction affordances |
| **`touch-action`** | `none \| pan-y \| manipulation`| No | **Yes (Browser gesture overrides)** | Canvas games, signature pads, swipe carousels |

---

## 10. Interview Questions

<details>
<summary>1. What does <code>pointer-events: none</code> do, and what happens to child elements inside an ancestor with this property?</summary>

`pointer-events: none` instructs the browser's hit-testing engine to completely ignore the element during event dispatch. Mouse clicks, hovering, dragging, and touch taps pass straight through the element to whatever DOM element lies beneath it.

Crucially, **pointer-events are inheritable**. Child elements inherit `pointer-events: none` by default. However, a child element can explicitly opt back into hit-testing by declaring `pointer-events: auto;`. The child will capture clicks and hovers while its parent remains completely click-through.
</details>

<details>
<summary>2. What is the mandatory syntax for defining a custom image cursor in CSS?</summary>

The syntax is:
```css
cursor: url(<image-url>) <x-hotspot>? <y-hotspot>?, <fallback-keyword>;
```
The **fallback generic keyword** (such as `auto`, `default`, or `pointer`) is **strictly mandatory**. If the fallback keyword is omitted, the CSS parser invalidates the entire declaration and ignores the custom image.
</details>

<details>
<summary>3. What is a cursor "hotspot" and why is it critical?</summary>

The hotspot represents the single pixel coordinate within the cursor image that corresponds to the active target position of the click event. 
- For an arrow cursor, the hotspot is at the tip: `0 0`.
- For a crosshair, the hotspot is at the exact center: `16 16` (in a 32x32 image).
If hotspot coordinates are omitted, the browser defaults to `0 0` (top-left). If the image is a target reticle or pencil, clicks will miss the intended visual target unless coordinates are specified.
</details>

<details>
<summary>4. Why was <code>touch-action: manipulation</code> widely implemented in responsive web design?</summary>

Historically, mobile browsers (like iOS Safari and Android Chrome) delayed all `click` events by approximately 300 milliseconds after the user lifted their finger. The browser waited to see if the user was going to double-tap to zoom into the page. 

`touch-action: manipulation` tells the browser that this element only supports panning and pinch-zooming, and that double-tap zooming is permanently disabled. As a result, the browser eliminates the 300ms delay, making taps feel instantaneous.
</details>

<details>
<summary>5. What are the security restrictions browsers enforce on custom CSS cursors?</summary>

To prevent clickjacking and UI spoofing attacks:
1. **Size Limits**: Browsers cap cursor images to a maximum size (typically 128x128 pixels; on older browsers, 32x32 pixels).
2. **Origin & Sandbox**: Custom cursors are automatically suppressed when the pointer exits the browser viewport or hovers over browser chrome (address bar, tabs).
3. **Animated Cursors**: Animated GIF/APNG cursors are heavily throttled or disallowed to prevent CPU exhaustion.
</details>

<details>
<summary>6. Why is setting <code>pointer-events: none</code> on disabled form buttons considered an anti-pattern?</summary>

While `pointer-events: none` prevents the button from firing `click` events:
1. It prevents tooltip hover triggers (e.g., explaining *why* the button is disabled).
2. The button can still be submitted via keyboard `Enter` in some form contexts unless `disabled` is set in HTML.
3. It does not inform screen readers that the button is disabled (it lacks the ARIA `disabled` state).
Always use the native HTML `<button disabled>` attribute.
</details>

<details>
<summary>7. What is the difference between <code>cursor: grab</code> and <code>cursor: grabbing</code>?</summary>

- `grab`: Displays an open hand icon, indicating to the user that the element can be picked up, dragged, or panned (used on Kanban cards, map tiles, and sliders).
- `grabbing`: Displays a clenched fist icon, activated during the `:active` mousedown/drag state to signify that the item is currently being held.
</details>

<details>
<summary>8. How does <code>touch-action: pan-x</code> enable better mobile swipe gestures?</summary>

`touch-action: pan-x` informs the browser that horizontal touch gestures should be handled natively by the browser, but vertical gestures are disabled. In contrast, `touch-action: pan-y` allows native vertical page scrolling while delegating horizontal swipe gestures to JavaScript touch handlers without triggering browser navigation swipes.
</details>

---

## 11. Practice Exercises

### Exercise 1: Click-Through Floating Watermark
**Goal**: Overlay a semi-transparent logo over a grid of interactive buttons that allows all clicks to pass through.

<details>
<summary>View Solution</summary>

```css
.watermark-overlay {
  position: absolute;
  inset: 0;
  background-image: url("/assets/logo-watermark.svg");
  background-repeat: no-repeat;
  background-position: center;
  opacity: 0.15;
  pointer-events: none; /* Required for click-through */
  z-index: 10;
}
```
</details>

### Exercise 2: Tactile Kanban Drag Surface
**Goal**: Style a draggable card with `grab` and `grabbing` cursor states.

<details>
<summary>View Solution</summary>

```css
.draggable-card {
  padding: 1rem;
  background-color: #ffffff;
  border-radius: 6px;
  border: 1px solid #cbd5e1;
  cursor: grab;
  user-select: none;
}

.draggable-card:active {
  cursor: grabbing;
  box-shadow: 0 8px 16px rgba(0, 0, 0, 0.1);
}
```
</details>

### Exercise 3: Custom SVG Pencil Cursor
**Goal**: Define a custom 32x32 SVG pencil cursor with an exact bottom-left hotspot at $(2, 30)$.

<details>
<summary>View Solution</summary>

```css
.sketch-canvas {
  cursor: url("/assets/pencil.svg") 2 30, crosshair;
  touch-action: none;
}
```
</details>

### Exercise 4: Child Override of Parent `pointer-events: none`
**Goal**: Create an overlay wrapper with `pointer-events: none` where a specific action button inside it remains clickable.

<details>
<summary>View Solution</summary>

```html
<div class="hud-overlay">
  <button type="button" class="clickable-hud-btn">Re-center Map</button>
</div>
```

```css
.hud-overlay {
  position: absolute;
  inset: 0;
  pointer-events: none; /* Passes clicks to map beneath */
}

.clickable-hud-btn {
  pointer-events: auto; /* Re-enables clicks for this specific button */
  position: absolute;
  bottom: 2rem;
  right: 2rem;
  padding: 0.75rem 1.5rem;
  background-color: #2563eb;
  color: #ffffff;
  border: none;
  border-radius: 6px;
  cursor: pointer;
}
```
</details>

### Exercise 5: Elimination of Mobile Tap Delay
**Goal**: Apply touch-action manipulation to all interactive buttons on a mobile checkout flow.

<details>
<summary>View Solution</summary>

```css
button,
a.action-link,
input[type="submit"] {
  touch-action: manipulation;
}
```
</details>

### Exercise 6: Table Resizing Column Cursor
**Goal**: Style a table column splitter element with horizontal resize cursors.

<details>
<summary>View Solution</summary>

```css
.col-splitter {
  width: 4px;
  background-color: transparent;
  cursor: col-resize;
  transition: background-color 0.2s ease;
}

.col-splitter:hover,
.col-splitter:active {
  background-color: #2563eb;
}
```
</details>

---

## 12. Summary / Cheat Sheet

| Property | Example Syntax | Role |
| :--- | :--- | :--- |
| `cursor` (System) | `cursor: pointer;` | Communicates visual interaction affordance |
| `cursor` (Custom) | `cursor: url(pin.svg) 16 16, auto;` | Custom cursor image with active hotspot point |
| `pointer-events: none` | `pointer-events: none;` | Ignores hit-testing; clicks pass to underlying element |
| `pointer-events: auto` | `pointer-events: auto;` | Re-enables hit-testing on children inside non-pointer trees |
| `touch-action: manipulation` | `touch-action: manipulation;` | Eliminates 300ms mobile double-tap zoom delay |
| `touch-action: none` | `touch-action: none;` | Disables browser touch scrolling (signature pads, canvas) |

---

## 13. Further Reading
- [W3C CSS Basic User Interface Module Level 4 - Cursor](https://www.w3.org/TR/css-ui-4/#cursor)
- [W3C Pointer Events Level 3](https://www.w3.org/TR/pointerevents3/)
- [MDN Web Docs: pointer-events](https://developer.mozilla.org/en-US/docs/Web/CSS/pointer-events)
- [MDN Web Docs: touch-action](https://developer.mozilla.org/en-US/docs/Web/CSS/touch-action)
- [web.dev: 300ms tap delay eliminated](https://developer.chrome.com/blog/300ms-tap-delay-gone-away/)

---

[Previous: 10-scrollbars-and-scroll-behaviour.md](10-scrollbars-and-scroll-behaviour.md) | [Module README](README.md) | [Next: 12-images-svg-and-css.md](12-images-svg-and-css.md)
