# React DevTools

React DevTools is a browser extension that lets you see your app as a tree of components rather than a pile of DOM nodes. You can inspect props and state, edit them live, find out why something rendered, and record performance profiles. Installing it early pays off in every later chapter.

## Prerequisites

[`02-vite.md`](./02-vite.md) — a running React app.

---

## Installing

Install **React Developer Tools** for your browser (Chrome, Edge, Firefox) from its extension store. There is also a standalone app for environments without a browser extension, such as React Native.

After installing, open your app's dev server and open the browser DevTools. Two new tabs appear: **Components** and **Profiler**. If the tabs don't show, reload the page; they only appear on pages running React.

> The extension's icon is colored on pages using a development build of React, and a different state on production builds. Use the dev server for full debugging features.

---

## The Components tab

Shows your component tree on the left and details for the selected component on the right.

For the selected component you can see:

- **Props** passed to it
- **Hooks** it uses, including current state values (`State`, `Ref`, `Context`, `Memo`, and so on)
- **Rendered by** — the chain of parent components
- **Source** — jump to the file in your editor, when source maps allow

### Things to try

1. **Select a component** and read its props and hooks.
2. **Edit state or props live** — double-click a value and change it; the UI updates immediately. Great for testing edge cases without writing code.
3. **Click the target icon** (select element in page) and pick something on screen to find the component that rendered it.
4. **Search** the tree by component name.
5. **Log to console** — right-click a component and choose to store it as a global variable (`$r`) to experiment in the console.

---

## Seeing re-renders

In the DevTools settings (gear icon), enable **Highlight updates when components render**. Interact with the app and components flash when they re-render. A flash on something that shouldn't have changed is your first clue to a performance problem.

Combined with the Profiler, it also answers a question you'll ask constantly: *why did this render?*

---

## The Profiler tab

The Profiler records what rendered, how long it took, and (when enabled) why.

Basic workflow:

1. Click **Record**.
2. Interact with the app (type, click, navigate).
3. Click **Stop**.
4. Inspect the **flame graph** (each bar is a component, wider means slower) and the **ranked** view (slowest first).

In the profiler settings, enable recording **why each component rendered** — it reports whether props, state, hooks, or a parent caused a render.

Deep profiling workflow in `../14-performance/00-profiling-and-measuring.md`.

---

## Filtering noise

Large apps include many wrapper components from libraries. Use the settings' **Components → Filters** to hide host DOM elements and library internals so the tree stays readable.

---

## Limitations

- **Production builds** hide most details: names are minified and some features are disabled. Debug against the dev server.
- **Profiling numbers from dev mode are inflated** because development builds do extra checks and `StrictMode` renders twice. Use them to find *relative* hot spots, not absolute timings. Measure real performance on a production build.
- The tree shows components, not necessarily the DOM — use the browser's Elements panel for markup and CSS.

---

## Common mistakes

- **Profiling in dev mode and trusting absolute times** — compare relative cost, then verify on a production build.
- **Not enabling "why did this render"** — you get durations but no explanation.
- **Looking at the Elements panel when you need Components** — props and state live in the Components tab.
- **Forgetting to reload after installing** — the tabs won't appear until the page reloads.

## Quick summary

- DevTools adds **Components** and **Profiler** tabs to browser DevTools
- Inspect and live-edit props and state in the Components tab
- Turn on highlight updates to spot unnecessary re-renders
- Record in the Profiler, and enable "why did this render" for explanations
- Dev-mode timings are for relative comparison only

## Next

**[`05-typescript-and-linting-setup.md`](./05-typescript-and-linting-setup.md)** configures TypeScript, ESLint, and Prettier so mistakes are caught in your editor. For debugging beyond DevTools, see `../18-testing-and-debugging/07-debugging.md`.
