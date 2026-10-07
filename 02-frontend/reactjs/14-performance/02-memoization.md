# Memoization

Memoization means **remembering a result so you don't redo the work**. React offers three tools: `React.memo` (skip re-rendering a component), `useMemo` (cache a computed value), and `useCallback` (cache a function). They're powerful when aimed at a measured problem and counterproductive as a habit.

(Basic syntax: [useMemo and useCallback](../03-hooks/07-useMemo-and-useCallback.md). This note is about **when they actually help**.)

## The cost-benefit reality

Memoization is not free:

- It adds **code** and **cognitive load** (dependency arrays, stale-closure risk).
- It adds **runtime work**: React must store the previous value and compare dependencies on every render.
- If the thing you memoized was cheap, you've made the app *slower and more complex*.

So the question is never "could this be memoized?" but **"is this measurably slow or wasteful, and will memoizing fix it?"** Measure first ([profiling](./00-profiling-and-measuring.md)), and try [restructuring](./01-rendering-performance.md) before memoizing.

## `React.memo`: skip re-rendering a component

```tsx
const ProjectRow = memo(function ProjectRow({ project, onSelect }: Props) {
  return <li onClick={() => onSelect(project.id)}>{project.name}</li>
})
```

When the parent re-renders, React compares the new props to the previous ones **shallowly** (`Object.is` per prop). If all are equal, it **skips** rendering `ProjectRow` and reuses the last result.

`memo` only helps when:

1. The component is **expensive to render** (or rendered hundreds of times), **and**
2. It **often re-renders with the same props**, **and**
3. Its props are **actually stable** between renders.

Point 3 is where it most often fails.

### Why `memo` silently does nothing

```tsx
function Parent() {
  const [count, setCount] = useState(0)

  return (
    <>
      <button onClick={() => setCount(count + 1)}>{count}</button>
      <ProjectRow
        project={{ id: "1", name: "Roadmap" }}      // ✗ new object every render
        onSelect={(id) => console.log(id)}          // ✗ new function every render
      />
      <ProjectRow project={p}>
        <Icon />                                    // ✗ `children` is a new element every render
      </ProjectRow>
    </>
  )
}
```

Each render creates new object/function/element references, so the shallow comparison fails and `memo` re-renders anyway. **`memo` is only as effective as the stability of the props you pass.** You need the next two tools to stabilize them, or to restructure so the props don't change.

### Custom comparison

```tsx
const Row = memo(RowImpl, (prev, next) => prev.item.id === next.item.id && prev.item.version === next.item.version)
```

Use rarely. A custom comparator is easy to get wrong (ignoring a prop that matters causes stale UI), and comparing deeply can cost more than rendering.

## `useMemo`: cache a value

Two legitimate uses.

### 1. Skip an expensive calculation

```tsx
const visible = useMemo(
  () => items.filter((i) => matches(i, query)).sort(byDate),
  [items, query]
)
```

Worth it when the calculation is genuinely heavy (large lists, complex transforms) **and** the component re-renders for unrelated reasons. A rough guide from the React docs: if `console.time` shows the work taking about a millisecond or more (measured with CPU throttling), it's a candidate. Filtering 20 items isn't.

### 2. Keep a reference stable

When a value is passed to a memoized child, used in another hook's dependency array, or provided as a context value:

```tsx
const options = useMemo(() => ({ sort, direction }), [sort, direction])
useEffect(() => { fetchWith(options) }, [options])              // effect only reruns when sort/direction change

const value = useMemo(() => ({ user, login, logout }), [user, login, logout])
return <AuthContext value={value}>{children}</AuthContext>        // stable context value
```

### `useMemo` is a hint, not a guarantee

React may discard cached values (for example, to free memory), and may in the future. **Your code must be correct without the memoization**; it should only be an optimization. Never use `useMemo` to hold state or to guarantee that something runs once.

## `useCallback`: cache a function

`useCallback(fn, deps)` is `useMemo(() => fn, deps)`: it returns the **same function reference** until dependencies change.

```tsx
function ProjectList({ projects }: { projects: Project[] }) {
  const [selectedId, setSelectedId] = useState<string | null>(null)

  const handleSelect = useCallback((id: string) => setSelectedId(id), [])   // setState is stable; no deps needed

  return projects.map((p) => <ProjectRow key={p.id} project={p} onSelect={handleSelect} />)
}
```

`useCallback` is **pointless by itself.** It only pays off paired with:

- A **`memo`-wrapped child** receiving the function as a prop, or
- Use of the function in a **dependency array** (`useEffect`, `useMemo`).

Wrapping every handler in `useCallback` with no memoized consumers adds overhead for zero benefit.

Prefer the **functional updater form** to keep dependency arrays empty and callbacks stable:

```tsx
const add = useCallback((item: Item) => setItems((prev) => [...prev, item]), [])   // no `items` dependency
```

## The full picture: all three working together

