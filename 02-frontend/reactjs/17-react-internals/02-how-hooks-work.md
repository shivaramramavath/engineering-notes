# How Hooks Work

Hooks look like magic: a plain function call, `useState(0)`, somehow remembers a value between renders, and `useEffect` somehow knows when to re-run. There's no magic, just a data structure on the [fiber](./00-fiber-architecture.md) and one strict rule that makes it work.

> Simplified mental model. React's real implementation has more moving parts (priorities, queues, dev checks), but the core idea below is accurate and explains every Rules-of-Hooks error you'll see.

## The core idea: hooks are a list, matched by call order

Each function component's fiber holds a **linked list of hook objects**, in the order the hooks were called during render, stored in `fiber.memoizedState`.

```tsx
function Profile() {
  const [name, setName] = useState("Ana")      // hook #1
  const [age, setAge] = useState(30)           // hook #2
  const ref = useRef(null)                     // hook #3
  useEffect(() => { /* … */ }, [name])         // hook #4
}
```

```text
Profile fiber
  memoizedState ──► [useState: "Ana"] ──► [useState: 30] ──► [useRef: {current}] ──► [useEffect: {deps:[name], …}]
                       hook #1               hook #2             hook #3                    hook #4
```

On the **first render** (mount), each hook call creates its hook object and appends it. On **every later render** (update), React walks the list **in the same order**, and the *n*-th hook call gets the *n*-th stored hook.

**There are no names.** React doesn't know that `name` is "the name state". It only knows "this is the first `useState` call, so give me hook #1's value". That's the entire reason for the Rules of Hooks.

## Why the rules exist

```tsx
function Bad({ loggedIn }: { loggedIn: boolean }) {
  if (loggedIn) {
    const [name, setName] = useState("Ana")    // ✗ sometimes hook #1, sometimes absent
  }
  const [count, setCount] = useState(0)        // hook #1 or hook #2, depending on `loggedIn`
}
```

Render 1 (`loggedIn = true`): list is `[name, count]`. Render 2 (`loggedIn = false`): the `if` is skipped, so `useState(0)` is now the **first** call and receives `name`'s stored value (`"Ana"`). The state is silently mismatched with its hook. React detects the changed number of hooks in development and throws.

| Rule | Reason |
|---|---|
| **Call hooks at the top level** (no `if`, loops, early returns before them, nested functions) | The call **order must be identical every render** |
| **Only call hooks from function components or custom hooks** | Hooks need the "currently rendering fiber" to attach to |

See [hook rules](../03-hooks/00-hook-rules.md) and the ESLint `react-hooks` plugin, which enforces both.

### "Invalid hook call"

The error "Invalid hook call. Hooks can only be called inside the body of a function component" has three usual causes:

- Calling a hook outside a component (in an event handler, a plain function, a class).
- **Two copies of React** in the bundle (mismatched versions or duplicated packages), so the library's `useState` talks to a different React than your renderer.
- Version mismatch between `react` and `react-dom`.

Mechanically: hooks call into a "current dispatcher" that React sets only while rendering a component. Outside rendering, there's no dispatcher, so the call fails.

## A toy implementation

To make the idea concrete, here's a minimal version using an **array and an index** (real React uses a linked list on the fiber, but the principle is identical):

```js
let hooks = []        // this component's hook slots
let cursor = 0        // which slot the next hook call uses

function useState(initial) {
  const slot = cursor
  if (!(slot in hooks)) hooks[slot] = initial          // mount: initialize once

  const setState = (next) => {
    hooks[slot] = typeof next === "function" ? next(hooks[slot]) : next
    rerender()                                         // schedule a new render
  }

  cursor++
  return [hooks[slot], setState]
}

function useRef(initial) {
  const slot = cursor
  if (!(slot in hooks)) hooks[slot] = { current: initial }
  cursor++
  return hooks[slot]                                   // same object every render
}

function renderComponent(Component, props) {
  cursor = 0                                           // reset before every render
  const output = Component(props)
  return output
}
```

Everything follows from this:

- **State persists** because it lives in `hooks`, outside the function.
- **Order matters** because slots are found by `cursor`.
- **A hook call in a loop or condition shifts the cursor** and corrupts every slot after it.
- **Each component instance has its own `hooks`** (in real React: its own fiber), so ten `<Counter />`s have ten independent states.

## `useState`: state plus an update queue

Calling `setState(next)` doesn't change a variable immediately. It:

1. Adds an **update** to the hook's queue, tagged with a [priority lane](./03-scheduler-and-lanes.md).
2. **Schedules a re-render** of the fiber (marking it and its ancestors as having pending work).
3. During the next render, `useState` **processes the queue**: applies each update to the previous state in order to compute the new state.

That's why:

```tsx
function handleClick() {
  setCount(count + 1)
  setCount(count + 1)
  setCount(count + 1)
  console.log(count)      // still the OLD value
}                         // result: count is +1, not +3
```

`count` is a **constant captured in this render's closure**. All three calls enqueue "set to `count + 1`" using the *same* old `count`. Use the functional form to chain from the latest queued value:

```tsx
setCount((c) => c + 1)    // update function: receives the result of the previous update
setCount((c) => c + 1)
setCount((c) => c + 1)    // → +3
```

Other consequences:

- **Batching**: several `setState` calls in one event produce one render, since they're all queued before React processes them ([state updates and batching](../02-state-and-rendering/01-state-updates-and-batching.md)).
- **Bailout**: if the computed new state is `Object.is`-equal to the old, React skips re-rendering children (and may skip the component's own render). Mutating an object and calling `setState(sameObject)` therefore does nothing.
- **`useReducer`** is the same mechanism with a custom reducer. `useState` is essentially `useReducer` with a built-in "replace or apply function" reducer.
- **Initializer**: `useState(() => expensive())` runs the function **only on mount** (and twice in StrictMode development).

## Closures: why values go stale

Every render calls your component anew, creating new variables and new functions. A function created during render **closes over that render's values**:

```tsx
function Timer() {
  const [count, setCount] = useState(0)

  useEffect(() => {
    const id = setInterval(() => {
      console.log(count)          // always 0: captured from the render where this effect was created
      setCount(count + 1)         // always sets 1
    }, 1000)
    return () => clearInterval(id)
  }, [])                          // effect created once, on the first render

  return <p>{count}</p>
}
```

The interval callback belongs to **render #1**, where `count` is `0`, forever. Fixes follow directly:

- `setCount((c) => c + 1)`: doesn't need the captured value.
- List the dependency (`[count]`) so the effect is recreated with fresh closures.
- Keep the latest value in a ref when you need a stable function that sees current data.

"Stale closure" isn't a React bug; it's JavaScript closures plus React rendering your function repeatedly. Each render is a snapshot ([state snapshots](../02-state-and-rendering/00-state-and-snapshots.md)).

## `useEffect` and `useLayoutEffect`

An effect hook stores an **effect object** on the fiber: the function, its cleanup, and the **dependency array** from the last render.

On each render, React compares the new deps to the stored ones **element by element with `Object.is`**:

- **No deps array**: runs after **every** render.
- **`[]`**: runs once after mount (and cleans up on unmount).
- **`[a, b]`**: re-runs only if `a` or `b` changed (by reference for objects and functions).

The effect function doesn't run during render. Render only **records** that this effect needs to run (a flag on the fiber), and the commit phase executes it:

```text
render:  detect deps changed → mark effect "needs to run"
commit:  ─ layout effects run synchronously (useLayoutEffect), before paint
         ─ browser paints
         ─ passive effects run (useEffect): previous effect's cleanup first, then the new effect
```

Key implications:

- **Cleanup runs before the next effect** (with the old closure) and on unmount, so subscriptions don't pile up.
- **Unstable dependencies** (objects/functions recreated every render) make the effect run every time, because `Object.is` sees a "change". That's the real reason for `useMemo`/`useCallback` around effect deps ([memoization](../14-performance/02-memoization.md#usememo-cache-a-value)).
- **Effects only run for committed renders**, so an interrupted render never runs them.
- **StrictMode** in development runs mount → cleanup → mount again to surface missing cleanup.
- A `setState` inside an effect triggers **another render** after the commit, which is why effect-driven derived state costs an extra pass ([you might not need an effect](../03-hooks/03-you-might-not-need-an-effect.md)).

## `useRef`

A ref hook stores **one object `{ current }`**, created on mount and returned identically on every render. Mutating `ref.current` just changes a property on that object. Nothing tells React, so **no re-render**. That's why refs are the right place for values that must persist but shouldn't affect the UI, and why reading/writing them during render breaks purity ([refs](../16-advanced-react/01-refs-and-imperative-handles.md)).

## `useMemo` and `useCallback`

Both store a pair `[value, deps]` in the hook:

```js
function useMemo(compute, deps) {
  const slot = /* this hook's stored [value, deps] */
  if (slot exists && depsEqual(slot.deps, deps)) return slot.value   // reuse
  const value = compute()
  store([value, deps])
  return value
}
// useCallback(fn, deps) ≡ useMemo(() => fn, deps)
```

That's all they are: a cache of one entry, keyed by the dependency array. React treats the cache as a **hint** and may discard it, so your code can't depend on it for correctness ([memoization](../14-performance/02-memoization.md#usememo-is-a-hint-not-a-guarantee)).

## `useContext` and `use`

`useContext` is the exception to the "hooks list" model: it doesn't store a value in a slot. It **reads the context's current value** and registers the fiber as a **dependent** of that context. When the provider's value changes, React finds dependent fibers and schedules them to re-render, regardless of any `memo` between them ([context performance](../13-state-management/01-context-patterns-and-performance.md#the-performance-rule)).

Because it doesn't occupy a hook slot, React 19's **`use(Context)`** can be called conditionally. `use(promise)` works similarly: it doesn't claim a slot, and instead throws a special "suspend" signal if the promise isn't resolved ([Suspense](../15-concurrent-and-modern-react/03-suspense.md#the-use-hook)).

## Custom hooks are just functions

A custom hook is an ordinary function that calls other hooks. There's no extra machinery:

```tsx
function useOnlineStatus() {
  const [online, setOnline] = useState(true)     // these are slots on the CALLING component's fiber
  useEffect(() => { /* subscribe */ }, [])
  return online
}
```

Each component that calls `useOnlineStatus()` gets **its own** slots, so hooks share **logic**, not **state**. Two components using the same custom hook have independent state. (To share state, lift it up, use context, or use a store.)

## Mount vs update: two implementations

React uses a different implementation of each hook for the first render and for later ones. Conceptually:

```text
first render (mount):   useState → create hook, store initial state, append to list
later renders (update): useState → take the NEXT hook in the list, process its update queue, return state
```

During development there's also a "rerender" variant and checks that compare the order of hooks between renders, which is where "Rendered more hooks than during the previous render" errors come from. That message means the call order or count changed, so look for an early `return` or a condition above a hook.

## What this explains

| Observation | Explanation |
|---|---|
| Hooks can't be conditional | Order-based slot matching |
| State resets when a component unmounts | The fiber (and its hook list) is destroyed |
| Two instances have separate state | Each has its own fiber/hook list |
| `setState` doesn't update the variable right away | It queues an update; the new value appears in the *next* render |
| Three `setCount(count + 1)` = +1 | Same stale closure value each time |
| `setState(sameObjectMutated)` does nothing | `Object.is` equality bailout |
| Effect re-runs every render with an object dep | New reference each render fails `Object.is` |
| `useRef` changes don't re-render | It's a plain mutable object React never watches |
| Custom hooks don't share state between callers | Slots belong to the caller's fiber |
| StrictMode runs effects twice in dev | Deliberate mount → cleanup → mount to test cleanup |

## Common mistakes

- **Conditional or looped hooks** (corrupting the order), or hooks after an early return.
- **Expecting `setState` to update the variable immediately**, or reading state right after setting it.
- **`setCount(count + 1)` repeated** instead of the functional form.
- **Stale closures** in intervals, timeouts, and event listeners created in effects with wrong dependency arrays.
- **Mutating state objects** and calling `setState` with the same reference.
- **Unstable effect dependencies** (inline objects/functions), causing effects to run every render.
- **Treating a custom hook as a way to share state** between components.
- **Two copies of React** (or mismatched `react`/`react-dom`), producing "Invalid hook call".
- **Disabling `exhaustive-deps`** to silence a warning instead of fixing the closure problem.
- **Relying on `useMemo` for correctness.**

## Quick summary

- Hooks are stored as an **ordered linked list on the fiber**; each call is matched to its slot by **call order**, which is why they can't be conditional.
- `useState` keeps state and a **queue of updates** processed on the next render; updates are batched, and state is a **snapshot per render** (closures capture that snapshot).
- Effects store their function and deps; React compares deps with `Object.is`, and runs effects in the **commit phase** (layout effects before paint, passive effects after).
- `useRef` is a stable mutable object; `useMemo`/`useCallback` cache `[value, deps]` as a hint; `useContext`/`use` read values and subscribe the fiber without using a slot.
- Custom hooks share **logic**, not state, since each caller has its own slots.
- "Invalid hook call" and "Rendered more hooks" point to a hook outside a component, duplicate React, or a changed call order.

## Next

[03 — Scheduler and lanes](./03-scheduler-and-lanes.md)
