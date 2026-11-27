# Viewport Meta and Modern Viewport Units

| Metadata | Details |
| :--- | :--- |
| **Level** | Core / Modern Standard |
| **Prerequisites** | `01-fundamentals/09-units.md`, `05-responsive-design/01-responsive-principles-and-mobile-first.md` |
| **Estimated Reading Time** | 22 minutes |
| **Status** | Complete |
| **Baseline Status** | Widely Available (`dvh`, `svh`, `lvh` Baseline 2023) |
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

Before modern smartphones, websites were built strictly for desktop monitors. When the iPhone debuted in 2007, mobile Safari needed a way to display existing desktop websites without breaking them. It introduced a **$980\text{px}$ virtual viewport**, rendering desktop websites at full resolution and scaling them down like a photograph.

To tell mobile browsers that your site is intentionally responsive and should render at physical screen resolution, you must declare the **Viewport Meta Tag** in your HTML.

Furthermore, for over a decade, mobile web developers suffered from the infamous **Mobile `100vh` Address Bar Bug**: setting an element to `height: 100vh` caused the bottom of the element (e.g. action buttons or modals) to be cut off and obscured beneath the mobile browser's retractable URL bar!

Modern CSS (Baseline 2023) finally solved this problem by introducing **Small (`svh`), Large (`lvh`), and Dynamic (`dvh`) viewport units**.

---

## 2 Core Concepts

### 1. The Canonical Viewport Meta Tag

Every responsive HTML document must include this tag in the `<head>`:

```html
<meta name="viewport" content="width=device-width, initial-scale=1.0">
```

- **`width=device-width`**: Sets the width of the layout viewport to the physical pixel width of the device in CSS reference pixels (e.g. $390\text{px}$ on iPhone 14, rather than $980\text{px}$).
- **`initial-scale=1.0`**: Establishes a 1:1 relationship between CSS pixels and device-independent screen pixels, preventing automatic zoom-out.

### 2. The Modern Viewport Unit Family

CSS Values and Units Level 4 introduced explicit dimensions corresponding to mobile browser UI states:

| Unit Prefix | Meaning | Mobile Browser URL Bar State | Best Use Case |
| :--- | :--- | :--- | :--- |
| **`svh` / `svw`** | **Small Viewport** | When the browser chrome (URL bar & toolbar) is **expanded** (smallest page view). | Modals, fixed bottom action bars, critical controls. |
| **`lvh` / `lvw`** | **Large Viewport** | When the browser chrome is **retracted/hidden** (largest page view). | Background photography, full-screen video players. |
| **`dvh` / `dvw`** | **Dynamic Viewport** | **Dynamically adjusts** in real time as the URL bar expands and retracts. | App shells, hero banners, full-height panels. |
| **`vh` / `vw`** | **Classic Viewport** | Static, maps to `lvh` on iOS Safari and Chrome mobile. | Simple desktop layouts. |

---

## 3 How It Works Internally

### The Mobile 100vh Address Bar Bug Deconstructed

```
Mobile Safari / Chrome Viewport Mechanics:

State A: User has not scrolled (URL bar is EXPANDED at bottom/top)
+-------------------------------------------------------------+
| Browser Address Bar (UI Chrome)                             |
|=============================================================|
|                                                             |
| Visible Viewport Content Area                               |
| (Height = 100svh)                                           |
|                                                             |
|                                                             |
| [100vh Modal Button Cut Off Here!] <=== HIDDEN BEHIND TOOLBAR!
|=============================================================|
| Browser Bottom Navigation Bar (UI Chrome)                   |
+-------------------------------------------------------------+
If you styled a modal with height: 100vh, the bottom 80px sits
behind the browser's navigation bar!

State B: User scrolls down (URL bar RETRACTS and collapses)
+-------------------------------------------------------------+
| Minimized URL Tab                                           |
|=============================================================|
|                                                             |
| Visible Viewport Content Area                               |
| (Height = 100lvh = 100dvh)                                  |
|                                                             |
|                                                             |
| [Modal Button Now Visible!]                                 |
+-------------------------------------------------------------+
```

