# Live Regions & Dynamic Status Messages

| Metadata | Details |
|:---|:---|
| **Level** | Advanced |
| **Prerequisites** | `06-accessibility/03-aria-roles-states-and-properties.md`, `06-accessibility/08-accessible-forms-and-error-announcement.md` |
| **Estimated Reading Time** | 29 minutes |
| **Status** | Current |
| **Baseline Status** | Widely Available (W3C WAI-ARIA 1.2 Standards) |
| **Browsers Tested** | Chrome 120+, Firefox 121+, Safari 17.2+, Edge 120+ |
| **Tested With** | NVDA 2024.1, JAWS 2024, VoiceOver, Chrome/Firefox DevTools |

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

### The Blind Spot of Asynchronous Web Applications
In traditional server-rendered websites, every user action triggered a full page reload, causing screen readers to restart reading from the top of the newly loaded document. Modern web applications (SPAs, Ajax, WebSockets), however, update content asynchronously without reloading the page:
- Items are added to a shopping cart.
- Live search queries return 42 matching results.
- A toast notification announces *"Settings saved successfully"*.
- A stock ticker or system status updates in real time.

For a sighted user, these visual updates are immediately apparent in the corner of their vision. However, **for a screen reader user, unless an update shifts keyboard focus or triggers an accessibility notification, the change is completely silent and invisible**. 

### WCAG 2.2 Success Criterion 4.1.3 (Status Messages)
To solve this without disorienting users by jarringly stealing keyboard focus, the W3C introduced **WCAG Success Criterion 4.1.3 (Status Messages - Level AA)**:
> *"In content implemented using markup languages, status messages can be programmatically determined through role or properties such that they can be presented to the user by assistive technologies without receiving focus."*

---

## 2. Core Concepts

### What is a Live Region?
A **Live Region** is an area of an HTML document marked with the `aria-live` attribute or a specialized live region role (`role="status"`, `role="alert"`). 

When a live region is defined, the browser engine monitors its DOM subtree for mutations (added text, modified nodes). Whenever content changes inside a live region, the browser automatically translates the mutation into an operating system accessibility speech event, instructing the screen reader to speak the new text **without moving the user's cursor or keyboard focus**.

### The `aria-live` Politeness Settings
The `aria-live` attribute supports three values:

```
+--------------------------------------------------------------------------+
|                        ARIA-LIVE POLITENESS MODES                        |
+--------------------------------------------------------------------------+
| Value       | Behavior                                | Common Use Cases |
+-------------+-----------------------------------------+------------------+
| off         | (Default) Content updates are completely| Background data  |
|             | ignored by the speech synthesis engine. | silently fetched |
+-------------+-----------------------------------------+------------------+
| polite      | The screen reader waits until the user  | Toast messages,  |
|             | finishes typing, reading, or tabbing    | search counts,   |
|             | before announcing the update gracefully.| cart additions   |
+-------------+-----------------------------------------+------------------+
| assertive   | The screen reader immediately INTERRUPTS| Session timeouts,|
|             | whatever it is currently speaking to    | server errors,   |
|             | blare the critical notification.        | urgent warnings  |
+--------------------------------------------------------------------------+
```

### Specialized Live Region Roles
WAI-ARIA defines shorthand roles with built-in politeness defaults:
- **`role="status"`**: Equivalent to `aria-live="polite"` and `aria-atomic="true"`. Used for non-urgent status messages (e.g., *"3 items added to cart"*).
- **`role="alert"`**: Equivalent to `aria-live="assertive"` and `aria-atomic="true"`. Used for critical errors or urgent warnings requiring immediate awareness.
- **`role="log"`**: Used for sequential append-only message feeds (e.g., chat logs, server console output).
- **`role="timer"`**: Used for elapsed time indicators or countdown stopwatches.

---

## 3. How It Works Internally

### The Mutation Observer & Platform Event Dispatch
Understanding how browser engines process live regions reveals why improper implementation fails:

