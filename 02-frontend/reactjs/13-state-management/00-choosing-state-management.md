# Choosing State Management

"Which state management library should I use?" is usually the wrong first question. The right first question is **"what kind of state is this?"** Most of the pain people associate with state management came from putting every kind of state in one global store.

## Step 1: classify the state

| Kind | Example | Right tool |
|---|---|---|
| **Server state** | Projects, user profile, comments | TanStack Query ([12](../12-server-state/README.md)) |
| **URL state** | Filters, page, sort, selected tab, open record | Search params / route ([routing](../10-routing/06-search-filter-and-url-state.md)) |
| **Form state** | Values, errors, dirty/touched | React Hook Form / `useState` ([forms](../06-forms/README.md)) |
| **Local UI state** | Is this dropdown open? Hover? Input text? | `useState` in that component |
| **Shared client state** | Theme, sidebar, cart, wizard progress, selection, modals | **This folder** |

Do this classification first, and the "global state" that remains is often a handful of values. Many apps end up with a tiny store (or just a couple of contexts), where they once had a giant Redux tree mirroring their API.

## Step 2: climb the ladder only when needed

### 1. `useState` in one component

Default. If only one component cares, stop here.

### 2. Lift state up

When two nearby components need the same value, move the state to their closest common parent and pass it down ([state structure and lifting](../02-state-and-rendering/02-state-structure-and-lifting.md)). Cheap, explicit, easy to trace. Passing props through 2–3 levels is **fine**; prop drilling only becomes a problem when it's deep and noisy.

### 3. Composition before context

Often the real problem is the component tree shape, not state location. Passing **components as `children`/props** can eliminate drilling without any global tool:

```tsx
// Instead of drilling `user` through Layout → Sidebar → Avatar:
<Layout sidebar={<Sidebar avatar={<Avatar user={user} />} />} />
```

See [children and composition](../01-fundamentals/03-children-and-composition.md).

### 4. Context

For values many components read, that **change rarely**: theme, locale, current user, feature flags. Context solves *delivery* (no drilling), not *management* ([01](./01-context-patterns-and-performance.md)).

### 5. `useReducer` + context

When one piece of shared state has **multiple actions and non-trivial transitions** (cart, wizard, undo stack), and you want the logic centralized and testable ([02](./02-reducer-and-context-pattern.md)).

### 6. A store library

When shared state is **updated often, read selectively by many components**, or you need persistence, devtools, middleware, or access **outside React** (API client, event handlers elsewhere). That's Zustand ([03](./03-zustand.md)) or Redux Toolkit ([04](./04-redux-toolkit.md)).

## Comparing the options

| | Context | `useReducer` + Context | **Zustand** | **Redux Toolkit** |
|---|---|---|---|---|
| Setup | None | Little | Tiny | Moderate |
| Boilerplate | Low | Medium | **Very low** | Medium (reduced by RTK) |
| Selective re-renders | **No**: all consumers re-render | No | **Yes** (selectors) | **Yes** (selectors) |
| Use outside React | No | No | **Yes** (`getState`) | Yes (`store.getState`, dispatch) |
| Devtools / time travel | React DevTools only | React DevTools only | Via middleware (Redux DevTools) | **Built in** |
| Middleware / side-effect tools | No | No | Middleware (`persist`, `devtools`, `immer`) | **Rich** (thunks, listeners) |
| Enforced structure | None | Reducer | Light (you choose) | **Strong** (slices, actions) |
| Learning curve | Low | Low–medium | **Low** | Medium |
| Best for | Theme, auth, locale | Local complex state trees, wizards | Most shared client state | Large apps/teams, heavy cross-cutting logic |

Other tools exist (Jotai and Recoil-style atomic state, MobX, XState for explicit state machines). The pattern is the same: pick based on the access shape (a few atoms vs. one store vs. an event log), not fashion.

## A decision flow

