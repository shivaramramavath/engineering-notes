# 03 — Hooks

Hooks are functions that let components use React features: state, side effects, refs, context, and more. This chapter covers the core hooks you'll use daily, plus how to build your own. Hooks that belong to a specific topic (transitions, external stores, imperative handles) live in that topic's chapter and are linked from the index below.

## What you'll be able to do after this chapter

- Follow the Rules of Hooks and explain why they exist
- Manage state with `useState` and `useReducer`
- Synchronize with external systems using `useEffect`, and recognize when you don't need one
- Store mutable values and reach DOM nodes with `useRef`
- Share data without prop drilling using `useContext`
- Memoize values and functions, and know when it's worthwhile
- Measure layout before paint with `useLayoutEffect`
- Extract reusable logic into custom hooks

## Reading order

| # | File | What it covers | Prerequisite |
|---|------|----------------|--------------|
| 00 | [00-hook-rules.md](./00-hook-rules.md) | The two rules and why they exist | `../02-state-and-rendering/` |
| 01 | [01-useState.md](./01-useState.md) | State API, lazy initialization, patterns | 00 |
| 02 | [02-useEffect.md](./02-useEffect.md) | Synchronizing with external systems | 01 |
| 03 | [03-you-might-not-need-an-effect.md](./03-you-might-not-need-an-effect.md) | The most common effect mistakes | 02 |
| 04 | [04-useRef.md](./04-useRef.md) | Mutable values and DOM access | 01 |
| 05 | [05-useContext.md](./05-useContext.md) | Sharing data through the tree | 01 |
| 06 | [06-useReducer.md](./06-useReducer.md) | Reducer-based state | 01 |
| 07 | [07-useMemo-and-useCallback.md](./07-useMemo-and-useCallback.md) | Memoizing values and functions | `../02-state-and-rendering/03-rendering.md` |
| 08 | [08-useLayoutEffect.md](./08-useLayoutEffect.md) | Layout effects before paint | 02 |
| 09 | [09-custom-hooks.md](./09-custom-hooks.md) | Designing and extracting hooks | 01–06 |
| 10 | [10-hook-recipes.md](./10-hook-recipes.md) | Ready-to-use hooks: debounce, local storage, and more | 09 |

## Hooks covered elsewhere

| Hook | Where |
|------|-------|
| `useTransition` | [`../15-concurrent-and-modern-react/01-transitions.md`](../15-concurrent-and-modern-react/01-transitions.md) |
| `useDeferredValue` | [`../15-concurrent-and-modern-react/02-useDeferredValue.md`](../15-concurrent-and-modern-react/02-useDeferredValue.md) |
| `useSyncExternalStore` | [`../16-advanced-react/02-external-stores.md`](../16-advanced-react/02-external-stores.md) |
| `useImperativeHandle` | [`../16-advanced-react/01-refs-and-imperative-handles.md`](../16-advanced-react/01-refs-and-imperative-handles.md) |
| `useActionState`, `useOptimistic`, `useFormStatus`, `use` | [`../15-concurrent-and-modern-react/05-react-19-features.md`](../15-concurrent-and-modern-react/05-react-19-features.md) |
| `useId` | Generates stable unique ids for accessibility attributes; see [`../08-accessibility/01-aria.md`](../08-accessibility/01-aria.md) |

## Boundary with other chapters

- **Mental model** (snapshots, batching, render phases): [`../02-state-and-rendering/`](../02-state-and-rendering/README.md). This chapter links to it rather than repeating it.
- **Hook internals** (how React stores hooks): [`../17-react-internals/02-how-hooks-work.md`](../17-react-internals/02-how-hooks-work.md).
- **Context and reducer patterns at scale**: [`../13-state-management/`](../13-state-management/README.md).

## Exercises

1. **Counter with step.** Build a counter whose step size is controlled by a second input. Use `useState` and the updater form.
2. **Document title.** Write an effect that sets `document.title` from state, and identify its dependency array.
3. **Auto-focus.** Use `useRef` to focus an input on mount and when a button is clicked.
4. **Theme context.** Create a `ThemeContext` with a provider and a `useTheme` custom hook that throws outside the provider.
5. **Convert to reducer.** Rewrite a todo list with `useState` to use `useReducer` with `add`, `toggle`, and `remove` actions.
6. **Remove an effect.** Take a component that stores a filtered list in state via an effect and replace it with a value computed during render.
7. **Build `useDebounce`.** Use it to delay a search request until the user stops typing.

## Next

**[`../04-typescript-with-react/README.md`](../04-typescript-with-react/README.md)** adds static types to components and hooks.
