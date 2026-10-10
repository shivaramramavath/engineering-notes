# Layout Animations

Some of the most satisfying UI motion is **animating between layouts**: a list reorders, a card expands, a tab underline slides to the new tab, a thumbnail grows into a full-screen view. In CSS this is nearly impossible, because layout properties (`width`, `height`, `top`, `left`, flex/grid position) don't animate cheaply, and the browser has no idea where an element *was* once React re-renders it somewhere else.

**Layout animations** solve that. You change the layout (a class, a sibling, a prop), and Motion animates each affected element from its old position and size to its new one, using cheap transforms.

## The `layout` prop

```tsx
import { motion } from "motion/react"

function Panel() {
  const [expanded, setExpanded] = useState(false)

  return (
    <motion.div
      layout                                          // "animate me whenever my layout changes"
      onClick={() => setExpanded((e) => !e)}
      className={expanded ? "w-80 p-6" : "w-40 p-3"}  // a plain, instant layout change
      transition={{ type: "spring", stiffness: 400, damping: 35 }}
    >
      <motion.h2 layout="position">Title</motion.h2>
      {expanded && <p>More detail that appears when expanded.</p>}
    </motion.div>
  )
}
```

You don't write any animation values. You make the **layout change** (here via class names), and `layout` makes the transition smooth. The same works for reordered lists, changed flex/grid arrangement, conditionally inserted siblings, and so on.

Variants:

- **`layout`** (or `layout={true}`): animate both position and size.
- **`layout="position"`**: animate position only (size changes snap). Use for content inside a resizing container so text doesn't stretch.
- **`layout="size"`**: animate size only.

## How it works: FLIP

Layout animation is an automated version of the **FLIP** technique (First, Last, Invert, Play):

```text
1. First    Before the change, measure the element's bounding box.
2. Last     Let React apply the change; measure the new bounding box.
3. Invert   Apply a transform that makes the element LOOK like it's still at the old position/size.
4. Play     Animate the transform back to identity. The element glides to its real new position.
```

```text
          before                     after (instant)              what the user sees
        ┌───────┐                                              ┌───────┐ → animates →
        │   A   │   (state change)         ┌─────────────┐     │   A   │   ┌─────────────┐
        └───────┘                          │      A      │     └───────┘   │      A      │
                                           └─────────────┘                 └─────────────┘
```

