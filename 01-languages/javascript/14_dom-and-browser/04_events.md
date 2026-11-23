# The DOM Event Architecture

> The DOM Event architecture provides an asynchronous, decoupled event-driven coordination model for web user interfaces, characterized by EventTarget dispatching, phase propagation, passive listeners, and cooperative cancellation.

---

## Metadata

| Attribute | Details |
| :--- | :--- |
| **Level** | Core / Intermediate |
| **Prerequisites** | [The DOM](./01_dom.md), [Event Loop](../12_event-loop/01_event-loop.md) |
| **Time to Read** | 24 minutes |
| **Status** | Complete |
| **Standard** | WHATWG DOM Living Standard / W3C UI Events / Baseline Widely Available |

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

Web applications are inherently event-driven: users click buttons, resize viewports, scroll containers, enter keyboard text, and submit forms, while background processes emit data arrivals and animations complete. 

Historically, DOM Level 0 provided simple inline handler properties (`element.onclick = fn`). While straightforward, Level 0 handlers had severe limitations: only a single callback could be registered per event type, there was no control over propagation direction, and handler registration tightly coupled markup to script.

The modern **DOM Level 2+ Event Architecture** provides an enterprise-grade messaging bus:
- Multiple independent listeners can observe the same event.
- Fine-grained control over capture and bubble phases.
- Advanced performance options (`passive` listeners to eliminate scroll jank).
- Integration with modern primitives like `AbortSignal` for automated listener teardown.

---

## 2. Core Concepts

### The Event Lifecycle: Capture, Target, and Bubble
When an event occurs on a deeply nested element (such as clicking a `<span>` inside a `<button>` inside a `<div>`), the event does not merely fire on the target: it travels through **three distinct phases**:

```text
               | |                       / \
---------------| |-----------------------| |---------------
1. CAPTURE     | | window                | | 3. BUBBLE
   PHASE       | | document              | |    PHASE
               | | <html>                | |
               | | <body>                | |
               | | <div>                 | |
               | v <button>              | |
---------------+-------------------------+-----------------
               | 2. TARGET PHASE (<span>)|
-----------------------------------------------------------
```

1. **Capture Phase (Trickling)**: The event descends from the `Window` object down the ancestor chain to the target's immediate parent.
2. **Target Phase**: The event reaches the target element (`event.target`).
3. **Bubble Phase**: The event ascends back up through parent ancestors to `Window`.

### `event.target` vs `event.currentTarget`
- **`event.target`**: The actual innermost DOM element that originated the event (e.g. the specific icon or text node clicked).
- **`event.currentTarget`**: The element currently processing the event handler (the element on which `.addEventListener()` was registered).

### Stopping Propagation & Preventing Default
- **`event.preventDefault()`**: Prevents the browser's default native action (e.g., following a link, submitting a form, checking a checkbox) without stopping event bubbling.
- **`event.stopPropagation()`**: Halts the event from traveling further up (or down) the DOM ancestor tree, but allows other listeners attached to the *current* element to execute.
- **`event.stopImmediatePropagation()`**: Halts tree traversal *and* immediately blocks all other listeners registered on the *same* element for this event.

---

## 3. How It Works Internally

### Event Dispatch Algorithm Execution Steps

```
 Browser detects user action (e.g. mouse click at x, y)
                     |
                     v
 1. Hit-Test: Identify deepest DOM element at coordinates -> Sets event.target
                     |
                     v
 2. Compute Propagation Path: Construct list of ancestors from Window to Target
                     |
                     v
 3. CAPTURE PHASE:
    Traverse path from Window down to Target's parent.
    Execute listeners registered with { capture: true }.
                     |
                     v
 4. TARGET PHASE:
    Execute all listeners registered on Target (regardless of capture flag).
                     |
                     v
 5. BUBBLE PHASE:
    Traverse path from Target's parent up to Window.
    Execute listeners registered with { capture: false } (the default).
                     |
                     v
 6. DEFAULT ACTION:
    If event.defaultPrevented is false, browser performs default native behavior.
```

---

## 4. Syntax & API Reference

### Registering and Removing Listeners

```javascript
// Full method signature:
// target.addEventListener(type, listener, optionsOrUseCapture)

// Basic listener
const onClick = (event) => console.log('Clicked', event.target);
element.addEventListener('click', onClick);

// Removing listener (MUST pass identical function reference!)
element.removeEventListener('click', onClick);

// Modern Options Object
element.addEventListener('scroll', onScroll, {
  capture: false,  // If true, fires during Capture Phase
  once: true,      // Automatically detaches after firing once
  passive: true,   // Informs engine handler will NEVER call preventDefault()
  signal: ac.signal// AbortSignal: listener is detached automatically when aborted!
});
```

