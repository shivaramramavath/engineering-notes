# Layered Architecture

[Feature folders](./00-feature-based-architecture.md) decide *where* code lives. **Layers** decide *what kind of responsibility* each piece of code has, **inside** a feature. The goal is simple: **keep the parts that change for different reasons apart**, so changing how data is fetched doesn't mean touching JSX, and changing a business rule doesn't mean touching the network layer.

```text
┌───────────────────────────────────────────┐
│ UI            components: render + events │   changes when the design changes
├───────────────────────────────────────────┤
│ Application   hooks: state, queries,      │   changes when the workflow changes
│               orchestration, side effects │
├───────────────────────────────────────────┤
│ Domain        pure logic, types, rules,   │   changes when the business rules change
│               mappers                     │
├───────────────────────────────────────────┤
│ Data access   API client, endpoints,      │   changes when the backend changes
│               storage, browser APIs       │
└───────────────────────────────────────────┘
        dependencies point DOWN only (UI → hooks → domain / data)
```

You don't need four literal folders for every feature. The point is the **separation of concerns**. A small feature may have these layers as four *files*, or even as sections of one file.

## The problem: everything in the component

```tsx
function CheckoutPage() {
  const [items, setItems] = useState<CartItem[]>([])
  const [coupon, setCoupon] = useState("")
  const [error, setError] = useState<string | null>(null)

  useEffect(() => {
    fetch("/api/cart")
      .then((r) => r.json())
      .then((data) => setItems(data.items.map((i: any) => ({ ...i, price: i.price_cents / 100 }))))
  }, [])

  const subtotal = items.reduce((s, i) => s + i.price * i.qty, 0)
  const discount = coupon === "SAVE10" ? subtotal * 0.1 : 0
  const shipping = subtotal - discount > 50 ? 0 : 4.99
  const total = subtotal - discount + shipping

  async function pay() {
    const res = await fetch("/api/orders", { method: "POST", body: JSON.stringify({ items, coupon }) })
    if (!res.ok) setError("Payment failed")
  }

  return (/* 150 lines of JSX */)
}
```

This one component knows the **URL shapes**, the **backend's field names** (`price_cents`), the **pricing rules** (coupon codes, free-shipping threshold), the **loading logic**, and the **markup**. Consequences:

- You can't test the pricing rules without rendering and mocking `fetch`.
- A backend rename breaks the component.
- A pricing change risks the UI, and a UI change risks the pricing.
- Another screen needing the same totals copies the logic (and diverges).

## Layer by layer

### Data access: talking to the outside world

The only layer that knows about **URLs, HTTP, storage, and transport details**. It returns data and throws typed errors, and nothing else.

```ts
// features/cart/api/cart.api.ts
import { api } from "@/shared/lib/api/client"

export type CartDto = { items: { id: string; name: string; price_cents: number; qty: number }[] }

export const cartApi = {
  get: (signal?: AbortSignal) => api.get<CartDto>("/cart", { signal }),
  checkout: (input: CheckoutRequestDto) => api.post<OrderDto>("/orders", input),
}
```

Details: [API architecture](./02-api-architecture.md) and the [API client](../11-api-integration/02-api-client.md). Components never call `fetch`.

### Domain: the rules, as pure code

**Types and pure functions** that express what the product *means*, independent of React and independent of the backend's wire format.

```ts
// features/cart/domain/cart.ts
export type CartItem = { id: string; name: string; price: number; qty: number }   // price in currency units

export function subtotal(items: CartItem[]) {
  return items.reduce((sum, i) => sum + i.price * i.qty, 0)
}

export function discountFor(coupon: string | null, sub: number) {
  return coupon === "SAVE10" ? sub * 0.1 : 0
}

export function shippingFor(amountAfterDiscount: number) {
  return amountAfterDiscount > 50 ? 0 : 4.99
}

export function totals(items: CartItem[], coupon: string | null) {
  const sub = subtotal(items)
  const discount = discountFor(coupon, sub)
  const shipping = shippingFor(sub - discount)
  return { subtotal: sub, discount, shipping, total: sub - discount + shipping }
}
```

```ts
// features/cart/domain/cart.mapper.ts: translate backend shape → domain shape
export function toCartItems(dto: CartDto): CartItem[] {
  return dto.items.map((i) => ({ id: i.id, name: i.name, price: i.price_cents / 100, qty: i.qty }))
}
```

No React, no `fetch`, no browser APIs. That means:

- **Trivial to test** with plain inputs and outputs ([pure logic tests](../18-testing-and-debugging/00-testing-fundamentals.md#testing-pure-logic)).
- **Reusable** anywhere: components, hooks, reducers, tests, even a Node script.
- **Stable**: it changes when *business rules* change, not when the UI or the API does.

Reducers belong here too ([reducer pattern](../13-state-management/02-reducer-and-context-pattern.md)).

### Application: orchestrating state and workflows (hooks)

The glue. Hooks connect the data layer to the domain and expose **exactly what the UI needs**: loading and error state, derived values, and actions.

```ts
// features/cart/hooks/useCart.ts
export function useCart(coupon: string | null) {
  const query = useQuery({
    queryKey: cartKeys.detail(),
    queryFn: ({ signal }) => cartApi.get(signal),
    select: toCartItems,                       // DTO → domain, via the mapper
  })

  const items = query.data ?? EMPTY
  const summary = useMemo(() => totals(items, coupon), [items, coupon])

  const checkout = useMutation({
    mutationFn: () => cartApi.checkout({ items: items.map(toLine), coupon }),
    onSuccess: () => queryClient.invalidateQueries({ queryKey: cartKeys.all }),
  })

  return { items, summary, isLoading: query.isPending, error: query.error, checkout }
}
```

This is where **React-specific concerns** live: [TanStack Query](../12-server-state/03-tanstack-query.md), context, [stores](../13-state-management/03-zustand.md), effects, cache invalidation, and combining multiple sources into one clean interface. The UI gets a small, task-shaped API instead of five raw hooks.

### UI: render and report events

Components **render what they're given** and **call what they're handed**:

```tsx
// features/cart/components/CheckoutPage.tsx
export function CheckoutPage() {
  const [coupon, setCoupon] = useState<string | null>(null)
  const { items, summary, isLoading, error, checkout } = useCart(coupon)

  if (isLoading) return <CartSkeleton />
  if (error) return <ErrorState error={error} />

  return (
    <>
      <CartLines items={items} />
      <CouponField value={coupon} onChange={setCoupon} />
      <OrderSummary {...summary} />
      <Button onClick={() => checkout.mutate()} disabled={checkout.isPending}>Pay</Button>
    </>
  )
}
```

The component has no pricing logic, no URLs, no backend field names. Changing the free-shipping threshold touches `cart.ts` and its tests only. Changing the API touches `cart.api.ts` and the mapper. Redesigning the page touches JSX.

Presentational pieces (`CartLines`, `OrderSummary`) can be even simpler: **props in, markup out**, reusable and easy to test or put in a [design system](../09-ui-components/09-design-system.md) if generic.

## The dependency rule

```text
UI  ──►  hooks  ──►  domain
              └─────►  data access
domain  ──►  (nothing: no React, no API, no browser)
```

- **Domain depends on nothing.** It's the most stable, most reusable layer.
- **Data access depends on the shared client**, not on React or UI.
- **Hooks** may depend on data access and domain.
- **UI** depends on hooks, domain types, and shared UI, but never on data access directly.
- **No upward imports.** A domain function never imports a hook. A data function never imports a component.

You can check this with the same lint approach as [feature boundaries](./00-feature-based-architecture.md#enforce-it-dont-just-document-it): restrict `@/…/api` imports from `components/`, and React imports from `domain/`.

```js
{
  files: ["src/features/*/domain/**"],
  rules: { "no-restricted-imports": ["error", { paths: ["react", "react-dom", "@tanstack/react-query"] }] },
}
```

## Mapping at the boundary

The backend's data shape is **not** your domain model. Wire formats have snake_case, cents, nullable everything, deprecated fields, and flat IDs. If components consume DTOs directly, every backend change ripples through the UI.

- **Map once, at the edge** (in the data or hook layer), into the shape your UI wants: camelCase, proper types (`Date`, not ISO strings; money as a unit your logic expects), no irrelevant fields.
- **Validate** what comes in with a schema (Zod and similar) when you don't control the API ([validating responses](../11-api-integration/02-api-client.md#validating-responses)). The schema can also *be* the mapper (`transform`).
- The mapping layer is where **backend changes get absorbed**: rename a field, add a default, adapt a version, in one place.

Don't over-apply this. If the API already returns exactly what the UI needs, a pass-through mapper is noise. Add the mapper when the shapes differ or you want insulation from an API you don't control.

## Where does each kind of logic go?

| Logic | Layer |
|---|---|
| "Orders over $50 ship free" | **Domain**: pure function |
| "Convert `price_cents` to dollars" | **Domain/data**: mapper |
| "Fetch the cart; refetch on window focus" | **Hooks** (query) |
| "After checkout, clear the cart cache and go to the receipt" | **Hooks** (mutation callbacks) / handler |
| "Show a skeleton while loading" | **UI** |
| "Which items are selected" (ephemeral) | **UI** state (`useState`) |
| "Debounce the search input" | **Hooks** (generic, in `shared/hooks`) |
| "What does a 409 mean for the user?" | **Hooks/domain** (error mapping), shown by **UI** ([03](./03-error-handling-architecture.md)) |
| "Format a price for display" | **Shared lib** (`Intl`), called from UI |

If you're unsure, ask: *does this need React?* If not, it probably belongs in domain or data. *Does it need to know about the screen?* If not, it shouldn't be in a component.

## Smart and dumb components

An older vocabulary for the same separation:

- **Container (smart) components** get data and handle logic.
- **Presentational (dumb) components** receive props and render.

Hooks replaced the need for separate container files: the *hook* is the container logic, and the component can be both. But the underlying idea remains useful: **keep a layer of components that only render props**. They're the easiest to test, reuse, and preview in isolation.

## Testing per layer

| Layer | How to test | Speed |
|---|---|---|
| Domain | Plain unit tests: input → output, `test.each` tables | Instant |
| Data access | Against a mocked network ([MSW](../18-testing-and-debugging/04-mocking-and-msw.md)), asserting requests/responses/mapping | Fast |
| Hooks | `renderHook` with providers, or via components ([hook testing](../18-testing-and-debugging/03-hook-testing.md)) | Fast |
| UI | RTL: render with props, interact ([component testing](../18-testing-and-debugging/02-component-testing-with-rtl.md)) | Fast |
| All together | Integration tests through the UI ([integration testing](../18-testing-and-debugging/05-integration-testing.md)) | Moderate |

Separated layers let you test **business rules exhaustively and cheaply** (domain), and only a few paths through the whole stack.

## Don't over-layer

Layers are a tool to manage complexity. Applied everywhere, they create *more* of it:

- **Pass-through layers** (a "service" that just calls the API function that just calls `fetch`) add files and indirection without adding meaning.
- **Interfaces and dependency-injection containers** copied from backend architectures rarely pay off in React. Plain modules and hooks are your seams, and tests substitute at the network (MSW), not through injected interfaces.
- **Four folders for a feature with 30 lines of logic** is ceremony.

A reasonable path:

1. Start with **component + a hook + the API function**.
2. When logic gets non-trivial or is reused, **extract it into a pure `domain` function**.
3. When the backend shape leaks into the UI or changes hurt, **add a mapper**.
4. Add layers *when a specific pain shows up*, not before.

The rule that earns its keep in even the smallest app: **components don't call `fetch`, and business rules aren't written inside JSX or effects.**

## Common mistakes

- **Fetching in components** (or `useEffect`), so UI knows URLs and transport details.
- **Business rules inside JSX or event handlers**, untestable and duplicated across screens.
- **Components consuming raw DTOs**, so backend renames break the UI.
- **A "god hook"** doing fetching, rules, formatting, and UI state for a whole page. Split into focused hooks and pure functions.
- **React in the domain layer** (hooks, JSX, `useMemo` inside rule functions), making rules hard to reuse and test.
- **Upward dependencies** (data layer importing a component's type, domain importing a hook).
- **Pass-through layers** and interface-heavy abstractions with no actual variation.
- **Duplicating logic across layers** (the same validation in the component, hook, *and* mapper).
- **Over-memoizing derived values** in hooks instead of keeping domain functions simple ([memoization](../14-performance/02-memoization.md)).
- **Treating layers as folders only**, without enforcing the dependency direction.
- **Doing it all upfront** instead of letting structure follow real complexity.

## Quick summary

- Layers separate things that change for different reasons: **UI** (render/events), **hooks** (state and orchestration), **domain** (pure rules, types, mappers), **data access** (HTTP/storage).
- **Dependencies point down.** Domain depends on nothing (no React, no network); components never call `fetch`.
- Put business rules in **pure functions** and test them directly; keep components "humble": props in, markup out.
- **Map backend DTOs to domain types once, at the edge**, and validate untrusted data with a schema.
- Hooks are where React-specific glue (queries, mutations, context, effects) lives, exposing a small task-shaped interface.
- Test each layer at the right level; use integration tests for the whole stack.
- **Don't over-layer**: add structure when real complexity appears, and avoid pass-through abstractions.

## Next

[02 — API architecture](./02-api-architecture.md)