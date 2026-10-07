# React Compiler

The React Compiler is a **build-time tool that automatically memoizes your components and values**. It analyzes your code and inserts the equivalent of `memo`, `useMemo`, and `useCallback` where it can prove they're safe and useful, so you don't have to write them by hand ([memoization](../14-performance/02-memoization.md)).

Its goal is to make the *default* way of writing React (plain, straightforward components) fast, instead of making you manually optimize.

> The compiler is a separate package that moves faster than React itself. The setup steps below are the shape of it, but confirm exact package names, options, and ESLint integration in the [React Compiler docs](https://react.dev/learn/react-compiler) for your versions.

## What it does

Without it, a re-rendering parent re-renders every child, and you memoize by hand to stop it:

```tsx
// Manual
const visible = useMemo(() => items.filter(matches(query)), [items, query])
const onSelect = useCallback((id: string) => select(id), [select])
const Row = memo(RowImpl)
```

With the compiler, you write:

```tsx
function List({ items, query }: Props) {
  const visible = items.filter(matches(query))
  const onSelect = (id: string) => select(id)
  return visible.map((item) => <Row key={item.id} item={item} onSelect={onSelect} />)
}
```

and the compiler produces code that **caches `visible`, `onSelect`, and the JSX** keyed by their inputs, recomputing only when an input changes. It can even memoize in places hand-written hooks can't, like after an early return or inside conditionals.

Under the hood it caches values in a per-component memo cache and skips work when the inputs are the same. It's **fine-grained**: individual values and JSX elements within a component, not just whole components.

## Setup (Vite)

```bash
npm install -D babel-plugin-react-compiler
```

With `@vitejs/plugin-react`, add the Babel plugin:

```ts
// vite.config.ts
import { defineConfig } from "vite"
import react from "@vitejs/plugin-react"

export default defineConfig({
  plugins: [
    react({
      babel: { plugins: [["babel-plugin-react-compiler", {}]] },
    }),
  ],
})
```

Notes:

- The compiler runs as a **Babel plugin** and must come **first** in the Babel plugin list.
- Newer Vite templates and plugin versions may offer a more direct option, so check the docs for what your template uses.
- It's designed for React 19. For **React 17/18** it also works with a small runtime package (`react-compiler-runtime`) and a `target` option in the config.
- Next.js, Expo, and other frameworks have their own switches (often a single config flag).

### ESLint

Install the React Hooks ESLint plugin version that includes compiler-powered rules. Those rules flag code the compiler can't safely optimize (and Rules-of-React violations), so you find problems before they silently disable optimization. The rule packaging has changed over time (a standalone `eslint-plugin-react-compiler`, later folded into `eslint-plugin-react-hooks`), so follow the current docs.

## It relies on the Rules of React

The compiler is only correct for code that follows the [Rules of React](../03-hooks/00-hook-rules.md):

- **Components and hooks are pure**: same inputs, same output, with no side effects during render.
- **Don't mutate props, state, or values from hooks** (or objects you've already passed to JSX or hooks).
- **Follow the Rules of Hooks** (call them at the top level, in the same order).

If code breaks the rules, the compiler **skips** that component (the best case) or, if the violation isn't detectable, can produce behavior that differs from what you expect. This is [the same purity](./00-concurrent-rendering.md#what-this-means-for-how-you-write-components) that concurrent rendering requires.

```tsx
function Bad({ items }: { items: Item[] }) {
  items.sort(byDate)            // ✗ mutates a prop: unsafe to memoize, may be skipped
  return <List items={items} />
}

function Good({ items }: { items: Item[] }) {
  const sorted = items.toSorted(byDate)    // ✓ new array
  return <List items={sorted} />
}
```

## Adopting it incrementally

You don't need to go all-in at once.

- **Opt-in mode** (`compilationMode: "annotation"`): only compile components/hooks marked with the `"use memo"` directive:

```tsx
function Heavy() {
  "use memo"
  // …
}
```

- **Opt-out**: in normal mode, exclude a specific component or hook that misbehaves with `"use no memo"`:

```tsx
function Legacy() {
  "use no memo"      // temporary escape hatch: leave it uncompiled while you fix the root cause
  // …
}
```

- **Directory scoping**: the config can restrict which source files are compiled, so you can roll it out folder by folder.
- Treat `"use no memo"` as a **debugging and migration tool**, not a permanent fix. Find and fix the underlying rule violation when you can.

## Verifying it's working

