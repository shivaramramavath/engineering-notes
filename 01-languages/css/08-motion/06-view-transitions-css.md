# CSS View Transitions

| Metadata | Details |
| :--- | :--- |
| **Topic** | View Transitions API, `@view-transition`, Pseudo-Element Tree (`::view-transition-*`), `view-transition-name`, Cross-Document Navigation |
| **Spec Level** | CSS View Transitions Module Level 1 / CSS View Transitions Module Level 2 |
| **Baseline Status** | Baseline 2023 / 2024 (SPA View Transitions Chrome 111+, Safari 18+; Cross-Document `@view-transition` Chrome 126+, Safari 18.2+) |
| **Browsers Tested** | Chrome 120+, Safari 18.0+, Edge 120+ |
| **Tested With** | Single-Page Apps (React/Next/Vanilla), Multi-Page Document Navigation, Chromium Compositor |
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

Historically, animating between two different page views or application states was one of the most notoriously complex tasks in frontend web development:
- In Single-Page Applications (SPAs), developers relied on massive JavaScript animation libraries to orchestrate DOM cloning, positioning math, coordinate translation, and exit/entry states.
- In Multi-Page Applications (MPAs), smooth page transitions were outright impossible: every link navigation caused an unavoidable white flash and complete page reload.

The **CSS View Transitions API** (Baseline 2023 for SPAs; Baseline 2024 for Cross-Document MPAs) introduces a native browser mechanism to seamlessly morph between visual states:
- **Instant Snapshotting**: The browser captures an image snapshot of the "old" state, updates the DOM, captures a snapshot of the "new" state, and smoothly animates between them.
- **Shared Element Morphing**: By assigning a matching `view-transition-name: product-hero;` to an element in both states, the browser automatically interpolates its position, scale, and aspect ratio from a small list thumbnail into a full-bleed hero banner.
- **Zero-JavaScript Multi-Page Transitions**: With the modern `@view-transition { navigation: auto; }` rule, multi-page websites can morph seamlessly across server-rendered HTML pages without single-page app frameworks.

---

## 2. Core Concepts

### Single-Page Transition Trigger (`document.startViewTransition`)
In JavaScript, wrapping any DOM update in `document.startViewTransition()` initiates the transition lifecycle:
```javascript
document.startViewTransition(() => {
  // Update the DOM state (e.g., render next route, toggle dark mode, update list)
  updateDOM();
});
```

### Multi-Page Cross-Document Transitions (`@view-transition`)
In CSS View Transitions Level 2 (Baseline 2024), standard multi-page sites opt into seamless cross-page transitions purely via CSS:
```css
@view-transition {
  navigation: auto; /* Enables native cross-document view transitions */
}
```

### The `view-transition-name` Identifier
By default, the entire page (`:root`) undergoes a cross-fade transition. To animate specific elements independently (such as an image expanding or a navbar remaining pinned):
```css
.product-card-image {
  view-transition-name: active-product-img;
}
```
**Strict Uniqueness Law**: In any given rendered frame, a `view-transition-name` must be **100% unique across the entire document**. If two elements share the same name simultaneously, the view transition is aborted.

### The Generated Pseudo-Element Tree
During a view transition, the browser constructs a temporary pseudo-element hierarchy at the root of the document:
```
::view-transition
 └─ ::view-transition-group(name)
     └─ ::view-transition-image-pair(name)
         ├─ ::view-transition-old(name)  [Snapshot of Old State]
         └─ ::view-transition-new(name)  [Live Capture of New State]
```
These pseudo-elements can be targeted, customized, and animated using standard CSS `@keyframes`.

---

## 3. How It Works Internally

```
The View Transition Lifecycle

1. Trigger: document.startViewTransition(callback)
       |
       v
2. Snapshot Old State: Browser takes GPU bitmap snapshot of matching elements
       |
       v
3. DOM Mutation: JavaScript callback executes (DOM mutates to new state)
       |
       v
4. Snapshot New State: Browser captures geometry and styles of new elements
       |
       v
5. Pseudo-Tree Construction: Generates ::view-transition-old and ::view-transition-new
       |
       v
6. GPU Compositor Animation: Interpolates size, position, and cross-fade
       |
       v
7. Cleanup: Animation finishes; pseudo-tree is discarded from DOM
```