```
JavaScript mutates DOM: liveRegion.textContent = "Saved draft";
                              │
                              ▼
           Browser DOM Mutation Engine detects change
                              │
                              ▼
             Does the node have aria-live or live role?
                              │
               ┌──────────────┴──────────────┐
              YES                            NO
               │                             │
               ▼                         Do nothing
   Evaluate Politeness Setting
               │
    ┌──────────┴──────────┐
    ▼                     ▼
"polite"              "assertive"
Queue speech event    Interrupt speech queue
               │                     │
               └──────────┬──────────┘
                          ▼
            OS Accessibility API Event:
     Windows: UIA_LiveRegionChangedEventId
     macOS:   AXLiveRegionChangedNotification
                          │
                          ▼
              Screen Reader announces:
                  "Saved draft"
```

### The "Pre-Rendered Container" Mandate
The single most frequent live region bug occurs when developers create the container and insert the text in the same JavaScript execution frame:
```javascript
// FATAL BUG: Does NOT announce in most screen readers!
const toast = document.createElement('div');
toast.setAttribute('role', 'status');
toast.textContent = 'Settings Saved';
document.body.appendChild(toast);
```
**Why this fails**: Screen readers register live regions upon DOM tree construction. If an element is injected with text already present, the browser does not perceive a *mutation*—it simply sees an existing static node.

**The Golden Rule**: The live region container **must already exist in the DOM** (even if visually empty) *before* JavaScript injects the new text string!

---

## 4. Syntax & Usage

### Companion Live Region Attributes

```
+--------------------------------------------------------------------------+
| ATTRIBUTE          | VALUES                  | FUNCTION                  |
+--------------------+-------------------------+---------------------------+
| aria-atomic        | true | false            | If true, speaks entire    |
|                    |                         | container; if false, only |
|                    |                         | speaks changed node.      |
+--------------------+-------------------------+---------------------------+
| aria-relevant      | additions, removals,    | Filters which mutation    |
|                    | text, all               | types trigger speech.     |
+--------------------+-------------------------+---------------------------+
| aria-busy          | true | false            | Suppresses speech while   |
|                    |                         | updates are in-flight.    |
+--------------------------------------------------------------------------+
```

### Visual Hiding vs Screen Reader Announcements
To hide status text from sighted users while keeping it audible to screen readers, use standard accessible screen-reader-only utility CSS:

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
  border: 0;
}
```
**Never use `display: none` or `visibility: hidden` on a live region**, as these CSS properties remove the element from the Accessibility Tree entirely!

---

## 5. Practical Examples

### Example 1: Basic — Live Search Results Counter
Announcing the number of matching search results without moving keyboard focus away from the search input box:

```html
<form action="/search" method="GET">
  <label for="search-input">Search Knowledge Base</label>
  <input 
    type="search" 
    id="search-input" 
    oninput="debounceSearch(this.value)"
    autocomplete="off"
  >
</form>

<!-- Pre-rendered empty live region in DOM -->
<div 
  id="search-status" 
  role="status" 
  aria-live="polite" 
  class="sr-only"
></div>

<script>
  let timer = null;

  function debounceSearch(query) {
    clearTimeout(timer);
    timer = setTimeout(() => {
      // Simulate API search query
      const resultsCount = query.length === 0 ? 0 : 42;
      
      const statusBox = document.getElementById('search-status');
      statusBox.textContent = `${resultsCount} articles found for "${query}"`;
    }, 400);
  }
</script>
```

### Example 2: Real-World — Accessible Toast Notification Queue
A complete toast notification manager that announces status updates politely:

```html
<!-- Toast container pre-rendered in page markup -->
<div 
  id="toast-container" 
  role="status" 
  aria-live="polite" 
  aria-atomic="true" 
  class="toast-wrapper"
></div>

<button type="button" onclick="showToast('Profile updated successfully.')">
  Update Profile
</button>

<style>
  .toast-wrapper {
    position: fixed;
    bottom: 20px;
    right: 20px;
    z-index: 9999;
  }
  .toast {
    background: #1b5e20;
    color: #fff;
    padding: 12px 24px;
    border-radius: 4px;
    box-shadow: 0 4px 12px rgba(0,0,0,0.15);
    margin-top: 8px;
  }
</style>