### Dynamic Sizing (`dvh`) Mechanics

When an element has `height: 100dvh`:
1. When the page first loads and browser toolbars are open, `100dvh` equals `100svh`.
2. As the user scrolls down and the toolbars collapse into minimal tabs, `100dvh` animates smoothly up to `100lvh`.
3. The content resizes dynamically without any JavaScript resize event listeners!

---

## 4 Syntax & Usage

```css
/* Full-Height Hero Section (Always perfectly fits screen) */
.hero-screen {
  min-height: 100dvh; /* Dynamic: resizes with URL bar */
}

/* Modal Dialog (Guaranteed visible button without scrolling) */
.modal-overlay {
  position: fixed;
  inset: 0;
  height: 100svh; /* Small: never hidden behind URL bar */
}

/* Background Artwork */
.fullscreen-bg {
  height: 100lvh; /* Large: covers full screen when chrome collapses */
}
```

> [!NOTE]
> **Baseline Status: Widely Available (Baseline 2023)**
> `dvh`, `svh`, `lvh`, `dvw`, `svw`, and `lvw` are supported across all modern mobile and desktop browsers: Chrome 108+, Safari 15.4+, Firefox 101+, Edge 108+.

---

## 5 Practical Examples

### Example 1: Accessible Fullscreen Mobile App Shell

```css
.mobile-app-shell {
  display: flex;
  flex-direction: column;
  /* Use 100dvh with a 100vh fallback for older legacy engines */
  min-height: 100vh;
  min-height: 100dvh;
  overflow: hidden;
}

.app-header {
  flex: 0 0 56px;
  background-color: #0f172a;
  color: #ffffff;
}

.app-main {
  flex: 1;
  overflow-y: auto;
  padding: 16px;
}

.app-footer-nav {
  flex: 0 0 64px;
  background-color: #ffffff;
  border-top: 1px solid #e2e8f0;
}
```

### Example 2: Mobile Modal Dialog Protected by `svh`

```css
.bottom-sheet-modal {
  position: fixed;
  bottom: 0;
  left: 0;
  right: 0;
  /* Max height bounded by Small Viewport so close button is never trapped */
  max-height: 90svh;
  overflow-y: auto;
  background-color: #ffffff;
  border-start-start-radius: 16px;
  border-start-end-radius: 16px;
  padding: 24px;
  box-shadow: 0 -10px 25px rgba(0, 0, 0, 0.2);
  z-index: 1000;
}
```

### Example 3: Edge Case — Safe Area Insets (`env(safe-area-inset-*)`)

On mobile devices with hardware display notches, rounded corners, or home-indicator bars (e.g. iPhone Home Bar), viewport units alone may still cause content to collide with hardware boundaries. Pair modern viewport units with CSS environment variables:

```css
.fixed-action-bar {
  position: fixed;
  bottom: 0;
  left: 0;
  right: 0;
  background-color: #0f172a;
  color: #ffffff;
  padding: 16px;
  /* Add safe area inset buffer for iPhone home bar */
  padding-bottom: calc(16px + env(safe-area-inset-bottom, 0px));
}
```

### Example 4: Failure Case — Disabling User Pinch-to-Zoom (Accessibility Violation)

```html
<!-- DANGEROUS ACCESSIBILITY VIOLATION: NEVER USE THIS -->
<meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
```
*Why this fails*: Disabling `user-scalable` or setting `maximum-scale=1.0` prevents low-vision users from pinching to zoom in on small text or images. This **fails WCAG 1.4.4 (Resize Text)**. Never disable user zooming.

### Example 5: Fullscreen Video Player with `lvh`