### Event Object Inspection Properties

```javascript
event.type;             // String name: 'click', 'keydown', 'submit'
event.target;           // Deepest element that initiated the event
event.currentTarget;    // Element that owns this event listener
event.eventPhase;       // 1: CAPTURING, 2: AT_TARGET, 3: BUBBLING
event.bubbles;          // Boolean: whether this event type bubbles
event.cancelable;       // Boolean: whether preventDefault() is allowed
event.defaultPrevented; // Boolean: true if preventDefault() was invoked
event.isTrusted;        // Boolean: true if user action; false if dispatched via script
event.timeStamp;        // High-resolution timestamp of event creation
```

---

## 5. Practical Examples

### Example 1: Clean Teardown via `AbortSignal` (Zero Memory Leak)
```javascript
function attachModalKeyboardListeners(modalElement) {
  const controller = new AbortController();
  const { signal } = controller;

  // Modern pattern: pass single AbortSignal to all listeners
  window.addEventListener('keydown', (e) => {
    if (e.key === 'Escape') closeModal();
  }, { signal });

  window.addEventListener('resize', () => {
    repositionModal(modalElement);
  }, { signal });

  function closeModal() {
    modalElement.hidden = true;
    // Detaches ALL listeners bound to this signal in one single call!
    controller.abort();
    console.log('All modal event listeners safely torn down');
  }

  return { close: closeModal };
}
```

### Example 2: High-Performance Smooth Scrolling with Passive Listeners
```javascript
// Passive listeners guarantee to the browser's compositor thread
// that preventDefault() will never be called, allowing instant 60 FPS scrolling.
const container = document.getElementById('scrollable-pane');

container.addEventListener('touchstart', (e) => {
  // e.preventDefault(); // TypeError / Console warning: Ignored in passive listener!
  recordTouchStart(e.touches[0].clientX);
}, { passive: true });

container.addEventListener('scroll', () => {
  updateScrollProgressIndicator();
}, { passive: true });
```

### Example 3: Modal Backdrop Click Dismissal
```javascript
const modalBackdrop = document.querySelector('.modal-backdrop');

modalBackdrop.addEventListener('click', (event) => {
  // If user clicked the backdrop itself, close modal.
  // If user clicked inside the modal content box, ignore!
  if (event.target === event.currentTarget) {
    modalBackdrop.classList.remove('open');
  }
});
```

### Example 4: Enforcing Execution Sequence with `stopImmediatePropagation`
```javascript
const sensitiveButton = document.getElementById('delete-account-btn');

// Security Guard Listener (registered first)
sensitiveButton.addEventListener('click', (event) => {
  const isConfirmed = confirm('Are you absolutely sure you want to delete your account?');
  if (!isConfirmed) {
    // Halts other listeners attached to THIS button from firing!
    event.stopImmediatePropagation();
    event.preventDefault();
  }
});

// Business Logic Listener (registered second)
sensitiveButton.addEventListener('click', () => {
  console.log('Account deletion in progress...');
});
```