<script>
  function showToast(message) {
    const container = document.getElementById('toast-container');
    
    // Clear and inject new text to trigger DOM mutation event
    container.textContent = '';
    
    setTimeout(() => {
      container.textContent = message;
      
      // Auto-clear visual appearance after 4 seconds
      setTimeout(() => {
        container.textContent = '';
      }, 4000);
    }, 50);
  }
</script>
```

### Example 3: Edge Case — Assertive Session Expiration Alert
When a banking or healthcare session is about to expire, use `role="alert"` (`aria-live="assertive"`) to break through speech queues:

```html
<div 
  id="session-alert" 
  role="alert" 
  aria-atomic="true" 
  class="sr-only"
></div>

<script>
  function triggerSessionWarning() {
    const alertBox = document.getElementById('session-alert');
    // Screen reader immediately halts current speech to blare this alert
    alertBox.textContent = 'Warning: Your secure banking session will expire in 2 minutes due to inactivity. Press Extend Session to remain logged in.';
  }
</script>
```

### Example 4: Failure Case — Broken Live Region Anti-Patterns
Common mistakes that completely break dynamic announcements:

```html
<!-- BROKEN: Using display:none on live region (Completely removed from AOM!) -->
<div role="status" style="display: none;" id="status-1">
  Item added to cart
</div>

<!-- BROKEN: Injecting full container + text simultaneously with JS -->
<script>
  // Fails because screen reader sees no mutation event!
  function badAnnounce(text) {
    const el = document.createElement('div');
    el.setAttribute('aria-live', 'polite');
    el.innerText = text;
    document.body.appendChild(el);
  }
</script>

<!-- BROKEN: Using aria-live="assertive" for trivial marketing messages -->
<!-- Aggressively interrupts users while they are reading important content -->
<div aria-live="assertive">
  Flash sale: 10% off socks!
</div>
```

### Example 5: Accessible Production-Grade — Shopping Cart Dynamic Update
Demonstrating combining `aria-live="polite"` with `aria-atomic="true"` so the entire count message is spoken coherently:

```html
<header>
  <button type="button" class="cart-trigger">
    <span>Cart</span>
    <!-- aria-atomic="true" ensures screen reader speaks "3 items", not just "3" -->
    <span id="cart-counter" role="status" aria-live="polite" aria-atomic="true">
      2 items ($48.00)
    </span>
  </button>
</header>

<main>
  <button type="button" onclick="addToCart('Ergonomic Keyboard', 89.00)">
    Add Ergonomic Keyboard ($89.00)
  </button>
</main>

<script>
  let totalItems = 2;
  let totalPrice = 48.00;

  function addToCart(itemName, price) {
    totalItems++;
    totalPrice += price;
    
    const counter = document.getElementById('cart-counter');
    counter.textContent = `${totalItems} items ($${totalPrice.toFixed(2)})`;
  }
