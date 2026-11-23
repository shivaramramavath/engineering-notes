# JavaScript Interview Mastery: DOM, Browser Architecture & Web APIs

> An advanced architectural interview guide covering event propagation phases, event delegation, Critical Rendering Path optimization, layout thrashing mitigation, and modern browser storage models.

| Metadata | Details |
| :--- | :--- |
| **Track** | JavaScript Interview Preparation (`26_interview`) |
| **Target Role** | Senior / Staff Frontend & Full-Stack Engineer |
| **Baseline** | ECMAScript 2024 / Evergreen Browsers |
| **Prerequisites** | [14_dom-and-browser](file:///e:/Learning/engineering-notes/01-languages/javascript/14_dom-and-browser), [15_networking](file:///e:/Learning/engineering-notes/01-languages/javascript/15_networking) |

---

## Table of Contents
1. [Core Concepts & Theoretical Framework](#1-core-concepts--theoretical-framework)
2. [Deep-Dive Technical Questions & Answers](#2-deep-dive-technical-questions--answers)
   - [Q1: What are the 3 phases of W3C Event Propagation, and how do you configure listeners for capturing?](#q1-what-are-the-3-phases-of-w3c-event-propagation-and-how-do-you-configure-listeners-for-capturing)
   - [Q2: Differentiate between `stopPropagation()`, `stopImmediatePropagation()`, and `preventDefault()`.](#q2-differentiate-between-stoppropagation-stopimmediatepropagation-and-preventdefault)
   - [Q3: What is Event Delegation, what are its performance benefits, and which events do NOT bubble?](#q3-what-is-event-delegation-what-are-its-performance-benefits-and-which-events-do-not-bubble)
   - [Q4: Detail the 5 stages of the browser Critical Rendering Path.](#q4-detail-the-5-stages-of-the-browser-critical-rendering-path)
   - [Q5: What is Layout Thrashing (Forced Synchronous Reflow), and how do you eliminate it?](#q5-what-is-layout-thrashing-forced-synchronous-reflow-and-how-do-you-eliminate-it)
   - [Q6: When should you use `requestAnimationFrame()` vs `requestIdleCallback()` vs `setTimeout()`?](#q6-when-should-you-use-requestanimationframe-vs-requestidlecallback-vs-settimeout)
   - [Q7: Compare Cookies, LocalStorage, SessionStorage, and IndexedDB across storage limits and security guarantees.](#q7-compare-cookies-localstorage-sessionstorage-and-indexeddb-across-storage-limits-and-security-guarantees)
   - [Q8: How does the browser enforce the Same-Origin Policy (SOP), and what exact headers configure CORS?](#q8-how-does-the-browser-enforce-the-same-origin-policy-sop-and-what-exact-headers-configure-cors)
3. [Tricky Output & Code Prediction Drills](#3-tricky-output--code-prediction-drills)
4. [Staff-Level Architecture & Discussion Prompts](#4-staff-level-architecture--discussion-prompts)

---

## 1. Core Concepts & Theoretical Framework

The browser runtime combines layout engines (Blink, WebKit, Gecko) with ECMAScript JIT engines (V8, JavaScriptCore, SpiderMonkey). High-performance web applications demand an exact understanding of how DOM updates trigger the hardware-accelerated compositor.

```
                           EVENT PROPAGATION
                                   │
              ┌────────────────────┼────────────────────┐
              ▼                    ▼                    ▼
       1. Capturing Phase   2. Target Phase      3. Bubbling Phase
       (Window -> Target)   (At target element)  (Target -> Window)
```

- **Render Loop**: The browser attempts to render at the native monitor refresh rate (typically 60Hz / 16.6ms per frame or 120Hz / 8.3ms). Long JavaScript execution on the main thread delays rendering, causing jank and dropped frames.

---

## 2. Deep-Dive Technical Questions & Answers

### Q1: What are the 3 phases of W3C Event Propagation, and how do you configure listeners for capturing?

<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

When an interaction occurs, the browser dispatches an event through 3 sequential phases:
1. **Capturing Phase (Trickling)**: The event descends from `window` $\to$ `document` $\to$ `<html>` $\to$ `<body>` down through the ancestor hierarchy to the target element's parent.
2. **Target Phase**: The event reaches the actual element where the user initiated the interaction (`event.target`). Listeners registered directly on the target fire in registration order.
3. **Bubbling Phase**: The event ascends upwards back through ancestor nodes until reaching `window`.

#### Listener Configuration:
By default, `addEventListener(type, listener)` registers for the **bubbling phase**. To intercept events during the capturing phase, pass `{ capture: true }` (or `true` as the third parameter):

```javascript
element.addEventListener('click', handler, { capture: true });
```
</details>

---

### Q2: Differentiate between `stopPropagation()`, `stopImmediatePropagation()`, and `preventDefault()`.

<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

- **`event.stopPropagation()`**: Halts the traversal of the event up or down the DOM tree. However, any other listeners attached to the *same* current element continue to execute.
- **`event.stopImmediatePropagation()`**: Halts propagation up/down the DOM tree AND **immediately prevents** any subsequent listeners on the *same* element from firing.
- **`event.preventDefault()`**: Does **not** halt propagation. It cancels the browser's default user-agent behavior associated with the event (e.g. following an `<a>` link, submitting a `<form>`, or checking a checkbox).

```javascript
button.addEventListener('click', (e) => {
  e.stopImmediatePropagation();
  console.log('Fires first');
});

button.addEventListener('click', (e) => {
  console.log('Will NEVER fire because of stopImmediatePropagation()');
});
```
</details>

---

### Q3: What is Event Delegation, what are its performance benefits, and which events do NOT bubble?

<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

#### Event Delegation:
Attaching a single event listener to a common ancestor node rather than binding separate listeners to hundreds of child elements. When children are clicked, the event bubbles to the parent where `e.target` identifies the specific child.

#### Benefits:
1. **Memory Conservation**: Eliminates thousands of individual handler closures.
2. **Dynamic DOM Support**: Automatically handles dynamically inserted or removed child elements without re-binding.

#### Non-Bubbling Events:
Certain events do **not** bubble up the DOM tree:
- `focus` & `blur` (use `focusin` & `focusout` instead for bubbling delegation)
- `mouseenter` & `mouseleave` (use `mouseover` & `mouseout` for delegation)
- `load`, `unload`, `abort`, `error`
- Media events (`play`, `pause`, `seeking`)
</details>

---

### Q4: Detail the 5 stages of the browser Critical Rendering Path.

<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

1. **DOM Construction**: HTML parser reads bytes, tokenizes tags into nodes, and constructs the Document Object Model (DOM) tree.
2. **CSSOM Construction**: CSS parser reads stylesheets and inline styles to construct the CSS Object Model (CSSOM) tree (render-blocking).
3. **Render Tree Generation**: The browser combines DOM and CSSOM into the Render Tree, filtering out invisible nodes (`display: none`, `<head>`, `<script>`).
4. **Layout (Reflow)**: Computes the exact geometric dimensions and screen coordinates (X, Y, width, height) of every render node.
5. **Paint & Composite**:
   - **Paint**: Rasterizes vector shapes, text, colors, shadows into bitmap layers.
   - **Composite**: GPU compositing draws separate layers onto the screen buffer (`transform`, `opacity`, `will-change`).
</details>

---

### Q5: What is Layout Thrashing (Forced Synchronous Reflow), and how do you eliminate it?

<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

Normally, browsers batch DOM style mutations and compute Layout once at the end of the frame.
**Layout Thrashing** occurs when JavaScript interleaves DOM style writes with geometry reads in a loop:

```javascript
// ANTI-PATTERN: Forces synchronous reflow on every iteration!
for (const el of elements) {
  const width = el.offsetWidth; // READ forces synchronous layout!
  el.style.width = (width + 10) + 'px'; // WRITE dirties layout!
}
```

#### Elimination Strategy:
Batch all reads together, followed by all writes, or schedule writes inside `requestAnimationFrame()`:

```javascript
// OPTIMIZED: Batch reads, then batch writes
const widths = elements.map((el) => el.offsetWidth); // Batch Read
elements.forEach((el, i) => {
  el.style.width = (widths[i] + 10) + 'px'; // Batch Write
});
```
</details>

---

### Q6: When should you use `requestAnimationFrame()` vs `requestIdleCallback()` vs `setTimeout()`?

<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

- **`requestAnimationFrame(callback)`**: Synchronized with the display hardware refresh rate (prior to paint). Used for smooth visual animations, canvas updates, and scroll responses.
- **`requestIdleCallback(callback [, options])`**: Executes during periods when the browser main thread is idle between frames. Ideal for non-essential analytics logging, telemetry dispatch, and background pre-fetching.
- **`setTimeout(callback, delay)`**: Dispatches to the macrotask queue. Unsynchronized with the display refresh; should **never** be used for UI animations.
</details>

---

### Q7: Compare Cookies, LocalStorage, SessionStorage, and IndexedDB across storage limits and security guarantees.

<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

| Storage Mechanism | Capacity | Persistence | Server Visibility | Security Flags | API Type |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **Cookies** | ~4 KB | Configurable (`Expires`/`Max-Age`) | Sent automatically on every HTTP request | `HttpOnly`, `Secure`, `SameSite` | Synchronous string |
| **LocalStorage** | ~5–10 MB | Persistent until explicitly cleared | Purely client-side | Accessible by any script (vulnerable to XSS) | Synchronous string |
| **SessionStorage**| ~5 MB | Cleared on tab close | Purely client-side | Accessible by any script (vulnerable to XSS) | Synchronous string |
| **IndexedDB** | >500 MB (GBs) | Persistent | Purely client-side | Sandboxed per origin (vulnerable to XSS) | Asynchronous transactions |
</details>

---

### Q8: How does the browser enforce the Same-Origin Policy (SOP), and what exact headers configure CORS?

<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

The **Same-Origin Policy (SOP)** restricts a script loaded on one origin (`protocol + domain + port`) from reading sensitive data from another origin.

#### Cross-Origin Resource Sharing (CORS):
Allows servers to relax SOP via response headers:
- `Access-Control-Allow-Origin: https://app.example.com`
- `Access-Control-Allow-Methods: GET, POST, PUT, DELETE`
- `Access-Control-Allow-Headers: Content-Type, Authorization`
- `Access-Control-Allow-Credentials: true`

#### Preflight Requests (`OPTIONS`):
Triggered when requests use non-simple methods (`PUT`, `DELETE`, `PATCH`), custom headers (e.g. `Authorization`), or non-simple content types (`application/json`). The browser transmits an `OPTIONS` flight check before sending the actual payload.
</details>

---

## 3. Tricky Output & Code Prediction Drills

### Drill 1: Event Propagation Ordering
```javascript
const parent = document.createElement('div');
const child = document.createElement('button');
parent.appendChild(child);

parent.addEventListener('click', () => console.log('1: Parent Capturing'), true);
parent.addEventListener('click', () => console.log('2: Parent Bubbling'), false);
child.addEventListener('click', () => console.log('3: Child Bubbling'), false);
child.addEventListener('click', () => console.log('4: Child Capturing'), true);

child.click();
```
<details>
<summary><strong>Output & Explanation</strong></summary>

```
1: Parent Capturing
3: Child Bubbling (or 4, then 3 based on registration order at target)
4: Child Capturing
2: Parent Bubbling
```
**Explanation:**
1. Capturing phase traverses parent: logs `1`.
2. Target phase reaches `child`: listeners attached directly to the target element fire in **order of registration**, regardless of the `capture` flag!
3. Bubbling phase returns to parent: logs `2`.
</details>

---

## 4. Staff-Level Architecture & Discussion Prompts

1. *How would you architect a zero-jank Virtualized Infinite List in vanilla JavaScript capable of scrolling 500,000 tabular items without exceeding 60fps frame budgets?*
2. *How do Cross-Origin Isolation headers (`COOP` and `COEP`) enable high-resolution timers (`performance.now()`) and `SharedArrayBuffer` while preventing Spectre side-channel memory attacks?*