Why this is clever: the **real layout is already final** (so everything else is correct and accessible), and only a cheap **`transform`** animates ([performance](./04-animation-performance-and-accessibility.md#what-makes-an-animation-cheap)). You never animate `width` or `top` directly.

Practical consequences of how it works:

- Motion **measures the DOM** before and after updates (it hooks into React's commit phase, [layout effects](../../03-hooks/08-useLayoutEffect.md)). Measuring has a cost, so animating hundreds of `layout` elements at once can be slow.
- Scale-based animation can **distort** content (text and children stretch during a size animation). Motion corrects child distortion for descendants marked with `layout` and for border radius and box shadow when they're set through `style` (not classes) on the motion element:

```tsx
<motion.div layout style={{ borderRadius: 16 }} />      // radius stays correct during the scale animation
```

- Use `layout="position"` on text and images inside a resizing parent to avoid squashed content.

## Reordering and list changes

With a keyed list, adding, removing, or reordering items makes the **siblings slide** to their new places:

```tsx
<ul>
  <AnimatePresence initial={false} mode="popLayout">
    {todos.map((todo) => (
      <motion.li
        key={todo.id}
        layout
        initial={{ opacity: 0, scale: 0.95 }}
        animate={{ opacity: 1, scale: 1 }}
        exit={{ opacity: 0, scale: 0.95 }}
        transition={{ type: "spring", stiffness: 500, damping: 40 }}
      >
        {todo.text}
      </motion.li>
    ))}
  </AnimatePresence>
</ul>
```

- **`layout` on every item** makes the remaining ones glide when one is removed or inserted.
- **`mode="popLayout"`** on `AnimatePresence` pops the exiting element out of the document flow, so siblings start moving immediately instead of waiting for the exit to finish. (An exiting element in `popLayout` needs its parent to have non-`static` positioning, and a component used as a child must forward its ref.)
- Keys must be **stable IDs** ([lists and keys](../../01-fundamentals/05-lists-and-keys.md)), or Motion can't tell which element moved.
- For drag-to-reorder interactions, use `Reorder` ([03](./03-gestures.md#reordering-lists)).

## Shared layout animations: `layoutId`

Give two **different** elements the same `layoutId`, and when one appears as the other disappears, Motion animates between them as if it were one element moving. This is how you get "shared element" transitions.

### Example 1: a sliding tab indicator

```tsx
function Tabs({ tabs, active, onChange }: Props) {
  return (
    <div role="tablist" className="relative flex">
      {tabs.map((tab) => (
        <button key={tab.id} role="tab" aria-selected={tab.id === active} onClick={() => onChange(tab.id)} className="relative px-4 py-2">
          {tab.label}
          {tab.id === active && (
            <motion.span
              layoutId="tab-underline"                     // same id on whichever tab is active
              className="absolute inset-x-0 bottom-0 h-0.5 bg-primary"
              transition={{ type: "spring", stiffness: 500, damping: 40 }}
            />
          )}
        </button>
      ))}
    </div>
  )
}
```

Only one `motion.span` with `layoutId="tab-underline"` is rendered at a time. When the active tab changes, the old one unmounts and the new one mounts, and Motion **morphs** the underline from the old tab's position to the new one's. (For the tab semantics themselves, see [Tabs](../../09-ui-components/04-tabs.md).)

### Example 2: card → detail view

```tsx
function Gallery({ items }: { items: Item[] }) {
  const [selected, setSelected] = useState<Item | null>(null)

  return (
    <LayoutGroup>
      <div className="grid grid-cols-3 gap-4">
        {items.map((item) => (
          <motion.button key={item.id} layoutId={`card-${item.id}`} onClick={() => setSelected(item)}>
            <motion.img layoutId={`img-${item.id}`} src={item.thumb} alt="" />
          </motion.button>
        ))}
      </div>

      <AnimatePresence>
        {selected && (
          <>
            <motion.div className="fixed inset-0 bg-black/50" initial={{ opacity: 0 }} animate={{ opacity: 1 }} exit={{ opacity: 0 }} onClick={() => setSelected(null)} />
            <motion.div layoutId={`card-${selected.id}`} className="fixed inset-8 grid place-items-center rounded-xl bg-background">
              <motion.img layoutId={`img-${selected.id}`} src={selected.full} alt={selected.title} />
            </motion.div>
          </>
        )}
      </AnimatePresence>
    </LayoutGroup>
  )
}
```

The thumbnail seems to grow into the large view and shrink back on close. The same `layoutId` links the small and large versions, and `AnimatePresence` handles the exit.

Notes on `layoutId`:

- IDs must be **unique per logical element** (include the item's ID).
- Use **`LayoutGroup`** to scope IDs, and to make layout animations in **separate components** coordinate with each other. Components that don't share a React parent need a shared `LayoutGroup` to animate in sync.
- Pair with **`AnimatePresence`** so the "leaving" element can animate out.
- A modal built this way still needs proper semantics: focus trapping, `role="dialog"`, `Esc` to close. Animation doesn't replace those ([dialogs](../../09-ui-components/02-dialogs-and-modals.md), [focus](./04-animation-performance-and-accessibility.md#focus-and-animated-presence)). Radix Dialog with `forceMount` and `asChild` is a solid base.

## Scroll containers and fixed elements

Layout measurement is relative to the page, so scrolling *during* a layout animation, or animating inside a scrollable container, can produce wrong results.

- Mark scrollable ancestors with **`layoutScroll`** so Motion accounts for their scroll offset:

```tsx
<motion.div layoutScroll style={{ overflow: "auto" }}>
  <motion.div layout>…</motion.div>
</motion.div>
```

- Mark `position: fixed` (or the root of a separate positioning context) with **`layoutRoot`**.
- Elements inside a `transform`ed parent can produce offsets, because layout is measured against the transformed box. If positions look wrong, check ancestors with transforms or scroll.

## Controlling timing

```tsx
<motion.div layout transition={{ layout: { type: "spring", stiffness: 300, damping: 30 } }} />   // only the layout animation
```

- A `transition` with a `layout` key configures just the layout animation, leaving other animated properties alone.
- For shared elements (`layoutId`), a slightly **slower spring or a tween around 250–400 ms** usually reads as the elements being "the same thing".
- Callbacks: `onLayoutAnimationStart` and `onLayoutAnimationComplete`, which are useful for things like deferring heavy work or focus changes.

## Cost and limits

- **Measuring is the cost.** `layout` triggers DOM reads on each affected element on every relevant render. Add `layout` only to elements that actually move, not to every node in a big tree.
- **Large lists** with `layout` on every row can be slow. [Virtualize](../../14-performance/04-virtualization.md) (and animate only visible rows), or limit animations to small lists.
- **Frequent re-renders** of a layout-animated tree (a parent updating every few ms) can retrigger measurement. Memoize stable children ([memoization](../../14-performance/02-memoization.md)) and keep high-frequency state out of the tree ([motion values](./01-framer-motion.md#motion-values-animation-without-re-rendering)).
- **Interrupting** a layout animation is handled gracefully (it continues from the current visual position), a major advantage over hand-rolled FLIP.
- **Server rendering**: layout animations only happen on the client after hydration.

## When to use which

| Goal | Use |
|---|---|
| Element moves/resizes because layout changed | **`layout`** |
| List items slide when siblings are added/removed/reordered | **`layout`** on items (+ `AnimatePresence`) |
| An element "becomes" another (tab indicator, card → modal) | **`layoutId`** |
| Drag to reorder | **`Reorder`** ([03](./03-gestures.md#reordering-lists)) |
| A simple CSS-only expand/collapse | Grid-rows trick ([00](./00-css-transitions-and-animations.md#animating-height-the-classic-problem)) |
| Heavy animation of many elements | Reconsider; consider simpler animations or fewer animated elements |

## Common mistakes

- **Forgetting stable keys**, so list items don't animate as the same element.
- **`layout` on everything**, adding measurement overhead on pages with many nodes.
- **Squashed or stretched text** during size animations. Use `layout="position"` on content, and `layout` on children that need correction.
- **Setting `borderRadius`/`boxShadow` via classes** on a scaled element, producing visible distortion. Set them through `style` on the motion element.
- **Using `layoutId` without `LayoutGroup`/`AnimatePresence`** when elements live in different components or need to animate out.
- **Duplicate `layoutId`s** rendered at the same time, giving unpredictable results.
- **Scroll containers without `layoutScroll`**, so animations jump.
- **Animating inside transformed ancestors** without accounting for them.
- **Treating the animated modal as done** while skipping focus management and dialog semantics.
- **Expecting layout animation on server render**, or not testing hydration flashes.
- **Animating a huge list** instead of virtualizing or limiting the animation.

## Quick summary

- **Layout animations** let you make an instant layout change and have Motion animate each element between its old and new position and size.
- It's an automated **FLIP**: measure first, measure last, invert with a `transform`, play it back, so only cheap transforms animate while real layout is already final.
- **`layout`** animates an element's own changes (`"position"` and `"size"` limit it); with **`AnimatePresence mode="popLayout"`** and stable keys, lists glide as items come and go.
- **`layoutId`** morphs between two different elements (tab indicators, card → detail), scoped with **`LayoutGroup`**.
- Set `borderRadius`/`boxShadow` via `style`, use `layoutScroll`/`layoutRoot` for scroll and fixed contexts, and keep the number of layout-animated elements modest.
- Animated modals and tabs still need real **semantics and focus management**.

## Next

[03 — Gestures](./03-gestures.md)