</script>
```

---

## 6. Best Practices

1. **Pre-render Live Regions in the Static DOM**: Always include the live region container in the initial HTML or insert it well before injecting dynamic text.
2. **Default to `aria-live="polite"`**: Use polite politeness for 95% of use cases. It waits until the user finishes typing or reading before speaking.
3. **Reserve `aria-live="assertive"` for True Emergencies**: Use assertive politeness only for session timeouts, critical server crashes, or life-safety alerts.
4. **Use `aria-atomic="true"` for Cohesive Status Statements**: Guarantees the screen reader speaks the full context (e.g., *"3 items in cart"*) rather than just the changed digit (*"3"*).
5. **Never Move Keyboard Focus for Status Messages**: If an update is a status message (WCAG 4.1.3), leave keyboard focus exactly where the user is working. Moving focus unexpectedly disorients the user.
6. **Use `.sr-only` Instead of `display: none`**: To visually hide status text, use screen-reader-only CSS clipping.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Severity | Impact | Correction |
|:---|:---|:---|:---|
| Using `display: none` on live region | Critical | Pruned from AOM; 0% chance of announcement | Use `.sr-only` CSS clipping |
| Dynamically creating live container and text simultaneously | High | Browser fails to detect a mutation; complete silence | Pre-render container in HTML |
| Stealing focus to announce a minor toast message | High | Disrupts user flow and violates WCAG 4.1.3 | Leave focus alone; use `role="status"` |
| Overusing `aria-live="assertive"` | High | Constant interruptions frustrate screen reader users | Switch to `aria-live="polite"` |
| Missing `aria-atomic="true"` on multi-word counters | Medium | Screen reader speaks bare detached numbers ("4") | Add `aria-atomic="true"` |
| Inverting live region logic (announcing removals as additions) | Medium | Confuses blind users regarding real state | Explicitly configure `aria-relevant` |

---

## 8. Performance, Accessibility, SEO & Security Notes

### Speech Synthesis Throttle
Failing to debounce live region updates (e.g., injecting text on every single keystroke of a search bar) will flood the operating system speech synthesis buffer. The screen reader will stutter through 50 stale phrases long after the user has finished typing. Always debounce live search announcements by 300 to 500ms.

### SEO Considerations
Search engine crawlers (Googlebot) index the static HTML and execute initial JavaScript. However, rapid live region mutations triggered by user events are not indexed into search snippets. Keep canonical content inside the standard DOM.

### Security: XSS via Live Announcements
Never inject unescaped third-party data or user input directly into `innerHTML` of a live region (`container.innerHTML = message`). An attacker can inject malicious `<script>` or `<img>` onerror vectors. Always use `element.textContent` or sanitized strings.

---

## 9. Comparison / When to Use What

### Live Region Selection Matrix

| Update Type | Recommended Role | `aria-live` | `aria-atomic` | Moves Focus? |
|:---|:---|:---:|:---:|:---:|
| **Toast / Notification** | `role="status"` | `polite` | `true` | NO |
| **Search Results Count** | `role="status"` | `polite` | `true` | NO |
| **Critical Session Timeout**| `role="alert"` | `assertive` | `true` | NO |
| **Form Validation Error**| `role="alert"` | `assertive` | `true` | YES (on submit) |
| **Live Chat Log** | `role="log"` | `polite` | `false` | NO |
| **Countdown Timer** | `role="timer"` | `off` / `polite`| `true` | NO |

---

## 10. Interview Questions

### Q1: What is a Live Region in WAI-ARIA, and what problem does it solve?
<details>
<summary>View Answer</summary>

A **Live Region** is a DOM container designated with `aria-live` (or live roles like `role="status"` or `role="alert"`). 

It solves the problem of asynchronous updates in Single Page Applications (SPAs). Sighted users immediately notice visual updates happening in the corner of the screen (e.g., search counts, toasts, cart badges), but screen reader users are unaware of changes outside their current cursor position. Live regions instruct the browser to detect DOM text mutations and announce them through operating system speech synthesis **without forcing a jarring shift in keyboard focus**.
</details>

### Q2: What is the difference between `aria-live="polite"` and `aria-live="assertive"`?
<details>
<summary>View Answer</summary>

- `aria-live="polite"`: The browser instructs the screen reader to wait until the user has completed their current task (e.g., finished speaking a focused link or sentence) before announcing the update. Used for standard toasts, cart updates, and status messages.
- `aria-live="assertive"`: The browser instructs the screen reader to immediately interrupt whatever it is currently speaking to broadcast the message. Reserved exclusively for time-critical emergencies (e.g., session timeout warnings, connection losses).
</details>

### Q3: Why does dynamically creating a `<div>`, adding `role="status"`, setting `textContent`, and appending it to the DOM often fail to announce in screen readers?
<details>
<summary>View Answer</summary>

Screen readers and browser accessibility engines monitor live regions by listening for **mutation events** within an existing registered live subtree.

When a container is created in JavaScript and injected with text already populated, the engine sees an atomic node insertion rather than an update to an established live region. To guarantee reliable announcements across all screen readers (NVDA, JAWS, VoiceOver), the live region container **must already exist in the DOM** before JavaScript mutates its `textContent`.
</details>

### Q4: What is the purpose of `aria-atomic="true"`?
<details>
<summary>View Answer</summary>

`aria-atomic` controls whether the screen reader announces only the specific DOM node that was mutated (`false`), or the **entire contents of the live region container** (`true`).

For example, in a shopping cart badge:
`<span role="status" aria-atomic="true">3 items in cart</span>`
If `aria-atomic="true"` is set and the number changes from 3 to 4, the screen reader announces the full sentence: *"4 items in cart"*. If `aria-atomic="false"` (the default in some contexts), it might announce only the bare number *"4"*, confusing the user.
</details>

### Q5: Why is moving keyboard focus to announce a toast notification considered an accessibility failure?
<details>
<summary>View Answer</summary>

Moving keyboard focus to a toast notification violates **WCAG 4.1.3 (Status Messages)** and creates severe disorientation:
1. It rips the user away from whatever input or article they were currently focused on.
2. In forms, it interrupts active typing, potentially causing keystrokes to be lost or misdirected.
Status updates must be announced passively via `role="status"` so the user can continue working uninterrupted.
</details>

### Q6: What implicit ARIA attributes are mapped to `role="status"` and `role="alert"`?
<details>
<summary>View Answer</summary>

- `role="status"`: Implicitly maps to `aria-live="polite"` and `aria-atomic="true"`.
- `role="alert"`: Implicitly maps to `aria-live="assertive"` and `aria-atomic="true"`.
</details>

### Q7: Why must authors avoid `display: none` when creating a visually hidden live region?
<details>
<summary>View Answer</summary>

`display: none` instructs the browser to remove the element completely from the layout engine and the **Accessibility Object Model (AOM)**. 

If an element is not in the Accessibility Tree, the accessibility engine will never emit `UIA_LiveRegionChanged` or `AXLiveRegionChanged` events, rendering the live region completely mute. Authors must use screen-reader-only CSS clipping (`.sr-only`) instead.
</details>

### Q8: What is `aria-busy="true"`, and when should it be used in conjunction with live regions?
<details>
<summary>View Answer</summary>

`aria-busy="true"` signals to assistive technology that a live region is currently undergoing multi-step DOM updates (e.g., streaming in multiple feed items, re-ordering a table).

While `aria-busy="true"` is active, the screen reader temporarily suppresses change announcements, preventing it from speaking broken or half-populated sentences. Once all updates are rendered, setting `aria-busy="false"` triggers a single, clean announcement.
</details>

---

## 11. Practice Exercises

### Exercise 1: Easy — Pre-rendered Status Region
**Problem**: Write the HTML for an empty, visually hidden live region that politely announces search result updates.

<details>
<summary>View Solution</summary>

```html
<div 
  id="search-announcer" 
  role="status" 
  aria-live="polite" 
  class="sr-only"