### Example 5: Custom Synthetic Event Dispatcher
```javascript
class ShoppingCart extends EventTarget {
  #items = [];

  addItem(item) {
    this.#items.push(item);

    // Dispatch custom event to any interested listeners
    this.dispatchEvent(new CustomEvent('cart:updated', {
      detail: {
        totalItems: this.#items.length,
        addedItem: item,
      },
    }));
  }
}

// Usage
const cart = new ShoppingCart();
cart.addEventListener('cart:updated', (e) => {
  console.log(`Cart now has ${e.detail.totalItems} items. Added: ${e.detail.addedItem.name}`);
});
cart.addItem({ name: 'Mechanical Keyboard', price: 150 });
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **Anonymous Function Leak in `removeEventListener`**: Passing an anonymous function or arrow function to `addEventListener` makes it impossible to remove later via `removeEventListener()`, because each invocation produces a new function reference. Always retain a named reference or use `{ signal }`.
2. **`isTrusted` Security Guard**: Synthetic events dispatched via JavaScript (`element.dispatchEvent()`) have `event.isTrusted === false`. Real user hardware events have `event.isTrusted === true`. Banking and authentication forms can inspect `isTrusted` to protect against automated clickjacking scripts.
3. **Passive Listener `preventDefault` Crash**: Invoking `event.preventDefault()` inside a passive listener does not prevent the default behavior; modern browsers log a warning or ignore the call completely.
4. **Target Phase Order Inversion**: In modern specifications, during the Target Phase (when the event reaches the target element itself), listeners execute in **registration order**, regardless of whether `capture` was set to true or false.

---

## 7. Common Mistakes & Anti-Patterns

| Mistake | Incorrect Pattern | Correct Solution | Why It Fails |
| :--- | :--- | :--- | :--- |
| **Anonymous Removal** | `el.removeEventListener('click', () => {})` | Pass stored reference or use `{ signal }` | Function reference does not match; listener remains active. |
| **Missing Passive on Scroll** | `window.addEventListener('scroll', fn)` | Add `{ passive: true }` | Forces browser compositor to wait on main thread, causing scroll stutter. |
| **`target` vs `currentTarget`** | Assuming `e.target` is the container | Use `e.currentTarget` for container | `e.target` could be a nested `<span>`, `<img>`, or `<i>` tag. |
| **Using `return false`** | `el.onclick = () => { return false; }` | Use `event.preventDefault()` | Inline returning false is legacy DOM Level 0; does not work in addEventListener. |
| **Forgetting Listener Teardown** | Leaving window listeners on unmounted view | Use `AbortSignal` or detach on unmount | Memory leaks: closures retain unmounted DOM and view state. |
| **Misunderstanding StopPropagation** | Assuming it stops other handlers on same tag | Use `event.stopImmediatePropagation()` | `stopPropagation()` only halts ancestor tree propagation. |

---

## 8. Performance Characteristics

- **Passive Listeners & Compositor Thread**: When touch/wheel listeners are marked `{ passive: true }`, the browser compositor thread can scroll the page immediately without blocking to check if JavaScript calls `preventDefault()`.
- **Memory Footprint of Event Listeners**: Each listener attached to an element allocates an internal C++ `RegisteredEventListener` node. Attaching individual listeners to 10,000 table cells wastes megabytes of RAM; use event delegation instead.

---

## 9. Engine Internals

1. **Blink `EventTarget` C++ Vector**: In Blink, every `EventTarget` maintains an `EventListenerMap` backed by a vector of event listeners sorted by phase.
2. **Hit-Testing**: When user clicks the screen, the compositor coordinates with the layout tree to perform **Hit Testing** (`LayoutView::HitTest`), computing the exact `blink::Node` under the mouse cursor.

---

## 10. Best Practices & Design Guidelines

1. **Use `{ signal }` for Multi-Listener Teardown**: Group related listeners under a single `AbortController` signal to clean up all event listeners with one `.abort()` call.
2. **Always Mark Scroll/Touch Listeners as `{ passive: true }`**: Preserve 60/120 FPS scrolling performance across touch and mouse wheel events.
3. **Use `event.currentTarget` When Delegating**: Always reference `event.currentTarget` to access the listening container reliably.
4. **Check `event.isTrusted` in High-Security Handlers**: Ensure critical purchase or delete clicks were initiated by a genuine user hardware interaction.

---

## 11. Real-World Case Study

### Resilient Toast Notification Queue with Gesture Dismissal

A mobile notification banner appears at the top of the viewport. Users can swipe up to dismiss it. Touch events must be smooth, cancellable, and automatically cleaned up when the banner dismisses or times out.

```javascript
class ToastNotification {
  #element;
  #controller = new AbortController();
  #startY = 0;
  #currentY = 0;

