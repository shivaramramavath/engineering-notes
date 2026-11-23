# DOM & Browser Events Cheatsheet

> Quick-reference guide covering DOM tree querying, node manipulation, event listener options, keyboard/pointer event properties, and observer APIs.

| Metadata | Details |
| :--- | :--- |
| **Track** | Reference Cheatsheets (`27_cheatsheets`) |
| **Baseline** | ECMAScript 2024 / Evergreen Browsers |
| **Level** | Reference / Quick Lookup |

---

## 1. DOM Querying & Traversal

| API | Return Value | Live / Static? | Performance |
| :--- | :--- | :--- | :--- |
| `document.getElementById('id')` | Element or `null` | Single Element | Fastest ($O(1)$) |
| `element.querySelector('selector')` | First matching Element or `null` | Static | Fast |
| `element.querySelectorAll('selector')` | `NodeList` | **Static** snapshot | Linear |
| `element.getElementsByClassName('name')` | `HTMLCollection` | **Live** collection | Fast |
| `element.closest('selector')` | Nearest matching ancestor | Single Element | Traverses upwards |
| `element.matches('selector')` | Boolean (`true`/`false`) | Pure check | Fast |

---

## 2. DOM Mutation & Insertion

```javascript
// Modern Insertion APIs (replaces appendChild/insertBefore):
parent.append(nodeOrString1, nodeOrString2); // Appends to end
parent.prepend(nodeOrString);                // Prepends to start
target.before(newNode);                      // Inserts as preceding sibling
target.after(newNode);                       // Inserts as following sibling
target.replaceWith(newNode);                 // Swaps target out
target.remove();                             // Removes itself from DOM tree

// Element Dimensions & Offsets:
const rect = element.getBoundingClientRect(); // { top, left, width, height, x, y }
const scrollY = window.scrollY || window.pageYOffset;
```

---

## 3. Event Listener Configuration

```javascript
element.addEventListener(type, listener, {
  capture: false, // true = capturing phase; false = bubbling phase (default)
  once: true,     // Automatically removes listener after first invocation
  passive: true,  // Guarantees listener won't call preventDefault() (enables smooth scroll)
  signal: abortController.signal // Removes listener when aborted
});
```

---

## 4. Modern Browser Observer APIs

| Observer API | Target Monitored | Common Production Use Case |
| :--- | :--- | :--- |
| `IntersectionObserver` | Element visibility relative to viewport | Image lazy loading, infinite scroll pagination |
| `ResizeObserver` | Element box dimensions changes | Responsive component sizing (container queries) |
| `MutationObserver` | DOM node tree mutations and attributes | Web extensions, micro-frontend hydration |
| `PerformanceObserver` | Browser performance timeline metrics | Core Web Vitals telemetry (LCP, INP, CLS) |

```javascript
// Example: Intersection Observer
const observer = new IntersectionObserver((entries) => {
  entries.forEach((entry) => {
    if (entry.isIntersecting) {
      entry.target.src = entry.target.dataset.src; // Lazy load image
      observer.unobserve(entry.target);
    }
  });
}, { threshold: 0.1 });

document.querySelectorAll('img[data-src]').forEach((img) => observer.observe(img));
```
