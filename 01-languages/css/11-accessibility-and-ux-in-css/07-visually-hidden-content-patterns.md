# 07. Visually Hidden Content Patterns

| Metadata | Details |
| :--- | :--- |
| **Spec Level & Status** | WCAG 2.1 / 2.2 Level A (SC 2.4.1 Bypass Blocks, SC 1.3.1 Info & Relationships) |
| **Browsers Tested** | Chrome 120+, Firefox 125+, Safari 17.4+ |
| **Tested With** | NVDA (Windows), VoiceOver (macOS / iOS), JAWS, Chrome DevTools A11y Tree |
| **Prerequisites** | `02-box-model-and-layout-basics/04-visibility-opacity-and-display-none.md`, `11-accessibility-and-ux-in-css/01-focus-visible-and-focus-styles.md` |

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
Visual user interfaces often convey context through spatial cues, iconography, or proximity that sighted users comprehend effortlessly:
- A search input with a magnifying glass icon has no visible "Search" text.
- A table row with three "Delete" buttons relies on visual column alignment to know *which* record is being deleted.
- A page header has 50 navigation links that keyboard users must tab through on every single page load before reaching main content.

When developers attempt to hide helper text or skip navigation from sighted users, they frequently reach for:
```css
/* FATAL ACCESSIBILITY BUGS */
.hidden-label {
  display: none; /* Removed completely from the Accessibility Tree! Screen readers are silent! */
}
.hidden-helper {
  visibility: hidden; /* Also removed from the Accessibility Tree! */
}
.hidden-text {
  font-size: 0; /* WebKit/Blink skips reading 0px text or reads characters phonetically! */
}
```
These CSS anti-patterns remove elements from the browser's **Accessibility Tree**, silencing assistive technologies and failing basic accessibility compliance.

### The Solution
The production **`.sr-only`** (Screen Reader Only) or **`.visually-hidden`** utility pattern hides content visually from the monitor screen while guaranteeing that:
1. The DOM node remains fully present in the browser's Accessibility Tree.
2. Screen readers (NVDA, JAWS, VoiceOver, TalkBack) announce the text in natural speech order.
3. Interactive elements (such as **"Skip to Main Content"** links) snap into full visual prominence the instant they receive keyboard focus (`:focus-visible`).

---

## 2. Core Concepts

### 1. How CSS Hiding Techniques Impact Screen Readers

```
┌─────────────────────────────────┬─────────────────┬────────────────────────┐
│ CSS Technique                   │ Sighted Screen  │ Screen Reader Speech   │
├─────────────────────────────────┼─────────────────┼────────────────────────┤
│ display: none                   │ Hidden          │ SILENT (Node Removed)  │
│ visibility: hidden              │ Hidden          │ SILENT (Node Removed)  │
│ hidden HTML attribute           │ Hidden          │ SILENT (Node Removed)  │
│ aria-hidden="true"              │ VISIBLE         │ SILENT (A11y Ignored)  │
│ opacity: 0                      │ Transparent     │ ANNOUNCED (Clickable!) │
│ left: -9999px (Legacy)          │ Hidden          │ ANNOUNCED (Causes lag) │
│ .sr-only (Production Pattern)   │ Hidden          │ ANNOUNCED (Flawless)   │
└─────────────────────────────────┴─────────────────┴────────────────────────┘
```

1. **`display: none` & `visibility: hidden`**: The browser engine explicitly omits these elements when generating the Accessibility Tree. Screen readers never know they exist.
2. **`aria-hidden="true"`**: Does not alter visual CSS layout, but instructs the accessibility engine to hide the subtree from assistive technologies (ideal for decorative icons).
3. **`.sr-only` / `.visually-hidden`**: Collapses the element's visual footprint to a $1\times 1\text{px}$ invisible clipping box while keeping the node inside the accessibility tree.

---

## 3. How It Works Internally

### Deconstructing the Production `.sr-only` CSS Rule
The universal, battle-tested `.sr-only` CSS definition must be implemented exactly:

```css
.sr-only,
.visually-hidden {
  position: absolute;
  width: 1px;
  height: 1px;
  padding: 0;
  margin: -1px;
  overflow: hidden;
  clip: rect(0, 0, 0, 0);
  white-space: nowrap;
  border-width: 0;
}
```