  constructor(message, durationMs = 5000) {
    this.#element = document.createElement('div');
    this.#element.className = 'toast-banner';
    this.#element.textContent = message;
    document.body.appendChild(this.#element);

    this.#bindGestures();
    this.#scheduleAutoDismiss(durationMs);
  }

  #bindGestures() {
    const { signal } = this.#controller;

    // Passive touchstart for instant response
    this.#element.addEventListener('touchstart', (e) => {
      this.#startY = e.touches[0].clientY;
      this.#currentY = this.#startY;
    }, { passive: true, signal });

    // Passive touchmove
    this.#element.addEventListener('touchmove', (e) => {
      this.#currentY = e.touches[0].clientY;
      const deltaY = this.#currentY - this.#startY;

      // Only allow swiping UP to dismiss
      if (deltaY < 0) {
        this.#element.style.transform = `translateY(${deltaY}px)`;
        this.#element.style.opacity = `${1 + deltaY / 100}`;
      }
    }, { passive: true, signal });

    // Touchend
    this.#element.addEventListener('touchend', () => {
      const deltaY = this.#currentY - this.#startY;
      if (deltaY < -50) {
        this.dismiss();
      } else {
        // Snap back
        this.#element.style.transform = '';
        this.#element.style.opacity = '';
      }
    }, { signal });
  }

  #scheduleAutoDismiss(ms) {
    setTimeout(() => this.dismiss(), ms);
  }

  dismiss() {
    // 1. Abort all attached gesture listeners atomically!
    this.#controller.abort();

    // 2. Animate out and remove element
    this.#element.style.transition = 'transform 0.2s ease, opacity 0.2s ease';
    this.#element.style.transform = 'translateY(-100%)';
    this.#element.style.opacity = '0';

    setTimeout(() => {
      this.#element.remove();
    }, 200);
  }
}
```

---

## 12. Interview Questions

<details>
<summary><strong>1. What are the three phases of DOM event propagation?</strong></summary>

1. **Capture Phase**: The event descends from the `Window` object down through the ancestor tree toward the target.
2. **Target Phase**: The event reaches the target element (`event.target`).
3. **Bubble Phase**: The event ascends back up from the target through its ancestors to the `Window` object.
</details>

<details>
<summary><strong>2. What is the difference between <code>event.target</code> and <code>event.currentTarget</code>?</strong></summary>

`event.target` is the innermost element that actually initiated or received the event (e.g. an icon inside a button). `event.currentTarget` is the element to which the event handler is currently attached.
</details>

<details>
<summary><strong>3. What does <code>event.preventDefault()</code> do?</strong></summary>

It prevents the browser from executing the native default action associated with the event (e.g. following an `<a>` link or submitting a `<form>`). It does *not* stop the event from bubbling up the DOM tree.
</details>

<details>
<summary><strong>4. How does <code>event.stopPropagation()</code> differ from <code>event.stopImmediatePropagation()</code>?</strong></summary>

`event.stopPropagation()` prevents the event from propagating further up (or down) the ancestor tree. `event.stopImmediatePropagation()` does that *and* additionally prevents any other listeners registered on the *same* element from executing.
</details>

<details>
<summary><strong>5. Why are <code>passive: true</code> event listeners critical for mobile scrolling performance?</strong></summary>

By default, touch and wheel listeners are synchronous: the browser's compositor thread must wait for JavaScript to execute to check if `preventDefault()` was called before scrolling the viewport. Marking a listener `passive: true` promises that `preventDefault()` will not be called, enabling the compositor to scroll smoothly at 60/120 FPS without waiting for the main thread.
</details>

<details>
<summary><strong>6. How can an <code>AbortSignal</code> be used to manage event listeners?</strong></summary>

Passing `{ signal: controller.signal }` in `addEventListener` options binds the listener's lifecycle to the signal. When `controller.abort()` is called, the browser automatically unregisters and tears down the listener without needing `removeEventListener`.
</details>

<details>
<summary><strong>7. What is <code>event.isTrusted</code>, and why is it important for security?</strong></summary>

`event.isTrusted` is a read-only boolean property that is `true` if the event was generated by a real user interaction (such as a physical mouse click or key press) and `false` if dispatched programmatically via script (`element.dispatchEvent()`).
</details>

<details>
<summary><strong>8. Why does <code>removeEventListener('click', () => {})</code> fail to remove a listener?</strong></summary>

Because an arrow function creates a new anonymous function object in memory. `removeEventListener` requires an exact reference match to the function originally passed to `addEventListener`.
</details>

---

## 13. Exercises & Challenges

### Challenge 1: Single-Execution One-Time Event Listener Polyfill
Implement a custom function `addOnceEventListener(target, type, callback)` that attaches an event listener that executes only once, without using the native `{ once: true }` option.

<details>
<summary>View Solution</summary>

```javascript
function addOnceEventListener(target, type, callback) {
  function handler(event) {
    target.removeEventListener(type, handler);
    callback.call(this, event);
  }

  target.addEventListener(type, handler);
}
```
</details>

### Challenge 2: Double-Click Detector with Custom Event
Implement a class `DoubleTapDetector` that listens for single clicks on an element and dispatches a synthetic custom event `'doubletap'` only if two consecutive clicks occur within 300 milliseconds.

<details>
<summary>View Solution</summary>

```javascript
class DoubleTapDetector {
  #lastClickTime = 0;
  #thresholdMs = 300;

  constructor(element) {
    element.addEventListener('click', (event) => {
      const now = performance.now();
      const elapsed = now - this.#lastClickTime;

      if (elapsed > 0 && elapsed <= this.#thresholdMs) {
        element.dispatchEvent(new CustomEvent('doubletap', {
          bubbles: true,
          detail: { originalEvent: event },
        }));
        this.#lastClickTime = 0; // Reset
      } else {
        this.#lastClickTime = now;
      }
    });
  }
}
```
</details>