---

## 4. Syntax & Usage

### Baseline & Feature Support
- **Single-Page View Transitions (`document.startViewTransition`)**: Baseline 2023 (Chrome 111+, Safari 18.0+, Edge 111+).
- **Cross-Document MPA Transitions (`@view-transition`)**: Baseline 2024 (Chrome 126+, Safari 18.2+, Edge 126+).
- **CSS Customization (`::view-transition-*`)**: Baseline 2023.

```css
/* Opt-in for Cross-Document Navigation (Multi-Page Apps) */
@view-transition {
  navigation: auto;
}

/* Unique Shared Element Tag */
.hero-header-img {
  view-transition-name: main-hero-image;
}

/* Customizing Transition Animation */
::view-transition-old(main-hero-image) {
  animation: 0.4s cubic-bezier(0.16, 1, 0.3, 1) both fadeOut;
}

::view-transition-new(main-hero-image) {
  animation: 0.4s cubic-bezier(0.16, 1, 0.3, 1) both fadeIn;
}
```

---

## 5. Practical Examples

### Example 1: Shared Thumbnail-to-Hero Expansion
```html
<!-- State 1: Gallery View -->
<div class="gallery">
  <img src="mountain.jpg" alt="" class="thumbnail" style="view-transition-name: hero-photo;">
</div>

<!-- State 2: Detail View (Loaded after click) -->
<div class="detail-page">
  <img src="mountain.jpg" alt="" class="hero-banner" style="view-transition-name: hero-photo;">
</div>
```

```css
/* Base styles */
.thumbnail {
  width: 120px;
  height: 120px;
  border-radius: 8px;
  object-fit: cover;
}

.hero-banner {
  width: 100%;
  height: 400px;
  border-radius: 0;
  object-fit: cover;
}

/* Customize transition timing between the two states */
::view-transition-group(hero-photo) {
  animation-duration: 0.45s;
  animation-timing-function: cubic-bezier(0.16, 1, 0.3, 1);
}
```

### Example 2: Circular Clip-Path Theme Toggle Transition
```html
<button type="button" class="theme-toggle-btn">Toggle Dark Mode</button>
```

```javascript
document.querySelector('.theme-toggle-btn').addEventListener('click', (e) => {
  // Check if browser supports View Transitions
  if (!document.startViewTransition) {
    document.documentElement.classList.toggle('dark-theme');
    return;
  }

  // Calculate click coordinate for expanding circle reveal
  const x = e.clientX;
  const y = e.clientY;
  const endRadius = Math.hypot(
    Math.max(x, innerWidth - x),
    Math.max(y, innerHeight - y)
  );

  const transition = document.startViewTransition(() => {
    document.documentElement.classList.toggle('dark-theme');
  });

  transition.ready.then(() => {
    // Animate circular clip-path reveal originating from click coordinates
    document.documentElement.animate(
      {
        clipPath: [
          `circle(0px at ${x}px ${y}px)`,
          `circle(${endRadius}px at ${x}px ${y}px)`
        ]
      },
      {
        duration: 500,
        easing: 'cubic-bezier(0.16, 1, 0.3, 1)',
        pseudoElement: '::view-transition-new(root)'
      }
    );
  });
});
```

### Example 3: Directional Slide Transitions (Left / Right)
```css
/* Custom slide keyframes */
@keyframes slideFromRight {
  from { translate: 100% 0; }
  to { translate: 0 0; }
}

@keyframes slideToLeft {
  from { translate: 0 0; }
  to { translate: -100% 0; }
}

/* Customizing root transition */
::view-transition-old(root) {
  animation: 0.3s ease both slideToLeft;
}

::view-transition-new(root) {
  animation: 0.3s ease both slideFromRight;
}
```

