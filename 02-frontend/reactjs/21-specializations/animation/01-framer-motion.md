# Motion (Framer Motion)

**Motion** (formerly Framer Motion) is the most widely used animation library for React. It gives you declarative animation props on ordinary elements, **exit animations**, spring physics, gestures, layout animations, and scroll-linked values, while animating **outside React's render cycle** so it stays smooth.

> **Naming and imports.** The library was renamed from "Framer Motion" to **Motion**. New code installs `motion` and imports from `motion/react`. The older `framer-motion` package (`import { motion } from "framer-motion"`) still exists and has nearly the same API, so older tutorials still apply. Check the current docs for your version.

```bash
npm install motion
```

```tsx
import { motion } from "motion/react"
```

## The basics: `motion.*` components

Every HTML/SVG element has a `motion` version that accepts animation props:

```tsx
<motion.div
  initial={{ opacity: 0, y: 12 }}       // starting state (on mount)
  animate={{ opacity: 1, y: 0 }}        // target state
  transition={{ duration: 0.3, ease: "easeOut" }}
>
  Hello
</motion.div>
```

- **`initial`**: the values at mount. Set `initial={false}` to skip the entrance and start in the `animate` state.
- **`animate`**: the target. **When it changes, Motion animates from wherever the element currently is** (interruptible, like a CSS transition).
- **`transition`**: how to get there (duration, easing, spring, delay).
- Animated values include `x`, `y`, `scale`, `rotate`, `opacity`, colors, `borderRadius`, `height: "auto"`, and more. Shorthands `x`/`y`/`scale`/`rotate` compile to **`transform`**, which is cheap ([performance](./04-animation-performance-and-accessibility.md#what-makes-an-animation-cheap)).
- Plain React state drives it:

```tsx
<motion.div animate={{ x: isOpen ? 0 : -280 }} transition={{ type: "spring", stiffness: 300, damping: 30 }} />
```

Toggling `isOpen` re-renders once; Motion animates the rest without per-frame React renders.

## Transitions

```tsx
transition={{ duration: 0.25, ease: "easeOut" }}            // tween (time-based)
transition={{ type: "spring", stiffness: 300, damping: 24 }} // spring
transition={{ delay: 0.1 }}
transition={{ repeat: Infinity, repeatType: "reverse", duration: 1 }}
transition={{ opacity: { duration: 0.2 }, x: { type: "spring", bounce: 0.2 } }}   // per-property
```

### Springs

A **spring** animation is driven by physics (stiffness, damping, mass) rather than a fixed duration and curve. Springs feel natural, **preserve velocity** when interrupted or gestured (a drag released at speed continues that momentum), and are why Motion's animations feel "alive":

```tsx
transition={{ type: "spring", stiffness: 400, damping: 30 }}   // snappy, little overshoot
transition={{ type: "spring", bounce: 0.25, duration: 0.5 }}   // easier "designer" parameters
```

- **Stiffness**: higher means faster and snappier. **Damping**: higher means less oscillation. Low damping bounces.
- For routine UI, use **stiff, well-damped** springs (little visible bounce). Big bouncy springs get tiring quickly.
- A tween (`duration` + `ease`) is more predictable for sequenced animations and when you need exact timing.

## Exit animations: `AnimatePresence`

React removes elements instantly, so there's nothing to animate out. `AnimatePresence` **keeps a removed child mounted until its `exit` animation finishes**:

```tsx
import { AnimatePresence, motion } from "motion/react"

function Toast({ message }: { message: string | null }) {
  return (
    <AnimatePresence>
      {message && (
        <motion.div
          key={message}                                   // identity: a new key = exit old, enter new
          initial={{ opacity: 0, y: 16 }}
          animate={{ opacity: 1, y: 0 }}
          exit={{ opacity: 0, y: -8 }}
          transition={{ duration: 0.2 }}
        >
          {message}
        </motion.div>
      )}
    </AnimatePresence>
  )
}
```

Rules that trip people up:

- **`AnimatePresence` must stay mounted**; the conditional goes **inside** it. Putting `AnimatePresence` inside the `{open && …}` removes it along with the child, so the exit never runs.
- **Direct children need a stable `key`** (that's how it tracks which ones left). For a conditional single child, the `key` can be the identity of the content.
- The animated element must be a **`motion` component** (or a component that forwards its ref to one) as a direct child.
- **Modes**: `mode="wait"` (old exits fully, then the new enters, good for page/route swaps), `"popLayout"` (exiting element is popped out of the flow so siblings reflow immediately, good for lists), and the default `"sync"` (both animate at once).
- `initial={false}` on `AnimatePresence` suppresses entrance animations on first render.
- For custom exit logic in a nested component, `usePresence`/`useIsPresence` let a child know it's exiting (or delay removal until a custom animation finishes).

### Lists

```tsx
<AnimatePresence mode="popLayout">
  {items.map((item) => (
    <motion.li
      key={item.id}                       // stable IDs, not indexes ([lists and keys](../../01-fundamentals/05-lists-and-keys.md))
      layout                              // siblings slide to fill the gap ([02](./02-layout-animations.md))
      initial={{ opacity: 0, scale: 0.9 }}
      animate={{ opacity: 1, scale: 1 }}
      exit={{ opacity: 0, scale: 0.9 }}
    >
      {item.name}
    </motion.li>
  ))}
</AnimatePresence>
```

Index keys break this: React reuses the element for a different item, so Motion sees an update, not an exit.

### Pair with focus and semantics

An exiting element is still in the DOM for its exit duration. Don't leave it focusable or announced as if it were active ([focus and presence](./04-animation-performance-and-accessibility.md#focus-and-animated-presence)).

## Variants: named states and orchestration

**Variants** are named animation states you can reuse and propagate down the tree, which makes parent/child coordination and staggering simple:

```tsx
const list = {
  hidden: { opacity: 0 },
  show: { opacity: 1, transition: { staggerChildren: 0.06, delayChildren: 0.1 } },
}
const item = {
  hidden: { opacity: 0, y: 12 },
  show: { opacity: 1, y: 0 },
}

<motion.ul variants={list} initial="hidden" animate="show">
  {items.map((i) => (
    <motion.li key={i.id} variants={item}>{i.name}</motion.li>   {/* inherits "hidden"/"show" from the parent */}
  ))}
</motion.ul>
```

- Children **inherit** the active variant label from the nearest motion ancestor (no props needed on them), so one `animate="show"` on the parent drives everything.
- **`staggerChildren`**, **`delayChildren`**, and `when: "beforeChildren" | "afterChildren"` orchestrate timing from the parent.
- Variants can be functions of a **`custom`** prop for per-item values:

```tsx
const item = { hidden: { opacity: 0 }, show: (i: number) => ({ opacity: 1, transition: { delay: i * 0.05 } }) }
<motion.li custom={index} variants={item} />
```

- Gesture props use variants too: `whileHover="hover"`, `whileTap="tap"`, so a parent hover can animate children ([gestures](./03-gestures.md)).

## Motion values: animation without re-rendering

A **motion value** holds a value that Motion can update **without React re-rendering**, which is the key to fast, continuous animation (drag, scroll, mouse-following).

```tsx
import { motion, useMotionValue, useTransform, useSpring } from "motion/react"

function Follower() {
  const x = useMotionValue(0)
  const opacity = useTransform(x, [-200, 0, 200], [0, 1, 0])   // derive one value from another
  const smoothX = useSpring(x, { stiffness: 300, damping: 30 })  // spring-smoothed copy

  return (
    <motion.div
      drag="x"
      style={{ x, opacity }}                 // motion values go in `style`
    />
  )
}
```

- `useMotionValue(initial)`, `useTransform(value, inputRange, outputRange)` (or a function), `useSpring`, `useVelocity`.
- Pass motion values via **`style`**. Motion updates the DOM directly each frame; your component **does not re-render**.
- Read with `value.get()`, subscribe with `value.on("change", fn)` (clean up on unmount), set with `value.set(...)`. Don't mirror motion values into React state at high frequency, or you lose the benefit ([render costs](./04-animation-performance-and-accessibility.md#keep-animation-out-of-react-renders)).

## Scroll and in-view

```tsx
import { useScroll, useTransform, motion, useInView } from "motion/react"

// 1. Scroll-linked values (progress bar, parallax)
function ProgressBar() {
  const { scrollYProgress } = useScroll()                        // 0 → 1 over the page
  return <motion.div style={{ scaleX: scrollYProgress, transformOrigin: "0 50%" }} className="fixed inset-x-0 top-0 h-1 bg-primary" />
}

// 2. Element-relative scroll
const ref = useRef(null)
const { scrollYProgress } = useScroll({ target: ref, offset: ["start end", "end start"] })
const y = useTransform(scrollYProgress, [0, 1], [-40, 40])      // parallax offset

// 3. Reveal when scrolled into view
<motion.div initial={{ opacity: 0, y: 24 }} whileInView={{ opacity: 1, y: 0 }} viewport={{ once: true, amount: 0.3 }} />
```

- `whileInView` with `viewport={{ once: true }}` is the simple "reveal on scroll" and runs once.
- `useInView(ref)` returns a boolean for imperative needs.
- Scroll-linked animation uses the browser's scroll infrastructure where available. Very heavy scroll effects can still hurt low-end devices, and **parallax is a common vestibular trigger**, so reduce or disable it for users who prefer reduced motion ([04](./04-animation-performance-and-accessibility.md#accessibility)).

## Imperative animation: `animate` and `useAnimate`

For sequences and animations triggered by events rather than state:

```tsx
import { useAnimate } from "motion/react"

function SaveButton() {
  const [scope, animate] = useAnimate()

  async function onSave() {
    await animate(scope.current, { scale: 0.95 }, { duration: 0.08 })
    await save()
    await animate(scope.current, { scale: [0.95, 1.05, 1] }, { duration: 0.25 })
  }

  return <button ref={scope} onClick={onSave}>Save</button>
}
```

`useAnimate` scopes selectors to a component and cleans up on unmount. A standalone `animate()` works outside React (numbers, DOM elements, motion values). `await animate(...)` lets you sequence naturally. Use declarative props for state-driven motion and the imperative API for "do this, then that" flows.

## Bundle size: `LazyMotion`

The full `motion` component carries every feature (gestures, layout, drag). To trim the initial bundle, use the lighter **`m`** component with feature bundles loaded by `LazyMotion`:

```tsx
import { LazyMotion, domAnimation, m } from "motion/react"

<LazyMotion features={domAnimation} strict>
  <m.div animate={{ opacity: 1 }} />       {/* `strict` errors if you accidentally use `motion.*` inside */}
</LazyMotion>
```

- `domAnimation` covers animation, variants, exit, and tap/hover/focus. Drag and layout need the larger `domMax`.
- Features can be **loaded asynchronously** (dynamic `import`) so the animation code isn't in the critical bundle ([code splitting](../../14-performance/03-code-splitting-and-lazy-loading.md)).
- Measure with the [bundle analyzer](../../14-performance/05-bundle-optimization.md#analyze-first) to see if it's worth it for you. Exact sizes and feature bundle names change between versions, so check the docs.

## Respecting reduced motion

```tsx
import { MotionConfig, useReducedMotion } from "motion/react"

<MotionConfig reducedMotion="user">     {/* respects the OS "reduce motion" setting for every child */}
  <App />
</MotionConfig>

const shouldReduce = useReducedMotion()
```

With `reducedMotion="user"`, Motion disables **transform and layout animations** (the movement) while still allowing opacity and color changes. Use `useReducedMotion()` when you need custom behavior. Put `MotionConfig` at the app root. See [04](./04-animation-performance-and-accessibility.md#accessibility).

## Motion with other tools

- **shadcn/Radix**: their components animate with CSS `data-[state=…]` by default. To use Motion for enter/exit on Radix primitives, render the content with `forceMount` inside `AnimatePresence` and make the content a `motion` element (via `asChild`). Check each primitive's docs for the exact pattern.
- **React Router page transitions**: wrap the outlet with `AnimatePresence mode="wait"` and key the child by location. Keep page transitions short; users notice latency more than motion. Also consider the View Transitions API ([00](./00-css-transitions-and-animations.md#view-transitions-and-scroll-driven-animation)).
- **Concurrent React**: Motion's animations run outside React, so they aren't blocked by a slow render. But a long render will still delay *starting* the animation, so keep renders cheap ([transitions](../../15-concurrent-and-modern-react/01-transitions.md)).
- **Server rendering**: `motion` components render their `initial` state on the server. Check for flash-of-content issues and `initial={false}` where appropriate.

## Common mistakes

- **Putting `AnimatePresence` inside the conditional** (exit never runs). It must stay mounted.
- **Missing or unstable `key`s on `AnimatePresence` children**, or index keys in lists.
- **Animating via React state per frame** (a `requestAnimationFrame` + `setState` loop) instead of motion values.
- **Animating layout properties** (`width`, `top`) when `x`/`y`/`scale` would do.
- **Overlong or overly bouncy transitions** for routine UI.
- **Forgetting `transformOrigin`** on scale-based progress bars and similar, so they scale from the center.
- **Using `motion.*` everywhere "just in case"**, adding weight and per-element overhead for static elements. Plain elements are fine.
- **Not cleaning up `value.on("change")` subscriptions.**
- **Ignoring reduced motion** (`MotionConfig reducedMotion="user"` takes one line).
- **Reaching for Motion when a CSS transition would do.**
- **Mixing old `framer-motion` and new `motion` imports** in one app, which can duplicate the library in your bundle.
- **Assuming v-to-v API names match** across tutorials. Check the docs for your version.

## Quick summary

- Motion adds **declarative animation props** (`initial`, `animate`, `exit`, `transition`) to `motion.*` elements and animates outside React's render cycle. Install `motion`, import from `motion/react`.
- Changing `animate` animates **from the current value**, so it's naturally interruptible.
- **Springs** (stiffness/damping or `bounce`/`duration`) feel natural and preserve velocity; use stiff, well-damped ones for UI.
- **`AnimatePresence`** enables exit animations: keep it mounted, give children stable keys, pick a `mode`.
- **Variants** coordinate parent and children (stagger, orchestration); **motion values** (`useMotionValue`/`useTransform`/`useSpring`) animate without re-renders; **`useScroll`/`whileInView`** link motion to scroll.
- Use `useAnimate` for imperative sequences, `LazyMotion` to trim bundle size, and **`MotionConfig reducedMotion="user"`** for accessibility.
- Prefer CSS when it's enough; use Motion for exit animations, orchestration, springs, gestures, and layout animation.

## Next

[02 — Layout animations](./02-layout-animations.md)