```css
.video-hero-container {
  width: 100vw;
  height: 100lvh; /* Covers the full canvas when controls collapse */
  position: relative;
  background-color: #000000;
}

.video-element {
  width: 100%;
  height: 100%;
  object-fit: cover;
}
```

---

## 6 Best Practices

1. **Always include the standard viewport meta tag**: `<meta name="viewport" content="width=device-width, initial-scale=1.0">`.
2. **Never disable user scaling**: Omit `user-scalable=no` and `maximum-scale=1.0` to respect WCAG accessibility standards.
3. **Use `100dvh` for full-height hero sections**: Allows the hero section to fill the screen cleanly whether the URL bar is collapsed or expanded.
4. **Use `svh` for fixed modal dialogues and sheets**: Guarantees that submit buttons or close icons are never hidden behind expanded browser chrome.
5. **Provide a fallback for older browsers**: Write `height: 100vh;` immediately before `height: 100dvh;`.
6. **Account for device notches with `env(safe-area-inset-*)`**: Especially on fixed bottom bars and floating action buttons.

---

## 7 Common Mistakes & Pitfalls

| Mistake | Why It Breaks | Solution |
| :--- | :--- | :--- |
| Action button hidden under mobile URL bar | Used legacy `height: 100vh`, which maps to large viewport | Use `height: 100svh` or `height: 100dvh` |
| Disabling user zooming | `user-scalable=no` violates WCAG 1.4.4 | Remove `user-scalable` and `maximum-scale` |
| Omitting `viewport` meta tag | Mobile browsers render at 980px desktop scale | Add `<meta name="viewport" content="width=device-width, initial-scale=1.0">` |
| Using `dvh` for high-frequency animations | Continuous URL bar resizing can trigger reflow jank | Use `svh` or fixed heights during active scroll animations |
| Fixed bottom nav overlapping iPhone home indicator | Missing hardware notch padding | Add `padding-bottom: env(safe-area-inset-bottom)` |
| `100vw` causing horizontal scroll on desktop | `100vw` includes desktop scrollbar width | Use `100%` on container widths instead of `100vw` |

---

## 8 Performance, Accessibility & Browser Support

### Layout Performance Considerations

- Elements styled with `dvh` recalculate their computed height when the mobile URL bar collapses or expands during scrolling.
- Modern engines optimize this on the compositor thread; however, applying `height: 100dvh` to hundreds of deeply nested cards can cause minor layout churn. Use `dvh` primarily for root app shells, hero containers, and modals.

### Accessibility Standards (WCAG 1.4.4 Resize Text)

- **The Zoom Guarantee**: Sighted users with low vision rely on native pinch-to-zoom to read small text.
- Any stylesheet or HTML configuration that disables `user-scalable` or clamps `maximum-scale` below `2.0` automatically fails WCAG Level AA compliance. Keep scaling enabled at all times.

---

## 9 Comparison / When to Use What

| Viewport Unit | Expands / Retracts? | Sizing Basis | Recommended Component |
| :--- | :--- | :--- | :--- |
| **`svh`** | Static (Smallest) | Viewport with **open toolbars** | Modals, bottom sheets, close buttons |
| **`lvh`** | Static (Largest) | Viewport with **collapsed toolbars** | Fullscreen video backgrounds, splash art |
| **`dvh`** | **Dynamic Real-Time** | Real-time viewport bounds | App shell root (`min-height: 100dvh`), hero banners |
| **`vh`** | Engine Dependent | Legacy viewport (often = `lvh`) | Desktop-only fallback |

---

## 10 Interview Questions

<details>
<summary><strong>1. What was the "Mobile 100vh Bug", and why did it happen?</strong></summary>

Mobile browsers (like iOS Safari and Chrome Android) feature dynamic interface chrome: when a page loads, the URL address bar and bottom navigation toolbar are expanded. When the user scrolls down, these toolbars retract. Historically, the browser engines defined `100vh` as the height of the screen when the toolbars are **retracted** (to prevent constant page reflows during scrolling). Consequently, on initial page load, `100vh` was taller than the visible screen, causing elements positioned at the bottom of the viewport (such as submit buttons or modals) to be cut off and hidden beneath the browser's UI chrome.
</details>