### Property-by-Property Anatomical Breakdown
1. **`position: absolute;`**: Takes the element completely out of normal document flow so it occupies zero physical space and cannot disrupt sibling flex, grid, or text layout.
2. **`width: 1px; height: 1px;`**: Some screen readers (notably older VoiceOver versions) ignore elements with $0\times 0\text{px}$ dimensions, treating them as un-rendered. $1\times 1\text{px}$ ensures the node is recognized.
3. **`padding: 0; margin: -1px;`**: Strips default box margins and pulls the $1\text{px}$ box off-screen to avoid any sub-pixel rendering artifact.
4. **`overflow: hidden;`**: Prevents text inside the $1\times 1\text{px}$ box from overflowing or drawing text glyphs onto the canvas.
5. **`clip: rect(0, 0, 0, 0);`**: Legacy clipping rectangle that clips all 4 edges to zero pixels. Ensures universal backward compatibility with older browsers.
6. **`white-space: nowrap;`**: Prevents words from breaking onto multiple lines, ensuring screen readers pronounce phrases as continuous sentences.
7. **`border-width: 0;`**: Strips any default borders that could leak 1px colored dots.

---

## 4. Syntax & Usage

### 1. The Standard Utility Block
```css
/* Base Screen Reader Only Utility */
.sr-only {
  position: absolute;
  width: 1px;
  height: 1px;
  padding: 0;
  margin: -1px;
  overflow: hidden;
  clip: rect(0, 0, 0, 0);
  white-space: nowrap;
  border-width: 0;
}

/* Reversible variant: Becomes visible on keyboard focus */
.sr-only-focusable:focus,
.sr-only-focusable:focus-visible {
  position: static;
  width: auto;
  height: auto;
  padding: inherit;
  margin: inherit;
  overflow: visible;
  clip: auto;
  white-space: normal;
}
```

---

## 5. Practical Examples

### Example 1: Accessible "Skip to Main Content" Link (WCAG SC 2.4.1)
Keyboard users should not be forced to press `Tab` 40 times through navigation menus on every page refresh. A Skip Link sits hidden until focused, then pops into view:

```html
<body>
  <!-- First focusable element on the entire page -->
  <a href="#main-content" class="skip-link">
    Skip to main content
  </a>

  <header class="site-header">
    <nav aria-label="Global Navigation">
      <!-- 50 Navigation Links -->
      <a href="/products">Products</a>
      <a href="/pricing">Pricing</a>
    </nav>
  </header>

  <main id="main-content" tabindex="-1">
    <h1>Dashboard Overview</h1>
    <p>Welcome back, Shiva.</p>
  </main>
</body>
```

```css
.skip-link {
  position: absolute;
  top: -9999px;
  left: 1rem;
  z-index: 10000;
  background-color: #000000;
  color: #ffffff;
  padding: 0.75rem 1.5rem;
  font-weight: 700;
  text-decoration: none;
  border-radius: 0 0 0.5rem 0.5rem;
  box-shadow: 0 10px 15px -3px rgba(0, 0, 0, 0.3);
  transition: top 0.15s ease-out;
}

/* Reveal link when user presses Tab upon entering the page */
.skip-link:focus,
.skip-link:focus-visible {
  top: 0;
  outline: 3px solid #38bdf8;
  outline-offset: 2px;
}

/* Ensure destination container doesn't show an outline when focused programmatically */
#main-content:focus {
  outline: none;
}
```

### Example 2: Icon-Only Search Button with Accessible Label
Sighted users see a magnifying glass icon; screen readers announce "Submit Search Query":

```html
<form role="search" class="search-bar">
  <label for="site-search" class="sr-only">Search documentation</label>
  <input type="search" id="site-search" placeholder="Search docs..." class="search-input">
  
  <button type="submit" class="search-btn">
    <svg class="search-icon" viewBox="0 0 20 20" fill="currentColor" aria-hidden="true">
      <path fill-rule="evenodd" d="M8 4a4 4 0 100 8 4 4 0 000-8zM2 8a6 6 0 1110.89 3.476l4.817 4.817a1 1 0 01-1.414 1.414l-4.816-4.816A6 6 0 012 8z" clip-rule="evenodd"/>
    </svg>
    <span class="sr-only">Submit search query</span>
  </button>
</form>
```