```text
Is it needed by only one component (or a parent and child)?
   └─ yes → useState / lift up
Does it change rarely and get read widely (theme, user, locale)?
   └─ yes → Context
Is it one cohesive state with several actions/transitions, scoped to a feature?
   └─ yes → useReducer + Context
Is it updated frequently, read selectively, or needed outside React?
   └─ yes → Zustand
Do you need strict structure, rich middleware, time-travel debugging,
or are you in a large team / existing Redux codebase?
   └─ yes → Redux Toolkit
```

## What goes where in a typical app

| Concern | Where |
|---|---|
| Logged-in user object and `status` | Context (changes rarely), or the query cache |
| Auth token | Module variable ([API client](../11-api-integration/02-api-client.md#the-token-store)) |
| Theme / dark mode | Context (or a small store) |
| Sidebar open/collapsed (persisted) | Zustand with `persist`, or context + `localStorage` |
| Which rows are selected in a table | Local state; URL if shareable |
| Cart contents | Reducer + context, or Zustand (persisted) |
| Multi-step wizard answers | Reducer in the wizard's provider, or form library |
| Command palette open | Local state / small store (opened from anywhere) |
| Toasts | A library that owns it ([Sonner](../09-ui-components/08-toasts-and-notifications.md)) |
| Projects, users, orders | **Query cache** |

## Criteria that should actually drive the choice

- **How many components read it, and how selectively?** A few readers: context is fine. Many readers each using a slice: you want selectors.
- **How often does it change?** Rarely: context. Many times per second (drag position, live cursors): not context.
- **How complex are the transitions?** Simple set/toggle: `useState`/store. Many named transitions with rules: reducer-style (reducer, Redux slice).
- **Does non-React code need it?** An interceptor reading a token, a socket handler updating a cart: you need a store you can reach without hooks.
- **Do you need persistence, undo, or devtools?** Middleware or Redux ecosystems earn their keep.
- **Team and codebase.** Consistency beats theoretical optimality. A team that already knows Redux Toolkit shouldn't switch for a todo list, and a small team shouldn't adopt Redux for a theme toggle.

## Anti-patterns

- **Server data in a global store.** You've built a cache with no invalidation. Use a [query cache](../12-server-state/00-server-vs-client-state.md#the-cardinal-rule-dont-copy-server-state).
- **Everything global "just in case."** Global state is harder to reason about, test, and reset. Keep state **as local as it can be**.
- **Mirroring URL state** in a store and syncing both ways.
- **One mega-context** holding unrelated values, which re-renders everything for anything.
- **Derived values stored in state** (`total`, `filteredItems`). Compute them ([derive, don't store](../12-server-state/00-server-vs-client-state.md#derive-dont-store)).
- **Premature library adoption.** Adding Redux for three booleans.
- **Late migration paralysis.** Moving from context to Zustand later is easy if components access state through custom hooks (`useCart()`), so keep that seam.

## Keep a seam

Whatever you choose, hide it behind **domain hooks**:

```tsx
export function useCart() {
  const items = useCartStore((s) => s.items)
  const add = useCartStore((s) => s.add)
  return { items, add }
}
```

Components call `useCart()`. Swapping context for Zustand, or Zustand for Redux, becomes a change in one file.

## Common mistakes

- **Choosing the library before classifying the state.**
- **Using a state library as a data-fetching layer.**
- **Using context for fast-changing values**, causing app-wide re-renders ([01](./01-context-patterns-and-performance.md)).
- **Avoiding prop passing at all costs** and globalizing things that two components share.
- **Reaching for Redux by default** in a new, small app (or avoiding it on principle in a large one that would benefit).
- **Letting components import the store directly everywhere**, with no domain-hook seam.
- **Persisting everything** to `localStorage`, including sensitive or server-owned data.

## Quick summary

- Classify first: server, URL, form, local UI, **shared client** state. Only the last needs this folder.
- Climb the ladder: `useState` → lift → composition → context → reducer + context → Zustand / Redux Toolkit.
- Context delivers values; it doesn't give selective updates. Stores do.
- Pick on update frequency, selectivity, complexity, non-React access, and tooling needs, not hype.
- Keep state as local as possible, never mirror server data, and hide the implementation behind domain hooks.

## Next

[01 — Context patterns and performance](./01-context-patterns-and-performance.md)