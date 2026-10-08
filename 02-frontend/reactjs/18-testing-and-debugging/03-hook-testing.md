# Hook Testing

Custom hooks hold reusable logic: debouncing, toggles, subscriptions, data fetching. You can't call a hook outside a component, so to test one directly you render a tiny host component for it. RTL's **`renderHook`** does exactly that.

## First: do you need to test the hook directly?

Most hooks are used by one component and are best tested **through that component's behavior** ([02](./02-component-testing-with-rtl.md)). A component test covers the hook, the wiring between them, and what the user sees.

Test a hook **directly** when:

- It's a **reusable** hook used in many places (`useDebouncedValue`, `useLocalStorage`, `useMediaQuery`).
- Its logic is **complex** (state machines, timing, subscriptions) and awkward to exercise through UI.
- It's part of a **shared library** where the hook *is* the public API.

Don't test a hook directly if it just wraps `useState` and `useQuery` for one screen. That would test implementation. And **don't mock hooks** in component tests to avoid their behavior; mock their *boundary* (the network) instead.

## `renderHook`

```tsx
import { renderHook, act } from "@testing-library/react"

function useCounter(initial = 0) {
  const [count, setCount] = useState(initial)
  const increment = useCallback(() => setCount((c) => c + 1), [])
  const reset = useCallback(() => setCount(initial), [initial])
  return { count, increment, reset }
}

it("increments and resets", () => {
  const { result } = renderHook(() => useCounter(5))

  expect(result.current.count).toBe(5)

  act(() => result.current.increment())
  expect(result.current.count).toBe(6)

  act(() => result.current.reset())
  expect(result.current.count).toBe(5)
})
```

- **`result.current`** is the hook's latest return value. It's a **live reference**: read it *after* acting, not destructured before (`const { count } = result.current` captures a stale snapshot).
- **`act(() => …)`** wraps anything that causes a state update (calling a returned function), so React processes the update before your assertion. (Import `act` from RTL or, in React 19, from `react`.)
- The hook is mounted in a real component, so effects and cleanup behave as in the app.

## Passing and changing arguments: `rerender`

```tsx
const { result, rerender } = renderHook(({ initial }) => useCounter(initial), {
  initialProps: { initial: 0 },
})

rerender({ initial: 10 })           // same hook instance, new arguments
act(() => result.current.reset())
expect(result.current.count).toBe(10)
```

`rerender` is how you test "what happens when props/arguments change", for example effects with dependencies, or memoization.

## Cleanup: `unmount`

```tsx
const { unmount } = renderHook(() => useWindowListener())
unmount()
expect(removeSpy).toHaveBeenCalled()      // cleanup ran: no leaked listeners
```

Testing cleanup catches leaked subscriptions and timers, which are real production bugs.

## Hooks that need context: `wrapper`

```tsx
import { QueryClient, QueryClientProvider } from "@tanstack/react-query"

function createWrapper() {
  const queryClient = new QueryClient({ defaultOptions: { queries: { retry: false } } })   // fresh per test
  return function Wrapper({ children }: { children: React.ReactNode }) {
    return <QueryClientProvider client={queryClient}>{children}</QueryClientProvider>
  }
}

const { result } = renderHook(() => useProject("42"), { wrapper: createWrapper() })
```

