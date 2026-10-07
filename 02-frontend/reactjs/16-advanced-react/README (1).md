# 16 — Advanced React

Most React code stays inside React's model: state in, UI out. This folder covers the places where your components have to **step outside it**: rendering into a different part of the DOM, talking to DOM elements directly, subscribing to data React doesn't own, and adapting the UI to different languages and regions.

These are not everyday tools. Each is an **escape hatch or integration point**, and each has a specific job and specific ways to go wrong.

```text
React tree  ───────────────►  DOM
   │  Portals (00)               render here instead of in my DOM parent
   │  Refs + imperative handles (01)    reach in and call DOM methods
   │
   ▲  External stores (02)       read from things React doesn't manage
   │
   └─ Internationalization (03)  same UI, many languages and locales
```

## Prerequisites

- [Hooks](../03-hooks/README.md): `useRef`, `useEffect`, `useLayoutEffect`
- [Concurrent rendering](../15-concurrent-and-modern-react/00-concurrent-rendering.md): explains *why* external stores need a special hook
- [Accessibility basics](../08-accessibility/README.md): portals and imperative focus both live or die on accessibility

## Contents

| # | File | What you'll learn |
|---|------|-------------------|
| 00 | [Portals](./00-portals.md) | `createPortal`, events and context through portals, when to prefer `<dialog>` |
| 01 | [Refs and imperative handles](./01-refs-and-imperative-handles.md) | DOM refs, callback refs, `useImperativeHandle`, `ref` as a prop |
| 02 | [External stores](./02-external-stores.md) | `useSyncExternalStore`, writing a store, browser APIs as stores |
| 03 | [Internationalization](./03-internationalization.md) | Translations, plurals, `Intl`, RTL, locale handling |

## Suggested order

The files are independent. Read 00 and 01 together if you build component libraries, 02 if you integrate non-React state or browser APIs, 03 when you need more than one language.

## The common rule

For every tool here, ask first: **can I do this with props, state, or composition?** Usually yes. Reach for these only when React's declarative model genuinely can't express the need.
