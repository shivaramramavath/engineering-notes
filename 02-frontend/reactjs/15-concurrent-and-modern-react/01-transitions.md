# Transitions

A **transition** is a state update you mark as *not urgent*: "this can take a moment, and if something more important happens, interrupt it". It's the primary way to keep an interface responsive while React does expensive rendering.

([Concurrent rendering](./00-concurrent-rendering.md) explains the foundation: interruptible renders.)

## The problem

```tsx
function Tabs() {
  const [tab, setTab] = useState("home")

  return (
    <>
      <TabButton onClick={() => setTab("home")}>Home</TabButton>
      <TabButton onClick={() => setTab("reports")}>Reports</TabButton>   {/* Reports is expensive to render */}
      {tab === "home" ? <Home /> : <Reports />}
    </>
  )
}
```

Click "Reports": the state change is urgent, so React renders `<Reports />` synchronously. The page freezes for 300 ms: the tab button doesn't even appear pressed until it finishes.

## The fix: `startTransition`

```tsx
import { startTransition, useState } from "react"

function selectTab(next: string) {
  startTransition(() => {
    setTab(next)            // this update is now a non-urgent transition
  })
}
```

Now React:

1. Keeps the **current UI** on screen and interactive.
2. Renders `<Reports />` **in the background**, in interruptible slices.
3. If you click another tab mid-render, it **abandons** the old render and starts on the new one.
4. When the new render finishes, it **commits** the result.

No frozen page, no stale intermediate state, and no manual debouncing or loading flags.

## `useTransition`: add a pending indicator

`startTransition` (imported from `react`) has no feedback. `useTransition` returns the same function plus an `isPending` flag:

```tsx
import { useTransition } from "react"

function Tabs() {
  const [tab, setTab] = useState("home")
  const [isPending, startTransition] = useTransition()

  function selectTab(next: string) {
    startTransition(() => setTab(next))
  }

  return (
    <>
      <TabButton onClick={() => selectTab("reports")}>Reports</TabButton>

      <div className={isPending ? "opacity-60 transition-opacity" : undefined} aria-busy={isPending}>
        {tab === "home" ? <Home /> : <Reports />}
      </div>
    </>
  )
}
```

`isPending` is `true` while the transition is rendering. Use it for **subtle feedback** (dim the old content, show a small spinner, slim progress bar), not to replace the screen with a skeleton. The point of transitions is that the old UI stays visible.

## Transitions and Suspense

This is where transitions matter most. Without one, switching to a component that **suspends** (waiting for data or a lazy chunk) replaces what's on screen with the Suspense fallback:

```tsx
// Click → <Suspense> fallback flashes in, replacing the current content
setPage("profile")
```

Wrap the update in a transition, and React **keeps showing the old UI** until the new content is ready:

```tsx
startTransition(() => setPage("profile"))      // old page stays; new page appears when ready
```

React only shows the fallback for a transition when it must, specifically for *new* boundaries that weren't already revealed. Content already visible stays put. See [Suspense](./03-suspense.md) and [lazy loading](../14-performance/03-code-splitting-and-lazy-loading.md#avoiding-the-loading-flash-waterfall).

Routers lean on this: navigation is naturally a transition, and some routing libraries wrap navigation state updates in transitions for you. Check your router's docs for specifics.

## Async transitions (React 19)

In React 19, the function passed to `startTransition` can be **async**. React tracks `isPending` for the whole duration:

```tsx
const [isPending, startTransition] = useTransition()

function save() {
  startTransition(async () => {
    await updateProfile(values)            // isPending stays true until this resolves
    startTransition(() => setSaved(true))  // state updates AFTER an await need their own startTransition
  })
}
```

Two things to know:

- `isPending` covers the **entire async operation**, which makes it a convenient loading flag for async work.
- **State updates after an `await` aren't automatically part of the transition**, because React can't track past the `await`. Wrap them in another `startTransition`, as above. (This is a current limitation of the API, so check the React docs for your version.)