Reuse the same providers as your component tests ([custom render](./02-component-testing-with-rtl.md#providers-a-custom-render)).

## Async hooks

For hooks that fetch or schedule, **wait for the state you expect**:

```tsx
import { waitFor } from "@testing-library/react"

it("loads a project", async () => {
  const { result } = renderHook(() => useProject("42"), { wrapper: createWrapper() })

  expect(result.current.isPending).toBe(true)

  await waitFor(() => expect(result.current.isSuccess).toBe(true))
  expect(result.current.data).toEqual({ id: "42", name: "Roadmap" })
})

it("exposes an error", async () => {
  server.use(http.get("/api/projects/42", () => HttpResponse.json({ message: "nope" }, { status: 500 })))
  const { result } = renderHook(() => useProject("42"), { wrapper: createWrapper() })

  await waitFor(() => expect(result.current.isError).toBe(true))
})
```

The data comes from a mocked network ([MSW](./04-mocking-and-msw.md)), so the real API client, query cache, and hook all run. Per-test overrides (`server.use`) simulate errors. Don't mock `useQuery` itself.

## Time-based hooks: fake timers

```tsx
function useDebouncedValue<T>(value: T, delay = 300) {
  const [debounced, setDebounced] = useState(value)
  useEffect(() => {
    const id = setTimeout(() => setDebounced(value), delay)
    return () => clearTimeout(id)
  }, [value, delay])
  return debounced
}

describe("useDebouncedValue", () => {
  beforeEach(() => { vi.useFakeTimers() })
  afterEach(() => { vi.useRealTimers() })

  it("updates only after the delay with no further changes", () => {
    const { result, rerender } = renderHook(({ value }) => useDebouncedValue(value, 300), {
      initialProps: { value: "a" },
    })

    rerender({ value: "ab" })
    rerender({ value: "abc" })
    expect(result.current).toBe("a")                 // not yet

    act(() => { vi.advanceTimersByTime(299) })
    expect(result.current).toBe("a")                 // still not

    act(() => { vi.advanceTimersByTime(1) })
    expect(result.current).toBe("abc")               // only the last value, after the full delay
  })
})
```

Advancing timers must be wrapped in `act`, because a timer firing triggers a state update. Always restore real timers afterward ([fake timers](./01-vitest.md#fake-timers)).

## Hooks with browser APIs and subscriptions

```tsx
it("tracks online status", () => {
  const { result } = renderHook(() => useOnlineStatus())
  expect(result.current).toBe(true)

  act(() => {
    vi.spyOn(navigator, "onLine", "get").mockReturnValue(false)
    window.dispatchEvent(new Event("offline"))
  })
  expect(result.current).toBe(false)
})
```

For hooks built on `useSyncExternalStore` ([external stores](../16-advanced-react/02-external-stores.md)), dispatch the event the store subscribes to and assert on `result.current`. For store hooks (Zustand), reset the store in `beforeEach` so state doesn't leak ([Zustand testing](../13-state-management/03-zustand.md#testing)).

## Testing the hook's contract, not its internals

```tsx
// ✗ implementation: assumes a particular internal structure
expect(setStateSpy).toHaveBeenCalledWith(…)

// ✓ contract: given inputs, what does the hook return, and how does it respond to actions?
expect(result.current.count).toBe(6)
```

Treat the hook as a black box: **arguments in, return value out, plus effects it triggers** (callbacks called, listeners removed, requests made).

## Checklist for a hook test file

- Initial return value
- Each action/function it returns, and the resulting value
- Behavior when arguments change (`rerender`)
- Async states: pending → success, and error
- Cleanup on unmount
- Edge cases (empty values, rapid changes, boundary delays)
- Stable identities if that's part of the contract (`expect(result.current.increment).toBe(prev.increment)` after a rerender, for callbacks consumers put in dependency arrays)

## Common mistakes

- **Testing a hook directly when a component test would cover it better.**
- **Destructuring `result.current` once** and asserting on stale values.
- **Forgetting `act`** around state-changing calls and timer advances, producing warnings and stale reads.
- **Mocking `useQuery`/`useState`** instead of mocking the network.
- **Sharing a `QueryClient` or store** between tests.
- **Leaving fake timers on**, hanging later tests.
- **Not testing cleanup**, so leaks ship.
- **Asserting before async work settles** instead of `waitFor`.
- **Calling a hook directly** (`useCounter()` in a test body). It throws, because hooks need a rendering component.
- **Over-testing trivial wrappers** around `useState`.

## Quick summary

- Prefer testing hooks **through components**; test directly when a hook is reusable, complex, or a public API.
- `renderHook(() => useX(args))` gives `result.current` (live), `rerender(newProps)`, and `unmount()`.
- Wrap state-changing calls and timer advances in **`act`**; use **`waitFor`** for async results.
- Provide context with the **`wrapper`** option (fresh `QueryClient`, retry off), and mock the **network** with MSW, not the hook.
- Use **fake timers** for debounce, polling, and timeouts, and always restore them.
- Test the hook's **contract**: initial value, actions, argument changes, async states, cleanup, edge cases.

## Next

[04 — Mocking and MSW](./04-mocking-and-msw.md)
