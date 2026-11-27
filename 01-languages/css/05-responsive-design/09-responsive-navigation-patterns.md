# Responsive Navigation Patterns

| Metadata | Details |
| :--- | :--- |
| **Level** | Core / Production Patterns |
| **Prerequisites** | `05-responsive-design/01-responsive-principles-and-mobile-first.md`, `05-responsive-design/03-media-queries.md` |
| **Estimated Reading Time** | 25 minutes |
| **Status** | Complete |
| **Baseline Status** | Widely Available |
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

Navigation is the central spine of any web application. On a $1440\text{px}$ desktop display, an application can comfortably display a brand logo, 8 top-level links, a search bar, and user account buttons in a single horizontal row.

On a $375\text{px}$ smartphone screen, that same menu will violently overflow and break into chaotic multiline wreckage.

Solving responsive navigation requires selecting the right structural pattern:
1. **The Slide-Out Mobile Drawer (Hamburger Menu)**: Hides links behind a toggle trigger on mobile, expanding to an inline row on desktop.
2. **The Horizontal Scrollable Tab Bar (App-Style Navigation)**: Retains all items on a single horizontal line that scrolls smoothly with swipe momentum on touch devices.
3. **The Native `<details>` / `<summary>` Dropdown Pattern**: Accessible, zero-JavaScript responsive accordion navigation.
4. **The Priority+ Navigation Pattern**: Displays as many links as can fit in the header, automatically stashing the remainder into a "More..." overflow menu.

---

## 2 Core Concepts

### 1. Progressive Navigation Transformation

In modern responsive architecture, mobile navigation is not an afterthought; it is the default baseline:
- Mobile ($< 768\text{px}$): Hidden off-canvas drawer or collapsible accordion.
- Desktop ($\ge 768\text{px}$): Horizontal inline flex bar.
- Transitions must be smooth, accessible to keyboard users, and respect `prefers-reduced-motion`.

### 2. The Pure-CSS vs. Accessible Hybrid Dilemma

Historically, tutorials taught the "Checkbox Hack" (`<input type="checkbox">` + `<label>` + `:checked ~ ul`) to create mobile menus without JavaScript.
**Why the Checkbox Hack is an Accessibility Anti-Pattern**:
- Screen readers announce a checkbox ("Checkbox unchecked") rather than an expandable menu button.
- Lacks `aria-expanded="false/true"` and `aria-controls`.
- Keyboard users cannot open or close the menu with standard `Enter` or `Space` keys without hacky tab configurations.

The accessible standards are:
- **Native HTML `<details>` and `<summary>`**: 100% pure CSS/HTML, natively accessible to screen readers.
- **Accessible Hybrid (Semantic `<button>` + CSS transition)**: A semantic button with `aria-expanded` that toggles an active CSS class.

---

## 3 How It Works Internally

```
Responsive Navigation State Machine:

Mobile Viewport (< 768px):
+-------------------------------------------------------------+
| [Brand Logo]                               [☰ Toggle Button]|
+-------------------------------------------------------------+
                                                    │
                                (When Clicked / Activated)
                                                    ▼
+-------------------------------------------------------------+
| [Brand Logo]                                [✕ Close Button]|
+-------------------------------------------------------------+
| • Home                                                      |
| • Products                                                  |
| • Documentation                                             |
| • Contact Us                                                |
+-------------------------------------------------------------+

Desktop Viewport (>= 768px):
+-------------------------------------------------------------------------+
| [Brand Logo]   Home   Products   Documentation   Contact Us   [Sign In] |
+-------------------------------------------------------------------------+
(Toggle button is display: none; links become an inline flex row!)
```

---

## 4 Syntax & Usage

```css
/* Mobile Baseline (< 768px) */
.nav-toggle-btn {
  display: block; /* Visible on mobile */
  background: transparent;
  border: none;
  cursor: pointer;
}

.nav-menu-list {
  display: none; /* Collapsed on mobile */
  flex-direction: column;
  position: absolute;
  top: 100%;
  left: 0;
  right: 0;
  background-color: #0f172a;
}

.nav-menu-list.is-open {
  display: flex; /* Open state */
}

/* Desktop Enhancement (>= 768px) */
@media (width >= 768px) {
  .nav-toggle-btn {
    display: none; /* Hide hamburger toggle on desktop */
  }

  .nav-menu-list {
    display: flex !important; /* Force permanent display on desktop */
    position: static;
    flex-direction: row;
    background-color: transparent;
  }
}
```

