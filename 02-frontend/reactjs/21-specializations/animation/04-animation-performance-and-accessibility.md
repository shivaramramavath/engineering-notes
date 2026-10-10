# Animation Performance and Accessibility

Two questions decide whether an animation is good: **does it run smoothly on a low-end phone**, and **can everyone use the interface it's in**? Answering "it looks great on my laptop" to both is how teams ship animation that stutters, drains batteries, and makes some users physically ill.

## The frame budget

At 60 fps, the browser has **~16.7 ms per frame** to run JavaScript, calculate styles, lay out, paint, and composite. At 120 Hz displays it's ~8 ms. Miss the budget and frames drop: the animation visibly stutters ("jank"). Everything in this note is about spending less per frame.

```text
JavaScript → Style → Layout → Paint → Composite
                       ▲        ▲         ▲
                  most expensive ····· cheapest
```

An animation that only needs **Composite** (transform, opacity) can run on the compositor thread, even while the main thread is busy. One that needs **Layout** reflows the page every frame.

## What makes an animation cheap

| Property | Pipeline stages it triggers | Verdict |
|---|---|---|
| `transform` (`translate`, `scale`, `rotate`), `opacity` | Composite only | ✅ Default choice |
| `filter`, `backdrop-filter`, `clip-path` | Composite or paint, scales with area | ⚠️ Fine on small elements, test on large ones and low-end devices |
| `background-color`, `color`, `box-shadow`, `border-radius` | Paint | ⚠️ OK occasionally, costly when large or many |
| `width`, `height`, `margin`, `padding`, `top`/`left`, `font-size` | **Layout** + paint | ❌ Avoid animating |

Practical rules:

- **Move with `transform`, not `top`/`left`.** Resize with `scale` (or a layout animation, which uses transforms under the hood, [02](./02-layout-animations.md)). Fade with `opacity`.
- **Animate small things.** A fading 24 px icon is trivial. A blurred full-screen backdrop is not.
- **Avoid animating while the main thread is busy.** JavaScript-driven animations stall when the main thread is blocked by a long render. Compositor-only CSS animations (transform/opacity) can continue smoothly. This is a real argument for CSS on critical paths like loading indicators.
- **`box-shadow` animations** are a common hidden cost. Animate the **opacity of a pseudo-element** holding the final shadow instead.
- **Many simultaneous animations** multiply cost. Stagger, limit, or animate fewer elements.
- **Images and video**: size assets to display size. Decoding and compositing huge images during animation hurts.

### `will-change`: a scalpel, not a default

`will-change: transform` hints the browser to promote an element to its own compositor layer in advance:

```css
.sheet { will-change: transform; }
```

Each promoted layer **consumes memory** and can make things *slower* when overused. Apply it only to elements that will animate **soon and repeatedly** (a sheet about to be dragged), remove it when idle, and never blanket it across many elements (`* { will-change: transform }` is an anti-pattern). Libraries like Motion manage layer promotion themselves in most cases.

## Keep animation out of React renders

The most common performance mistake in React animation: **driving it with state.**

```tsx
// ✗ re-renders this component 60 times per second
function Bad() {
  const [x, setX] = useState(0)
  useEffect(() => {
    let raf = requestAnimationFrame(function tick() {
      setX((x) => x + 1)
      raf = requestAnimationFrame(tick)
    })
    return () => cancelAnimationFrame(raf)
  }, [])
  return <div style={{ transform: `translateX(${x}px)` }}>…</div>
}
```

Each frame runs the component, reconciles its subtree, and commits, which is far more than the animation needs, and it competes with everything else React is doing ([rendering](../../17-react-internals/00-fiber-architecture.md)).

Better, in order of preference:

1. **CSS** transitions or keyframes. The browser animates, and React isn't involved after the class toggles ([00](./00-css-transitions-and-animations.md)).
2. **Motion's `animate`/`while*` props.** One React render to change the target, then Motion animates directly on the DOM ([01](./01-framer-motion.md)).
3. **Motion values** (`useMotionValue`, `useTransform`, `useSpring`) for continuously changing values (drag, scroll, pointer-following). They update `style` **without re-rendering** ([01](./01-framer-motion.md#motion-values-animation-without-re-rendering)).
4. **Refs and imperative updates** in a `requestAnimationFrame` loop (writing `el.style.transform`) when you're outside a library. Keep the value in a ref, not state ([refs](../../16-advanced-react/01-refs-and-imperative-handles.md)).

Related habits:

- **Isolate** animated components from expensive siblings, so a re-render near the animation doesn't also re-render a heavy tree ([move state down](../../14-performance/01-rendering-performance.md#fix-1-move-state-down-colocate)).
- **Don't subscribe to scroll or pointer events with `setState`.** Use motion values, or `IntersectionObserver` for visibility.
- With layout animations, avoid parents that re-render on every tick ([02](./02-layout-animations.md#cost-and-limits)).

## Measuring

Don't guess. Check on a throttled CPU ([profiling](../../14-performance/00-profiling-and-measuring.md#test-under-realistic-conditions)):

- **Chrome DevTools → Performance**: record the animation. Look for long frames, long tasks, and purple (Layout) or green (Paint) bars repeating every frame, which signal layout/paint-heavy animation.
- **Rendering tab**: **FPS meter**, **Paint flashing** (highlights repainted regions: a flashing area during a "transform" animation means something else is repainting), **Layer borders** (see compositor layers), and **Layout Shift Regions**.
- **Layers panel** to inspect layer count and memory.
- Test on a **real mid-range phone** if you can. Laptop GPUs hide problems.
- **React Profiler** to confirm components *aren't* re-rendering each frame ([Profiler](../../14-performance/00-profiling-and-measuring.md#react-devtools-profiler)).

Animation also affects **Core Web Vitals**: shifting elements cause [CLS](../../14-performance/00-profiling-and-measuring.md#what-to-measure-core-web-vitals) (animate with `transform`, which doesn't cause layout shift), and long main-thread work during an interaction hurts INP.

## Bundle and load cost

- Animation libraries aren't free. Use **`LazyMotion`** with the smaller feature set, and load heavy features asynchronously ([01](./01-framer-motion.md#bundle-size-lazymotion)).
- Don't pull in a large library for a single fade. A CSS transition has **zero** JavaScript cost.
- Lottie/animation files (JSON/video) can be large. Lazy-load, and consider CSS/SVG for simple cases.
- Defer **non-essential** animation until after first paint and interaction readiness, so it doesn't compete with [LCP](../../14-performance/06-network-performance.md).

## Accessibility

Animation can be a barrier. Millions of people have **vestibular disorders, migraines, motion sensitivity, attention conditions, or epilepsy**, and large or sustained motion can cause dizziness, nausea, disorientation, or seizures. Treat motion as something users can control.

### Honor `prefers-reduced-motion`

Operating systems expose a "reduce motion" preference, available in CSS and JavaScript:

```css
@media (prefers-reduced-motion: reduce) {
  .hero-parallax { transform: none !important; }
  .modal { transition: opacity 150ms; }        /* keep a fade; remove the slide/zoom */
}
```

```tsx
// Motion: respect it app-wide
<MotionConfig reducedMotion="user"><App /></MotionConfig>

// or per component
const reduce = useReducedMotion()
<motion.div animate={reduce ? { opacity: 1 } : { opacity: 1, y: 0 }} initial={reduce ? { opacity: 0 } : { opacity: 0, y: 24 }} />
```

What "reduce" should mean:

- **Remove or replace** large, spatial, or looping motion: parallax, zooming, sliding across the screen, large rotations, bouncing, autoplaying background motion, spinning loaders at huge scale.
- **Keep** gentle, non-spatial feedback: opacity fades, color changes, small state indications. Users still need to see *that* something changed.
- It is "reduce," not "eliminate all feedback." A page with no visible state changes at all is also hard to use.
- `MotionConfig reducedMotion="user"` disables transform and layout animations while preserving opacity and color changes, which is a sensible default. Check the docs for the version you use.

Also provide an **in-app setting** where it makes sense (some users want motion off for your app but not the OS), and make sure it's discoverable.

### WCAG guidelines to know

- **2.2.2 Pause, Stop, Hide**: moving, blinking, or scrolling content that starts automatically and lasts **more than 5 seconds** (carousels, animated backgrounds, auto-updating feeds) needs a way to **pause, stop, or hide** it.
- **2.3.1 Three Flashes or Below Threshold**: never flash more than three times per second. Flashing can trigger seizures. Avoid strobing effects entirely.
- **2.3.3 Animation from Interactions** (AAA): motion triggered by interaction should be disableable unless essential. Honoring `prefers-reduced-motion` is the standard way.
- **1.4.1 Use of Color / 4.1.3 Status Messages**: don't convey information **only** through animation. A shake on a form field isn't an error message ([errors](../../08-accessibility/04-accessibility-checklist.md)).
- **2.5.1 Pointer Gestures / 2.5.7 Dragging Movements**: dragging and path-based gestures need alternatives ([gestures](./03-gestures.md#accessibility-of-gestures)).

Check the current WCAG version and your legal requirements, since criteria and levels vary by jurisdiction.

### Specific motion to be careful with

- **Parallax and scroll-jacking**: frequent vestibular triggers. Disable under reduced motion, and don't hijack native scrolling.
- **Large-scale zooms and spins**, **full-screen transitions**, and **background videos/animations** that loop.
- **Autoplaying carousels**: provide pause controls, and don't auto-advance past the point users can read.
- **Infinite or looping animations** in the user's peripheral vision (a pulsing badge next to a paragraph being read). They steal attention, which is a particular problem for people with ADHD or cognitive disabilities.
- **Skeleton shimmer** is generally fine, but keep it subtle and respect reduced motion by switching to a static or slow pulse.

### Focus and animated presence

Animated enter/exit creates a window where an element is **visually changing but still in the DOM**:

- **Exiting elements stay focusable and readable** by assistive technology until they unmount. During an exit animation, make them inert: `inert` attribute, `aria-hidden="true"`, and/or `pointer-events: none`. Motion's `AnimatePresence` can tell a child it's exiting (`useIsPresence`) so you can do this.
- **Closed-but-mounted panels** (the "keep it mounted" CSS approach, [00](./00-css-transitions-and-animations.md#1-keep-it-mounted-toggle-a-state-attribute)) must be removed from the tab order and accessibility tree when closed (`visibility: hidden` applied after the transition, `inert`, or `hidden`).
- **Move focus to the right place when content enters** (dialogs get focus on open, and return it to the trigger on close, [focus management](../../08-accessibility/02-keyboard-and-focus-management.md)). Don't wait for an entrance animation to finish before moving focus, since keyboard users shouldn't be stuck on invisible or unreachable elements.
- **Don't animate focus rings away.** Focus indicators should appear **instantly** or with a very short transition. A slow fade-in delays feedback.
- **Route transitions** shouldn't trap or lose focus. After navigation, focus should land on the new content's heading or container ([navigation](../../10-routing/03-navigation.md#scroll-and-focus)).
- **Layout animations** must not leave focused elements off-screen or moving under the keyboard user's cursor.

### Timing and legibility

- Keep **interactive feedback fast** (100–250 ms). Slow animations make interfaces feel unresponsive and force users to wait.
- Don't **delay access** to content behind an animation (a 2-second intro sequence the user must sit through).
- **Text shouldn't move while being read.** Avoid animating text content during reading, and avoid layout shifts that move targets as users try to click.
- Provide **pause or skip** for long sequences and onboarding animations.

## Testing animations

- **Unit and integration tests (jsdom)** don't run real animation frames. Make tests **deterministic by disabling animation**: set reduced motion in the test environment, use your animation library's "skip animations" configuration (Motion offers a global setting for this, so check the docs for the exact name), or add a test-only CSS rule that zeroes durations.
- **Don't assert on mid-animation visual states.** Assert on the **end state** and on accessibility behavior (inert, focus, roles).
- With **exit animations**, wait for the element to be removed (`waitForElementToBeRemoved`, [RTL](../../18-testing-and-debugging/02-component-testing-with-rtl.md#async-ui)) rather than asserting immediately.
- **E2E (Playwright)**: set `reducedMotion: "reduce"` in the browser context to speed tests and remove flakiness, and add a few runs with motion **enabled** for visual or real-browser checks. Playwright's auto-waiting handles elements still moving ([E2E](../../18-testing-and-debugging/06-e2e-testing-playwright.md)).
- **Test the reduced-motion path itself**, since it's a real user experience, not an afterthought.

## A checklist before shipping animation

**Performance**

- [ ] Animates only `transform`/`opacity` (or a library layout animation), not `width`/`height`/`top`/`left`
- [ ] No per-frame React state updates; motion values or CSS used instead
- [ ] Verified on throttled CPU (and a real low-end device if possible), with no dropped frames or long tasks
- [ ] `will-change` used sparingly, if at all
- [ ] Library size justified, with `LazyMotion` or CSS used where it's enough
- [ ] No layout shift caused by the animation (CLS)

**Accessibility**

- [ ] `prefers-reduced-motion` honored (movement replaced by gentle fades, not just removed wholesale)
- [ ] Nothing flashes more than three times per second
- [ ] Auto-playing or looping motion over 5 seconds can be paused or stopped
- [ ] Gestures have keyboard or button alternatives
- [ ] Exiting or hidden elements are inert and out of the tab order; focus management is correct
- [ ] Information isn't conveyed by animation alone
- [ ] Focus indicators are visible immediately

**Quality**

- [ ] Durations and easings are consistent and short (generally 100–400 ms)
- [ ] Animation serves a purpose (feedback, continuity, hierarchy), not decoration alone
- [ ] Animations are interruptible (reverse or retarget smoothly)
- [ ] Tests run with animation disabled and cover the end states

## Common mistakes

- **Animating layout properties** (`width`, `height`, `top`, `left`) instead of `transform`.
- **Driving animation with React state** at frame rate.
- **`will-change` everywhere**, wasting memory and slowing rendering.
- **Testing only on a fast desktop**, not on throttled or real mobile hardware.
- **Ignoring `prefers-reduced-motion`**, or implementing it by removing all feedback.
- **Parallax, scroll-jacking, and autoplaying motion** without a way to stop it.
- **Exit animations that leave elements focusable** or readable by screen readers.
- **Animation as the only signal** (shake to indicate error, color pulse to indicate success).
- **Slow, decorative transitions** on frequent interactions, so users wait for the UI.
- **Heavy animation libraries** loaded eagerly for trivial effects.
- **Flaky tests** caused by real animations in jsdom or E2E, instead of disabling them.
- **Assuming "smooth on my machine" means smooth** for your users.
- **Focus rings that animate in slowly** or are hidden by hover-style effects.
- **Loops and constant motion** next to reading content, causing distraction.

## Quick summary

- You have ~**16.7 ms per frame**. Stay on **compositor-only** properties (`transform`, `opacity`) and avoid animating layout properties.
- **Don't drive animation with React state.** Prefer CSS, Motion's declarative props, and **motion values**; keep per-frame work out of renders.
- Use **`will-change` sparingly**, measure with the DevTools Performance and Rendering tools on **throttled CPU**, and keep library weight in check (`LazyMotion`, or CSS).
- **Accessibility is not optional**: honor `prefers-reduced-motion` (replace large movement with gentle fades), avoid flashing, let users pause long-running motion, and never use animation as the only signal.
- Manage **focus and inertness** around animated enter/exit, and give gestures **non-gesture alternatives**.
- Make tests deterministic by **disabling animation**, assert on end states, and test the reduced-motion path.
- Motion should be **short, consistent, interruptible, and purposeful**.

## Next

Continue to [React Flow](../react-flow/README.md).