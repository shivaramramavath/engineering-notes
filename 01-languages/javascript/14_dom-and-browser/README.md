# 14 · DOM and Browser

In the browser, JavaScript's main job is to **read and change the page** and to **react to the user**. The browser exposes the page as a tree of objects (the **DOM**) and offers many platform APIs for events, storage, observation, navigation and device features.

```
HTML text ──parse──► DOM tree ──style/layout──► pixels
                       ▲   │
        your JS reads/writes   events flow back to your JS
```

## Reading order

| # | File | You will learn |
|---|------|----------------|
| 1 | [DOM](./01_dom.md) | The tree, node types, traversal, live vs static collections, rendering basics |
| 2 | [Selectors](./02_selectors.md) | `querySelector`, `closest`, `matches`, collections |
| 3 | [Element Manipulation](./03_element-manipulation.md) | Create, insert, remove, attributes, classes, styles, safe HTML |
| 4 | [Events](./04_events.md) | Listeners, capture/bubble, the event object, custom events |
| 5 | [Event Delegation](./05_event-delegation.md) | One listener for many elements, dynamic content |
| 6 | [Forms](./06_forms.md) | `FormData`, validation API, inputs, files |
| 7 | [Observers](./07_observers.md) | `IntersectionObserver`, `MutationObserver`, `ResizeObserver` |
| 8 | [Browser Storage](./08_browser-storage.md) | `localStorage`, cookies, IndexedDB, quotas, security |
| 9 | [Web APIs](./09_web-apis.md) | History, Clipboard, Geolocation, Notifications, permissions |
| 10 | [Web Components](./10_web-components.md) | Custom elements, Shadow DOM, templates and slots |

## Where the code runs

| Context | Has DOM? | Notes |
|---------|----------|-------|
| Main thread (`window`) | Yes | one per tab/frame; heavy work blocks the UI |
| Web Worker | No | no `document`/`window`, communicates by messages |
| Service Worker | No | network proxy, offline, push |
| Node.js | No | use `jsdom`/`happy-dom` for DOM-like testing |

## Goal

By the end you can find and change elements efficiently, handle events robustly, validate forms, observe the page, persist data and use common browser APIs safely.

## Prerequisites

- [Event Loop](../12_event-loop/00_README.md) (rendering and tasks)
- [Modules](../13_modules/01_es-modules.md) (`<script type="module">`)

**Next:** [DOM](./01_dom.md)