---

## 5 Practical Examples

### Example 1: Accessible Pure-HTML/CSS Collapsible Navigation (`<details>`)

```html
<header class="site-header">
  <a href="/" class="brand-logo">Acme Inc.</a>
  
  <details class="nav-disclosure">
    <summary class="nav-summary" aria-label="Toggle Navigation Menu">
      <span class="hamburger-icon" aria-hidden="true"></span>
    </summary>
    
    <nav class="nav-dropdown">
      <a href="#features">Features</a>
      <a href="#pricing">Pricing</a>
      <a href="#docs">Docs</a>
    </nav>
  </details>
</header>
```

```css
.site-header {
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding: 16px 24px;
  background-color: #0f172a;
  color: #ffffff;
  position: relative;
}

.brand-logo {
  color: #ffffff;
  font-weight: 700;
  text-decoration: none;
}

/* Mobile Disclosure */
.nav-summary {
  list-style: none;
  cursor: pointer;
  padding: 8px;
}

.nav-summary::-webkit-details-marker {
  display: none;
}

.hamburger-icon {
  display: block;
  width: 24px;
  height: 2px;
  background-color: #ffffff;
  box-shadow: 0 7px 0 0 #ffffff, 0 -7px 0 0 #ffffff;
}

.nav-dropdown {
  position: absolute;
  top: 100%;
  left: 0;
  right: 0;
  background-color: #1e293b;
  display: flex;
  flex-direction: column;
  padding: 16px;
  gap: 12px;
}

.nav-dropdown a {
  color: #f8fafc;
  text-decoration: none;
}

/* Desktop: Expand inline permanently */
@media (width >= 768px) {
  .nav-disclosure {
    display: block;
  }

  .nav-summary {
    display: none; /* Hide toggle icon */
  }

  .nav-disclosure[open] .nav-dropdown,
  .nav-dropdown {
    position: static;
    display: flex !important;
    flex-direction: row;
    background-color: transparent;
    padding: 0;
    gap: 20px;
  }
}
```

### Example 2: Horizontal Scrolling App-Style Tab Bar

For mobile web applications with frequent category switching (like YouTube or Twitter), a horizontal scrolling tab bar is often superior to a hidden hamburger menu:

```css
.scrollable-tab-nav {
  display: flex;
  overflow-x: auto;
  scrollbar-width: none; /* Hide scrollbars in Firefox */
  -webkit-overflow-scrolling: touch; /* Momentum scroll on iOS */
  gap: 8px;
  padding: 8px 16px;
  background-color: #ffffff;
  border-bottom: 1px solid #e2e8f0;
}

.scrollable-tab-nav::-webkit-scrollbar {
  display: none; /* Hide scrollbar in Chrome/Safari */
}

.tab-link {
  flex-shrink: 0; /* Prevents chips from squishing */
  padding: 6px 16px;
  background-color: #f1f5f9;
  color: #334155;
  border-radius: 9999px;
  text-decoration: none;
  font-size: 0.875rem;
  font-weight: 500;
  white-space: nowrap;
}

.tab-link.active {
  background-color: #0f172a;
  color: #ffffff;
}
```

### Example 3: Edge Case — Off-Canvas Drawer with Backdrop Dimming

```css
.drawer-backdrop {
  position: fixed;
  inset: 0;
  background-color: rgba(0, 0, 0, 0.5);
  backdrop-filter: blur(2px);
  z-index: 500;
  opacity: 0;
  pointer-events: none;
  transition: opacity 0.3s ease;
}

.drawer-menu {
  position: fixed;
  top: 0;
  bottom: 0;
  left: 0;
  width: 280px;
  background-color: #ffffff;
  z-index: 600;
  transform: translateX(-100%); /* Stashed off-screen */
  transition: transform 0.3s cubic-bezier(0.16, 1, 0.3, 1);
  padding: 24px;
}

/* Active State */
.drawer-open .drawer-backdrop {
  opacity: 1;
  pointer-events: auto;
}

.drawer-open .drawer-menu {
  transform: translateX(0); /* Slides in */
}

@media (width >= 1024px) {
  .drawer-backdrop, .drawer-menu {
    display: none; /* Desktop uses static inline navbar */
  }
}
```

### Example 4: Failure Case — Broken Focus Trap in Hidden Menus