></div>

<style>
  .sr-only {
    position: absolute;
    width: 1px;
    height: 1px;
    padding: 0;
    margin: -1px;
    overflow: hidden;
    clip: rect(0, 0, 0, 0);
    border: 0;
  }
</style>
```
</details>

### Exercise 2: Easy — Urgent Server Connection Warning
**Problem**: Write the semantic HTML for an alert container that uses `role="alert"` to warn users that their network connection was lost.

<details>
<summary>View Solution</summary>

```html
<div 
  id="connection-alert" 
  role="alert" 
  aria-atomic="true"
>
  Connection lost. Attempting to reconnect to live server...
</div>
```
</details>

### Exercise 3: Medium — Announcing Deleted Table Rows
**Problem**: A user deletes a row from a table. Write the JavaScript logic to populate an existing `#live-status` container with the confirmation message: "Row deleted: Sarah Connor".

<details>
<summary>View Solution</summary>

```javascript
function confirmRowDeletion(employeeName) {
  const statusBox = document.getElementById('live-status');
  
  // Clear and update to ensure DOM mutation event triggers
  statusBox.textContent = '';
  setTimeout(() => {
    statusBox.textContent = `Row deleted: ${employeeName}`;
  }, 50);
}
```
</details>

### Exercise 4: Medium — Debouncing Live Search Announcements
**Problem**: Write a JavaScript function that waits 500ms after the user stops typing in `#search-input` before updating `#live-results-count`.

<details>
<summary>View Solution</summary>