<details>
<summary><strong>2. How do `svh`, `lvh`, and `dvh` resolve the mobile viewport issue?</strong></summary>

- `svh` (Small Viewport Height): Measures the viewport when browser toolbars are fully **expanded** (guaranteeing that content is never obscured).
- `lvh` (Large Viewport Height): Measures the viewport when browser toolbars are fully **retracted**.
- `dvh` (Dynamic Viewport Height): Automatically adjusts in real time between `svh` and `lvh` as the user scrolls and the toolbars expand or retract.
</details>

<details>
<summary><strong>3. What are the two essential attributes in the standard viewport meta tag, and what do they do?</strong></summary>

- `width=device-width`: Tells the browser to set the viewport width to the device's screen width in CSS pixels (e.g. 390px), rather than the default 980px desktop fallback canvas.
- `initial-scale=1.0`: Sets the initial zoom level to 1:1, preventing default zoom-out.
</details>

<details>
<summary><strong>4. Why is setting `user-scalable=no` considered a critical anti-pattern?</strong></summary>

Setting `user-scalable=no` disables pinch-to-zoom on mobile devices, making it impossible for low-vision users to enlarge small text or images. This violates WCAG Success Criterion 1.4.4 (Resize Text) and significantly degrades accessibility.
</details>

<details>
<summary><strong>5. What is the difference between `100vw` and `width: 100%` on desktop browsers?</strong></summary>

`100%` sizes the element to 100% of its parent container's content box, automatically subtracting any vertical scrollbar that may exist. In contrast, `100vw` represents 100% of the entire browser window including the physical scrollbar width (typically 15-17px on Windows). Setting `width: 100vw` on desktop causes an unwanted horizontal scrollbar!
</details>

<details>
<summary><strong>6. How do CSS environment variables (`env()`) complement viewport units on modern mobile devices?</strong></summary>

Modern smartphones feature physical hardware intrusions like camera notches and rounded screen corners. Even if an element is sized with `100svh`, its bottom edge might collide with the iPhone home indicator bar. Using `padding-bottom: env(safe-area-inset-bottom);` adds a dynamic padding buffer to keep interactive controls out of the hardware intrusion area.
</details>

<details>
<summary><strong>7. How do you provide a fallback for older browsers that do not support `dvh`?</strong></summary>

By declaring the legacy `vh` unit immediately before the `dvh` declaration:
```css
.hero {
  min-height: 100vh;
  min-height: 100dvh;
}
```
Browsers that do not understand `dvh` will drop it and use `100vh`, while modern browsers will override `vh` with `dvh`.
</details>

<details>
<summary><strong>8. What are `svw` and `lvw`, and when do they differ from `vw`?</strong></summary>

They represent small and large viewport width units. On mobile devices with dynamic sidebars or folding dual screens (such as foldable smartphones), the horizontal viewport width can change dynamically based on whether sidebars or auxiliary screens are open.
</details>

---

## 11 Practice Exercises

### Exercise 1 (Easy): Modern Full-Height Hero Section
- **Task**: Style a hero section that fits the mobile screen dynamically with a legacy fallback.

<details>
<summary>View Solution</summary>

```css
.hero-screen {
  min-height: 100vh;  /* Fallback */
  min-height: 100dvh; /* Modern Dynamic */
  display: flex;
  align-items: center;
  justify-content: center;
  padding: 24px;
}
```
</details>

### Exercise 2 (Easy): Safe Bottom Navigation Bar
- **Task**: Pin a navigation bar to the bottom of the screen with iPhone home bar safe area padding.

<details>
<summary>View Solution</summary>