```css
.search-btn {
  display: inline-flex;
  align-items: center;
  justify-content: center;
  width: 2.5rem;
  height: 2.5rem;
  background: #2563eb;
  color: #ffffff;
  border: none;
  border-radius: 0.375rem;
  cursor: pointer;
}

.search-icon {
  width: 1.25rem;
  height: 1.25rem;
}

.search-btn:focus-visible {
  outline: 2px solid #2563eb;
  outline-offset: 2px;
}
```

### Example 3: Disambiguating Generic "Read More" Links
Screen reader users often generate a list of all links on a page (`Insert + F7` in NVDA or `VO + U` in VoiceOver). A list of 10 identical "Read More" links is useless. Use `.sr-only` to provide context:

```html
<article class="news-card">
  <h3>PostgreSQL 17 Query Optimization</h3>
  <p>Learn how the new b-tree indexing features boost performance...</p>
  <a href="/posts/pg17-opt" class="card-link">
    Read more<span class="sr-only"> about PostgreSQL 17 Query Optimization</span> &rarr;
  </a>
</article>
```

```css
.card-link {
  color: #0284c7;
  font-weight: 600;
  text-decoration: underline;
}

.card-link:focus-visible {
  outline: 2px solid #0284c7;
  outline-offset: 2px;
}
```

### Example 4: Data Table Row Action Context
When multiple rows have an "Edit" button, sighted users know which item is being edited by the row it sits in. Screen readers need explicit row context:

```html
<table class="data-table">
  <thead>
    <tr>
      <th>User</th>
      <th>Role</th>
      <th>Actions</th>
    </tr>
  </thead>
  <tbody>
    <tr>
      <td>Sarah Connor</td>
      <td>Admin</td>
      <td>
        <button type="button" class="btn-action">
          Edit<span class="sr-only"> Sarah Connor</span>
        </button>
        <button type="button" class="btn-action btn-action--danger">
          Delete<span class="sr-only"> Sarah Connor</span>
        </button>
      </td>
    </tr>
  </tbody>
</table>
```

### Example 5: ARIA Live Region Visually Hidden Status Announcements
Announce dynamic status updates (like shopping cart additions) to screen readers without rendering visual alerts:

```html
<div class="sr-only" role="status" aria-live="polite" id="cart-announcer">
  Item successfully added to your shopping cart. Total items: 3.
</div>
```

---

## 6. Best Practices

1. **Use the Tested `.sr-only` Snippet**: Never invent your own hiding CSS using `opacity: 0` or `font-size: 0`. Use the canonical 8-line `.sr-only` snippet.
2. **Implement Skip Links as the Very First Body Child**: Place `<a href="#main" class="skip-link">` as the immediate first element in `<body>` so keyboard users encounter it on their first `Tab` press.
3. **Disambiguate Generic Link Text**: Supplement "Read More", "Learn More", and "Download" links with hidden context: `Read more<span class="sr-only"> about the Q3 financial report</span>`.
4. **Pair Decorative Icons with `aria-hidden="true"`**: If an icon is purely visual and supplemented by `.sr-only` text, always mark the `<svg>` or `<i>` tag with `aria-hidden="true"` to prevent screen readers from announcing random glyph titles.
5. **Make Skip Links Pop Out on Focus**: Ensure skip links transition into view with high-contrast styling when `:focus` or `:focus-visible` matches.
6. **Set `tabindex="-1"` on Skip Link Targets**: Anchor targets like `<main id="main" tabindex="-1">` allow keyboard focus to move cleanly to the destination without adding the container to the regular tab sequence.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Consequence | Correct Pattern |
| :--- | :--- | :--- |
| **Using `display: none` for screen reader text** | Node is removed from the Accessibility Tree; screen readers say nothing. | Use `.sr-only` with $1\times 1\text{px}$ clipping box. |
| **Using `font-size: 0` to hide text** | WebKit and Blink engines discard zero-sized text, causing silent skips. | Use `.sr-only` snippet. |
| **Using `visibility: hidden`** | Removes node from accessibility tree; fails accessibility tests. | Use `.sr-only` snippet. |
| **Using `left: -9999px` without `position: absolute`** | Expands the browser canvas by 10,000px, triggering giant horizontal scrollbars. | Always combine with `position: absolute` and prefer `clip`. |
| **Hiding interactive elements permanently with `.sr-only`** | Screen reader users can focus a button they cannot see, creating confusion. | Only hide non-interactive labels, OR use `.sr-only-focusable`. |
| **Forgetting `white-space: nowrap` in `.sr-only`** | $1\times 1\text{px}$ element causes text to wrap character-by-character, confusing phonetic pronunciation. | Include `white-space: nowrap;`. |