```css
/* FAILURE PATTERN */
.mobile-nav-list {
  /* BUG: Using opacity: 0 or transform: translateX(-100%) without visibility: hidden!
     Even though the menu is visually off-screen, keyboard users pressing Tab
     will still focus the invisible links off-screen! */
  opacity: 0;
  transform: translateX(-100%);
}

/* FIXED PATTERN */
.mobile-nav-list-fixed {
  opacity: 0;
  transform: translateX(-100%);
  visibility: hidden; /* Removes elements from keyboard tab order! */
  transition: opacity 0.3s ease, transform 0.3s ease, visibility 0.3s;
}

.mobile-nav-list-fixed.is-open {
  opacity: 1;
  transform: translateX(0);
  visibility: visible;
}
```

### Example 5: Sticky Mobile Header with Blur

```css
.sticky-mobile-nav {
  position: sticky;
  top: 0;
  z-index: 100;
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding: 12px 16px;
  background-color: rgba(255, 255, 255, 0.9);
  backdrop-filter: blur(8px);
  border-bottom: 1px solid #e2e8f0;
}
```

---

## 6 Best Practices

1. **Always use `visibility: hidden` or `display: none` on closed mobile menus**: Prevents keyboard users from tabbing into invisible, off-screen links.
2. **Ensure touch targets on mobile links are at least $44\text{px}$ tall**: Avoid tightly packed text links that cause accidental taps.
3. **Use native `<details>`/`<summary>` if a zero-JavaScript menu is required**: Far more accessible than the checkbox hack.
4. **Always pair mobile menus with an accessible close trigger**: Users must be able to dismiss the menu with an explicit close button or `Escape` key.
5. **Hide scrollbars cleanly on horizontal tab bars**: Use `scrollbar-width: none;` and `-webkit-overflow-scrolling: touch;`.
6. **Ensure desktop media queries permanently display menu links**: Add `display: flex !important;` inside `@media (width >= 768px)` so links don't stay hidden if resized.

---

## 7 Common Mistakes & Pitfalls

| Mistake | Why It Breaks | Fix |
| :--- | :--- | :--- |
| Keyboard focus enters hidden menu | Used `transform: translateX(-100%)` without `visibility: hidden` | Add `visibility: hidden` when menu is closed |
| Checkbox hack confuses screen readers | Announces "Checkbox unchecked" instead of a navigation menu | Use semantic `<button>` or `<details>` |
| Menu stays hidden after resizing to desktop | JavaScript added `style="display: none"` that media queries can't override | Clear inline styles on resize or use CSS classes |
| Horizontal tabs squishing on mobile | Flex items default to `flex-shrink: 1` | Add `flex-shrink: 0` to tab chips |
| Body scrolls behind open mobile drawer | Scrolling the menu accidentally scrolls the background webpage | Add `overflow: hidden` to `<body>` when menu is open |
| Tiny hamburger icon hard to tap | Click target is smaller than human finger | Expand button hit target to `44px x 44px` |

---

## 8 Performance, Accessibility & Browser Support

### Off-Canvas Hardware Acceleration

- When animating mobile drawers, always animate **`transform: translateX(...)`** and **`opacity`**.
- Never animate `left`, `width`, or `margin-left`, which trigger expensive layout reflows and stutter on low-end mobile CPUs.
- `transform` executes on the browser's GPU compositor thread at 60/120 FPS.

### Accessibility Standards (WCAG 2.1 & 2.2)

- **Menu Toggles (WCAG 4.1.2 Name, Role, Value)**: Mobile toggle buttons must have an accessible label (`aria-label="Open Navigation Menu"`) and dynamically update `aria-expanded="false/true"`.
- **Keyboard Tab Order**: When the mobile drawer opens, keyboard focus must move into the menu; when closed, focus must return to the toggle button.

---

## 9 Comparison / When to Use What

| Pattern | Best Viewport | Pros | Cons |
| :--- | :--- | :--- | :--- |
| **Hamburger Drawer** | Mobile ($< 768\text{px}$) | Handles 15+ links, clean header | Hides content behind extra tap |
| **Horizontal Scrolling Tabs** | Mobile / App | High engagement, zero taps to see links | Best for $\le 8$ items only |
| **`<details>` Dropdown** | Responsive | Zero JavaScript required, native a11y | Limited animation capabilities |
| **Priority+ Overflow** | Desktop & Tablet | Maximizes visible screen real estate | Requires JavaScript width measurement |

