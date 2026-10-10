# CSS Transitions and Animations

Before reaching for a JavaScript animation library, check what the browser can do on its own. CSS animations run on the browser's **compositor** where possible, cost no JavaScript bundle, don't re-render React components, and are interruptible by default. For a large share of UI motion (hover, focus, expand/collapse, fade-in), CSS is the right and simplest answer.

## Transitions: animate between two states

A **transition** animates a property from its old value to its new value whenever it changes:

```css
.button {
  background: var(--primary);
  transform: translateY(0);
  transition: background-color 150ms ease, transform 150ms ease;
}

.button:hover {
  background: var(--primary-hover);
  transform: translateY(-1px);
}
```

```text
transition: <property> <duration> <timing-function> <delay>
```

- **`transition-property`**: which property to animate. Name them explicitly. Avoid `transition: all`, which animates properties you didn't mean to (and costs more).
- **`transition-duration`**: how long.
- **`transition-timing-function`**: the easing curve.
- **`transition-delay`**: wait before starting.

In React, the trigger is just **a change in class, attribute, or inline style**. The browser does the animating:

```tsx
<div className={cn("overflow-hidden transition-opacity duration-200", isVisible ? "opacity-100" : "opacity-0")} />

<button aria-expanded={open} className="[&[aria-expanded=true]>svg]:rotate-90 [&>svg]:transition-transform">
  <ChevronIcon />
</button>
```

React toggles the class, and the browser interpolates. No library, no re-render per frame.

Transitions are **interruptible**: if the state flips back mid-transition, the browser reverses smoothly from the *current* value instead of snapping. That's a major reason to prefer them for interactive UI.

## Keyframe animations: multi-step and self-running

When there's no "from state → to state" trigger (loading spinners, looping effects, entrance sequences), use `@keyframes`:

```css
@keyframes fade-in-up {
  from { opacity: 0; transform: translateY(8px); }
  to   { opacity: 1; transform: translateY(0); }
}

.card {
  animation: fade-in-up 300ms ease-out both;
}

@keyframes spin { to { transform: rotate(360deg); } }
.spinner { animation: spin 800ms linear infinite; }
```

```text
animation: <name> <duration> <timing> <delay> <iteration-count> <direction> <fill-mode>
```

- **`fill-mode: both`** (or `forwards`/`backwards`) keeps the first/last keyframe applied before/after the animation. Without it, a "fade in" element can flash visible before the animation starts, or snap back after it ends.
- **`animation-delay`** + a per-item CSS variable gives staggered entrances:

```tsx
{items.map((item, i) => (
  <li key={item.id} className="animate-fade-in-up" style={{ animationDelay: `${i * 40}ms` }}>
    {item.name}
  </li>
))}
```

- A keyframe animation **runs when the element is added** (or the class is applied). It does **not** retrigger just because React re-rendered. To replay it on a data change, change the element's `key`, which remounts it ([reconciliation](../../17-react-internals/01-reconciliation.md)).
- Keyframes are less naturally interruptible than transitions: removing the class snaps the element back instead of reversing.

## Tailwind

Tailwind exposes all of this as utilities, covering most needs without writing CSS ([Tailwind](../../07-styling/02-tailwindcss.md)):

```tsx
<button className="transition-colors duration-150 ease-out hover:bg-primary/90" />
<div className="transition-all duration-300 data-[state=open]:opacity-100 data-[state=closed]:opacity-0" />
<span className="animate-spin" />         {/* built-in: spin, ping, pulse, bounce */}
<div className="motion-safe:animate-fade-in motion-reduce:animate-none" />
```

Define custom keyframes in your theme config (the exact mechanism differs between Tailwind v3 and v4; check the docs). Radix and shadcn components expose `data-[state=open|closed]` attributes that pair directly with these utilities, which is how their dialogs and menus animate ([dialogs](../../09-ui-components/02-dialogs-and-modals.md)).

## What to animate (and what not to)

Browsers render a frame in stages: **style → layout → paint → composite**. Properties differ in how much work they trigger:

| Property | Triggers | Cost |
|---|---|---|
| `transform` (translate, scale, rotate), `opacity` | **Composite only** | ✅ Cheap; often GPU-accelerated |
| `filter`, `backdrop-filter` | Composite, but can be heavy on large areas | ⚠️ Test on low-end devices |
| `color`, `background-color`, `box-shadow`, `border-color` | **Paint** | ⚠️ Fine for small elements, costly for large or many |
| `width`, `height`, `top`/`left`, `margin`, `padding`, `font-size` | **Layout** (reflows the page), then paint | ❌ Expensive; avoid animating |