### Example 4: Pinning Sticky Elements with Isolated Names
```css
/* Keep navbar fixed in place while rest of page transitions underneath */
.persistent-navbar {
  view-transition-name: global-navbar;
}

::view-transition-group(global-navbar) {
  /* Suppress movement animation; keeps header perfectly stationary */
  animation: none;
}
```

### Example 5: Reduced Motion Fallback
```css
@media (prefers-reduced-motion: reduce) {
  ::view-transition-group(*),
  ::view-transition-old(*),
  ::view-transition-new(*) {
    animation-duration: 0.01ms !important;
  }
}
```

---

## 6. Best Practices

1. **Always Check for API Support Before Calling**: `if (!document.startViewTransition) { updateDOM(); return; }`. Never assume all browsers support the API without checking.
2. **Ensure `view-transition-name` is Unique**: Never assign the same `view-transition-name` to multiple visible elements on the same page. If a list contains 20 items, only assign the name to the single clicked card.
3. **Use `@view-transition { navigation: auto; }` for Multi-Page Sites**: Enable seamless page morphing across standard server-rendered HTML pages without needing SPA frameworks.
4. **Isolate Persistent Elements**: Give headers, footers, and persistent audio players unique `view-transition-name` tags so they do not participate in full-page cross-fades.
5. **Honor `prefers-reduced-motion`**: Override `::view-transition-group(*)` animation durations to `0.01ms` for users with motion sensitivity.
6. **Set `contain: layout` or `overflow: clip` on Named Elements**: View transitions capture the border box and overflow of named elements; ensure clipping boundaries are consistent between old and new states.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Correction |
| :--- | :--- | :--- |
| **Duplicate `view-transition-name` on page** | Spec violation: multiple elements sharing a name causes the transition to abort with an error. | Apply the name dynamically only to the active clicked item. |
| **Calling `startViewTransition` without updating DOM** | Snapshot waits for DOM mutation; if callback does nothing, transition hangs. | Execute state change or DOM mutation inside the callback. |
| **Omitting reduced-motion override** | Full-page camera sweeps cause vestibular motion sickness. | Add `@media (prefers-reduced-motion: reduce)` rule. |
| **Un-scoped root transitions overriding named transitions** | Styling `::view-transition-old(root)` incorrectly overwrites custom named elements. | Target specific names: `::view-transition-old(hero)`. |
| **Assuming cross-document transitions work cross-origin** | Cross-document view transitions only operate on **same-origin navigations**. | Ensure both URLs share origin and protocol. |
| **Async DOM update without returning a Promise** | If DOM update is asynchronous, transition starts before new DOM renders. | Return a Promise from the callback: `startViewTransition(async () => ...)`. |

---

## 8. Performance, Accessibility & Browser Support

### Accessibility Compliance (WCAG 2.2 AA)
- **Criterion 2.3.3 (Animation from Interactions)**: Screen morphing and sliding page transitions must be neutralized when the operating system requests reduced motion.

### GPU Snapshotting Memory Overhead
During a view transition, the browser rasterizes snapshots into GPU memory textures. If dozens of elements have unique `view-transition-name` declarations, multiple textures are created simultaneously. Keep named elements limited to primary hero graphics, cards, and navigation bars.

---

## 9. Comparison / When to Use What

| Technique | Scope | JavaScript Overhead | Framework Dependency |
| :--- | :--- | :--- | :--- |
| **CSS View Transitions** | Cross-document (MPA) or SPA | **Zero to Minimal** | **None (Native Web Platform)** |
| **Framer Motion / GSAP** | Single-Page Apps (React/Vue) | High (~30–60 KB bundle) | Requires framework integration |
| **CSS `@keyframes`** | Single element in isolation | Zero | None |

---

## 10. Interview Questions

<details>
<summary>1. How does the View Transitions API work internally across DOM updates?</summary>

