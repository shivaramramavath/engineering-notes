# Event Delegation

> Event delegation is an architectural performance pattern that leverages DOM event bubbling to manage events across hundreds or thousands of child elements using a single, centralized parent listener.

---

## Metadata

| Attribute | Details |
| :--- | :--- |
| **Level** | Core / Intermediate |
| **Prerequisites** | [The DOM](./01_dom.md), [DOM Events](./04_events.md) |
| **Time to Read** | 22 minutes |
| **Status** | Complete |
| **Standard** | WHATWG DOM Living Standard / Baseline Widely Available |

---

## Table of Contents

1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & API Reference](#4-syntax--api-reference)
5. [Practical Examples](#5-practical-examples)
6. [Edge Cases, Pitfalls & Gotchas](#6-edge-cases-pitfalls--gotchas)
7. [Common Mistakes & Anti-Patterns](#7-common-mistakes--anti-patterns)
8. [Performance Characteristics](#8-performance-characteristics)
9. [Engine Internals](#9-engine-internals)
10. [Best Practices & Design Guidelines](#10-best-practices--design-guidelines)
11. [Real-World Case Study](#11-real-world-case-study)
12. [Interview Questions](#12-interview-questions)
13. [Exercises & Challenges](#13-exercises--challenges)

---

## 1. What & Why

In data-rich web applications—such as e-commerce product grids, infinite scrolling feeds, and virtualized data tables—a container often holds hundreds or thousands of interactive items. Attaching individual event listeners to every single button or row introduces major architectural issues:
1. **Memory Bloat**: Each listener consumes heap memory in both the V8 engine and the browser's C++ DOM representation.
2. **Dynamic Elements**: When new items are loaded via AJAX or rendered dynamically, new listeners must be manually attached, increasing code complexity and bug surface area.
3. **Memory Leaks**: Removing items from the DOM without detaching listeners can retain closures and orphaned references in memory.

**Event Delegation** eliminates these problems by exploiting the DOM's native **bubbling phase**. Instead of binding listeners to every child, a single listener is attached to a shared ancestor. When a user interacts with a child, the event bubbles up to the parent, which inspects `event.target` (using `element.closest()`) to handle the action.

---

## 2. Core Concepts

### Bubbling-Based Dispatch
Event delegation relies on the invariant that user interaction events (`click`, `keydown`, `input`, `change`) bubble up the ancestor chain:

```text
+-------------------------------------------------------------+
| Container: <ul id="todo-list">                              |
|   Attach ONE listener here: list.addEventListener('click')  |
|                                                             |
|   ├── Item 1: <li><button class="delete">X</button></li>    |
|   ├── Item 2: <li><button class="delete">X</button></li>    |
|   └── Item N: <li><button class="delete">X</button></li>    |
+-------------------------------------------------------------+
               ▲
               │ Click bubbles upward
```

### The Nested Element Problem: Why `target.matches()` Is Insufficient
Buttons often contain internal elements, such as SVG icons or `<span>` labels:
```html
<button class="btn-delete">
  <svg><path d="..." /></svg>
  <span>Delete</span>
</button>
```
If the user clicks the icon, `event.target` is the `<svg>` or `<path>`, **not** the `<button>`. A naive check like `event.target.matches('.btn-delete')` returns `false`!
The modern solution is **`event.target.closest(selector)`**, which climbs the ancestor chain to find the matching button element.

---

## 3. How It Works Internally

### Event Traversal in Delegation

```
 User clicks <svg> inside <button class="delete"> inside <div id="table">
                                    │
                                    ▼
 Hit-test sets event.target = <svg>
                                    │
                                    ▼
 Bubble phase ascends to <div id="table">
                                    │
                                    ▼
 Delegated listener on <div id="table"> executes:
                                    │
                                    ▼
 [1] Check: const btn = event.target.closest('button.delete');
     - Starts at <svg> (No match)
     - Moves to <button class="delete"> (MATCH FOUND!)
                                    │
                                    ▼
 [2] Check boundary: Does container contain btn?
     container.contains(btn) === true
                                    │
                                    ▼
 [3] Execute button action: deleteRow(btn.dataset.id);
```

---

## 4. Syntax & API Reference

### Canonical Event Delegation Pattern

```javascript
container.addEventListener('click', (event) => {
  // 1. Find nearest matching interactive element
  const targetElement = event.target.closest('.action-button');

  // 2. Guard: verify match exists AND is within this container
  if (!targetElement || !container.contains(targetElement)) {
    return;
  }

  // 3. Extract metadata and execute action
  const action = targetElement.dataset.action;
  handleAction(action, targetElement);
});
```

### Non-Bubbling Events & Their Bubbling Counterparts

Not all DOM events bubble! For events that do not bubble, delegation requires using either the **Capture Phase** or bubbling alternative event types:

| Non-Bubbling Event | Bubbling Alternative | Delegation Strategy |
| :--- | :--- | :--- |
| `focus` | `focusin` | Use `focusin` or `{ capture: true }` |
| `blur` | `focusout` | Use `focusout` or `{ capture: true }` |
| `mouseenter` | `mouseover` | Use `mouseover` (caution: bubbles through all descendants) |
| `mouseleave` | `mouseout` | Use `mouseout` |
| `load` / `error` (images) | None | Must use `{ capture: true }` on container |

---

## 5. Practical Examples

### Example 1: Action Dispatcher on Data Table
```javascript
const table = document.getElementById('users-table');

table.addEventListener('click', (event) => {
  const btn = event.target.closest('button[data-action]');
  if (!btn || !table.contains(btn)) return;

  const row = btn.closest('tr');
  const userId = row.dataset.userId;
  const action = btn.dataset.action;

  switch (action) {
    case 'edit':
      openUserEditor(userId);
      break;
    case 'delete':
      confirmDeleteUser(userId, row);
      break;
    case 'promote':
      promoteUser(userId);
      break;
  }
});
```

### Example 2: Delegating Form Validation with `focusin` and `focusout`
```javascript
const form = document.getElementById('registration-form');

// focusin and focusout bubble; standard focus and blur DO NOT!
form.addEventListener('focusout', (event) => {
  const input = event.target;
  if (!input.matches('input[data-validate]')) return;

  validateField(input);
});

function validateField(input) {
  const isValid = input.checkValidity();
  input.classList.toggle('is-invalid', !isValid);
  input.classList.toggle('is-valid', isValid);
}
```

### Example 3: Handling Dynamically Injected Elements Seamlessly
```javascript
const feed = document.getElementById('infinite-feed');

// Attach listener ONCE upfront
feed.addEventListener('click', (event) => {
  const likeBtn = event.target.closest('.btn-like');
  if (likeBtn && feed.contains(likeBtn)) {
    toggleLike(likeBtn);
  }
});

// Asynchronously injecting 20 new posts later...
async function loadMorePosts() {
  const posts = await fetch('/api/posts').then(r => r.json());
  for (const post of posts) {
    const postEl = document.createElement('article');
    postEl.innerHTML = `<h3>${post.title}</h3><button class="btn-like">Like</button>`;
    feed.appendChild(postEl);
    // ZERO event listener attachment needed here! Works automatically!
  }
}
```

### Example 4: Image Error Handling via Capture Phase Delegation
```javascript
// Image 'error' events DO NOT bubble.
// We must set { capture: true } to delegate error events at container level!
const gallery = document.getElementById('photo-gallery');

gallery.addEventListener('error', (event) => {
  if (event.target.tagName === 'IMG') {
    console.warn(`Image failed to load: ${event.target.src}`);
    event.target.src = '/images/placeholder.png'; // Fallback image
  }
}, { capture: true }); // Intercepts event during Capture Phase!
```

### Example 5: Generic Delegation Helper Utility
```javascript
function delegate(container, eventType, selector, handler) {
  container.addEventListener(eventType, (event) => {
    const matchedEl = event.target.closest(selector);
    if (matchedEl && container.contains(matchedEl)) {
      handler.call(matchedEl, event, matchedEl);
    }
  });
}

// Usage
delegate(document.body, 'click', '.modal-close', (event, btn) => {
  console.log('Closed modal from button:', btn);
});
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **`event.stopPropagation()` Breaks Delegation**: If an intermediate child element calls `event.stopPropagation()`, the event never bubbles up to the parent container, silently breaking delegated handlers.
2. **Boundary Containment Check**: If `.closest(selector)` matches an element that lies *outside* your container (e.g., matching a parent card when clicking an element near the boundary), the listener executes improperly. Always verify `container.contains(targetElement)`.
3. **SVG `target` Issues**: In older browsers or specialized SVG hierarchies, `event.target` inside an SVG element may be an `SVGAnimatedString` or non-standard node. Using `.closest()` safely handles modern SVG nodes.
4. **Synthetic Click Events from Keyboard**: Pressing `Enter` or `Space` on a focused button dispatches a synthetic `click` event with `clientX: 0, clientY: 0`. Delegated click listeners receive these events identically to mouse clicks.

---

## 7. Common Mistakes & Anti-Patterns

| Mistake | Incorrect Pattern | Correct Solution | Why It Fails |
| :--- | :--- | :--- | :--- |
| **Using `target.matches()`** | `if (e.target.matches('.btn'))` | `const btn = e.target.closest('.btn')` | Fails if user clicks an icon or span nested inside the button. |
| **Delegating `focus`/`blur` on Bubble**| `container.addEventListener('focus', fn)` | Use `focusin` or `{ capture: true }` | `focus` and `blur` do not bubble. |
| **Attaching Delegation to `document`**| `document.addEventListener('click', ...)`| Scope listener to nearest static container | Increases overhead: evaluates selector on every page click. |
| **Stopping Propagation Unnecessarily**| Calling `e.stopPropagation()` in child | Only stop propagation if strictly required | Breaks higher-level analytics, tracking, and delegation. |
| **Missing Boundary Check** | Assuming `closest()` found item inside container | Verify `container.contains(matchedEl)` | Can match ancestors outside the listening component. |
| **Re-attaching in Loop** | Attaching delegated listener on every data fetch | Attach listener once during component initialization | Duplicates event execution count on every load. |

---

## 8. Performance Characteristics

- **Memory Overhead**: 1 delegated listener replaces 5,000 individual closures, saving several megabytes of heap memory in data-heavy tables.
- **Garbage Collection Optimization**: Virtualized lists can discard and replace DOM nodes without worrying about detached event listener memory retention.
- **Selector Matching Cost**: `element.closest()` climbs a short ancestor chain (usually 1–4 hops), executing in $< 1\mu\text{s}$ in modern engines.

---

## 9. Engine Internals

1. **Blink Event Path Allocation**: When an event occurs, Blink allocates a `blink::EventPath` vector containing all ancestors.
2. **Early Exit in Hit-Testing**: Delegated listeners allow the browser's event dispatcher to bypass allocating listener structures for thousands of individual DOM child nodes.

---

## 10. Best Practices & Design Guidelines

1. **Use `data-action` Attributes for Delegated Buttons**: Identify interactive buttons via explicit `data-action="delete"` attributes rather than styling classes.
2. **Anchor Listeners to Stable Containers**: Attach delegated listeners to static containers that are never removed or destroyed during normal UI re-renders.
3. **Combine with Keyboard Accessibility**: Ensure interactive delegated elements are natively focusable (`<button>`, `<a href>`) or have `role="button"` and `tabindex="0"`.
4. **Use `focusin` / `focusout` for Form Delegation**: When delegating input focus events, always use `focusin` and `focusout`.

---

## 11. Real-World Case Study

### High-Volume Infinite-Scroll E-Commerce Catalog

An e-commerce mobile site renders 10,000 product cards. Each card contains "Quick Add to Cart", "Add to Wishlist", "Expand Image Gallery", and "Share" buttons. Naive per-button listener attachment caused high memory consumption and 400ms stutter during infinite scroll loads.

```javascript
class ProductCatalogController {
  #catalogContainer;

  constructor(containerElement) {
    this.#catalogContainer = containerElement;
    this.#bindDelegatedEvents();
  }

  #bindDelegatedEvents() {
    this.#catalogContainer.addEventListener('click', (event) => {
      // 1. Locate nearest action button
      const actionButton = event.target.closest('[data-catalog-action]');
      if (!actionButton || !this.#catalogContainer.contains(actionButton)) {
        return;
      }

      // 2. Identify parent product card
      const card = actionButton.closest('.product-card');
      if (!card) return;

      const productId = card.dataset.productId;
      const action = actionButton.dataset.catalogAction;

      event.preventDefault();

      // 3. Dispatch to appropriate domain action
      switch (action) {
        case 'quick-add':
          this.#addToCart(productId, actionButton);
          break;
        case 'wishlist':
          this.#toggleWishlist(productId, actionButton);
          break;
        case 'gallery-next':
          this.#rotateGallery(card, 1);
          break;
      }
    });
  }

  #addToCart(productId, btn) {
    btn.disabled = true;
    btn.textContent = 'Adding...';
    // API call and cart drawer notification...
    setTimeout(() => {
      btn.disabled = false;
      btn.textContent = 'Added!';
    }, 600);
  }

  #toggleWishlist(productId, btn) {
    btn.classList.toggle('active');
  }

  #rotateGallery(card, direction) {
    const img = card.querySelector('.product-thumb');
    // Rotate image thumbnails smoothly
  }
}

// Initialized ONCE on the container
new ProductCatalogController(document.getElementById('catalog-grid'));
```

---

## 12. Interview Questions

<details>
<summary><strong>1. What is Event Delegation and how does it leverage DOM event bubbling?</strong></summary>

Event delegation is a pattern where an event listener is attached to a parent container instead of individual child elements. When an event fires on a child, it bubbles up to the parent container, which inspects the event's target to identify which child originated the interaction and executes the appropriate logic.
</details>

<details>
<summary><strong>2. Why is <code>event.target.matches(selector)</code> prone to bugs in event delegation?</strong></summary>

If the target element contains child elements (such as an `<i>` or `<span>` inside a `<button>`), clicking the child makes `event.target` point to the child, which does not match the button's selector. Using `event.target.closest(selector)` solves this by searching upward to find the button.
</details>

<details>
<summary><strong>3. Which common DOM events do NOT bubble, and how do you delegate them?</strong></summary>

Events like `focus`, `blur`, `load`, and `error` do not bubble. To delegate them, you can either:
1. Use bubbling alternative events (`focusin` instead of `focus`, `focusout` instead of `blur`).
2. Register the delegated listener during the **Capture Phase** using `{ capture: true }`.
</details>

<details>
<summary><strong>4. What happens to event delegation if a child element calls <code>event.stopPropagation()</code>?</strong></summary>

The event immediately stops bubbling up the DOM tree and will never reach the parent container, preventing the delegated event listener from firing.
</details>

<details>
<summary><strong>5. Why is it important to check <code>container.contains(matchedElement)</code> when using <code>closest()</code>?</strong></summary>

`closest()` searches all ancestors up to the `<html>` root. If an element outside the container matches the selector, `closest()` could return an element that does not belong to the delegated container. Checking `container.contains()` ensures the matched element is a legitimate child.
</details>

<details>
<summary><strong>6. How does event delegation simplify memory management for dynamic content?</strong></summary>

When dynamic elements are added or removed (e.g. infinite scroll or single-page app view updates), no new listeners need to be attached and no old listeners need to be manually detached, preventing memory leaks and reducing CPU overhead.
</details>

<details>
<summary><strong>7. When should you AVOID using event delegation?</strong></summary>

Avoid event delegation for high-frequency events that fire dozens of times per second (such as `mousemove` or `scroll`) if the selector matching logic inside the handler is computationally expensive, or for components with strictly isolated, single-button lifecycles.
</details>

<details>
<summary><strong>8. What is the difference between <code>event.target</code> and <code>event.currentTarget</code> in a delegated listener?</strong></summary>

`event.target` is the deepest element that was actually clicked. `event.currentTarget` is the parent container element to which the delegated listener was attached.
</details>

---

## 13. Exercises & Challenges

### Challenge 1: Implement Delegated Link Navigation Interceptor
Write an event delegation script attached to `document.body` that intercepts clicks on all internal links (`<a href="...">`), prevents the full page reload if the link is on the same origin, and invokes `router.navigate(url)`.

<details>
<summary>View Solution</summary>

```javascript
function interceptInternalLinks(router) {
  document.body.addEventListener('click', (event) => {
    const link = event.target.closest('a[href]');
    if (!link || !document.body.contains(link)) return;

    // Ignore clicks with modifier keys (new tab open)
    if (event.metaKey || event.ctrlKey || event.shiftKey || event.altKey) return;
    // Ignore links targeting external windows
    if (link.target && link.target !== '_self') return;

    const url = new URL(link.href, window.location.origin);

    // Verify same-origin
    if (url.origin === window.location.origin) {
      event.preventDefault();
      router.navigate(url.pathname + url.search + url.hash);
    }
  });
}
```
</details>

### Challenge 2: Implement a Delegated Drag-and-Drop List Reorder
Create a lightweight reorderable list using event delegation on the `<ul>` element that listens to delegated `dragstart` and `dragover` events without attaching listeners to individual `<li>` items.

<details>
<summary>View Solution</summary>

```javascript
function setupDelegatedReorder(listElement) {
  let draggedItem = null;

  listElement.addEventListener('dragstart', (e) => {
    const item = e.target.closest('li[draggable="true"]');
    if (!item) return;
    draggedItem = item;
    e.dataTransfer.effectAllowed = 'move';
  });

  listElement.addEventListener('dragover', (e) => {
    e.preventDefault();
    const targetItem = e.target.closest('li[draggable="true"]');
    if (!targetItem || targetItem === draggedItem) return;

    const rect = targetItem.getBoundingClientRect();
    const isAfter = e.clientY > rect.top + rect.height / 2;
    targetItem[isAfter ? 'after' : 'before'](draggedItem);
  });

  listElement.addEventListener('dragend', () => {
    draggedItem = null;
  });
}
```
</details>