---

## 8. Performance, Accessibility & Browser Support

### Accessibility Tree Inspection in DevTools
Modern browsers let you directly inspect what screen readers perceive:
1. Open Chrome DevTools $\rightarrow$ Select the **Elements** panel.
2. In the right-hand sidebar tabs, click **Accessibility**.
3. Select an element on the page. DevTools displays the computed **Accessibility Tree** representation, including its Name, Role, and ARIA properties.
4. Verify that elements with `.sr-only` are listed in the Accessibility Tree, while elements with `display: none` are marked as `Ignored: Element is not displayed`.

---

## 9. Comparison / When to Use What

| Technique | Visual Display | Accessibility Tree | Focusable? | Primary Use Case |
| :--- | :--- | :--- | :--- | :--- |
| **`display: none`** | Hidden | Removed | No | Collapsed menus, inactive modal dialogs |
| **`visibility: hidden`** | Hidden (reserves space) | Removed | No | Temporarily invisible UI animations |
| **`aria-hidden="true"`** | Visible | Removed | No | Purely decorative SVG icons and visual glyphs |
| **`.sr-only`** | Hidden | Preserved | No (text only) | Screen reader helper text, disambiguation |
| **`.skip-link`** | Hidden until focused | Preserved | Yes | Bypass navigation to main content |

---

## 10. Interview Questions

### Q1: Why do `display: none` and `visibility: hidden` fail when attempting to provide helper text for screen readers?
<details>
<summary>Answer</summary>
The browser engine creates two distinct internal trees: the **DOM Tree** (HTML structure) and the **Accessibility Tree** (the semantic data fed to assistive technologies).

When a CSS rule sets `display: none` or `visibility: hidden`, the browser engine explicitly removes that element and its descendants from the Accessibility Tree. Because screen readers consume the Accessibility Tree rather than raw HTML markup, the screen reader has zero awareness of the hidden element, rendering it completely silent.
</details>

### Q2: Walk through the standard 8-line `.sr-only` snippet and explain the purpose of `clip: rect(0, 0, 0, 0)` and `white-space: nowrap`.
<details>
<summary>Answer</summary>
- **`clip: rect(0, 0, 0, 0)`**: Clips all 4 edges of the $1\times 1\text{px}$ box to zero pixels. Even though modern CSS offers `clip-path`, `clip: rect(0, 0, 0, 0)` remains the universal gold standard because it functions across all legacy browsers without creating stacking-context side effects.
- **`white-space: nowrap`**: Prevents words from breaking onto multiple lines. In a $1\times 1\text{px}$ container, the browser would normally attempt to wrap each word onto a new line, creating a box with 0px width and hundreds of stacked lines. In some screen readers, this causes each word to be pronounced with unnatural pauses or spelled out phonetically character-by-character. `white-space: nowrap` ensures text is read as a fluid sentence.
</details>

### Q3: What is a "Skip to Content" link, which WCAG Success Criterion does it satisfy, and how should it be styled in CSS?
<details>
<summary>Answer</summary>
A "Skip to Content" link is an internal navigational anchor placed at the very top of the DOM (the first focusable element inside `<body>`) pointing to `#main` or `#content`. It satisfies **WCAG 2.1 / 2.2 SC 2.4.1 Bypass Blocks (Level A)**.

It allows keyboard navigators and switch-device users to bypass repetitive header navigation links and jump directly to the primary page content.
In CSS:
1. It is hidden off-screen by default (`position: absolute; top: -9999px; left: 1rem;`).
2. When the user presses `Tab` upon landing on the page, the link receives focus, triggering `:focus` and `:focus-visible`.
3. The CSS immediately snaps it into view (`top: 0; outline: 3px solid #38bdf8; z-index: 10000;`) with high contrast and prominent padding.
</details>

