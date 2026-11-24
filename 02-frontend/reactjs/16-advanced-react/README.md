# 16 — Advanced React & Architecture Primitives

This module covers advanced, expert-level architectural capabilities in React: DOM portals, imperative handles, concurrent-safe external store subscriptions, enterprise internationalization (i18n), custom host renderers via `react-reconciler`, the React Flight wire protocol powering React Server Components, and enterprise micro-frontends with Module Federation.

---

## What you'll be able to do after this module

- **Escape the DOM Hierarchy**: Use `createPortal` to render modals, tooltips, and floating menus at the document root while preserving React event bubbling.
- **Control Imperative Handles**: Expose clean, restricted APIs from child components to parents using `useImperativeHandle` and React 19's native `ref` prop.
- **Integrate Non-React Stores Safely**: Use `useSyncExternalStore` to subscribe to browser APIs (window size, online status) and external stores without visual tearing in Concurrent React.
- **Architect Scalable Internationalization**: Build multi-language, localized applications with `react-i18next`, pluralization, and right-to-left (RTL) layout switching.
- **Build Custom React Renderers**: Implement the `HostConfig` specification using `react-reconciler` to render declarative JSX to Canvas, WebGL scene graphs, or CLI terminals.
- **Deconstruct the Flight Protocol**: Understand the low-level streaming wire format (`react-server-dom`) that serializes Server Components and Client References (`$L`) across HTTP.
- **Scale with Micro-Frontends**: Decouple large organizations with Webpack 5 / Rspack Module Federation, enforcing strict React singletons and isolated error boundaries.

---

## Module Syllabus

| # | Note | Description | Key Concept / Tool |
|---|------|-------------|:---:|
| 00 | [00-portals.md](./00-portals.md) | `createPortal`, mounting outside root, event bubbling across portals | React Portals |
| 01 | [01-refs-and-imperative-handles.md](./01-refs-and-imperative-handles.md) | `useImperativeHandle`, forwarding refs in React 19, DOM manipulation | Refs & Handles |
| 02 | [02-external-stores.md](./02-external-stores.md) | `useSyncExternalStore`, tearing prevention, subscribing to external state | External Stores |
| 03 | [03-internationalization.md](./03-internationalization.md) | i18n architecture, `react-i18next`, ICU formats, RTL support | Internationalization |
| 04 | [04-custom-renderers-reconciler.md](./04-custom-renderers-reconciler.md) | `react-reconciler`, `HostConfig`, building 2D Canvas and Terminal engines | Custom Renderers |
| 05 | [05-flight-protocol-server-dom.md](./05-flight-protocol-server-dom.md) | React Flight wire format, RSC streaming serialization, `$L` references | RSC Internals |
| 06 | [06-micro-frontends-module-federation.md](./06-micro-frontends-module-federation.md) | Micro-frontends with Module Federation, shared singletons, dynamic remotes | Module Federation |

---

## Advanced Architecture Matrix

| Capability | Standard Tool | Low-Level Internal API | Primary Use Case |
| :--- | :--- | :--- | :--- |
| **DOM Escaping** | Radix Dialog / Floating UI | `createPortal()` | Modal overlays, tooltips, flyout drawers |
| **External State** | Zustand / Jotai | `useSyncExternalStore()` | Subscribing to non-React mutable stores |
| **Custom Platform** | React Three Fiber / Ink | `react-reconciler` | WebGL 3D scenes, Canvas, CLI terminals |
| **Server Streaming**| Next.js / Waku | `react-server-dom` (Flight) | Zero-bundle Server Components wire streaming |
| **Distributed Team**| Turborepo | Module Federation | Independent team builds & micro-frontends |

---

## Boundary with Other Modules

- **React Internals**: Read [`17-react-internals/`](../17-react-internals/README.md) for the Fiber work loop and reconciliation algorithm.
- **Hooks Reference**: Read [`03-hooks/`](../03-hooks/README.md) for individual hook references (`useImperativeHandle`, `useSyncExternalStore`).
- **Concurrent React**: Read [`15-concurrent-and-modern-react/`](../15-concurrent-and-modern-react/README.md) for transitions and server components.

---
