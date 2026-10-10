# Gestures

Direct manipulation (hovering, pressing, dragging, swiping) is where animation stops being decoration and becomes **feedback**. The element responds to your finger or cursor, follows it, and settles with believable physics. Motion bundles gesture recognition with animation, so a drag can end in a spring without you wiring up event listeners.

## Hover, tap, and focus

```tsx
<motion.button
  whileHover={{ scale: 1.04 }}
  whileTap={{ scale: 0.96 }}
  whileFocus={{ boxShadow: "0 0 0 3px rgba(59,130,246,0.5)" }}
  transition={{ type: "spring", stiffness: 500, damping: 30 }}
>
  Save
</motion.button>
```

- **`whileHover`**: applied while a *mouse/pen* hovers. Touch screens don't "hover," and Motion ignores false hover from touch.
- **`whileTap`**: applied while the element is pressed (pointer down, or keyboard activation on focusable elements like buttons).
- **`whileFocus`**: applied while focused. Use it **in addition to**, not instead of, a visible focus ring ([accessibility](./04-animation-performance-and-accessibility.md#accessibility)).
- **`whileInView`** (scroll-based) lives in [01](./01-framer-motion.md#scroll-and-in-view).
- Each `while*` prop **reverts automatically** when the gesture ends, animating back to the `animate` state.
- Gesture callbacks: `onHoverStart`, `onHoverEnd`, `onTap`, `onTapStart`, `onTapCancel`. **`onTap` is more forgiving than `onClick`** for a pointer that slides off the element, but for actions prefer `onClick` on real buttons for correct keyboard and assistive-tech behavior.

Keep hover/tap effects **small and quick** (a 2–5% scale, ~100–200 ms). They confirm interactivity, nothing more. A card that leaps 15% on hover is distracting and can push neighbors around.

## Drag

```tsx
<motion.div
  drag                      // or drag="x" / drag="y"
  dragConstraints={{ left: -100, right: 100, top: 0, bottom: 0 }}
  dragElastic={0.2}         // how far past the constraints it can be pulled (0 = hard wall)
  whileDrag={{ scale: 1.05, cursor: "grabbing" }}
  className="h-24 w-24 cursor-grab rounded-lg bg-primary"
/>
```

- **`drag`**: `true`, `"x"`, or `"y"` to constrain the axis.
- **`dragConstraints`**: pixel bounds, or a **ref to a container**:

```tsx
const containerRef = useRef<HTMLDivElement>(null)
<div ref={containerRef} className="relative h-64 w-full">
  <motion.div drag dragConstraints={containerRef} />
</div>
```

- **`dragElastic`**: rubber-banding past the constraints (0–1).
- **`dragMomentum`**: whether releasing at speed continues moving (inertia). On by default.
- **`dragSnapToOrigin`**: spring back to the start on release.
- **`dragTransition`**: tune the momentum and snapping behavior (for example `bounceStiffness`, `bounceDamping`, `power`, `modifyTarget`).
- **`dragListener={false}` + `useDragControls`**: start a drag from a **handle**, not the whole element:

```tsx
const controls = useDragControls()

<motion.div drag="y" dragControls={controls} dragListener={false}>
  <div onPointerDown={(e) => controls.start(e)} className="cursor-grab touch-none">⠿</div>   {/* the handle */}
  <Content />
</motion.div>
```

- Events: `onDragStart`, `onDrag`, `onDragEnd(event, info)`, where `info.point`, `info.offset` (distance from the start), and **`info.velocity`** are available.

### Touch and scrolling conflicts

On touch devices, the browser wants to use drags for **scrolling and zooming**. For a draggable element to receive the gesture, tell the browser what it may not handle:

```css
.draggable        { touch-action: none; }     /* all gestures go to your code */
.draggable-x      { touch-action: pan-y; }    /* browser keeps vertical scroll; you get horizontal drags */
.draggable-y      { touch-action: pan-x; }
```

Motion applies appropriate `touch-action` for `drag="x"`/`"y"`, but **handles** and custom setups often need it set explicitly. A draggable full-screen element with `touch-action: none` traps scrolling, so be deliberate about what area is draggable. For drags inside a scrolling page, use a small handle.

## Swipe to dismiss

Combine **offset and velocity** at drag end: a short, fast flick should dismiss just like a long, slow drag:

```tsx
const SWIPE_DISTANCE = 120      // px
const SWIPE_VELOCITY = 500      // px/s

function SwipeCard({ onDismiss, children }: Props) {
  return (
    <motion.div
      drag="x"
      dragConstraints={{ left: 0, right: 0 }}      // snaps back unless dismissed
      dragElastic={0.6}
      onDragEnd={(_, info) => {
        const farEnough = Math.abs(info.offset.x) > SWIPE_DISTANCE
        const fastEnough = Math.abs(info.velocity.x) > SWIPE_VELOCITY
        if (farEnough || fastEnough) onDismiss()
      }}
      exit={{ opacity: 0, x: 300 }}
    >
      {children}
    </motion.div>
  )
}
```

Wrap in `AnimatePresence` so `exit` runs when the parent removes it. Tune thresholds on a real device: too sensitive and accidental flicks dismiss things, too stiff and it feels broken.

**Provide a non-gesture alternative.** A swipeable item must also have a button or menu action for the same operation (see accessibility below).

## Combining drag with motion values

Because drag writes to motion values, you can derive effects from it **without re-rendering**:

```tsx
const x = useMotionValue(0)
const rotate = useTransform(x, [-200, 200], [-12, 12])
const background = useTransform(x, [-100, 0, 100], ["#ef4444", "#ffffff", "#22c55e"])   // red ← → green

<motion.div drag="x" style={{ x, rotate, background }} dragConstraints={{ left: 0, right: 0 }} />
```

A swipe-card UI (tilt with the drag, color shifts hint at the action) is mostly this. See [motion values](./01-framer-motion.md#motion-values-animation-without-re-rendering).

## Pan (without moving the element)

`onPan`, `onPanStart`, `onPanEnd` recognize a pan gesture **without dragging the element**, useful for custom gestures such as pull-to-refresh, swiping between pages, or scrubbing:

```tsx
<motion.div onPan={(_, info) => setProgress(clamp(info.offset.x / 300, 0, 1))} style={{ touchAction: "pan-y" }} />
```

Prefer writing pan deltas into a **motion value** over React state for smooth, high-frequency updates.

## Reordering lists

For drag-to-reorder lists, Motion provides `Reorder`:

```tsx
import { Reorder } from "motion/react"

function SortableList() {
  const [items, setItems] = useState(["Design", "Build", "Test", "Ship"])

  return (
    <Reorder.Group axis="y" values={items} onReorder={setItems} className="space-y-2">
      {items.map((item) => (
        <Reorder.Item key={item} value={item} className="cursor-grab rounded border bg-background p-3" whileDrag={{ scale: 1.03 }}>
          {item}
        </Reorder.Item>
      ))}
    </Reorder.Group>
  )
}
```

- `values` is the array in state, and `onReorder` gives you the new order (just `setItems`). **`value` and `key` should be the item's stable identity**.
- Items animate into place automatically (it uses layout animation, [02](./02-layout-animations.md)).
- It suits **simple, single-axis lists** of modest size. For multi-container boards (Kanban), grids, keyboard-accessible sorting, collision handling, or large lists, a dedicated drag-and-drop library such as **dnd-kit** is a stronger foundation (and has built-in keyboard and screen reader support). It can be combined with Motion for the visuals.
- Persist the order with your data layer after `onReorder` (consider [optimistic updates](../../12-server-state/06-optimistic-updates.md)).

## Alternative gesture libraries

If you need richer gesture recognition (multi-touch pinch/zoom, rotate, wheel, scroll gestures) or want gestures decoupled from the animation engine, **`@use-gesture/react`** provides hooks like `useDrag`, `usePinch`, and `useWheel` and pairs naturally with spring libraries. Motion covers the common cases (hover, tap, drag, pan). Reach for another library when you hit its limits. Check each library's current docs, since these ecosystems move.

## Accessibility of gestures

Gesture-only interactions exclude people who can't perform them (motor impairments, keyboard or switch users, screen reader users, someone using a mouse with precision difficulties). WCAG requires alternatives for **dragging** and **path-based or multi-point gestures** ([accessibility checklist](../../08-accessibility/04-accessibility-checklist.md)):

- **Every gesture needs a single-pointer or keyboard alternative**: a "Dismiss" button for swipe-to-dismiss, up/down "Move" buttons (or arrow-key handling) for reorderable lists, a slider with arrow keys for a draggable value.
- **Real controls for real actions.** Use `<button>`, links, and form controls for the action itself. The gesture is an enhancement layered on top.
- **Focus must remain visible** and stay predictable during and after drag operations ([focus management](../../08-accessibility/02-keyboard-and-focus-management.md)).
- **Announce results** of drag operations (an `aria-live` region: "Moved Build to position 2 of 4") for screen reader users.
- **Don't require precision or speed.** Provide generous targets and avoid velocity-only triggers as the *only* way to complete an action.
- **Reduced motion** applies to physics and momentum too ([04](./04-animation-performance-and-accessibility.md#accessibility)).

## Testing gestures

jsdom has no real pointer geometry or layout, so drag behavior isn't testable there. Test **the logic** (what `onDragEnd` decides given an offset and velocity, as a pure function) and the **non-gesture alternatives** in unit/integration tests, and cover real drags in a real browser with Playwright ([E2E](../../18-testing-and-debugging/06-e2e-testing-playwright.md)), using explicit mouse down/move/up steps with intermediate moves.

## Common mistakes

- **Gesture-only interactions** with no keyboard or button alternative.
- **Making large scrolling regions draggable** (`touch-action: none`), trapping page scroll on touch devices.
- **No drag handle** on list items, so scrolling and dragging collide.
- **Dismiss decisions based on distance only**, so quick flicks fail. Combine offset and velocity.
- **Putting high-frequency gesture values in React state** instead of motion values.
- **Oversized hover/tap effects** that shift layout or distract.
- **Relying on `whileHover` on touch devices** to reveal essential information (touch has no hover).
- **Replacing a visible focus indicator** with a hover-like effect.
- **Using `Reorder` for complex boards** (multi-list, grids, keyboard needs) instead of a dedicated DnD library.
- **Forgetting `AnimatePresence`**, so swipe-dismissed items vanish instead of animating out.
- **Untuned thresholds**, never tested on a real touch device.
- **Using `onTap` for important actions** instead of `onClick` on a real button.

## Quick summary

- Motion gives you **`whileHover`/`whileTap`/`whileFocus`** (auto-reverting), plus callbacks for hover, tap, and pan.
- **`drag`** (with `dragConstraints`, `dragElastic`, momentum, `whileDrag`) makes any element draggable; use `dragControls` + `dragListener={false}` for handles.
- **Swipe-to-dismiss** = check **offset and velocity** in `onDragEnd`, with `AnimatePresence` for the exit.
- Drive derived effects with **motion values** (`useTransform`) to avoid re-renders; use `Reorder` for simple sortable lists and a DnD library for complex ones.
- Manage **`touch-action`** so drags and scrolling don't fight on touch devices.
- Every gesture needs an **accessible alternative** (buttons, keyboard, live announcements); test logic in unit tests and real drags in a browser.

## Next

[04 — Animation performance and accessibility](./04-animation-performance-and-accessibility.md)