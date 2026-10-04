# useLayoutEffect

`useLayoutEffect` is a version of `useEffect` that fires **before the browser repaints the screen**. It exists for one job: reading layout (sizes, positions) and re-rendering synchronously so the user never sees an intermediate, wrong frame. It's rarely needed, and it can hurt performance, so use it only when `useEffect` produces visible flicker.

## Prerequisites

[`02-useEffect.md`](./02-useEffect.md) and [`04-useRef.md`](./04-useRef.md)

---

## Timing: `useEffect` vs `useLayoutEffect`

```
Render → Commit (DOM updated) → useLayoutEffect → Browser paints → useEffect
```

| | `useEffect` | `useLayoutEffect` |
|---|-------------|-------------------|
| Runs | After the browser paints | After DOM updates, **before** paint |
| Blocks painting | No | **Yes** (the browser waits) |
| State updates inside | Visible as a second paint (possible flicker) | Processed before paint; the user sees only the final result |
| Use for | Almost everything | Measuring layout, then adjusting before paint |
| Server rendering | Fine (doesn't run on the server) | Warns or misbehaves on the server (see below) |

Both have the same signature: `useLayoutEffect(setup, dependencies?)`, with optional cleanup returned from setup.

---

## The classic example: a tooltip that must be positioned

A tooltip needs its **own height** to decide whether to render above or below its target. You can only know that after it's in the DOM.

With `useEffect`, the flow is:

1. Render tooltip at a default position → browser **paints** it (user may see it jump).
2. Effect measures it and sets new position.
3. Re-render → paints again at the corrected position.

That's two paints, and the flicker is visible. With `useLayoutEffect`:

```jsx
import { useRef, useState, useLayoutEffect } from "react";

function Tooltip({ children, targetRect }) {
  const ref = useRef(null);
  const [tooltipHeight, setTooltipHeight] = useState(0);

  useLayoutEffect(() => {
    const { height } = ref.current.getBoundingClientRect();
    setTooltipHeight(height);
  }, []);

  let top = 0;
  if (targetRect) {
    top = targetRect.top - tooltipHeight;
    if (top < 0) top = targetRect.bottom;   // flip below if no room above
  }

  return (
    <div ref={ref} style={{ position: "absolute", top }}>
      {children}
    </div>
  );
}
```

Flow:

1. Render with `tooltipHeight = 0`.
2. DOM is updated, but **the browser hasn't painted**.
3. `useLayoutEffect` measures the real height and calls `setTooltipHeight`.
4. React re-renders **synchronously, before paint**.
5. The browser paints once, in the correct place.

---

## When you need it

Reach for it when **all** of these are true:

1. You must **read layout** (via `getBoundingClientRect`, `offsetHeight`, scroll positions) or **write DOM styles** synchronously,
2. Using `useEffect` visibly flickers or shows a wrong frame, and
3. You can't compute the answer purely during render.

Typical cases: tooltips, popovers, and dropdown positioning; measuring an element to animate its height; restoring scroll position; syncing sizes between elements.

Always **try `useEffect` first**. If you don't see flicker, you don't need this hook.

---

## Costs and caveats

- **It blocks painting.** Slow work inside a layout effect delays the first visible frame. Keep it tiny and fast: measure, set state, done.
- **Updates inside cause a second synchronous render** (before paint), which is still extra work.
- **Server rendering.** Layout effects don't run on the server, and React historically warned about this during server rendering, because the output could differ from the client's first render. Alternatives: render something that works without measurement on the server, use `useEffect` for non-critical work, or defer layout-dependent UI until after mount. Check the React docs for the current guidance for your React version and framework.
- **Don't use it for data fetching or subscriptions** — those don't need to block paint.

---

## `useInsertionEffect`

A related hook, `useInsertionEffect`, runs even *earlier* (before React makes DOM changes) and is intended **only for CSS-in-JS library authors** to inject `<style>` rules. You won't need it in application code.

---

## Choosing correctly

| Need | Hook |
|------|------|
| Fetch data, subscribe, log, sync with the network | `useEffect` |
| Measure DOM and adjust layout before the user sees it | `useLayoutEffect` |
| Inject styles (library code) | `useInsertionEffect` |
| Calculate from props/state | Neither — compute during render ([`03-you-might-not-need-an-effect.md`](./03-you-might-not-need-an-effect.md)) |

---

## Debugging flicker

To confirm you need it: throttle the CPU in browser DevTools and watch the UI. If an element renders at the wrong spot for a frame and then jumps, a layout effect is the likely fix. If you can't see any jump, `useEffect` is fine.

---

## Common mistakes

- **Using it by default instead of `useEffect`** — blocks paint for no benefit.
- **Doing slow work in it** — delays every frame.
- **Using it on the server without a plan** — warnings and potential hydration mismatches.
- **Using it to fetch or subscribe** — unnecessary; those belong in `useEffect`.
- **Measuring during render** — the DOM isn't updated yet; measure in a layout effect or ref callback.
- **Forgetting dependencies** — same rules as `useEffect` ([`02-useEffect.md`](./02-useEffect.md)).

## Quick summary

- `useLayoutEffect` runs after DOM updates but **before the browser paints**; `useEffect` runs after
- Use it to measure layout and update state without a visible flicker
- Try `useEffect` first; use the layout version only when you see a wrong intermediate frame
- Keep the work small, since it blocks painting
- Take care with server rendering, where layout effects don't run
- Same dependency and cleanup rules as `useEffect`

## Next

**[`09-custom-hooks.md`](./09-custom-hooks.md)** shows how to extract reusable logic into your own hooks.