1. When `document.startViewTransition(callback)` is invoked, the browser captures a bitmap snapshot of the current DOM state (and all elements with a `view-transition-name`).
2. The browser pauses rendering and executes the provided `callback()`, which mutates the DOM to the new state.
3. The browser captures the geometry, bounds, and bitmap snapshots of the new DOM state.
4. The browser constructs a temporary pseudo-element tree (`::view-transition`), layering `::view-transition-old` and `::view-transition-new`.
5. The GPU compositor smoothly interpolates between the old and new geometries, executing cross-fades and size morphs.
6. Once the animation completes, the pseudo-element tree is discarded, and normal rendering resumes.
</details>

<details>
<summary>2. What is the strict uniqueness rule for <code>view-transition-name</code>, and what happens if it is violated?</summary>

At any given point during the transition lifecycle, every `view-transition-name` present in the document must be **strictly unique**. 
If two or more visible elements share the exact same name (e.g., giving 10 blog post cards `view-transition-name: card;`), the browser engine cannot determine which element corresponds to which snapshot. As a result, the browser automatically **aborts the view transition**, immediately snapping to the new DOM state without animation and logging a console error.
</details>

<details>
<summary>3. What is the purpose of <code>@view-transition { navigation: auto; }</code> in modern CSS?</summary>

It enables **Cross-Document View Transitions** for traditional Multi-Page Applications (MPAs). 
Without requiring a Single-Page Application (SPA) architecture, service workers, or JavaScript router libraries, clicking a standard HTML `<a href="/about">` link will cause the browser to smoothly morph between the old HTML document and the newly fetched HTML document on same-origin navigations.
</details>

<details>
<summary>4. What are the key pseudo-elements generated in the View Transition tree?</summary>

- `::view-transition`: The root container covering the entire viewport.
- `::view-transition-group(name)`: Positioned and sized to encompass the element; animates position and dimensions.
- `::view-transition-image-pair(name)`: Isolates the old and new visual snapshots.
- `::view-transition-old(name)`: The static bitmap snapshot of the outgoing element state.
- `::view-transition-new(name)`: The live capture of the incoming element state.
</details>

<details>
<summary>5. How can you prevent a persistent header from cross-fading during a view transition?</summary>

Assign the header an isolated `view-transition-name`:
```css
.site-header {
  view-transition-name: site-header;
}
```
Because the header exists in both states with the exact same position and name, the browser will not fade it out with the root document; it will remain pinned continuously on screen.
</details>

<details>
<summary>6. How do you handle asynchronous DOM updates (e.g., fetching data before rendering) with <code>startViewTransition</code>?</summary>

Pass an `async` function (or a function returning a Promise) into `document.startViewTransition()`:
```javascript
document.startViewTransition(async () => {
  const data = await fetchPageData();
  renderContent(data);
});
```
The browser will hold the "old" snapshot frozen on screen until the Promise resolves, preventing empty or loading flashes.
</details>

<details>
<summary>7. How do you customize the duration or easing curve of a view transition?</summary>

Target the `::view-transition-group` pseudo-element:
```css
::view-transition-group(hero-image) {
  animation-duration: 0.5s;
  animation-timing-function: cubic-bezier(0.16, 1, 0.3, 1);
}
```
</details>

<details>
<summary>8. How do you support users who have <code>prefers-reduced-motion: reduce</code> enabled?</summary>

Target all view transition pseudo-elements and set their animation duration to 0.01ms:
```css
@media (prefers-reduced-motion: reduce) {
  ::view-transition-group(*),
  ::view-transition-old(*),
  ::view-transition-new(*) {
    animation-duration: 0.01ms !important;
  }
}
```
</details>

---

## 11. Practice Exercises

### Exercise 1: Single-Line Multi-Page View Transition Opt-In
**Goal**: Enable native cross-document view transitions across all pages in a multi-page website.

<details>
<summary>View Solution</summary>

```css
@view-transition {
  navigation: auto;
}
```
</details>