```css
.bottom-nav {
  position: fixed;
  bottom: 0;
  left: 0;
  right: 0;
  background-color: #0f172a;
  color: #ffffff;
  padding: 12px 16px;
  padding-bottom: calc(12px + env(safe-area-inset-bottom, 0px));
}
```
</details>

### Exercise 3 (Medium): Mobile Modal Dialog Protected by `svh`
- **Task**: Create a modal dialog whose height never exceeds the screen even when mobile toolbars are expanded.

<details>
<summary>View Solution</summary>

```css
.mobile-modal-card {
  width: min(90vw, 480px);
  max-height: 85svh; /* Bounded by small viewport */
  overflow-y: auto;
  background: #ffffff;
  border-radius: 12px;
  padding: 24px;
}
```
</details>

### Exercise 4 (Medium): Fullscreen Splash Screen with Large Viewport
- **Task**: Style a splash screen container using `lvh` to cover the full screen when browser chrome collapses.

<details>
<summary>View Solution</summary>

```css
.splash-container {
  width: 100%;
  height: 100lvh;
  background: linear-gradient(135deg, #1e3a8a, #0f172a);
  display: grid;
  place-items: center;
}
```
</details>

### Exercise 5 (Hard): Resilient App Shell with Dynamic Viewport
- **Task**: Build an app shell with a top header, scrollable body, and bottom bar that adapts to URL bar scrolling without layout jump.

<details>
<summary>View Solution</summary>

```css
.app-container {
  display: flex;
  flex-direction: column;
  height: 100vh;
  height: 100dvh;
  overflow: hidden;
}

.app-header {
  flex: 0 0 60px;
  background: #0f172a;
  color: #ffffff;
}

.app-content {
  flex: 1;
  min-height: 0;
  overflow-y: auto;
  padding: 16px;
}

.app-footer {
  flex: 0 0 60px;
  background: #f8fafc;
  border-top: 1px solid #e2e8f0;
}
```
</details>

### Exercise 6 (Hard): Accessible Viewport Audit
- **Task**: Write the complete `<head>` metadata block satisfying all modern mobile responsiveness and accessibility standards.

<details>
<summary>View Solution</summary>

```html
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <!-- Perfect canonical responsive viewport: allows user zoom -->
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Accessible Responsive Application</title>
</head>
<body>
  <!-- App content -->
</body>
</html>
```
</details>

---

## 12 Summary / Cheat Sheet

```css
/* Viewport Meta (In HTML <head>) */
<meta name="viewport" content="width=device-width, initial-scale=1.0">

/* Modern Viewport Units (Baseline 2023) */
100svh  /* Small: When URL bar is EXPANDED (Use for modals & buttons) */
100lvh  /* Large: When URL bar is COLLAPSED (Use for background art) */
100dvh  /* Dynamic: Real-time resize (Use for hero & app shells) */

/* Safe Area Insets (Notches) */
padding-bottom: calc(16px + env(safe-area-inset-bottom, 0px));
```

---

## 13 Further Reading

- [MDN Web Docs: CSS Viewport units](https://developer.mozilla.org/en-US/docs/Web/CSS/length#viewport-percentage_lengths)
- [web.dev: The large, small, and dynamic viewport units](https://web.dev/blog/viewport-units)
- [W3C CSS Values and Units Module Level 4: Viewport-percentage Lengths](https://www.w3.org/TR/css-values-4/#viewport-relative-lengths)
- [WebKit: Designing Websites for iPhone X (Safe Areas)](https://webkit.org/blog/7929/designing-websites-for-iphone-x/)

---

[Previous: 01-responsive-principles-and-mobile-first.md](file:///e:/Learning/engineering-notes/01-languages/css/05-responsive-design/01-responsive-principles-and-mobile-first.md) | [Section Index](file:///e:/Learning/engineering-notes/01-languages/css/05-responsive-design/README.md) | [Next: 03-media-queries.md](file:///e:/Learning/engineering-notes/01-languages/css/05-responsive-design/03-media-queries.md)
