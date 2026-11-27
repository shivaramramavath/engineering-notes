# Meta Charset, Viewport Scaling, and Color Scheme Architecture

| Metadata | Details |
| :--- | :--- |
| **Specification** | WHATWG HTML Living Standard (§4.2.5 The `meta` element) / CSS Color Adjustment Level 1 |
| **Prerequisites** | `05-head-metadata-and-seo/01-head-anatomy.md`, `01-fundamentals/07-character-encoding-and-entities.md` |
| **Estimated Reading Time** | 25 minutes |
| **Status** | Production Ready |
| **Baseline Status** | Widely Available (`charset`, `viewport`, `color-scheme`, `theme-color` with media queries) |
| **Browsers Tested** | Chrome 124+, Firefox 125+, Safari 17.4+, Edge 124+ |
| **Tested With** | W3C Nu HTML Validator, iOS Safari notch simulator, Android Chrome virtual keyboard |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. [Practical Examples](#5-practical-examples)
   - [Example 1: The Canonical UTF-8 and Responsive Viewport Baseline](#example-1-the-canonical-utf-8-and-responsive-viewport-baseline)
   - [Example 2: Notch and Safe Area Handling with `viewport-fit=cover`](#example-2-notch-and-safe-area-handling-with-viewport-fitcover)
   - [Example 3: Mobile Virtual Keyboard Adaptation with `interactive-widget`](#example-3-mobile-virtual-keyboard-adaptation-with-interactive-widget)
   - [Example 4: Complete Light/Dark Mode Theming via `color-scheme` and `theme-color`](#example-4-complete-lightdark-mode-theming-via-color-scheme-and-theme-color)
   - [Example 5: Preventing Zoom Lockout Accessibility Failures](#example-5-preventing-zoom-lockout-accessibility-failures)
6. [Best Practices](#6-best-practices)
7. [Common Mistakes & Pitfalls](#7-common-mistakes--pitfalls)
8. [Performance, Accessibility, SEO & Security Notes](#8-performance-accessibility-seo--security-notes)
9. [Comparison / When to Use What](#9-comparison--when-to-use-what)
10. [Interview Questions](#10-interview-questions)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary / Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

Before modern standards, browsing the web on mobile devices was an exercise in frustration: mobile browsers assumed every website was built for a 980-pixel desktop monitor, rendering tiny unreadable text that required double-tapping and pinch-to-zoom to read. Meanwhile, mismatched character encodings generated corrupt text symbols (the infamous "mojibake"), and dark mode users were blinded by sudden pure-white flashes before stylesheets loaded.

Three foundational `<meta>` directives resolve these platform challenges:
1. **`<meta charset="utf-8">`**: Unifies global character encoding, guaranteeing universal support for all human languages, mathematical symbols, and emoji without byte corruption.
2. **`<meta name="viewport">`**: Bridges the gap between physical mobile hardware pixels and CSS layout pixels, establishing responsive layouts, handling display cutouts (notches), and configuring virtual keyboard behavior.
3. **`<meta name="color-scheme">` and `<meta name="theme-color">`**: Informs the browser engine of dark mode capabilities before CSS evaluates, rendering native controls (scrollbars, form inputs) in the user's preferred scheme and coloring the mobile browser shell.

---

## 2. Core Concepts

### Character Encoding: The 1024-Byte Rule
A web browser receives an incoming HTML file as a raw binary byte stream. To translate bytes into characters (e.g., `0xE2 0x82 0xAC` &rarr; `€`), it must determine the encoding algorithm. 
- The WHATWG specification establishes **UTF-8** as the mandatory default encoding of the web.
- The `<meta charset="utf-8">` declaration must be placed completely within the **first 1024 bytes** of the HTML document.
- If omitted or placed too late, the browser might guess an incorrect legacy encoding (like Windows-1252), and encountering an unexpected byte later will force a costly document buffer reset and re-parse.

### The Mobile Viewport Architecture
By default, mobile browser engines (Safari on iOS, Chrome on Android) render pages inside a virtual "layout viewport" typically 980px wide, and scale the result down to fit the physical screen.
```html
<meta name="viewport" content="width=device-width, initial-scale=1">
```
- **`width=device-width`**: Sets the width of the layout viewport to the physical device-independent pixel width of the device screen (e.g., 390px on an iPhone, 412px on a Google Pixel).
- **`initial-scale=1`**: Establishes a 1:1 relationship between CSS pixels and device-independent pixels, preventing artificial initial zoom.

### The Modern Viewport Extensions
- **`viewport-fit=cover`**: Expands the viewport into the device's physical display cutouts (notches and dynamic islands), unlocking the CSS `env(safe-area-inset-*)` variables.
- **`interactive-widget`** (Baseline 2023): Controls how the mobile virtual keyboard affects page layout:
  - `resizes-content`: Resizes the visual and layout viewports when the on-screen keyboard opens (default in Chrome).
  - `overlays-content`: Keeps the layout viewport fixed and overlays the virtual keyboard on top (default in Safari).

---

## 3. How It Works Internally

```
+-----------------------------------------------------------------------------------------+
|                  Mobile Viewport & Layout Viewport Translation                          |
+-----------------------------------------------------------------------------------------+

  Scenario A: No Viewport Tag (Legacy Desktop Simulation)
  +---------------------------------------------------------------+
  | Virtual 980px Canvas                                          |
  | [ Content rendered tiny ]                                     |
  +---------------------------------------------------------------+
         | (Scaled down by 0.39x)
         v
  Physical Screen (390px) -> User MUST pinch-to-zoom to read!

-------------------------------------------------------------------------------------------

  Scenario B: <meta name="viewport" content="width=device-width, initial-scale=1">
  +-------------------------------+
  | CSS Layout Viewport: 390px    |
  | 100vw = 390px                 |
  | Font sizes render accurately  |
  +-------------------------------+
         | (1:1 Direct Mapping)
         v
  Physical Screen (390px) -> Crisp, legible responsive layout immediately!
```

---

## 4. Syntax & Usage

```html
<head>
  <!-- 1. Universal Encoding -->
  <meta charset="utf-8" />

  <!-- 2. Responsive Mobile Viewport with Safe Area Support -->
  <meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover" />

  <!-- 3. Pre-render Native Dark Mode Support -->
  <meta name="color-scheme" content="light dark" />

  <!-- 4. Dynamic Mobile Browser Address Bar Tinting -->
  <meta name="theme-color" media="(prefers-color-scheme: light)" content="#ffffff" />
  <meta name="theme-color" media="(prefers-color-scheme: dark)" content="#0f172a" />
</head>
```

> **Cross-Browser & Baseline Status Box**
>
> - `<meta charset="utf-8">`: **Baseline Widely Available** since HTML5.
> - `viewport-fit=cover`: **Baseline Widely Available** across iOS Safari and Android Chrome.
> - `color-scheme` in meta: **Baseline Widely Available** (Chrome 81+, Safari 13.1+, Firefox 96+).
> - `interactive-widget`: **Baseline Newly Available (2023)** across Chrome 108+, Safari 17.0+, Edge 108+.

---

## 5. Practical Examples

### Example 1: The Canonical UTF-8 and Responsive Viewport Baseline
The essential, immutable starting configuration for any professional web application.

```html
<!DOCTYPE html>
<html lang="en">
<head>
  <!-- MUST be declared in the first 1024 bytes -->
  <meta charset="utf-8" />
  
  <!-- Establishes true 1:1 responsive viewport scaling -->
  <meta name="viewport" content="width=device-width, initial-scale=1" />

  <title>Responsive Architecture Baseline</title>
</head>
<body>
  <h1>Page renders sharply across all devices</h1>
</body>
</html>
```

---

### Example 2: Notch and Safe Area Handling with `viewport-fit=cover`
Ensuring full-bleed background designs extend behind phone notches while preventing text from being clipped by hardware borders.

```html
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8" />
  <!-- viewport-fit=cover extends layout into physical notches and home bar -->
  <meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover" />
  <title>Edge-to-Edge Mobile Layout</title>
</head>
<body>
  <header class="app-bar">
    <h2>Mobile Secure Portal</h2>
  </header>

  <main class="page-content">
    <p>Main content area protected by safe area insets.</p>
  </main>

  <style>
    /* Full-bleed header extends into the physical top notch */
    .app-bar {
      background-color: #0284c7;
      color: #ffffff;
      /* Add safe area padding to prevent text from being hidden behind notch/camera */
      padding-top: calc(1rem + env(safe-area-inset-top));
      padding-left: calc(1rem + env(safe-area-inset-left));
      padding-right: calc(1rem + env(safe-area-inset-right));
      padding-bottom: 1rem;
    }

    /* Bottom padding protects navigation from home indicator bar */
    .page-content {
      padding: 1rem;
      padding-bottom: calc(2rem + env(safe-area-inset-bottom));
    }
  </style>
</body>
</html>
```

---

### Example 3: Mobile Virtual Keyboard Adaptation with `interactive-widget`
Preventing mobile on-screen keyboards from distorting fixed-position bottom navigation bars.

```html
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8" />
  <!-- 
    interactive-widget=overlays-content:
    When the virtual keyboard pops up, the layout viewport does NOT resize;
    the keyboard floats over the content instead of compressing fixed UI elements.
  -->
  <meta 
    name="viewport" 
    content="width=device-width, initial-scale=1, interactive-widget=overlays-content" 
  />
  <title>Chat Application Interface</title>
</head>
<body>
  <div class="chat-container">
    <div class="messages">Chat messages scroll here...</div>

    <form class="chat-input-bar">
      <input type="text" placeholder="Type a message..." />
      <button type="submit">Send</button>
    </form>
  </div>
</body>
</html>
```

---

### Example 4: Complete Light/Dark Mode Theming via `color-scheme` and `theme-color`
Eliminating the dreaded "white flash" when loading dark-mode pages and matching the mobile browser address bar color to the document theme.

```html
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1" />
  
  <!-- 
    Informs browser to render default background, scrollbars, and form controls
    in dark mode IMMEDIATELY before external CSS is downloaded. Eliminates white flashes!
  -->
  <meta name="color-scheme" content="light dark" />

  <!-- Colors the mobile browser URL bar to match the brand palette in light mode -->
  <meta name="theme-color" media="(prefers-color-scheme: light)" content="#f8fafc" />

  <!-- Colors the mobile browser URL bar in dark mode -->
  <meta name="theme-color" media="(prefers-color-scheme: dark)" content="#0f172a" />

  <title>Theme Aware Architecture</title>
</head>
<body>
  <main>
    <h1>Seamless Dark Mode Support</h1>
    <input type="text" placeholder="Native dark input control" />
  </main>

  <style>
    :root {
      color-scheme: light dark;
      --bg: #f8fafc;
      --text: #0f172a;
    }
    @media (prefers-color-scheme: dark) {
      :root {
        --bg: #0f172a;
        --text: #f8fafc;
      }
    }
    body {
      background-color: var(--bg);
      color: var(--text);
      font-family: sans-serif;
    }
  </style>
</body>
</html>
```

---

### Example 5: Preventing Zoom Lockout Accessibility Failures
Demonstrating the difference between a compliant, accessible viewport and an illegal zoom-locked viewport.

```html
<!-- DANGEROUS WCAG 1.4.4 VIOLATION -->
<!-- 
  DO NOT USE:
  user-scalable=no, maximum-scale=1
  Blocks visually impaired users from zooming into text!
-->
<!-- <meta name="viewport" content="width=device-width, initial-scale=1, user-scalable=no, maximum-scale=1"> -->

<!-- COMPLIANT PRODUCTION PATTERN: Allows users to pinch-zoom freely -->
<meta name="viewport" content="width=device-width, initial-scale=1" />
```

---

## 6. Best Practices

- [ ] **Always declare `<meta charset="utf-8">` as the first tag inside `<head>`**: Guarantees placement within the first 1024 bytes and prevents document re-parse cycles.
- [ ] **Always declare `<meta name="viewport" content="width=device-width, initial-scale=1">`**: Mandatory baseline for all responsive design.
- [ ] **NEVER disable user zoom with `user-scalable=no` or `maximum-scale=1`**: Violates WCAG 2.2 Criterion 1.4.4 (Resize Text) and severely impairs low-vision users.
- [ ] **Declare `<meta name="color-scheme" content="light dark">`**: Instructs the browser to render native scrollbars, checkmarks, inputs, and canvas backgrounds in dark mode before CSS downloads.
- [ ] **Pair `<meta name="theme-color">` with `media` queries**: Ensures the mobile browser chrome (address bar / status bar) matches both light and dark system color themes.
- [ ] **Use `viewport-fit=cover` in conjunction with `env(safe-area-inset-*)`**: Never use `viewport-fit=cover` without adding safe area padding in CSS, or critical interactive buttons will be hidden behind hardware notches.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Severity | Consequence | Correction |
| :--- | :--- | :--- | :--- |
| Disabling zoom (`user-scalable=no`) | **Critical** | Low-vision users cannot zoom in to read text; fails WCAG 1.4.4. | Remove `user-scalable=no` and `maximum-scale=1`. |
| Placing `<meta charset>` after 1024 bytes | **High** | Browser may guess wrong encoding and restart document parsing from scratch. | Place `<meta charset="utf-8">` immediately after `<head>`. |
| Omitting `<meta name="viewport">` | **Critical** | Mobile devices render a tiny 980px desktop view; responsive media queries fail. | Add standard `width=device-width, initial-scale=1`. |
| Setting `viewport-fit=cover` without CSS safe areas | **High** | Interactive buttons and text clip behind phone speaker notches and home indicators. | Add CSS `padding: env(safe-area-inset-top)` to containers. |
| Hardcoding single `theme-color` for dark mode sites | **Moderate** | Mobile browser URL bar stays blinding white on dark mode devices. | Use `<meta name="theme-color" media="(prefers-color-scheme: dark)">`. |
| Using legacy `<meta http-equiv="Content-Type" ...>` | **Low** | Verbose obsolete syntax (`text/html; charset=UTF-8`). | Use concise modern syntax: `<meta charset="utf-8">`. |

---

## 8. Performance, Accessibility, SEO & Security Notes

### Accessibility (WCAG 2.2 AA)
- **Guideline 1.4.4 Resize Text (Level AA)**: Content must be capable of being scaled up to 200% without loss of content or functionality. Setting `user-scalable=no` or `maximum-scale=1` directly fails this criterion. Modern iOS Safari and Chrome explicitly ignore `user-scalable=no` by default to protect accessibility, but it should never be authored.

### Core Web Vitals
- **Cumulative Layout Shift (CLS)**: Omitting the viewport meta tag causes mobile devices to compute layout at 980px and then snap or re-render, creating massive layout instability.
- **First Contentful Paint (FCP)**: Declaring `<meta name="color-scheme" content="dark">` avoids the brief white canvas paint that normally occurs before dark mode CSS stylesheets are parsed.

---

## 9. Comparison / When to Use What

| Directive / Tag | Location | Purpose | When to Use |
| :--- | :--- | :--- | :--- |
| **`charset="utf-8"`** | First line of `<head>` | Byte decoding algorithm | **Mandatory on every HTML page.** |
| **`viewport` (standard)** | Second line of `<head>` | 1:1 mobile pixel scaling | **Mandatory on every responsive page.** |
| **`viewport-fit=cover`** | Inside `viewport` meta | Unlocks notch and safe areas | Mobile apps, PWAs, full-bleed hero designs. |
| **`interactive-widget`** | Inside `viewport` meta | Controls virtual keyboard resize | Chat apps, forms with sticky footers. |
| **`color-scheme`** | Top of `<head>` | Pre-CSS dark mode signals | Any website supporting dark mode. |
| **`theme-color`** | Top of `<head>` | Mobile browser address bar tint | Mobile branding and PWA polish. |

---

## 10. Interview Questions

<details>
<summary><strong>1. Why is <code>user-scalable=no</code> considered an accessibility anti-pattern and a WCAG failure?</strong></summary>

WCAG 2.2 Criterion 1.4.4 (Resize Text) requires that text can be enlarged to at least 200% without assistive technology and without loss of content or functionality. Setting `user-scalable=no` or `maximum-scale=1.0` attempts to disable the native pinch-to-zoom feature on mobile browsers, stranding low-vision users who cannot read small font sizes. Because of this severe accessibility impact, modern mobile browsers (including iOS Safari) explicitly override and ignore `user-scalable=no`.
</details>

<details>
<summary><strong>2. What does <code>width=device-width</code> in the viewport meta tag actually accomplish?</strong></summary>

Without this directive, mobile browsers assume they are rendering a legacy desktop page, setting their internal layout viewport to a default width of 980 pixels and shrinking the entire page to fit the screen. Declaring `width=device-width` instructs the browser to set the layout viewport to the device's actual device-independent pixel width (e.g., 390px), ensuring that CSS media queries (e.g., `@media (max-width: 600px)`) evaluate accurately.
</details>

<details>
<summary><strong>3. What is the difference between physical hardware pixels and CSS device-independent pixels?</strong></summary>

Physical pixels are the microscopic physical light-emitting diodes on a screen (e.g., a modern smartphone may have 1170 physical horizontal pixels). CSS device-independent pixels (or points) are an abstract coordinate unit normalized for human viewing distance (e.g., 390 CSS pixels). The relationship between them is defined by the Device Pixel Ratio:
$$\text{DPR} = \frac{\text{Physical Hardware Pixels}}{\text{CSS Device-Independent Pixels}} = \frac{1170}{390} = 3.0$$
</details>

<details>
<summary><strong>4. How does <code>viewport-fit=cover</code> work in tandem with CSS <code>env(safe-area-inset-*)</code>?</strong></summary>

On devices with screen cutouts (such as iPhone notches or camera punch-holes), mobile browsers default to `viewport-fit=auto`, which places white or black letterbox bars around the viewport to keep content away from the notch. Setting `viewport-fit=cover` forces the viewport to expand across the entire physical display surface. To prevent content from being clipped by the notch or rounded corners, developers must use the CSS environment variables: `env(safe-area-inset-top)`, `env(safe-area-inset-bottom)`, `env(safe-area-inset-left)`, and `env(safe-area-inset-right)`.
</details>

<details>
<summary><strong>5. Why is declaring <code>&lt;meta name="color-scheme" content="light dark"&gt;</code> superior to relying purely on CSS media queries?</strong></summary>

CSS media queries (`@media (prefers-color-scheme: dark)`) only take effect after the external CSS stylesheet has been fully downloaded, parsed, and applied. Before that happens, the browser engine renders an initial canvas background and native form controls using its hardcoded default (typically pure white). Placing `<meta name="color-scheme" content="light dark">` in the HTML `<head>` signals the browser engine *before* stylesheets load, eliminating blinding white flashes and rendering native scrollbars in dark mode instantly.
</details>

<details>
<summary><strong>6. What problem does the modern <code>interactive-widget</code> viewport value solve?</strong></summary>

When a user taps an `<input>` on a mobile device and the on-screen virtual keyboard opens, different browser engines handled layout changes inconsistently: Chromium resized the layout viewport (compressing fixed UI footers), while WebKit overlaid the keyboard on top (hiding inputs). `interactive-widget=resizes-content` explicitly forces the layout to shrink, while `interactive-widget=overlays-content` keeps the layout size fixed, providing predictable cross-browser virtual keyboard behavior.
</details>

<details>
<summary><strong>7. How does the browser handle multiple <code>&lt;meta name="theme-color"&gt;</code> tags?</strong></summary>

Browsers evaluate `media` attributes on `<meta name="theme-color">` elements against the user's current environment. For example:
```html
<meta name="theme-color" media="(prefers-color-scheme: light)" content="#ffffff">
<meta name="theme-color" media="(prefers-color-scheme: dark)" content="#111827">
```
When the user toggles dark mode, the browser matches the corresponding media query and updates the mobile address bar and status bar color dynamically.
</details>

<details>
<summary><strong>8. What is the consequence if the character encoding is not declared within the first 1024 bytes?</strong></summary>

The browser may begin tokenizing the document using a guessed default encoding (e.g., Windows-1252). If it later encounters a character or declaration requiring UTF-8 beyond the 1024-byte mark, the HTML parser is forced to dump its current token tree, reset its internal character stream decoder, and re-parse the entire document from byte 0, causing measurable CPU and rendering latency.
</details>

---

## 11. Practice Exercises

### Exercise 1: Canonical Base Head Setup (Easy)
- **Task**: Author an HTML `<head>` containing `<meta charset="utf-8">`, standard mobile viewport scaling, and an accessible `<title>`. Ensure all elements are in the correct sequence.

<details>
<summary>View Solution</summary>

```html
<head>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1" />
  <title>Acme Global Solutions</title>
</head>
```
</details>

---

### Exercise 2: Safe-Area Ready Mobile Web App Head (Medium)
- **Task**: Create a `<head>` configuration for a mobile web application that:
  1. Expands into device notches (`viewport-fit=cover`).
  2. Declares native dark and light mode support (`color-scheme`).
  3. Tints the mobile browser status bar to `#ffffff` in light mode and `#1e293b` in dark mode.

<details>
<summary>View Solution</summary>

```html
<head>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover" />
  
  <meta name="color-scheme" content="light dark" />
  <meta name="theme-color" media="(prefers-color-scheme: light)" content="#ffffff" />
  <meta name="theme-color" media="(prefers-color-scheme: dark)" content="#1e293b" />

  <title>Mobile Web Application</title>
</head>
```
</details>

---

### Exercise 3: Viewport Refactoring and Accessibility Repair (Hard)
- **Task**: An audit of an existing enterprise portal reveals the following line:
  ```html
  <meta name="viewport" content="width=640, initial-scale=0.5, user-scalable=no, maximum-scale=1">
  ```
  Identify all 4 major flaws and provide the modern, accessible, responsive replacement markup.

<details>
<summary>View Solution</summary>

**Flaws Identified:**
1. **Hardcoded Width (`width=640`)**: Forces fixed 640px layout, breaking responsive CSS on modern phones (360px–430px).
2. **Artificial Zoom (`initial-scale=0.5`)**: Shrinks the initial display to half-size, making text illegible.
3. **Zoom Disabled (`user-scalable=no`)**: Fails WCAG 1.4.4; visually impaired users cannot zoom in.
4. **Capped Scale (`maximum-scale=1`)**: Restricts scaling, locking out accessibility zoom tools.

**Corrected Modern Markup:**
```html
<meta name="viewport" content="width=device-width, initial-scale=1" />
```
</details>

---

## 12. Summary / Cheat Sheet

```
+-------------------------------------------------------------------------------------------------+
|                                 META DIRECTIVES QUICK REFERENCE                                 |
+-------------------------------------------------------------------------------------------------+
| Directive / Tag          | Purpose                                | Best Practice Value         |
+-------------------------------------------------------------------------------------------------+
| <meta charset="utf-8">   | Global character encoding              | Place in first 1024 bytes   |
| width=device-width       | Normalizes layout viewport to screen   | Standard baseline           |
| initial-scale=1          | 1:1 pixel coordinate mapping           | Standard baseline           |
| viewport-fit=cover       | Extends layout into phone notches      | Pair with env(safe-area-*)  |
| interactive-widget       | Manages virtual keyboard layout        | overlays-content / resizes  |
| color-scheme="light dark"| Eliminates white flash in dark mode    | Place at top of <head>      |
| theme-color              | Tints mobile browser URL bar           | Pair with media queries     |
| user-scalable=no         | NEVER USE (Fails WCAG 1.4.4)           | Omit completely             |
+-------------------------------------------------------------------------------------------------+
```

---

## 13. Further Reading

- [WHATWG HTML Living Standard: Standard metadata names (viewport, theme-color)](https://html.spec.whatwg.org/multipage/semantics.html#standard-metadata-names)
- [MDN Web Docs: Viewport meta tag](https://developer.mozilla.org/en-US/docs/Web/HTML/Viewport_meta_tag)
- [W3C CSS Color Adjustment: `color-scheme`](https://www.w3.org/TR/css-color-adjust-1/#color-scheme-prop)
- [web.dev: Responsive web design basics](https://web.dev/learn/design/responsive-design)

---

[← Previous: Head Anatomy](01-head-anatomy.md) | [Section Index](README.md) | [Next: Title, Description & Canonical →](03-title-description-and-canonical.md)