### Exercise 2: Shared Hero Image Morphing
**Goal**: Configure CSS so an article card thumbnail smoothly morphs into the article header hero image.

<details>
<summary>View Solution</summary>

```css
.card-thumbnail,
.article-hero-image {
  view-transition-name: article-hero;
}

::view-transition-group(article-hero) {
  animation-duration: 0.4s;
  animation-timing-function: cubic-bezier(0.16, 1, 0.3, 1);
}
```
</details>

### Exercise 3: Isolate Persistent Footer
**Goal**: Keep a floating bottom navigation bar from flickering during view transitions.

<details>
<summary>View Solution</summary>

```css
.bottom-nav-bar {
  view-transition-name: persistent-bottom-nav;
}

::view-transition-group(persistent-bottom-nav) {
  animation: none; /* Disables movement/fade completely */
}
```
</details>

### Exercise 4: Custom Page Slide-In Transition
**Goal**: Override the default root cross-fade with a horizontal slide animation.

<details>
<summary>View Solution</summary>

```css
@keyframes slideOutToLeft {
  from { translate: 0 0; }
  to { translate: -100% 0; }
}

@keyframes slideInFromRight {
  from { translate: 100% 0; }
  to { translate: 0 0; }
}

::view-transition-old(root) {
  animation: 0.35s cubic-bezier(0.16, 1, 0.3, 1) both slideOutToLeft;
}

::view-transition-new(root) {
  animation: 0.35s cubic-bezier(0.16, 1, 0.3, 1) both slideInFromRight;
}
```
</details>

### Exercise 5: Graceful Feature Detection in JavaScript
**Goal**: Write a helper function that safely wraps DOM updates in a view transition if supported.

<details>
<summary>View Solution</summary>

```javascript
function transitionView(updateCallback) {
  if (document.startViewTransition) {
    return document.startViewTransition(updateCallback);
  }
  // Graceful fallback for older browsers
  updateCallback();
  return { finished: Promise.resolve() };
}
```
</details>

### Exercise 6: Complete Vestibular Safety Guard
**Goal**: Ensure all view transitions respect user operating system motion preferences.

<details>
<summary>View Solution</summary>

```css
@media (prefers-reduced-motion: reduce) {
  ::view-transition-group(*),
  ::view-transition-old(*),
  ::view-transition-new(*) {
    animation-duration: 0.01ms !important;
  }
}
```
</details>

---

## 12. Summary / Cheat Sheet

| Feature | Example Syntax | Role |
| :--- | :--- | :--- |
| `document.startViewTransition()` | `document.startViewTransition(() => update())` | Triggers SPA view transition lifecycle |
| `@view-transition` | `@view-transition { navigation: auto; }` | Enables native multi-page (MPA) transitions |
| `view-transition-name` | `view-transition-name: hero;` | Uniquely tags element for shared morphing |
| `::view-transition-old` | `::view-transition-old(root)` | Outgoing snapshot image |
| `::view-transition-new` | `::view-transition-new(root)` | Incoming snapshot image |
| `::view-transition-group` | `::view-transition-group(hero)` | Controls transition duration and timing curves |

---

## 13. Further Reading
- [W3C CSS View Transitions Module Level 1](https://www.w3.org/TR/css-view-transitions-1/)
- [W3C CSS View Transitions Module Level 2](https://www.w3.org/TR/css-view-transitions-2/)
- [MDN Web Docs: View Transitions API](https://developer.mozilla.org/en-US/docs/Web/API/View_Transitions_API)
- [web.dev: Smooth and simple transitions with the View Transition API](https://developer.chrome.com/docs/web-platform/view-transitions/)
- [Bramus Van Damme: Cross-Document View Transitions Guide](https://developer.chrome.com/docs/web-platform/view-transitions/same-document/)

---

[Previous: 05-scroll-driven-animations.md](05-scroll-driven-animations.md) | [Module README](README.md) | [Next: 07-starting-style-and-discrete-animation.md](07-starting-style-and-discrete-animation.md)
