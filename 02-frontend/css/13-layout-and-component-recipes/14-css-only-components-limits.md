# CSS-Only Components and Their Limits

| Topic | Module | Level | Baseline Status | Browsers Tested | Date | Prerequisites |
|---|---|---|---|---|---|---|
| CSS-Only Components & Limits | 13: Layout & Component Recipes | Advanced / Architectural | Widely Available | Chrome 120+, Firefox 120+, Safari 17.4+ | 2026-03-31 | `:has()`, Details/Summary, Popover API, Accessibility |

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
9. [Comparison: CSS-Only vs HTML-First vs JS-Augmented](#9-comparison-css-only-vs-html-first-vs-js-augmented)
10. [Interview Questions](#10-interview-questions)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary / Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

The web development community has long celebrated "Pure CSS" components—modals, tabs, carousels, accordions, and dropdowns built without a single byte of JavaScript. Demonstrating that CSS can store state via the "checkbox hack" or `:target` pseudo-class became a hallmark of technical CSS wizardry.

However, in commercial, enterprise, and production environments, the pursuit of "CSS-Only" components frequently crosses the line from engineering ingenuity into severe accessibility violations, maintainability nightmares, and degraded user experiences:
- **The "Checkbox Hack" Accessibility Disaster**: Storing toggle state inside hidden `<input type="checkbox">` elements causes screen readers to announce "Checkbox, unchecked" when the user encounters a navigation drawer or modal trigger, completely obscuring the true UI pattern.
- **Lost Keyboard Choreography**: Pure CSS tabs and menus cannot listen to keyboard arrow keys (<kbd>→</kbd>, <kbd>←</kbd>, <kbd>↑</kbd>, <kbd>↓</kbd>) or wrap focus in compliance with W3C WAI-ARIA authoring practices.
- **Missing Focus Traps and Inertness**: Pure CSS modal hacks cannot prevent keyboard users from tabbing outside the modal into background page links.

With the advent of modern Web Platform standards (native `<dialog>`, the HTML Popover API, CSS Anchor Positioning, and CSS `:has()`), the boundary between CSS and JavaScript has matured. We must clearly delineate:
- **Where CSS-only shines**: Declarative presentation, responsive adaptations, smooth animations, native disclosure (`<details>`), and light-dismiss popovers.
- **Where JavaScript is mandatory**: Complex keyboard focus management, live state synchronization, data mutations, and assistive technology choreography.

---

## 2. Core Concepts

### 1. The Separation of Responsibilities
Modern production web architecture follows a tripartite division of labor:
1. **HTML**: Defines semantic structure, document hierarchy, and native platform controls (`<button>`, `<dialog>`, `<details>`, `popover`).
2. **CSS**: Dictates visual presentation, spatial layouts, responsive container behaviors, and hardware-accelerated animations.
3. **JavaScript**: Manages dynamic application state, asynchronous data mutations, complex accessibility choreography (arrow-key focus management), and persistence.

```
+-------------------------------------------------------------------+
|                        THE PRODUCTION TRIAD                       |
|                                                                   |
|   HTML: Native Controls (<dialog>, <details>, popover, <button>)   |
|     │                                                             |
|     ▼                                                             |
|   CSS: Presentation, Layout, Discrete Transitions, Top Layer       |
|     │                                                             |
|     ▼                                                             |
|   JavaScript: ARIA State Sync, Keyboard Arrow Traversal, Mutations|
+-------------------------------------------------------------------+
```

### 2. Modern Native CSS-Capable Patterns (No JS Needed)
With modern platform standards, several patterns that previously required JavaScript are now 100% native:
- **Exclusive Accordions**: `<details name="faq">` handles mutex expansion natively.
- **Light-Dismiss Popovers**: `<button popovertarget="menu">` handles top-layer placement, outside-click closing, and <kbd>Esc</kbd> dismissal without JavaScript.
- **Scroll Snap Carousels**: `scroll-snap-type: x mandatory` provides native momentum swiping.
- **Smooth Height Animations**: CSS Grid `0fr` to `1fr` transitions animate dynamic content heights.
- **Discrete Entry/Exit Animations**: `@starting-style` animates elements from `display: none`.

### 3. The Non-Negotiable JavaScript Boundary
CSS cannot—and should not—attempt to handle:
- **Keyboard Arrow-Key Traversal**: Cycling through tabs in an ARIA `tablist` requires JavaScript `keydown` handlers for <kbd>ArrowLeft</kbd> and <kbd>ArrowRight</kbd>.
- **Focus Trapping**: Confining tab navigation strictly inside non-native overlays requires JavaScript focus loops (or using native `<dialog showModal()>`).
- **Dynamic Data Mutation**: Fetching data, optimistic updates, and synchronizing state with LocalStorage or backend APIs.

---

## 3. How It Works Internally

When a developer uses a CSS-only hack versus a standards-compliant native element, the browser engine executes fundamentally different accessibility and layout paths:

```
[THE CHECKBOX HACK (ANTI-PATTERN)]
HTML: <input type="checkbox" id="modal-toggle"> <label for="modal-toggle">Open</label>
Accessibility Tree:
  └── Checkbox: "Open" (State: Unchecked)
Keyboard Navigation:
  └── User tabs to input -> Pressing Space checks checkbox
Flaw: Screen reader never informs user that a dialog opened;
      Tabbing continues right past the "modal" into background links!

=============================================================================

[NATIVE STANDARDS PATTERN]
HTML: <button popovertarget="info-panel">Open Info</button>
      <div id="info-panel" popover>...</div>
Accessibility Tree:
  └── Button: "Open Info" (Expanded: false, Controls: info-panel)
Top-Layer Promotion:
  └── Browser automatically hovers element above all stacking contexts
Light Dismiss:
  └── Native browser event loop traps Esc and backdrop clicks natively!
```

---

## 4. Syntax & Usage

### The Modern Platform Baseline
| Capability | Can CSS/HTML Do It Alone? | Required Modern Platform Feature |
|---|---|---|
| **Exclusive Accordion** | Yes | HTML `<details name="...">` (Baseline 2024) |
| **Light Dismiss Flyout** | Yes | HTML Popover API (`popover="auto"`) |
| **Top Layer Scrim** | Yes | Native `::backdrop` pseudo-element |
| **Modal Focus Trap** | Yes (via `<dialog>`) | Requires 1 line JS: `dialog.showModal()` |
| **Arrow Key Tab Cycling** | No | Requires JavaScript `keydown` listener |
| **Form Data Persistence**| No | Requires JavaScript `localStorage` / Fetch |

---

## 5. Practical Examples

### Example 1: The Modern CSS/HTML Native Popover vs Legacy Checkbox Hack
```html
<!-- LEGACY ANTI-PATTERN: The Checkbox Hack (Avoid in Production) -->
<!--
<input type="checkbox" id="drawer-toggle" class="visually-hidden">
<label for="drawer-toggle">Toggle Menu</label>
<nav class="drawer">...</nav>
Problem: Inaccessible to screen readers; confusing checkbox semantics.
-->

<!-- MODERN PRODUCTION PATTERN: Native Popover API (Zero JS Required) -->
<button type="button" class="btn-popover" popovertarget="user-dropdown">
  Account Settings ▾
</button>

<div id="user-dropdown" class="account-menu" popover>
  <a href="#profile">User Profile</a>
  <a href="#billing">Billing & Subscriptions</a>
  <a href="#security">Security Keys</a>
  <hr>
  <a href="#logout" class="text-danger">Log Out</a>
</div>
```

```css
.btn-popover {
  padding: 0.5rem 1rem;
  background: #ffffff;
  border: 1px solid #cbd5e1;
  border-radius: 6px;
  cursor: pointer;
  font-weight: 500;
}

/* Native Popover in Browser Top Layer */
.account-menu {
  border: 1px solid #e2e8f0;
  border-radius: 8px;
  background: #ffffff;
  padding: 0.5rem 0;
  box-shadow: 0 10px 15px -3px rgba(0, 0, 0, 0.1);
  min-width: 200px;
  margin: 0;
  inset: auto auto auto auto; /* Default fallback positioning */
  
  opacity: 0;
  transform: translateY(-8px);
  transition: opacity 150ms ease, transform 150ms ease, display 150ms allow-discrete;
}

.account-menu:popover-open {
  opacity: 1;
  transform: translateY(0);
}

@starting-style {
  .account-menu:popover-open {
    opacity: 0;
    transform: translateY(-8px);
  }
}

.account-menu a {
  display: block;
  padding: 0.5rem 1rem;
  color: #334155;
  text-decoration: none;
  font-size: 0.875rem;
}

.account-menu a:hover,
.account-menu a:focus-visible {
  background: #f1f5f9;
  color: #0f172a;
}
```

---

### Example 2: Where CSS-Only Fails: The Accessible Tabs Dilemma
```html
<!-- HTML Structure for Production-Grade Accessible Tabs -->
<div class="tabs-component">
  <div role="tablist" aria-label="Cloud Configuration">
    <button type="button" role="tab" aria-selected="true" id="tab-1" aria-controls="panel-1">
      Compute Instances
    </button>
    <button type="button" role="tab" aria-selected="false" id="tab-2" aria-controls="panel-2" tabindex="-1">
      Storage Volumes
    </button>
    <button type="button" role="tab" aria-selected="false" id="tab-3" aria-controls="panel-3" tabindex="-1">
      Network Security
    </button>
  </div>

  <div id="panel-1" role="tabpanel" aria-labelledby="tab-1" tabindex="0">
    <p>Configure dedicated CPU clusters and autoscale thresholds.</p>
  </div>
  <div id="panel-2" role="tabpanel" aria-labelledby="tab-2" tabindex="0" hidden>
    <p>Manage encrypted NVMe volumes and snapshot schedules.</p>
  </div>
  <div id="panel-3" role="tabpanel" aria-labelledby="tab-3" tabindex="0" hidden>
    <p>Define zero-trust ingress rules and firewall CIDR blocks.</p>
  </div>
</div>
```

```css
/* CSS Handles 100% of the Presentation & Fluid Polish */
[role="tablist"] {
  display: flex;
  gap: 0.5rem;
  border-bottom: 2px solid #e2e8f0;
}

[role="tab"] {
  background: transparent;
  border: none;
  border-bottom: 2px solid transparent;
  margin-bottom: -2px;
  padding: 0.75rem 1.25rem;
  font-weight: 500;
  color: #64748b;
  cursor: pointer;
  transition: all 150ms ease;
}

[role="tab"]:hover {
  color: #0f172a;
}

[role="tab"][aria-selected="true"] {
  color: #2563eb;
  border-bottom-color: #2563eb;
  font-weight: 600;
}

[role="tabpanel"] {
  padding: 1.5rem 0;
}

[role="tabpanel"][hidden] {
  display: none;
}
```

```javascript
// JAVASCRIPT IS MANDATORY FOR WAI-ARIA ARROW-KEY NAVIGATION
// CSS alone cannot cycle keyboard focus or synchronize aria-selected!
const tablist = document.querySelector('[role="tablist"]');
const tabs = tablist.querySelectorAll('[role="tab"]');

tablist.addEventListener('keydown', (e) => {
  let index = Array.from(tabs).indexOf(document.activeElement);
  if (e.key === 'ArrowRight') {
    index = (index + 1) % tabs.length;
    tabs[index].focus();
    tabs[index].click();
  } else if (e.key === 'ArrowLeft') {
    index = (index - 1 + tabs.length) % tabs.length;
    tabs[index].focus();
    tabs[index].click();
  }
});
```

---

### Example 3: CSS-Only Momentum Scroll Snap Carousel
```html
<div class="carousel-viewport">
  <div class="carousel-slide">
    <div class="slide-content">Slide 1: High-Performance Networking</div>
  </div>
  <div class="carousel-slide">
    <div class="slide-content">Slide 2: Instant Edge Rollouts</div>
  </div>
  <div class="carousel-slide">
    <div class="slide-content">Slide 3: Enterprise Zero Trust</div>
  </div>
</div>
```

```css
.carousel-viewport {
  display: flex;
  overflow-x: auto;
  scroll-snap-type: x mandatory;
  scroll-behavior: smooth;
  gap: 1.5rem;
  padding: 1.5rem;
  scrollbar-width: none;
  -webkit-overflow-scrolling: touch;
}

.carousel-viewport::-webkit-scrollbar {
  display: none;
}

.carousel-slide {
  flex: 0 0 calc(100% - 2rem);
  scroll-snap-align: center;
  max-width: 480px;
}

.slide-content {
  background: #ffffff;
  border: 1px solid #cbd5e1;
  border-radius: 12px;
  padding: 3rem 2rem;
  box-shadow: 0 4px 6px rgba(0, 0, 0, 0.05);
  font-weight: 600;
  text-align: center;
}
```

---

## 6. Best Practices

1. **Adopt the "HTML First, CSS Second, JS Third" Hierarchy**: Maximize semantic HTML elements (`<dialog>`, `<details>`, `<button>`, `popover`) first. Layer CSS for layouts and animations. Add JavaScript only where state synchronization or dynamic keyboard navigation demands it.
2. **Never Use the Checkbox Hack for Dialogs or Navigation**: Checkboxes communicate a binary on/off setting to assistive technology; using them to trigger dialogs or menus confuses screen reader users.
3. **Use Native `<dialog showModal()>` over CSS-Only Modals**: Native `<dialog>` handles top-layer promotion, screen reader inert backgrounds, and keyboard focus trapping with a single line of script.
4. **Use HTML Popover API for Tooltips and Menus**: Standardizes light dismissal (<kbd>Esc</kbd> / outside click) natively in the browser without JavaScript event listeners.
5. **Always Retain Visible Focus**: Ensure all interactive controls have explicit `:focus-visible` outlines. Never remove focus styling for the sake of visual minimalism.
6. **Recognize When JavaScript is Essential**: Do not bend CSS into an unmaintainable pretzel to avoid writing 5 lines of accessible JavaScript.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Root Cause | Symptom | Correct Architecture |
|---|---|---|---|
| **Checkbox Hack Modal** | Using `<input type="checkbox">` to open a modal | Screen reader announces "Checkbox"; focus is not trapped | Use native `<dialog>` and `dialog.showModal()` |
| **Missing Arrow Keys in Tabs** | Building pure CSS radio-button tabs | Tabbing works, but <kbd>ArrowLeft</kbd>/<kbd>ArrowRight</kbd> keys fail to navigate tabs per ARIA | Use ARIA `role="tablist"` + lightweight JS key listener |
| **Background Clickable in Modal** | CSS-only modal with `position: fixed` | Keyboard users tab behind the modal into active page links | Use `<dialog>` which makes background `inert` automatically |
| **Un-Dismissible Tooltip** | Hover-only tooltip that cannot be dismissed | User moves mouse over page; tooltip sticks and blocks content | Add <kbd>Esc</kbd> dismissal or use Popover API |
| **Scroll Lock Layout Shift** | Toggling `overflow: hidden` on body via hack | Page jerks 15px right as scrollbar disappears | Add `html { scrollbar-gutter: stable; }` |
| **Over-engineered CSS State Machine**| Using 50 CSS selectors to emulate a game/wizard | Code is impossible to maintain or debug | Use simple, readable JavaScript state managers |

---

## 8. Performance, Accessibility & Browser Support

### Rendering Performance
- **Zero-Script Advantage**: True CSS and declarative HTML components (such as `<details>` or Popover) eliminate script parse, compile, and execution time, drastically boosting **Interaction to Next Paint (INP)** scores.
- **Top Layer Isolation**: Native popovers and dialogs operate in the browser's top layer, allowing the engine to optimize rendering independently of parent DOM subtrees.

### Accessibility Protocols
- **W3C APG Conformance**: Always verify components against the W3C WAI-ARIA Authoring Practices Guide (APG). If a pattern mandates specific keyboard behaviors (such as Home/End key jumps in tabs), JavaScript is non-negotiable.

---

## 9. Comparison: CSS-Only vs HTML-First vs JS-Augmented

| Feature | CSS-Only (Checkbox/Target) | HTML-First (Details/Popover) | JS-Augmented (ARIA APG) |
|---|---|---|---|
| **JavaScript Dependency** | 0 Bytes | 0 Bytes | ~1–3 KB script |
| **Screen Reader Semantics**| Broken / Misleading | 100% Native | 100% WAI-ARIA Compliant |
| **Keyboard Traversal** | Tab key only | Native Browser defaults | Full Arrow, Home, End keys |
| **Light Dismiss (Esc/Click)**| Requires target/hacks | Built-in (Popover API) | Fully customizable |
| **Maintenance Burden** | High (Brittle selectors) | Very Low (Standard HTML) | Low (Clear division of labor)|
| **Production Suitability** | Prototype / Hack Only | Fully Recommended | Fully Recommended |

---

## 10. Interview Questions

<details>
<summary><strong>1. Why is the "checkbox hack" considered an anti-pattern in modern accessible web development?</strong></summary>

**Answer:**
The checkbox hack repurposes an `<input type="checkbox">` and `<label>` to store binary UI state (e.g. open/closed navigation drawer) without JavaScript.

Critical accessibility violations:
1. **Semantic Misrepresentation**: Screen readers announce the trigger as a "Checkbox, unchecked" instead of a "Button, menu, collapsed".
2. **Missing State Feedback**: Toggling the checkbox does not announce whether a drawer or modal opened.
3. **No Focus Trapping**: Keyboard users can tab straight through the open drawer into invisible background elements.
4. **No <kbd>Esc</kbd> Key Support**: Pressing Escape does not close the component.
Modern alternatives like the HTML Popover API and native `<dialog>` solve these problems cleanly without accessibility compromises.
</details>

<details>
<summary><strong>2. What UI patterns can now be built 100% declaratively without JavaScript in Baseline modern browsers?</strong></summary>

**Answer:**
1. **Exclusive Accordions**: `<details name="faq">` handles single-open mutex behavior natively.
2. **Non-Modal Popovers**: `<button popovertarget="menu">` provides top-layer placement, outside-click closing, and <kbd>Esc</kbd> dismissal.
3. **Discrete Animation**: `@starting-style` and `transition-behavior: allow-discrete` animate elements opening from `display: none`.
4. **Dynamic Height Transitions**: Animating `grid-template-rows: 0fr` to `1fr` smoothly expands containers to intrinsic content height.
5. **Touch Carousels**: `scroll-snap-type: x mandatory` provides momentum swiping with zero script.
</details>

<details>
<summary><strong>3. Where does the hard boundary lie where JavaScript becomes mandatory for accessible components?</strong></summary>

**Answer:**
JavaScript is non-negotiable for:
1. **Complex Keyboard Navigation**: Arrow-key navigation in tablists, menu bars, comboboxes, and roving `tabindex` models.
2. **Dynamic Focus Trapping**: Confining keyboard focus inside custom dialogs (unless using native `.showModal()`).
3. **ARIA State Synchronization**: Dynamically updating `aria-expanded`, `aria-selected`, or `aria-valuenow`.
4. **Data Mutations & Persistence**: Communicating with backend APIs, handling form submissions, and persisting state across browser refreshes.
</details>

<details>
<summary><strong>4. How does the HTML Popover API replace custom JavaScript dropdown libraries?</strong></summary>

**Answer:**
The Popover API standardizes floating overlay mechanics directly in browser C++ code:
- Triggered declaratively: `<button popovertarget="my-popover">`.
- Renders in the browser's **Top Layer**, immune to parent `overflow: hidden` or `z-index` stacking context traps.
- Native **Light Dismiss**: Clicking outside or pressing <kbd>Esc</kbd> dismisses the popover automatically.
- Built-in accessibility: Assistive technology understands the relationship between the trigger and the popover.
</details>

<details>
<summary><strong>5. Why is native `<dialog>` with 1 line of JS superior to a pure CSS modal hack?</strong></summary>

**Answer:**
Invoking `dialog.showModal()` executes complex accessibility logic that pure CSS cannot replicate:
1. The browser automatically places the dialog in the **Top Layer**.
2. The entire background document is automatically marked **inert**, preventing keyboard focus and mouse clicks from escaping.
3. Keyboard focus is strictly trapped inside the dialog.
4. Pressing <kbd>Esc</kbd> automatically closes the dialog and restores focus to the invoking button.
Achieving this with pure CSS is impossible; achieving it with custom JS requires hundreds of lines of code.
</details>

<details>
<summary><strong>6. What is the Architectural Rule of Thumb regarding CSS-only solutions?</strong></summary>

**Answer:**
*"Use HTML for semantic structure and native browser controls; use CSS for visual styling, responsive layouts, and hardware-accelerated animations; use JavaScript for state management, assistive technology choreography, and dynamic data mutations."*
Do not force CSS to perform tasks that violate accessibility or degrade user experience.
</details>

<details>
<summary><strong>7. How do CSS Scroll-Driven Animations eliminate JavaScript scroll listeners?</strong></summary>

**Answer:**
Traditionally, building reading progress bars or parallax effects required attaching `window.addEventListener('scroll', ...)` and querying scroll positions, causing continuous layout thrashing.

With CSS Scroll-Driven Animations (`animation-timeline: scroll()` or `view()`), the browser engine links animation progress directly to the compositor's scroll offset on the GPU, executing silky-smooth scroll effects with zero main-thread JavaScript execution.
</details>

<details>
<summary><strong>8. How should high contrast mode (`forced-colors: active`) be tested on CSS-only components?</strong></summary>

**Answer:**
High Contrast Mode strips background colors, drop-shadows, and custom SVG fills. CSS-only components that rely on visual background colors to indicate state (e.g. an active tab or checked toggle) will fail in high contrast mode unless reinforced with explicit system borders: `border: 2px solid Highlight;`.
</details>

---

## 11. Practice Exercises

### Exercise 1: Build a Pure HTML/CSS Popover Menu
**Task**: Create a button that toggles an account dropdown menu using the native Popover API, complete with outside-click light dismissal and smooth discrete fading.
<details>
<summary>View Verified Solution</summary>

```html
<button type="button" popovertarget="nav-popover">Menu ▾</button>

<div id="nav-popover" popover>
  <nav>
    <a href="#dash">Dashboard</a>
    <a href="#settings">Settings</a>
  </nav>
</div>
```

```css
#nav-popover {
  border: 1px solid #cbd5e1;
  border-radius: 8px;
  padding: 0.5rem;
  background: #ffffff;
  box-shadow: 0 10px 15px rgba(0, 0, 0, 0.1);
  opacity: 0;
  transition: opacity 150ms ease, display 150ms allow-discrete;
}

#nav-popover:popover-open {
  opacity: 1;
}

@starting-style {
  #nav-popover:popover-open {
    opacity: 0;
  }
}
```
</details>

---

### Exercise 2: Native Exclusive Accordion Setup
**Task**: Author 3 FAQ disclosure items using native HTML elements where opening one question closes all others, with zero JavaScript.
<details>
<summary>View Verified Solution</summary>

```html
<details name="exclusive-faq" open>
  <summary>What is the service SLA?</summary>
  <p>99.999% monthly uptime guarantee.</p>
</details>

<details name="exclusive-faq">
  <summary>Where are servers located?</summary>
  <p>Distributed across 32 global edge datacenters.</p>
</details>

<details name="exclusive-faq">
  <summary>How is data encrypted?</summary>
  <p>AES-256 at rest, TLS 1.3 in transit.</p>
</details>
```
</details>

---

### Exercise 3: CSS Scroll-Snap Card Ribbon
**Task**: Build a horizontal scroll container where cards snap cleanly into center alignment on touch or mouse wheel scroll.
<details>
<summary>View Verified Solution</summary>

```css
.card-ribbon {
  display: flex;
  overflow-x: auto;
  scroll-snap-type: x mandatory;
  gap: 1rem;
  padding: 1rem;
}

.card-ribbon > .card {
  flex: 0 0 280px;
  scroll-snap-align: center;
  background: #ffffff;
  border: 1px solid #e2e8f0;
  border-radius: 8px;
  padding: 1.5rem;
}
```
</details>

---

### Exercise 4: Clean Summary Triangle Reset
**Task**: Write the CSS rule to remove the default disclosure marker across Chrome, Firefox, and Safari while maintaining keyboard usability.
<details>
<summary>View Verified Solution</summary>

```css
summary {
  list-style: none;
  cursor: pointer;
}

summary::-webkit-details-marker {
  display: none;
}
```
</details>

---

### Exercise 5: Adding a 1-Line JavaScript Focus Trap Dialog
**Task**: Create a dialog that leverages native `<dialog>` and opens with proper screen-reader inert background using 1 line of JS.
<details>
<summary>View Verified Solution</summary>

```html
<button type="button" onclick="document.getElementById('my-dialog').showModal()">
  Open Secure Dialog
</button>

<dialog id="my-dialog">
  <h3>Secure Confirmation</h3>
  <p>Keyboard focus is now safely trapped inside this modal.</p>
  <button type="button" onclick="document.getElementById('my-dialog').close()">Close</button>
</dialog>
```
</details>

---

### Exercise 6: CSS Grid Intrinsic Dynamic Height Transition
**Task**: Create an expandable container that animates from 0px to its intrinsic height using CSS Grid.
<details>
<summary>View Verified Solution</summary>

```css
.expand-box {
  display: grid;
  grid-template-rows: 0fr;
  transition: grid-template-rows 200ms ease;
}

.expand-box.is-expanded {
  grid-template-rows: 1fr;
}

.expand-box > .inner {
  overflow: hidden;
  min-height: 0;
}
```
</details>

---

## 12. Summary / Cheat Sheet

```
+------------------------------------------------------------------------------------+
|                      CSS-ONLY COMPONENTS & LIMITS CHEAT SHEET                      |
+------------------------------------------------------------------------------------+
| 1. Native HTML First:                                                              |
|    - Exclusive accordions: <details name="group">                                  |
|    - Native popovers: <button popovertarget="menu">                                |
|    - Modal overlays: <dialog>.showModal()                                          |
+------------------------------------------------------------------------------------+
| 2. CSS presentation & polish:                                                      |
|    - Layout: CSS Grid, Flexbox, Subgrid, Container Queries                         |
|    - Animations: Transitions, Keyframes, @starting-style, Discrete transitions     |
|    - Scroll Snapping: scroll-snap-type: x mandatory                                |
+------------------------------------------------------------------------------------+
| 3. Mandatory JavaScript boundaries:                                                |
|    - Keyboard Arrow navigation (<kbd>→</kbd>, <kbd>←</kbd>) in ARIA tablists       |
|    - Dynamic focus traps in non-dialog overlays                                    |
|    - Asynchronous API data mutations & storage persistence                         |
+------------------------------------------------------------------------------------+
| 4. Production Rule:                                                                |
|    Never use the "checkbox hack" for modals, drawers, or menus in production.      |
+------------------------------------------------------------------------------------+
```

---

## 13. Further Reading
- [W3C WAI-ARIA Authoring Practices Guide (APG)](https://www.w3.org/WAI/ARIA/apg/)
- [MDN Web Docs: HTML Popover API](https://developer.mozilla.org/en-US/docs/Web/API/Popover_API)
- [HTML Standard: The `<dialog>` Element](https://html.spec.whatwg.org/multipage/interactive-elements.html#the-dialog-element)
- [Adrian Roselli: Avoid the Checkbox Hack](https://adrianroselli.com/2017/10/dont-use-the-checkbox-hack.html)
- [web.dev: HTML First, CSS Second, JS Third](https://web.dev/learn/design/)

---

## Navigation
- **Previous**: [Skeleton Loaders and Empty States](file:///e:/Learning/engineering-notes/01-languages/css/13-layout-and-component-recipes/13-skeleton-loaders-and-empty-states.md)
- **Next**: [Module 14: Production Reference Projects](file:///e:/Learning/engineering-notes/01-languages/css/14-projects/README.md)