- **React DevTools** marks compiled components with a **"Memo ✨"** badge in the component tree.
- In the [Profiler](../14-performance/00-profiling-and-measuring.md#react-devtools-profiler), compiled components that don't need to re-render show as not rendered.
- The Babel plugin can be configured to log components it skipped and why, which is useful when adopting it on an existing codebase.
- **Measure** a before/after on a real interaction. The compiler removes *unnecessary* work; it doesn't speed up an inherently slow render.

## What changes in the code you write

- **Stop writing `useMemo`, `useCallback`, and `memo` by default.** Write plain code, and add manual memoization only where profiling shows a need the compiler didn't cover.
- **Existing manual memoization still works.** You don't have to rip it out. The compiler works alongside it, though removing redundant memoization over time simplifies code.
- **Keep manual `useMemo`/`useCallback` where the exact referential identity matters**, for example a value used as an effect dependency where you need precise control, or one passed to a non-React library that compares by reference.
- **Rules of React become more important**, since purity is now what makes the optimization safe.
- Keep `useDeferredValue`'s heavy child memoized. The compiler usually does it, but confirm in the Profiler ([02](./02-useDeferredValue.md#it-needs-memo-to-do-anything)).

## What it doesn't do

- **It doesn't fix slow algorithms or huge renders.** A 10,000-row list still needs [virtualization](../14-performance/04-virtualization.md); a heavy computation is still heavy the first time.
- **It doesn't remove re-renders caused by context.** A component reading a changing context still re-renders ([context performance](../13-state-management/01-context-patterns-and-performance.md)).
- **It doesn't make network requests, bundles, or images faster.**
- **It can't fix impure code.** It will skip it.
- **It doesn't replace state placement.** Moving state down ([rendering performance](../14-performance/01-rendering-performance.md#fix-1-move-state-down-colocate)) is still the best fix and works with or without it.

## Gotchas and troubleshooting

- **A component isn't compiled** → check the ESLint output and the plugin's logging. Common causes: mutation of props/state, reading/writing refs during render, calling hooks conditionally, or patterns the compiler doesn't support yet.
- **A component behaves differently once compiled** → almost always an unnoticed Rules-of-React violation (hidden mutation, reading a mutable value during render). Add `"use no memo"` to confirm, then fix the cause.
- **Third-party libraries** whose hooks return objects/functions that mutate or change identity in non-standard ways can interact badly with automatic memoization (stale UI is the symptom). Check the library's issue tracker and docs for compiler guidance, and opt the affected component out if needed.
- **Effects with missing dependencies** become more visible, because the compiler assumes your code is correct. Keep `exhaustive-deps` clean.
- **Build time** increases slightly because of the extra analysis pass.
- **Debugging**: compiled output is harder to read, so use source maps and the DevTools badge rather than reading the generated code.

## Should you adopt it?

| Situation | Suggestion |
|---|---|
| New project on React 19 | Enable it from the start, with ESLint rules on |
| Existing app with lots of manual memoization | Enable it incrementally; keep existing hooks; remove ones you can verify are redundant |
| App with Rules-of-React violations | Fix those first (lint will tell you where), or use opt-in mode |
| Heavy reliance on libraries with unusual patterns | Test carefully; use opt-out for affected components |
| Pre-18 React | Upgrade first |

## Common mistakes

- **Enabling it and ignoring the ESLint warnings**, so violations quietly disable optimization.
- **Believing it fixes all performance problems** and skipping profiling.
- **Leaving `"use no memo"` everywhere** as a permanent workaround.
- **Mutating props, state, or hook results** and expecting memoization to work.
- **Deleting all manual memoization at once** without measuring, especially effect dependencies.
- **Reading or writing `ref.current` during render.**
- **Expecting it to stop context-driven re-renders.**
- **Putting the Babel plugin in the wrong position**, or using a stale plugin/React version combination.
- **Skipping the Profiler check**, so you don't know whether it's actually applied.

## Quick summary

- The compiler **automatically memoizes** components, values, and JSX at build time, so plain code gets the benefits of `memo`/`useMemo`/`useCallback`.
- Install the Babel plugin (and the ESLint rules); verify with the **"Memo ✨"** badge and the Profiler.
- It's safe only for code that follows the **Rules of React**; violating code is skipped or misbehaves.
- Adopt incrementally with **`"use memo"`** (opt-in) and **`"use no memo"`** (opt-out, a temporary escape hatch).
- Write simpler code, keep manual memoization only where precise identity control or measured gaps demand it.
- It removes unnecessary work; it doesn't fix slow renders, large lists, context re-renders, or network cost.

## Next

[07 — Server Components and SSR](./07-server-components-and-ssr.md)