---

## 10 Interview Questions

<details>
<summary><strong>1. Why is the "CSS Checkbox Hack" for mobile menus considered bad practice in production?</strong></summary>

The checkbox hack uses a hidden `<input type="checkbox">` and `<label>` to toggle navigation states with `:checked ~ ul`. This is an accessibility anti-pattern because screen readers announce the control as a form checkbox ("Check box, unchecked") rather than an expandable navigation menu button. It cannot communicate `aria-expanded="true/false"` or `aria-controls`, and keyboard accessibility is severely compromised.
</details>

<details>
<summary><strong>2. How do you prevent keyboard focus from entering an off-screen mobile navigation menu?</strong></summary>

When an off-canvas drawer is animated off-screen using `transform: translateX(-100%)`, the elements are still technically visible to the keyboard tab navigation sequence. To remove them from tab order, you must pair the transform with **`visibility: hidden`** or **`display: none`**. When the menu opens, restore `visibility: visible`.
</details>

<details>
<summary><strong>3. How does the HTML <code>&lt;details&gt;</code> and <code>&lt;summary&gt;</code> element enable accessible pure-CSS navigation?</strong></summary>

`<details>` and `<summary>` are semantic HTML5 disclosure elements with built-in accessibility. Screen readers natively understand that `<summary>` is an interactive disclosure button and automatically announce whether the details are collapsed or expanded. In CSS, authors can style the open state using the `:open` pseudo-class or attribute selector `details[open]`.
</details>

<details>
<summary><strong>4. Why should you animate <code>transform: translateX()</code> instead of <code>left</code> when opening a mobile drawer?</strong></summary>

Animating `left` triggers the browser's layout (reflow) and paint stages on every animation frame, consuming CPU cycles and causing choppy animations on mobile devices. Animating `transform` bypasses layout and paint, executing strictly on the GPU compositor thread for a fluid 60fps/120fps animation.
</details>

<details>
<summary><strong>5. How do you implement momentum scrolling on a horizontal mobile tab bar?</strong></summary>

By setting `overflow-x: auto;` and applying the vendor-prefixed property `-webkit-overflow-scrolling: touch;`. This enables native inertia flick-scrolling on iOS WebKit devices.
</details>

<details>
<summary><strong>6. What is "Scroll Chaining", and how do you stop a mobile drawer from scrolling the background webpage?</strong></summary>

When a user scrolls to the bottom of a scrollable mobile menu, continuing to scroll causes the underlying body page to scroll. Setting `overscroll-behavior: contain;` on the menu drawer confines the scroll gesture to the drawer, preventing scroll chaining to the parent page.
</details>

<details>
<summary><strong>7. How do you ensure desktop navigation displays properly if a user resizes their window while the mobile menu is closed?</strong></summary>

Ensure your desktop media query (`@media (width >= 768px)`) explicitly resets the menu display:
```css
@media (width >= 768px) {
  .nav-menu {
    display: flex !important;
    position: static;
    visibility: visible !important;
    transform: none !important;
  }
}
```
</details>

<details>
<summary><strong>8. What is the Priority+ navigation pattern?</strong></summary>

The Priority+ pattern displays the most critical navigation links in the header as space permits. As the screen narrows, links that no longer fit are progressively moved into a "More..." dropdown menu, maximizing visible navigation at every viewport size.
</details>

---

## 11 Practice Exercises

### Exercise 1 (Easy): Clean Mobile Nav Toggle Visibility
- **Task**: Show the hamburger button exclusively on screens under 768px, and display an inline link row on screens 768px and wider.

<details>
<summary>View Solution</summary>

```css
/* Mobile */
.hamburger-btn {
  display: block;
}
.nav-links {
  display: none;
}

/* Desktop */
@media (width >= 768px) {
  .hamburger-btn {
    display: none;
  }
  .nav-links {
    display: flex;
    gap: 16px;
  }
}
```
</details>

### Exercise 2 (Easy): Horizontal App Tab Bar
- **Task**: Style a horizontal tab strip that scrolls smoothly with hidden scrollbars.

<details>
<summary>View Solution</summary>

```css
.tab-strip {
  display: flex;
  overflow-x: auto;
  scrollbar-width: none;
  gap: 8px;
  padding: 8px 16px;
}

.tab-strip::-webkit-scrollbar {
  display: none;
}

.tab-item {
  flex-shrink: 0;
  white-space: nowrap;
}
```
</details>