**Rule: animate `transform` and `opacity`.** Express movement as `translate`, size changes as `scale`, and visibility as `opacity`. Animating `width` or `left` forces the browser to recompute layout every frame, which janks on busy pages ([rendering costs](../../14-performance/01-rendering-performance.md#dom-and-layout-performance)). Details in [04](./04-animation-performance-and-accessibility.md#what-makes-an-animation-cheap).

## Easing and duration

Motion should feel **natural and quick**.

- **Durations**: micro-interactions (hover, press) **100–200 ms**; small elements entering/leaving (menus, tooltips) **150–250 ms**; larger elements (modals, drawers, page transitions) **250–400 ms**. Longer than ~500 ms usually feels sluggish in a UI.
- **Enter** with `ease-out` (fast start, gentle stop) so the response feels immediate. **Exit** with `ease-in` or a shorter duration. Exits can be faster than entrances because the user's attention has moved on.
- **`linear`** only for constant-rate motion (spinners, progress).
- Custom curves with `cubic-bezier(x1, y1, x2, y2)`, or the newer `linear()` function for spring-like and bouncy curves. For real spring physics, [Motion](./01-framer-motion.md#springs) is the tool.
- Be **consistent**: reuse the same durations and curves across the app (as tokens, [design system](../../09-ui-components/09-design-system.md)), so motion feels like one product.

```css
:root {
  --ease-out: cubic-bezier(0.16, 1, 0.3, 1);
  --duration-fast: 150ms;
  --duration-base: 250ms;
}
```

## The hard part: enter and exit

CSS animates *changes to existing elements*. React **adds and removes** elements, and removal is instant, so an exit animation has nothing to run on. Options:

### 1. Keep it mounted; toggle a state attribute

Render the element always, and animate visibility. This is what Radix/shadcn do via `data-state`:

```tsx
<div
  data-state={open ? "open" : "closed"}
  className="transition duration-200 data-[state=closed]:pointer-events-none data-[state=closed]:opacity-0 data-[state=closed]:translate-y-2"
  aria-hidden={!open}
/>
```

Simple and robust, but the closed element stays in the DOM. Make sure it's **not focusable or announced** while hidden (`inert`, `visibility: hidden` after the transition, or `aria-hidden` plus `pointer-events: none`), and consider unmounting after it finishes if it's heavy.

### 2. Delay the unmount until the animation ends

Track a "closing" state and unmount on `transitionend`/`animationend`. It's manual and error-prone, but works. This is exactly the bookkeeping `AnimatePresence` automates ([01](./01-framer-motion.md#exit-animations-animatepresence)).

### 3. Modern CSS: `@starting-style` and discrete transitions

Newer CSS lets plain transitions handle **entry** and **`display: none` exits** without JavaScript:

```css
.popover {
  opacity: 1;
  transform: scale(1);
  transition: opacity 200ms, transform 200ms, display 200ms allow-discrete, overlay 200ms allow-discrete;

  @starting-style {            /* the "from" state when the element first renders */
    opacity: 0;
    transform: scale(0.96);
  }
}

.popover[hidden] {             /* the "to" state when display becomes none */
  opacity: 0;
  transform: scale(0.96);
  display: none;
}
```

- **`@starting-style`** defines the initial style for a just-inserted element, so a CSS transition runs on mount (previously impossible without keyframes).
- **`transition-behavior: allow-discrete`** (or `allow-discrete` in the shorthand) lets discrete properties like `display` participate, delaying the switch to `none` until the transition ends.
- This is **recent**. Support arrived across major browsers in 2023–2024 and may not cover your support matrix. Check current browser support before relying on it, and provide a graceful fallback (the element simply appears or disappears instantly).

## Animating height: the classic problem

You can't transition to `height: auto`. Techniques:

```css
/* Grid trick: animate the row track between 0fr and 1fr */
.collapsible { display: grid; grid-template-rows: 0fr; transition: grid-template-rows 250ms ease; }
.collapsible[data-open="true"] { grid-template-rows: 1fr; }
.collapsible > .inner { overflow: hidden; }
```

```tsx
<div className="collapsible" data-open={open}>
  <div className="inner">{children}</div>
</div>
```

The grid-row technique animates real layout (it isn't compositor-only), but it's simple, accessible, and fine for modest content. Recent CSS can also interpolate to intrinsic sizes (`interpolate-size: allow-keywords`), but support is limited at the time of writing. For anything demanding or orchestrated, [Motion](./01-framer-motion.md) can animate `height: "auto"` directly.

## View Transitions and scroll-driven animation

Two newer browser features worth knowing about:

- **View Transitions API** (`document.startViewTransition(() => updateDOM())`) animates between two DOM states by snapshotting old and new views, so you get cross-fades and shared-element morphs for page or list changes with little code. It integrates with SPA navigation, and some routers and React itself are adding hooks for it. Support and framework integration are still evolving, so check the current docs. It's a progressive enhancement, not a requirement.
- **Scroll-driven animations** (`animation-timeline: scroll()` / `view()`) tie a CSS animation's progress to scroll position (reveal-on-scroll, progress bars, parallax) with no JavaScript and no scroll listeners. Browser support is partial at the time of writing, so use it as an enhancement and keep a fallback ([Motion's scroll utilities](./01-framer-motion.md#scroll-and-in-view) otherwise).

## Respect reduced motion

Some users get **motion sickness, vertigo, or distraction** from animation, and set "reduce motion" in their OS. Honor it:

```css
@media (prefers-reduced-motion: reduce) {
  .card { animation: none; }
  .panel { transition: opacity 150ms; transform: none; }    /* keep fades, drop movement */
}
```

In Tailwind: `motion-reduce:transition-none`, `motion-safe:animate-…`. "Reduce" doesn't mean "remove all feedback". Replace large movement (sliding, zooming, parallax) with gentler alternatives (a fade), and keep state changes understandable. Full guidance in [04](./04-animation-performance-and-accessibility.md#accessibility).

## When CSS isn't enough

Move to [Motion](./01-framer-motion.md) when you need:

- **Exit animations** for unmounting elements without manual bookkeeping
- **Orchestration**: staggering and sequencing children, or parent/child coordination
- **Spring physics** and velocity-aware animation
- **Gesture-driven** values (drag, swipe), or values linked to scroll or other values
- **Layout animations** between arbitrary layouts ([02](./02-layout-animations.md))
- **Animating to/from `auto`** sizes or between different units

## Common mistakes

- **`transition: all`**, animating unintended (and expensive) properties.
- **Animating `width`, `height`, `top`, `left`** instead of `transform`, causing layout thrash and jank.
- **Missing `fill-mode`**, so elements flash or snap back around keyframe animations.
- **Expecting a keyframe animation to replay on re-render.** It needs a remount (`key`) or a retriggered class.
- **Trying to animate unmount with CSS alone**, with no plan for removal.
- **Hidden elements still focusable or announced**, since closed panels in the DOM still need `inert`/`visibility: hidden`.
- **Too long or too bouncy** for routine UI. Animation should support the task, not delay it.
- **Inconsistent durations and easings** across components.
- **Ignoring `prefers-reduced-motion`.**
- **Relying on very new CSS** (`@starting-style`, scroll-driven animations, `interpolate-size`) without checking support or providing fallbacks.
- **Overusing `will-change`**, which reserves memory and can slow things down ([04](./04-animation-performance-and-accessibility.md#what-makes-an-animation-cheap)).

## Quick summary

- Use **CSS transitions** for state changes and **keyframes** for self-running or multi-step effects. They're cheap, JavaScript-free, and (for transitions) interruptible.
- Animate **`transform` and `opacity`**. Avoid animating layout properties (`width`, `height`, `top`, `left`).
- React toggles a class or attribute; the browser interpolates. Replay keyframes by remounting with a `key`.
- **Exit animations** are the hard part: keep the element mounted with a state attribute, delay unmount, use modern CSS (`@starting-style` + `allow-discrete`, with fallbacks), or use Motion's `AnimatePresence`.
- Keep durations short (about 100–300 ms for most UI), use `ease-out` for entrances, be consistent via tokens.
- Honor `prefers-reduced-motion` by swapping movement for gentler feedback.
- Reach for Motion when you need exit animation, orchestration, springs, gestures, or layout animation.

## Next

[01 — Motion (Framer Motion)](./01-framer-motion.md)