### Q4: Why is using `font-size: 0` or `color: transparent` considered an accessibility anti-pattern for hiding text?
<details>
<summary>Answer</summary>
1. **`font-size: 0`**: In WebKit (Safari / iOS) and Chromium, layout engines optimize out text with zero font size, treating it as degenerate geometry and frequently omitting it from screen reader speech or reading it as individual broken letters.
2. **`color: transparent`**: Transparent text still occupies layout space in the document flow, pushing surrounding elements. Furthermore, if users enable high-contrast modes or custom user stylesheets, the transparency is stripped, causing unexpected visual text overlaps.
</details>

### Q5: What is the purpose of placing `tabindex="-1"` on the destination element of a skip link (e.g. `<main id="main-content" tabindex="-1">`)?
<details>
<summary>Answer</summary>
Historically, when a user clicked or activated an anchor link `<a href="#main-content">`, browsers scrolled the viewport to the target, but failed to move the internal keyboard focus ring. The next `Tab` press would jump back up to the second element in the header.

Setting `tabindex="-1"` on `<main>` allows the browser to programmatically shift keyboard focus to the main container when the skip link is clicked, ensuring that subsequent `Tab` key presses continue seamlessly from within the main content area.
</details>

### Q6: How does `aria-hidden="true"` differ from `.sr-only`, and when should they be used together?
<details>
<summary>Answer</summary>
They are functional opposites:
- **`aria-hidden="true"`**: Hides an element from the **Accessibility Tree** while keeping it **visually visible** on the screen.
- **`.sr-only`**: Hides an element **visually from the screen** while keeping it **announced in the Accessibility Tree**.

They are frequently used together when building accessible icon buttons:
```html
<button>
  <svg aria-hidden="true">...</svg> <!-- Sighted users see it; screen readers ignore it -->
  <span class="sr-only">Close Settings</span> <!-- Sighted users don't see it; screen readers announce it -->
</button>
```
</details>

### Q7: Why is `left: -9999px` without `position: absolute` dangerous in responsive layouts?
<details>
<summary>Answer</summary>
If `position: absolute` is omitted, the element remains in normal document flow. Shifting it `9999px` to the left causes the element's layout geometry to span 10,000 pixels across the viewport, forcing the browser to render a massive horizontal scrollbar and causing severe layout thrashing.
</details>

### Q8: How can a developer test whether visually hidden text is functioning correctly without installing a native screen reader?
<details>
<summary>Answer</summary>
1. **Chrome DevTools Accessibility Tree**: Inspect the element $\rightarrow$ Accessibility tab $\rightarrow$ Verify the node appears with its computed Accessible Name.
2. **DevTools Console**: Inspect the element's accessible name via `getComputedAccessibleName($0)`.
3. **Built-in Screen Readers**: Enable **Narrator** on Windows (`Win + Ctrl + Enter`) or **VoiceOver** on macOS (`Cmd + F5`) for 30 seconds to hear the synthesized speech output.
</details>

---

## 11. Practice Exercises

### Exercise 1: Build a Fully Accessible Skip-to-Content Link
**Objective**: Construct an accessible skip navigation link that stays hidden off-screen until focused via keyboard Tab, then animates down with high-contrast styling and proper target focus handling.

<details>
<summary>Solution</summary>

```html
<a href="#primary-content" class="skip-navigation">
  Skip to primary content
</a>

<header>
  <nav aria-label="Main">
    <a href="/home">Home</a>
    <a href="/about">About</a>
  </nav>
</header>

<main id="primary-content" tabindex="-1">
  <h1>Executive Summary</h1>
  <p>Main content starts here...</p>
</main>
```

