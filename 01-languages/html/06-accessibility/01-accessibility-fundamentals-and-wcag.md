# Accessibility Fundamentals & WCAG 2.2

| Metadata | Details |
|:---|:---|
| **Level** | Foundational to Advanced |
| **Prerequisites** | `01-fundamentals/01-document-structure.md`, `02-semantics-and-structure/01-semantic-html-principles.md` |
| **Estimated Reading Time** | 28 minutes |
| **Status** | Current |
| **Baseline Status** | Widely Available (Universal Web Standards & W3C Recommendations) |
| **Browsers Tested** | Chrome 120+, Firefox 121+, Safari 17.2+, Edge 120+ |
| **Tested With** | NVDA 2024.1, JAWS 2024, VoiceOver (macOS Sonoma / iOS 17), axe-core 4.8 |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. [Practical Examples](#5-practical-examples)
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

### What is Digital Accessibility (a11y)?
Digital Accessibility (shortened as **a11y**, representing the letter *a* followed by 11 characters and ending in *y*) is the discipline of engineering digital products so that they can be perceived, understood, navigated, and interacted with by everyone, including people with auditory, cognitive, neurological, physical, speech, and visual disabilities. 

In HTML engineering, accessibility is not a decorative veneer or an external plugin. The web platform was architected from inception to be fundamentally accessible by default. When an author writes native, standards-compliant HTML elements, the web browser automatically maps those elements into system-level accessibility APIs exposed by the host operating system.

### Why WCAG 2.2 Matters
The **Web Content Accessibility Guidelines (WCAG)**, published by the World Wide Web Consortium (W3C) Web Accessibility Initiative (WAI), represents the globally accepted technical benchmark for digital accessibility. On **October 5, 2023**, the W3C published **WCAG 2.2** as an official W3C Recommendation.

WCAG 2.2 directly informs international legal standards:
- **United States**: Americans with Disabilities Act (ADA Title II / Title III) and Section 508 of the Rehabilitation Act.
- **European Union**: European Accessibility Act (EAA - Directive 2019/882) and EN 301 549, mandating WCAG compliance for private and public sector e-commerce, banking, and digital services.
- **United Kingdom**: Public Sector Bodies Accessibility Regulations 2018 and Equality Act 2010.

Building accessible HTML is not merely a legal hedge against crippling civil litigation; it directly expands total addressable market (TAM), improves search engine crawlability, reduces customer bounce rates, and ensures that older adults and individuals facing situational impairments (e.g., bright sunlight on mobile displays, broken input hardware, temporary motor injuries) retain seamless access to essential digital infrastructure.

---

## 2. Core Concepts

### The Four Principles: POUR
WCAG is organized hierarchically into 4 top-level principles known by the acronym **POUR**:

```
+---------------------------------------------------------------+
|                    WCAG 2.2 ARCHITECTURE                      |
+---------------------------------------------------------------+
| 1. PERCEIVABLE    | Information cannot be invisible to all     |
|                   | user senses (Visual, Audio, Tactile).      |
+-------------------+-------------------------------------------+
| 2. OPERABLE       | Interface controls must be operable via   |
|                   | any modality (Keyboard, Voice, Switch).   |
+-------------------+-------------------------------------------+
| 3. UNDERSTANDABLE | Content and operational flows must be     |
|                   | predictable, readable, and forgiving.     |
+-------------------+-------------------------------------------+
| 4. ROBUST         | Markup must parse cleanly across assistive|
|                   | tech, legacy browsers, and modern user agents|
+---------------------------------------------------------------+
```

### Conformance Levels: A, AA, and AAA
WCAG structures individual Success Criteria across three progressive conformance tiers:
1. **Level A (Minimum Baseline)**: Essential requirements without which certain user groups find it completely impossible to access digital content (e.g., non-text content having text alternatives, keyboard accessibility, no keyboard traps).
2. **Level AA (Industry Standard Benchmark)**: Removes significant barriers for the majority of individuals with disabilities. Level AA is the universal standard required by corporate compliance, international legislation, and enterprise contracts (e.g., 4.5:1 text contrast, visible focus indicators, bypass blocks, target size constraints).
3. **Level AAA (Specialized Enhancement)**: The highest standard of accessibility. While individual criteria are highly valuable, the W3C explicitly notes that it is not recommended that Level AAA compliance be required as a blanket policy for entire websites, as some content cannot satisfy all AAA requirements.

### Key Additions in WCAG 2.2
WCAG 2.2 introduced 9 new success criteria and officially deprecated one legacy criterion (4.1.1 Parsing, which was declared obsolete because modern WHATWG HTML5 parsers natively handle error recovery and malformed markup):

1. **2.4.11 Focus Not Obscured (Minimum) (AA)**: When a UI component receives keyboard focus, it must not be entirely hidden behind author-created floating banners, sticky footers, or non-modal drawers.
2. **2.4.12 Focus Not Obscured (Enhanced) (AAA)**: When focused, no part of the component may be hidden by author overlays.
3. **2.4.13 Focus Appearance (AAA)**: Prescribes explicit pixel area, contrast (3:1), and perimeter bounds for keyboard focus rings.
4. **2.5.7 Dragging Movements (AA)**: Any action relying on dragging (e.g., drag-and-drop sortable lists, sliders) must offer an equivalent single-pointer alternative (e.g., move-up/down buttons).
5. **2.5.8 Target Size (Minimum) (AA)**: Interactive targets must have an area of at least 24x24 CSS pixels, or have sufficient spacing to an adjacent target so that a 24px diameter circle centered on the target does not intersect another target.
6. **3.2.6 Consistent Help (A)**: Help mechanisms (contact info, live chat, FAQ links) must appear in the exact same relative order across multiple pages.
7. **3.3.7 Redundant Entry (A)**: Information previously entered by a user within a multi-step checkout or application flow must be auto-populated or available for selection rather than forcing re-typing.
8. **3.3.8 Accessible Authentication (Minimum) (AA)**: Authentication flows must not rely on cognitive function tests (e.g., memorizing passwords, solving math puzzles, transcribing CAPTCHAs) unless an alternative or copy-paste support is provided.
9. **3.3.9 Accessible Authentication (Enhanced) (AAA)**: Stricter version eliminating cognitive tests completely, even object recognition.

---

## 3. How It Works Internally

### The Dual-Tree Engine Architecture
When a browser engine renders a web document, it does not communicate the Raw DOM directly to assistive technologies. Instead, the browser engine (Blink in Chromium, Gecko in Firefox, WebKit in Safari) maintains two parallel internal trees:
1. **The DOM Tree (Document Object Model)**: The programmatic hierarchy of all HTML element nodes, attribute nodes, and text nodes used by the JavaScript engine and CSS styling engine.
2. **The Accessibility Tree (AOM / Platform Accessibility APIs)**: A specialized, pruned representation of semantic nodes containing only the elements that have meaning to assistive software.

```
       HTML Network Stream (Bytes)
                  │
                  ▼
         WHATWG HTML Parser
                  │
                  ▼
         DOM Tree Constructed
                  │
        ┌─────────┴─────────┐
        ▼                   ▼
    CSSOM Tree       Accessibility Engine
        │             (Evaluates native semantics,
        │              AccName calculation, ARIA)
        ▼                   ▼
   Render Tree      Accessibility Tree (AOM)
   (Layout & Style)         │
        │                   ▼
        ▼         OS Accessibility APIs
   Raster & Paint     (Windows: UIA / IAccessible2)
        │             (macOS: NSAccessibility)
        ▼             (Linux: ATK / AT-SPI)
   GPU Display Pixels (iOS: UIAccessibility)
                            │
                            ▼
                    Screen Readers / AT
                 (NVDA, JAWS, VoiceOver)
```

### Operating System Accessibility Bridges
The browser acts as an intermediary translation layer. The DOM element `<button>` is mapped by the browser to native operating system accessibility primitives:
- On **Windows**: Exposed via Microsoft UI Automation (UIA) and IAccessible2.
- On **macOS**: Exposed via `NSAccessibility` protocol (`NSAccessibilityButtonRole`).
- On **Linux**: Exposed via AT-SPI (Assistive Technology Service Provider Interface) and ATK.
- On **iOS / iPadOS**: Exposed via `UIAccessibilityElement`.
- On **Android**: Exposed via Android Accessibility Framework (`AccessibilityNodeInfo`).

When an author uses non-semantic markup such as `<div onclick="...">`, the browser creates a generic node in the DOM. Because a `<div>` has no native semantic role, keyboard tab stop, or accessible action mapping, the accessibility tree marks the node as `generic` or completely prunes it from the tree. The screen reader user is left completely unaware that an interactive element exists on the screen.

---

## 4. Syntax & Usage

### Baseline Conformance Syntax Matrix

```
+-----------------------------------------------------------------------+
| FEATURE / CRITERION         | BASELINE STATUS | AT SUPPORT (2024-2026)|
+-----------------------------+-----------------+-----------------------+
| Native Landmarks (<main>)   | Widely Available| 100% (NVDA/JAWS/VO)   |
| Accessible Form Labels      | Widely Available| 100% (Universal)      |
| Focus Ring (:focus-visible) | Widely Available| 100% (All Engines)    |
| Target Size (24x24px min)   | Widely Available| CSS Layout Dependent  |
| No Redundant Entry (Forms)  | Widely Available| HTML Autocomplete Spec|
+-----------------------------------------------------------------------+
```

### The Semantic Foundation Pattern
To satisfy WCAG 2.2 AA out of the box, every HTML document must implement the foundational semantic contract:

```html
<!DOCTYPE html>
<html lang="en" dir="ltr">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Accessible Document Foundation — Enterprise Portal</title>
</head>
<body>
  <!-- Bypass Block: WCAG 2.4.1 (Level A) -->
  <a href="#main-content" class="skip-link">Skip to main content</a>

  <!-- Header Landmark: WCAG 1.3.1 (Level A) -->
  <header>
    <a href="/" aria-label="Acme Home">
      <svg aria-hidden="true" width="32" height="32"><circle cx="16" cy="16" r="14"/></svg>
      <span>Acme Corp</span>
    </a>
    <nav aria-label="Primary Navigation">
      <ul>
        <li><a href="/products">Products</a></li>
        <li><a href="/services">Services</a></li>
      </ul>
    </nav>
  </header>

  <!-- Main Landmark & Target of Skip Link -->
  <main id="main-content" tabindex="-1">
    <h1>Account Overview</h1>
    <!-- Interactive, labeled content -->
  </main>
</body>
</html>
```

---

## 5. Practical Examples

### Example 1: Basic — Native Semantics vs ARIA Decoration
Demonstrating how pure native HTML satisfies WCAG 2.1.1 (Keyboard), 4.1.2 (Name, Role, Value), and 1.3.1 (Info and Relationships) without a single ARIA attribute.

```html
<!-- ACCESSIBLE BY DEFAULT (0 ARIA lines required) -->
<button type="button" onclick="handleSave()">Save Draft</button>

<a href="/dashboard">Return to Dashboard</a>

<form action="/search" method="GET">
  <label for="site-search">Search documentation</label>
  <input type="search" id="site-search" name="q" required>
  <button type="submit">Search</button>
</form>
```

### Example 2: Real-World — WCAG 2.2 Accessible Authentication Flow (3.3.8)
WCAG 2.2 Success Criterion 3.3.8 mandates that authentication must not force cognitive memorization tests without alternatives. Web applications must support password managers via correct autofill tokens, allow clipboard pasting, and integrate WebAuthn passkeys.

```html
<form action="/login" method="POST" autocomplete="on">
  <h2>Sign in to your account</h2>
  
  <div class="form-group">
    <label for="login-email">Email address</label>
    <input 
      type="email" 
      id="login-email" 
      name="email" 
      autocomplete="username webauthn"
      required
      aria-describedby="email-hint"
    >
    <span id="email-hint" class="hint">Use your corporate or personal login email.</span>
  </div>

  <div class="form-group">
    <label for="login-password">Password</label>
    <!-- CRITICAL: Never block paste on password inputs (violates WCAG 3.3.8) -->
    <input 
      type="password" 
      id="login-password" 
      name="password" 
      autocomplete="current-password"
      required
    >
  </div>

  <!-- Single-pointer accessible alternative to complex verification -->
  <button type="submit" class="btn-primary">Sign In</button>
  
  <p>
    <a href="/recover-password">Forgot password?</a>
  </p>
</form>
```

### Example 3: Edge Case — WCAG 2.2 Target Size Minimum (2.5.8 Level AA)
WCAG 2.2 requires interactive pointer targets to be at least 24x24 CSS pixels, or have offset spacing preventing accidental adjacent clicks. Inline links within a sentence are exempt, but toolbar buttons and icon links must comply.

```html
<style>
  /* Ensuring small visual icons achieve the 24x24px physical hit target */
  .icon-button {
    display: inline-flex;
    align-items: center;
    justify-content: center;
    width: 24px;
    height: 24px;
    padding: 0;
    margin-right: 8px; /* Spacing offset satisfying 2.5.8 */
    border: 1px solid #ccc;
    background: transparent;
    cursor: pointer;
  }
  
  /* Focus Appearance: Visible focus ring with 3:1 contrast against background */
  .icon-button:focus-visible {
    outline: 2px solid #005a9c;
    outline-offset: 2px;
  }
</style>

<div role="toolbar" aria-label="Text Formatting">
  <button type="button" class="icon-button" aria-label="Format Bold">
    <strong aria-hidden="true">B</strong>
  </button>
  <button type="button" class="icon-button" aria-label="Format Italic">
    <em aria-hidden="true">I</em>
  </button>
  <button type="button" class="icon-button" aria-label="Format Underline">
    <u aria-hidden="true">U</u>
  </button>
</div>
```

### Example 4: Failure Case — Broken Accessibility Anti-Patterns
The following code commits catastrophic WCAG violations:

```html
<!-- BROKEN: Fails 4.1.2 (Name, Role, Value), 2.1.1 (Keyboard), 2.4.7 (Focus Visible) -->
<div class="blue-btn" onclick="submitForm()">Submit</div>

<!-- BROKEN: Positive tabindex disrupts natural DOM order (Fails 2.4.3 Focus Order) -->
<input type="text" tabindex="5" placeholder="First Name">
<input type="text" tabindex="1" placeholder="Last Name">

<!-- BROKEN: Focus outline explicitly removed (Fails 2.4.7 Focus Visible) -->
<style>
  * { outline: none !important; }
</style>

<!-- BROKEN: Placeholder used as sole label (Fails 1.3.1 Info and Relationships, 3.3.2 Labels) -->
<input type="text" placeholder="Credit Card Number">
```

### Example 5: Accessible Production-Grade — WCAG 2.2 Redundant Entry (3.3.7) Form Pattern
Demonstrating multi-step shipping and billing address population to satisfy WCAG 3.3.7 Level A.

```html
<form action="/checkout/payment" method="POST">
  <fieldset>
    <legend>Shipping Address</legend>
    
    <div class="field">
      <label for="ship-name">Full name</label>
      <input type="text" id="ship-name" name="shipping_name" autocomplete="shipping name" required>
    </div>
    
    <div class="field">
      <label for="ship-street">Street address</label>
      <input type="text" id="ship-street" name="shipping_street" autocomplete="shipping street-address" required>
    </div>
    
    <div class="field">
      <label for="ship-city">City</label>
      <input type="text" id="ship-city" name="shipping_city" autocomplete="shipping address-level2" required>
    </div>
  </fieldset>

  <fieldset>
    <legend>Billing Address</legend>
    
    <!-- WCAG 3.3.7 Compliance: Checkbox prevents forcing user to re-enter redundant data -->
    <div class="checkbox-field">
      <input type="checkbox" id="same-as-shipping" name="same_as_shipping" onchange="toggleBilling(this)">
      <label for="same-as-shipping">Billing address is same as shipping address</label>
    </div>

    <div id="billing-fields">
      <div class="field">
        <label for="bill-name">Full name on card</label>
        <input type="text" id="bill-name" name="billing_name" autocomplete="billing name">
      </div>
      <div class="field">
        <label for="bill-street">Billing street address</label>
        <input type="text" id="bill-street" name="billing_street" autocomplete="billing street-address">
      </div>
    </div>
  </fieldset>

  <button type="submit">Proceed to Payment</button>
</form>

<script>
  function toggleBilling(checkbox) {
    const billingGroup = document.getElementById('billing-fields');
    if (checkbox.checked) {
      document.getElementById('bill-name').value = document.getElementById('ship-name').value;
      document.getElementById('bill-street').value = document.getElementById('ship-street').value;
      billingGroup.hidden = true;
    } else {
      billingGroup.hidden = false;
    }
  }
</script>
```

---

## 6. Best Practices

1. **Rely on Native HTML Elements First**: Every native element (`<button>`, `<a>`, `<input>`, `<select>`, `<dialog>`, `<details>`) includes built-in keyboard navigation, state management, and OS accessibility mappings.
2. **Never Suppress Keyboard Focus Outlines**: Retain standard browser focus outlines or style them with `:focus-visible` providing at least a 3:1 contrast ratio against the surrounding background.
3. **Always Declare the Document Language**: Provide a valid BCP 47 language code on the root element (`<html lang="en">`) so screen readers load the correct pronunciation engine.
4. **Ensure Logical Tab Order**: Never use positive `tabindex` values (`tabindex="1"`). Rely entirely on DOM source order, using `tabindex="0"` for custom widgets and `tabindex="-1"` for script-focused containers.
5. **Support Copy-and-Paste Across All Inputs**: Disabling pasting on password or confirmation fields directly breaches WCAG 2.2 AA (3.3.8 Accessible Authentication) and severely harms users of password managers.
6. **Provide Explicit, Programmatic Labels**: Every form input must have a linked `<label for="id">` or an unambiguous accessible name.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Severity | WCAG Success Criterion | Correction |
|:---|:---|:---|:---|
| Removing focus indicators (`outline: none` without replacement) | Critical | 2.4.7 Focus Visible (AA) | Replace with custom `:focus-visible` ring with >= 3:1 contrast |
| Building interactive buttons using `<div onclick="...">` | Critical | 4.1.2 Name, Role, Value (A) & 2.1.1 Keyboard (A) | Use native `<button type="button">` |
| Missing `alt` attribute on `<img>` element | High | 1.1.1 Non-text Content (A) | Supply descriptive `alt="text"` or `alt=""` if purely decorative |
| Using positive `tabindex` (`tabindex="3"`) | High | 2.4.3 Focus Order (A) | Reorder elements in HTML source; use only `0` or `-1` |
| Text contrast below 4.5:1 against background | High | 1.4.3 Contrast (Minimum) (AA) | Darken text or lighten background to achieve >= 4.5:1 ratio |
| Disabling user paste on password inputs | High | 3.3.8 Accessible Authentication (AA) | Remove JavaScript `onpaste="return false;"` event listeners |
| Overlapping sticky footers obscuring focused elements | Medium | 2.4.11 Focus Not Obscured (AA) | Apply CSS `scroll-padding-bottom` or `scroll-margin` to focusable elements |
| Touch targets under 24x24px without spacing | Medium | 2.5.8 Target Size (Minimum) (AA) | Ensure minimum target dimension or 24px circular offset margin |

---

## 8. Performance, Accessibility, SEO & Security Notes

### Accessibility Engine Performance
Overloading the DOM with tens of thousands of deeply nested, useless `<div>` containers significantly degrades the performance of the browser's accessibility engine. Every time a DOM mutation occurs, the browser recalculates the Accessibility Object Model (AOM). A lean, semantic DOM tree updates in sub-millisecond time, whereas deeply nested "div soup" triggers severe main-thread frame drops during screen reader speech synthesis.

### SEO Synergy
Search engine web crawlers (Googlebot, Bingbot) do not have human eyes; they interact with web pages exactly like screen readers. Semantic landmarks (`<header>`, `<nav>`, `<main>`, `<article>`, `<aside>`), clean heading hierarchies (`<h1>`-`<h6>`), descriptive anchor text, and meaningful `alt` attributes directly optimize search indexation and rich SERP snippet generation.

### Security Implications
Accessible HTML directly enhances security. Using standard HTML forms with proper `autocomplete` tokens allows browsers to generate cryptographically secure, high-entropy passwords and prevents users from falling back to insecure, easily guessed credentials. Furthermore, eliminating custom JavaScript-driven dropdowns and modals in favor of native `<dialog>` and `<select>` minimizes attack surface against DOM-based Cross-Site Scripting (XSS).

---

## 9. Comparison / When to Use What

### Semantic HTML vs ARIA vs Custom JavaScript

| Requirement | Native Semantic HTML | ARIA Overrides | Custom JS Divs |
|:---|:---|:---|:---|
| **Trigger Action** | `<button type="button">` | `<div role="button">` | `<div onclick="...">` (Unusable by screen readers) |
| **Navigate URL** | `<a href="...">` | `<span role="link">` | `<span onclick="location.href=...">` (Broken) |
| **Modal Overlay** | `<dialog>` | `<div role="dialog" aria-modal="true">` | `<div>` with `z-index: 9999` (Leads to focus leaks) |
| **Collapsible Section** | `<details>` + `<summary>` | `<button aria-expanded="...">` + `<div>` | Handcrafted JS accordion (High maintenance) |
| **Form Input** | `<input type="text">` | `<div role="textbox" contenteditable>` | Custom simulated input (Inaccessible) |

---

## 10. Interview Questions

### Q1: What are the four core principles of WCAG, and how do they apply to HTML development?
<details>
<summary>View Answer</summary>

The four principles are **POUR**:
1. **Perceivable**: Information and user interface components must be presentable to users in ways they can perceive (e.g., text alternatives for non-text content via `alt` attributes, captions on video via `<track>`, sufficient color contrast).
2. **Operable**: Interface components and navigation must be operable via any hardware modality (e.g., all functionality accessible via keyboard without trapping, sufficient time to read content, avoidance of seizure-inducing flashes, target sizes >= 24x24px).
3. **Understandable**: Information and operation must be clear and predictable (e.g., specifying `<html lang="en">`, consistent navigation order, helpful form labels, clear error identification and suggestions).
4. **Robust**: Content must be robust enough to be reliably interpreted by a wide variety of user agents, including assistive technologies (e.g., valid semantic HTML elements, correct accessible names, adherence to platform accessibility APIs).
</details>

### Q2: What major changes were introduced in WCAG 2.2 compared to WCAG 2.1?
<details>
<summary>View Answer</summary>

WCAG 2.2 was officially published on October 5, 2023. Key updates include:
- **Deprecated 4.1.1 (Parsing)**: Removed because modern HTML5 parser specifications require browsers to handle malformed markup identically.
- **2.4.11 Focus Not Obscured (Minimum) (AA)**: Ensures keyboard-focused elements are not completely covered by author-created sticky headers, footers, or non-modal banners.
- **2.5.7 Dragging Movements (AA)**: Requires single-pointer alternatives (e.g., buttons) for dragging operations.
- **2.5.8 Target Size (Minimum) (AA)**: Requires interactive targets to be at least 24x24 CSS pixels, or have spacing preventing accidental activation.
- **3.2.6 Consistent Help (A)**: Help mechanisms must appear in the same relative position across pages.
- **3.3.7 Redundant Entry (A)**: Prevents forcing users to re-enter previously submitted information in the same process.
- **3.3.8 Accessible Authentication (Minimum) (AA)**: Forbids requiring cognitive function tests (like memorizing passwords or transcribing puzzles) for logins, requiring copy-paste support, password managers, or WebAuthn.
</details>

### Q3: How does the browser's Accessibility Object Model (AOM) differ from the Document Object Model (DOM)?
<details>
<summary>View Answer</summary>

The **DOM** is the complete, object-oriented representation of the HTML document containing all elements, styling, and text nodes used by the layout engine and JavaScript.

The **Accessibility Tree (AOM)** is a secondary, pruned tree derived from the DOM and computed styles. It contains only nodes with semantic value for assistive technology users. Elements purely used for presentation (like decorative `<span>` tags without attributes) are stripped out. Each accessibility node exposes specific properties mandated by OS platform APIs:
- **Role**: What the element is (e.g., button, heading, link).
- **Name**: The human-readable label (e.g., "Submit Application").
- **State**: Current condition (e.g., `focused`, `expanded`, `disabled`, `checked`).
- **Value**: Current magnitude or text content (e.g., "50%").
</details>

### Q4: Why is positive `tabindex` (`tabindex="1"`, `tabindex="2"`) considered an anti-pattern?
<details>
<summary>View Answer</summary>

Positive `tabindex` forces elements into a custom keyboard tab sequence that takes absolute precedence before any default elements (`tabindex="0"` or native links/buttons) in the document.

This creates severe usability problems:
1. It breaks the expected reading flow, jumping the focus cursor unpredictably across visual coordinates.
2. It makes maintenance catastrophic: adding any new interactive component requires manually renumbering every other positive `tabindex` in the entire codebase.
3. It directly violates **WCAG 2.4.3 (Focus Order)**.

The industry standard rule is to only ever use `tabindex="0"` (to insert a custom element into natural DOM order) or `tabindex="-1"` (to make an element focusable only via JavaScript `.focus()`).
</details>

### Q5: How does the First Rule of ARIA guide accessibility engineering?
<details>
<summary>View Answer</summary>

The First Rule of ARIA states: *"If you can use a native HTML element or attribute with the semantics and behavior you require already built in, instead of re-purposing an element and adding an ARIA role, state or property to make it accessible, then do so."*

Native elements like `<button>` come with built-in accessibility roles, states, focusability, and keyboard event listeners (`Enter` and `Space` key activation). Attempting to recreate this using `<div role="button" tabindex="0">` requires dozens of lines of error-prone JavaScript to replicate focus styling, disabled states, keydown handling, and touch responsiveness.
</details>

### Q6: What is the difference between WCAG Conformance Levels A, AA, and AAA?
<details>
<summary>View Answer</summary>

- **Level A**: The absolute minimum baseline. Without these, content is completely impassable for individuals with specific disabilities (e.g., keyboard access, image alt text).
- **Level AA**: The standard international legal benchmark for web applications. Addresses the most common and severe barriers for people with disabilities (e.g., color contrast, visible focus, target size, error identification). Most accessibility laws (ADA, EAA, Section 508) mandate Level AA.
- **Level AAA**: The highest level of accessibility optimization. Includes specialized criteria (e.g., sign language interpretation, 7:1 contrast, no cognitive tests whatsoever). The W3C does not recommend mandating Level AAA as a blanket requirement across entire websites because it is technically impossible for certain types of dynamic content to satisfy all AAA criteria.
</details>

### Q7: Explain WCAG 2.2 Criterion 3.3.8 (Accessible Authentication). What does it require of login forms?
<details>
<summary>View Answer</summary>

WCAG 3.3.8 mandates that authentication processes must not rely on a **cognitive function test** (such as memorizing arbitrary character sequences, recognizing objects, or solving mathematical problems) unless:
1. An alternative method that is not a cognitive function test is provided.
2. A mechanism is available to assist the user in completing the test (e.g., allowing password managers to autofill credentials and allowing the user to paste their password from the clipboard).

To comply:
- Never disable pasting on login password inputs (`onpaste="return false"`).
- Always include `autocomplete="username"` and `autocomplete="current-password"`.
- Support WebAuthn / Passkeys (`autocomplete="webauthn"`).
</details>

### Q8: What is the difference between `:focus` and `:focus-visible` in CSS, and why is `:focus-visible` preferred?
<details>
<summary>View Answer</summary>

- `:focus` matches whenever an element has focus, regardless of whether the user clicked it with a mouse, touched it on a phone screen, or navigated to it with the keyboard `Tab` key. Because mouse clicks trigger `:focus`, developers frequently remove the outline (`outline: none`), accidentally destroying keyboard accessibility.
- `:focus-visible` matches heuristically only when the user agent determines that focus should be visibly indicated—most notably when navigating via keyboard (`Tab` key) or switch devices.

Using `:focus-visible` allows designers to suppress focus outlines for mouse clicks while guaranteeing a prominent, high-contrast focus ring for keyboard users, cleanly satisfying **WCAG 2.4.7 (Focus Visible)**.
</details>

---

## 11. Practice Exercises

### Exercise 1: Easy — Repairing a Keyboard Focus Ring (Level A/AA)
**Problem**: An external stylesheet applied `* { outline: none; }`. Write the semantic CSS to restore an accessible, high-contrast keyboard focus indicator using `:focus-visible` without showing outlines on mouse clicks.

<details>
<summary>View Solution</summary>

```css
/* Remove unwanted default outlines cleanly */
:focus:not(:focus-visible) {
  outline: none;
}

/* Provide clear, high-contrast 3:1 focus ring for keyboard users */
:focus-visible {
  outline: 3px solid #005a9c;
  outline-offset: 2px;
}
```
</details>

### Exercise 2: Easy — Document Language & Title Structure
**Problem**: Write a minimal HTML document skeleton that satisfies WCAG 3.1.1 (Language of Page) and 2.4.2 (Page Titled).

<details>
<summary>View Solution</summary>

```html
<!DOCTYPE html>
<html lang="en" dir="ltr">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>User Settings — Acme Cloud Portal</title>
</head>
<body>
  <main>
    <h1>User Settings</h1>
  </main>
</body>
</html>
```
</details>

### Exercise 3: Medium — Fixing a "Div Button" Anti-Pattern
**Problem**: Refactor the following inaccessible snippet into fully standards-compliant HTML:
`<div class="btn" onclick="saveData()">Save Profile</div>`

<details>
<summary>View Solution</summary>

```html
<!-- Native button provides role, keyboard activation (Enter/Space), and natural tab stop -->
<button type="button" class="btn" onclick="saveData()">
  Save Profile
</button>
```
</details>

### Exercise 4: Medium — Implementing WCAG 2.2 Target Size Minimum (2.5.8)
**Problem**: Create an accessible pagination bar where each page number link has a touch target of at least 24x24 CSS pixels.

<details>
<summary>View Solution</summary>

```html
<nav aria-label="Pagination Navigation">
  <ul style="display: flex; list-style: none; padding: 0; gap: 8px;">
    <li>
      <a href="?page=1" style="display: inline-flex; align-items: center; justify-content: center; min-width: 24px; min-height: 24px; text-decoration: none; border: 1px solid #767676; padding: 4px;" aria-current="page">1</a>
    </li>
    <li>
      <a href="?page=2" style="display: inline-flex; align-items: center; justify-content: center; min-width: 24px; min-height: 24px; text-decoration: none; border: 1px solid #767676; padding: 4px;">2</a>
    </li>
    <li>
      <a href="?page=3" style="display: inline-flex; align-items: center; justify-content: center; min-width: 24px; min-height: 24px; text-decoration: none; border: 1px solid #767676; padding: 4px;">3</a>
    </li>
  </ul>
</nav>
```
</details>

### Exercise 5: Hard — Implementing Single-Pointer Alternative for Drag-and-Drop (WCAG 2.2 2.5.7)
**Problem**: A task list has drag handles for re-ordering items. Write semantic HTML providing a single-pointer button alternative to satisfy WCAG 2.5.7 Dragging Movements (Level AA).

<details>
<summary>View Solution</summary>

```html
<ul aria-label="Prioritized Tasks">
  <li id="task-1">
    <span>Prepare Quarterly Audit</span>
    <div class="task-controls" role="group" aria-label="Task priority controls for Prepare Quarterly Audit">
      <!-- Drag handle for mouse users -->
      <button type="button" draggable="true" aria-hidden="true" title="Drag to reorder">::</button>
      
      <!-- Single-pointer accessible alternatives satisfying WCAG 2.5.7 -->
      <button type="button" onclick="moveUp('task-1')" aria-label="Move Prepare Quarterly Audit up">Move Up</button>
      <button type="button" onclick="moveDown('task-1')" aria-label="Move Prepare Quarterly Audit down">Move Down</button>
    </div>
  </li>
</ul>
```
</details>

### Exercise 6: Hard — Preventing Obscured Focus from Fixed Banners (WCAG 2.2 2.4.11)
**Problem**: A web app features a fixed sticky header (height 80px) and a fixed bottom cookie banner (height 60px). When users press `Tab`, focused elements scroll behind these bars. Write the CSS solution using modern CSS scroll padding properties.

<details>
<summary>View Solution</summary>

```css
/* Modern CSS solution for WCAG 2.4.11 Focus Not Obscured */
html {
  /* Tells browser scrolling mechanisms to offset focused elements by the height of fixed bars */
  scroll-padding-top: 90px;    /* 80px header + 10px buffer */
  scroll-padding-bottom: 70px; /* 60px banner + 10px buffer */
  scroll-behavior: smooth;
}

/* Ensure individual interactive elements also account for scroll margin */
:focus-visible {
  scroll-margin-top: 90px;
  scroll-margin-bottom: 70px;
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **POUR Principles**: Perceivable, Operable, Understandable, Robust.
- **Level AA is the Global Legal Target**: ADA, EAA, and Section 508 mandate Level AA conformance.
- **First Rule of ARIA**: Never use ARIA when native HTML can achieve the same semantic role and behavior.
- **WCAG 2.2 Additions**:
  - `2.4.11`: Focus not completely obscured by sticky overlays.
  - `2.5.7`: Dragging actions must have single-click button alternatives.
  - `2.5.8`: Interactive hit targets must be >= 24x24 CSS pixels or spaced apart.
  - `3.3.7`: Multi-step forms must not require redundant re-entry of data.
  - `3.3.8`: Never disable paste on password fields; support password managers and passkeys.
- **Tabindex Rule**: Only use `tabindex="0"` (natural insertion) or `tabindex="-1"` (programmatic focus). Never use positive `tabindex`.
- **Focus Rings**: Never set `outline: none` without replacing it with a high-contrast `:focus-visible` rule.

---

## 13. Further Reading

- [W3C WCAG 2.2 Recommendation (Official Specification)](https://www.w3.org/TR/WCAG22/)
- [W3C Web Accessibility Initiative (WAI) Fundamentals](https://www.w3.org/WAI/fundamentals/)
- [MDN Web Docs: Accessibility Guide](https://developer.mozilla.org/en-US/docs/Web/Accessibility)
- [European Accessibility Act (EAA Directive 2019/882)](https://ec.europa.eu/social/main.jsp?catId=1202)
- [WebAIM: Web Accessibility In Mind](https://webaim.org/)

---

[Previous: Section Overview (README.md)](README.md) | [Section Index](README.md) | [Next: 02 - Semantic-First Architecture & ARIA Rules ->](02-semantic-first-and-the-aria-rules.md)