```tsx
const ProjectRow = memo(function ProjectRow({ project, onSelect }: RowProps) { /* … */ })

function ProjectList({ projects, query }: Props) {
  const visible = useMemo(() => projects.filter((p) => p.name.includes(query)), [projects, query])
  const handleSelect = useCallback((id: string) => select(id), [select])

  return visible.map((p) => <ProjectRow key={p.id} project={p} onSelect={handleSelect} />)
}
```

- `useMemo` caches the filtered array.
- `useCallback` keeps `onSelect` stable.
- `memo` lets each row skip rendering when *its* `project` and `onSelect` didn't change.

Remove any one of them and the chain breaks. That fragility is why memoization is hard to maintain by hand, and why measuring matters.

## When memoization genuinely helps

| Situation | Tool |
|---|---|
| Long list where each row is costly and the parent updates often | `memo` on the row + stable props |
| Heavy derived data (large sort/filter/aggregate) | `useMemo` |
| Value used as an effect dependency, to avoid re-running the effect | `useMemo` / `useCallback` |
| Context provider `value` | `useMemo` ([context](../13-state-management/01-context-patterns-and-performance.md)) |
| Expensive child re-rendering because of **unrelated** parent state | Restructure first; `memo` if that's not possible |
| Third-party components that are `memo`-based or compare by reference (chart/map libraries) | Stable props via `useMemo` |

## When it doesn't help (or hurts)

- **Cheap components or calculations**, where overhead exceeds savings.
- **Props that change on nearly every render anyway** (fresh data each time): the comparison cost is wasted.
- **`memo` with unstable props** (see above).
- **Everything memoized "just in case"**: a codebase full of `useCallback` nobody can reason about.
- **Hiding a structural problem.** If `memo` is papering over state that's too high in the tree, fix the structure ([01](./01-rendering-performance.md#fix-1-move-state-down-colocate)).

## Stale closures

Memoized functions capture the values from the render they were created in. A wrong dependency array gives you stale data:

```tsx
const handleSave = useCallback(() => save(draft), [])      // ✗ always saves the FIRST render's draft
const handleSave = useCallback(() => save(draft), [draft]) // ✓ (but now it changes whenever draft does)
```

Keep the ESLint `react-hooks/exhaustive-deps` rule on and treat its warnings as bugs. If you need a function that always sees the latest values *without* changing identity, a ref can hold it:

```tsx
const latest = useRef(handler)
useEffect(() => { latest.current = handler })
const stable = useCallback((...args) => latest.current(...args), [])
```

(Recent React versions also provide `useEffectEvent` for effect-specific cases; check the docs for your version.)

## The React Compiler changes the default

The [React Compiler](../15-concurrent-and-modern-react/06-react-compiler.md) is a build-time tool that **automatically memoizes** components and values where it can prove it's safe. With it enabled:

- You mostly **stop writing `useMemo`/`useCallback`/`memo` by hand** for render optimization.
- Existing manual memoization still works, and remains useful where precise control matters, for example a value used as an effect dependency.
- It only works on code following the [Rules of React](../03-hooks/00-hook-rules.md) (pure components, no mutation of props or state). Code that breaks the rules is skipped or can behave incorrectly.

If your project uses the compiler, the guidance becomes: **write straightforward code, fix structural problems, and add manual memoization only when profiling says it's needed.** If it doesn't, everything above applies as written.

## Measuring a memoization

```tsx
// Before/after: React Profiler "why did this render" + ranked chart
// With throttled CPU, compare commit durations for the same interaction.
```

1. Record the interaction in the Profiler and note the ranked durations and render counts.
2. Apply **one** memoization.
3. Record again. Did the component stop rendering ("didn't render" in grey)? Did the commit get faster?
4. If not noticeable, **revert it.** Complexity without payoff is a cost.

## Common mistakes

- **Memoizing by default**, without measuring.
- **`memo` with inline objects/functions/JSX children**, so it never skips.
- **`useCallback` without a memoized consumer or dependency use**, with no effect at all.
- **Wrong or missing dependencies**, causing stale closures (or disabling the lint rule to silence it).
- **Treating `useMemo` as a semantic guarantee** (it's an optimization hint).
- **Comparing deeply** in a custom `memo` comparator, costing more than the render.
- **Memoizing a cheap value** and paying comparison overhead on every render.
- **Using memoization to avoid fixing state placement.**
- **Forgetting that context updates bypass `memo`**: a memoized component reading a changing context still re-renders.
- **Hand-memoizing everywhere in a compiler-enabled codebase** for no reason.

## Quick summary

- Memoization is a **targeted** optimization: use it when profiling shows a slow or needlessly repeated render, after trying to restructure.
- **`memo`** skips a component when props are shallow-equal; it's defeated by unstable props.
- **`useMemo`** caches a costly calculation or stabilizes a reference (effect deps, context values). **`useCallback`** stabilizes a function, and is only useful with a memoized consumer or in a dependency array.
- The three work as a chain: break one link and the others do nothing.
- Correctness must never depend on memoization; keep `exhaustive-deps` honest to avoid stale closures.
- The React Compiler can do most of this automatically; with it, hand-memoize sparingly.

## Next

[03 — Code splitting and lazy loading](./03-code-splitting-and-lazy-loading.md)