### Exercise 3 (Medium): Off-Canvas Drawer with Visibility Guard
- **Task**: Create an off-canvas drawer that uses `visibility: hidden` when closed to prevent keyboard tab trapping.

<details>
<summary>View Solution</summary>

```css
.drawer {
  position: fixed;
  top: 0;
  bottom: 0;
  left: 0;
  width: 260px;
  background: #ffffff;
  transform: translateX(-100%);
  visibility: hidden;
  transition: transform 0.3s ease, visibility 0.3s;
  z-index: 1000;
}

.drawer.is-active {
  transform: translateX(0);
  visibility: visible;
}
```
</details>

### Exercise 4 (Medium): Accessible HTML `<details>` Header
- **Task**: Style a zero-JavaScript header with `<details>` that expands inline on desktop.

<details>
<summary>View Solution</summary>

```css
details summary {
  list-style: none;
  cursor: pointer;
}

details summary::-webkit-details-marker {
  display: none;
}

@media (width >= 768px) {
  details summary {
    display: none;
  }
  
  details nav {
    display: flex !important;
    gap: 16px;
  }
}
```
</details>

### Exercise 5 (Hard): GPU-Accelerated Drawer with Dimming Backdrop
- **Task**: Author the complete CSS for a slide-out drawer that animates on the compositor thread with a blurred, dimming backdrop.

<details>
<summary>View Solution</summary>

```css
.backdrop {
  position: fixed;
  inset: 0;
  background: rgba(15, 23, 42, 0.6);
  backdrop-filter: blur(4px);
  opacity: 0;
  pointer-events: none;
  transition: opacity 0.25s ease;
  z-index: 50;
}

.drawer-panel {
  position: fixed;
  top: 0;
  bottom: 0;
  left: 0;
  width: 300px;
  background: #ffffff;
  transform: translateX(-100%);
  visibility: hidden;
  transition: transform 0.25s cubic-bezier(0.16, 1, 0.3, 1), visibility 0.25s;
  z-index: 60;
  padding: 24px;
}

/* Active State */
.menu-open .backdrop {
  opacity: 1;
  pointer-events: auto;
}

.menu-open .drawer-panel {
  transform: translateX(0);
  visibility: visible;
}
```
</details>

### Exercise 6 (Hard): Overscroll Containment on Mobile Drawer
- **Task**: Prevent the body page from scrolling when a user scrolls to the bottom of the mobile drawer.

<details>
<summary>View Solution</summary>

```css
.scrollable-drawer-body {
  height: 100%;
  overflow-y: auto;
  overscroll-behavior-y: contain; /* Prevents scroll chaining to body */
  padding: 16px;
}
```
</details>

---

## 12 Summary / Cheat Sheet

```css
/* Navigation Checklist */

/* 1. Never let closed menus capture focus: */
.closed-menu {
  visibility: hidden; /* Or display: none */
}

/* 2. Animate transforms, not layout offsets: */
.drawer {
  transform: translateX(-100%);
  transition: transform 0.3s ease;
}

/* 3. Stop background scroll chaining: */
.drawer-content {
  overscroll-behavior: contain;
}

/* 4. Horizontal Tabs: */
.tab-bar {
  display: flex;
  overflow-x: auto;
  scrollbar-width: none;
}
.tab-bar .tab { flex-shrink: 0; }
```

---

## 13 Further Reading

- [MDN Web Docs: Styling navigation bars](https://developer.mozilla.org/en-US/docs/Learn/CSS/Howto/Navigation_bar)
- [W3C WAI-ARIA Authoring Practices: Disclosure Navigation Menu](https://www.w3.org/WAI/ARIA/apg/patterns/disclosure/)
- [web.dev: Building a navigation component](https://web.dev/patterns/nav/)
- [CSS-Tricks: In Praise of the Unambiguous Click Menu](https://css-tricks.com/in-praise-of-the-unambiguous-click-menu/)

---

[Previous: 08-interaction-media-features.md](file:///e:/Learning/engineering-notes/01-languages/css/05-responsive-design/08-interaction-media-features.md) | [Section Index](file:///e:/Learning/engineering-notes/01-languages/css/05-responsive-design/README.md) | [Next: 10-responsive-tables-and-data.md](file:///e:/Learning/engineering-notes/01-languages/css/05-responsive-design/10-responsive-tables-and-data.md)