```javascript
let debounceTimer = null;

document.getElementById('search-input').addEventListener('input', function(e) {
  clearTimeout(debounceTimer);
  
  const query = e.target.value.trim();
  debounceTimer = setTimeout(() => {
    const resultsCount = query ? 14 : 0;
    const announcer = document.getElementById('live-results-count');
    announcer.textContent = `${resultsCount} search results for "${query}"`;
  }, 500);
});
```
</details>

### Exercise 5: Hard — Accessible Live Toast Queue Manager
**Problem**: Build a reusable JavaScript class `ToastManager` that renders toasts visually for sighted users and announces them via an accessible `role="status"` container without moving focus.

<details>
<summary>View Solution</summary>

```html
<div id="toast-region" role="status" aria-live="polite" aria-atomic="true" class="toast-area"></div>

<script>
  class ToastManager {
    constructor(containerId) {
      this.container = document.getElementById(containerId);
    }

    notify(message) {
      // 1. Create visual toast
      const toastEl = document.createElement('div');
      toastEl.className = 'toast-bubble';
      toastEl.textContent = message;
      this.container.appendChild(toastEl);

      // 2. Clear visual toast after 4 seconds
      setTimeout(() => {
        if (toastEl.parentNode) {
          toastEl.parentNode.removeChild(toastEl);
        }
      }, 4000);
    }
  }

  const toasts = new ToastManager('toast-region');
</script>
```
</details>

### Exercise 6: Hard — Managing `aria-busy` during Data Fetching
**Problem**: Write a fetch wrapper function that sets `aria-busy="true"` on a live feed container while an asynchronous request is pending, and toggles it back to `false` when data is rendered.

<details>
<summary>View Solution</summary>

```javascript
async function loadLiveFeed(url, containerId) {
  const container = document.getElementById(containerId);
  
  // Suppress partial screen reader announcements
  container.setAttribute('aria-busy', 'true');
  
  try {
    const response = await fetch(url);
    const data = await response.json();
    
    // Render dynamic updates
    container.innerHTML = data.items.map(item => `
      <div class="feed-item">${item.title}</div>
    `).join('');
  } catch (err) {
    container.textContent = 'Error loading latest updates.';
  } finally {
    // Release busy flag: screen reader speaks stable content
    container.setAttribute('aria-busy', 'false');
  }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **WCAG 4.1.3 (Status Messages)**: Dynamic status updates must be announced without stealing keyboard focus.
- **`aria-live="polite"`**: Screen reader waits for speech pauses before speaking (default for toasts, counters).
- **`aria-live="assertive"`**: Screen reader interrupts immediately (emergencies, session timeouts only).
- **`role="status"`**: Shorthand for `aria-live="polite"` + `aria-atomic="true"`.
- **`role="alert"`**: Shorthand for `aria-live="assertive"` + `aria-atomic="true"`.
- **Pre-rendered Container**: Container must exist in DOM before text is injected to trigger mutation events.
- **Visual Hiding**: Never use `display: none` on live regions. Use `.sr-only` CSS clipping.
- **`aria-atomic="true"`**: Forces announcement of the entire container string instead of just the mutated node.

---

## 13. Further Reading

- [W3C WCAG 2.2: Understanding Success Criterion 4.1.3 Status Messages](https://www.w3.org/WAI/WCAG22/Understanding/status-messages.html)
- [W3C WAI-ARIA 1.2: Live Region Attributes](https://www.w3.org/TR/wai-aria-1.2/#attrs_liveregions)
- [MDN Web Docs: ARIA Live Regions Guide](https://developer.mozilla.org/en-US/docs/Web/Accessibility/ARIA/ARIA_Live_Regions)
- [Sarah Higley: Playing with ARIA Live](https://sarahmhigley.com/writing/playing-with-aria-live/)

---

[Previous: 10 - Accessible Dialogs, Popovers & Menus (10-accessible-dialogs-popovers-and-menus.md)](10-accessible-dialogs-popovers-and-menus.md) | [Section Index](README.md) | [Next: 12 - Accessible Multimedia & Captions ->](12-accessible-multimedia-and-captions.md)