This is the mechanism behind **Actions**, covered in [React 19 features](./05-react-19-features.md#actions). Form actions and `useActionState` are built on async transitions.

## When to use a transition

Good fits:

- **Switching views/tabs** that render heavy content.
- **Navigation** between routes or screens.
- **Filtering, sorting, or re-rendering a large list or table** in response to input.
- **Non-urgent consequences of an urgent update**: the input updates immediately; the expensive results are the transition.
- Updates that cause a Suspense boundary to suspend, where you want to keep the old UI.

```tsx
function SearchPage() {
  const [query, setQuery] = useState("")          // urgent: the input
  const [results, setResults] = useState<Item[]>([])
  const [isPending, startTransition] = useTransition()

  function onChange(e: React.ChangeEvent<HTMLInputElement>) {
    const next = e.target.value
    setQuery(next)                                 // urgent: keep typing instant
    startTransition(() => setResults(filterHugeList(next)))   // non-urgent: heavy list can lag
  }
  // …
}
```

Two `setState` calls with different priorities: the input is updated immediately, and the heavy results are rendered at low priority and can be interrupted by the next keystroke.

## What you can't do

- **Control a text input with a transition update.** The value must update synchronously, or typing feels broken (characters appear late or jump). Keep the input's state urgent and put only the *expensive derivative* in the transition. If the input and the expensive result both come from one value, use [`useDeferredValue`](./02-useDeferredValue.md) instead.
- **Wrap anything but state updates.** `startTransition` marks `setState` calls made *inside it, synchronously*, as non-urgent. Other code inside just runs normally.
- **Use it to make an update happen *later* for timing purposes.** It's about priority and interruptibility, not delay. For timing, use debouncing or timers.
- **Rely on it as a fix for slow network.** It keeps old UI while waiting; it doesn't speed anything up.
- **Mark an event's own immediate feedback as a transition** (a clicked button's pressed state, a form's validation message).

## `useTransition` vs `useDeferredValue`

Both mark work as non-urgent. They differ in *where you have control*:

| | `useTransition` / `startTransition` | `useDeferredValue` |
|---|---|---|
| You control | The code that **sets** the state | The code that **receives** the value |
| Use when | You own the `setState` call | The value arrives as a prop or from a source you don't control |
| Gives you | `isPending` | A lagging copy of the value |
| Typical | Tab switches, navigation, event handlers | Search text → heavy list, props from a parent |

See [02](./02-useDeferredValue.md).

## Transitions vs debouncing

| | Transition | Debounce |
|---|---|---|
| Delay | None: starts rendering immediately, interruptibly | Fixed wait before doing anything |
| Adapts to device speed | **Yes**: fast devices finish instantly | No: everyone waits the same |
| Reduces network requests | **No** | **Yes** |
| Best for | Expensive **rendering** | Expensive **side effects** (requests) |

For search-as-you-type, you often want both: a transition/deferred value for the rendering, and a debounce on the network request ([search state](../10-routing/06-search-filter-and-url-state.md#debounced-search-input)).

## Errors inside transitions

If a transition causes a render error, it behaves like any render error and propagates to the nearest [error boundary](./04-error-boundaries.md). Transitions don't catch errors.

## Common mistakes

- **Putting the text input's own state in a transition**, so typing lags or characters drop.
- **Expecting a transition to speed up the render**, rather than keep the UI responsive.
- **Showing a full-screen skeleton on `isPending`** and defeating the point (old UI visible).
- **Forgetting that updates after `await` need their own `startTransition`.**
- **Using a transition when the real fix is to render less** ([rendering performance](../14-performance/01-rendering-performance.md)).
- **Wrapping non-state work** and assuming it's deprioritized.
- **Using transitions to debounce network requests.**
- **No `isPending` feedback** for long transitions, leaving users unsure anything happened.
- **Marking urgent feedback (button press, validation) as a transition.**

## Quick summary

- A transition is a **non-urgent state update**: interruptible, replaceable, and it never blocks urgent input.
- `startTransition(() => setState(...))` marks it; `useTransition` also gives `isPending` for subtle feedback.
- With Suspense, transitions **keep the old UI** instead of flashing a fallback.
- React 19 allows **async** transitions: `isPending` covers the whole operation, but updates after `await` need their own `startTransition`.
- Never put a controlled input's own state in a transition; use [`useDeferredValue`](./02-useDeferredValue.md) for derived heavy work.
- Transitions improve responsiveness, not speed; fix avoidable slow renders first.

## Next

[02 — useDeferredValue](./02-useDeferredValue.md)