```css
.skip-navigation {
  position: absolute;
  top: -100px;
  left: 1rem;
  z-index: 9999;
  background-color: #0f172a;
  color: #ffffff;
  padding: 0.75rem 1.25rem;
  font-weight: 700;
  font-size: 0.875rem;
  text-decoration: none;
  border-radius: 0 0 0.5rem 0.5rem;
  box-shadow: 0 4px 6px -1px rgba(0, 0, 0, 0.2);
  transition: top 0.2s cubic-bezier(0.16, 1, 0.3, 1);
}

.skip-navigation:focus,
.skip-navigation:focus-visible {
  top: 0;
  outline: 3px solid #38bdf8;
  outline-offset: 2px;
}

#primary-content:focus {
  outline: none;
}
```
</details>

### Exercise 2: Accessible Disambiguated Card Actions
**Objective**: Build a product card with multiple action buttons ("View", "Remove") that display simple labels visually, but announce full product titles to screen readers.

<details>
<summary>Solution</summary>

```html
<article class="product-item">
  <h3 class="product-title">Industrial Edge Router XR-400</h3>
  <div class="product-actions">
    <a href="/products/xr400" class="btn-link">
      View<span class="sr-only"> details for Industrial Edge Router XR-400</span>
    </a>
    <button type="button" class="btn-remove">
      Remove<span class="sr-only"> Industrial Edge Router XR-400 from inventory</span>
    </button>
  </div>
</article>
```

```css
.sr-only {
  position: absolute;
  width: 1px;
  height: 1px;
  padding: 0;
  margin: -1px;
  overflow: hidden;
  clip: rect(0, 0, 0, 0);
  white-space: nowrap;
  border-width: 0;
}

.product-item {
  border: 1px solid #e2e8f0;
  border-radius: 0.5rem;
  padding: 1.25rem;
  max-width: 380px;
}

.product-title {
  margin: 0 0 1rem;
  font-size: 1.125rem;
  color: #0f172a;
}

.product-actions {
  display: flex;
  gap: 0.75rem;
}

.btn-link, .btn-remove {
  padding: 0.5rem 1rem;
  font-size: 0.875rem;
  font-weight: 600;
  border-radius: 0.375rem;
  cursor: pointer;
  text-decoration: none;
}

.btn-link {
  background-color: #eff6ff;
  color: #1d4ed8;
  border: 1px solid #bfdbfe;
}

.btn-remove {
  background-color: #fef2f2;
  color: #b91c1c;
  border: 1px solid #fecaca;
}

.btn-link:focus-visible {
  outline: 2px solid #1d4ed8;
  outline-offset: 2px;
}

.btn-remove:focus-visible {
  outline: 2px solid #b91c1c;
  outline-offset: 2px;
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **`display: none` / `visibility: hidden`**: Hides from both monitor AND screen reader.
- **`aria-hidden="true"`**: Hides from screen reader, keeps visible on monitor (use on decorative icons).
- **`.sr-only`**: Hides visually from monitor, preserves announcement in screen reader.
- **The Golden 8-Line Pattern**:
  `position: absolute; width: 1px; height: 1px; padding: 0; margin: -1px; overflow: hidden; clip: rect(0, 0, 0, 0); white-space: nowrap; border-width: 0;`
- **Skip Links**: Must be first focusable child in `<body>`; reveals on `:focus-visible`.
- **Target `tabindex="-1"`**: Always put `tabindex="-1"` on the destination element of a skip link.
- **Disambiguate Links**: Add `<span class="sr-only"> context</span>` inside repeated "Read More" or "Delete" buttons.

---

## 13. Further Reading
- [W3C WCAG 2.2: Understanding Success Criterion 2.4.1 Bypass Blocks](https://www.w3.org/WAI/WCAG22/Understanding/bypass-blocks.html)
- [WebAIM: CSS in Action - Invisible Content Just for Screen Reader Users](https://webaim.org/techniques/css/invisiblecontent/)
- [The A11y Project: How-to Hide Content](https://www.a11yproject.com/posts/how-to-hide-content/)
- [Scott O'Hara: Inclusively Hidden](https://www.scottohara.me/blog/2017/04/14/inclusively-hidden.html)
- [Chrome DevTools: Full Accessibility Tree View](https://developer.chrome.com/docs/devtools/accessibility/reference/)

---

| Previous Note | Next Note |
| :--- | :--- |
| [06. Touch Targets and Hit Areas](06-touch-targets-and-hit-areas.md) | [08. Readable Line Length and Spacing](08-readable-line-length-and-spacing.md) |
