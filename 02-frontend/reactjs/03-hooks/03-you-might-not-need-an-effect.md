# You Might Not Need an Effect

Effects are an **escape hatch** for synchronizing with things outside React. If no external system is involved, an effect is usually unnecessary — and adds extra renders, bugs, and confusion. This file catalogs the most common misuses and the better alternative for each.

## Prerequisites

[`02-useEffect.md`](./02-useEffect.md) and [`../02-state-and-rendering/02-state-structure-and-lifting.md`](../02-state-and-rendering/02-state-structure-and-lifting.md)

---

## The test

Before writing an effect, ask: **"Am I synchronizing with something outside React?"**

- Yes (network, browser API, third-party widget, subscription) → an effect may be right.
- No (only transforming data, responding to a user action, or resetting state) → you probably don't need one.

The typical bad pattern: *state changes → effect runs → sets more state → another render.* That chain wastes work and is hard to follow.

---

## 1. Derive data during render

**Don't** store values you can compute from props or state:

```jsx
// ❌ Redundant state + effect: extra render, and can briefly show stale data
const [firstName, setFirstName] = useState("Ada");
const [lastName, setLastName] = useState("Lovelace");
const [fullName, setFullName] = useState("");

useEffect(() => {
  setFullName(`${firstName} ${lastName}`);
}, [firstName, lastName]);

// ✅ Calculate during render
const fullName = `${firstName} ${lastName}`;
```

The same applies to filtered lists, totals, formatted values, and so on.

---

## 2. Cache expensive calculations with `useMemo`, not an effect

```jsx
// ❌
const [visible, setVisible] = useState([]);
useEffect(() => {
  setVisible(getFilteredTodos(todos, filter));
}, [todos, filter]);

// ✅
const visible = getFilteredTodos(todos, filter);

// ✅ If it's genuinely slow (measure first), memoize:
const visible = useMemo(() => getFilteredTodos(todos, filter), [todos, filter]);
```

See [`07-useMemo-and-useCallback.md`](./07-useMemo-and-useCallback.md).

---

## 3. Reset state with a `key`

```jsx
// ❌ Manually resetting when the prop changes
function Profile({ userId }) {
  const [comment, setComment] = useState("");
  useEffect(() => {
    setComment("");
  }, [userId]);
}

// ✅ Give the component an identity so React resets it automatically
<Profile key={userId} userId={userId} />
```

Because `userId` changes the `key`, React discards the old component and creates a fresh one — including **all** of its state, without a flash of stale content. See [`../02-state-and-rendering/05-state-preservation-and-reset.md`](../02-state-and-rendering/05-state-preservation-and-reset.md).

---

## 4. Adjust state when a prop changes — by restructuring

When only *part* of the state should change as a prop changes, first try to avoid needing it:

```jsx
// ❌ Resetting selection when the list changes
useEffect(() => {
  setSelection(null);
}, [items]);

// ✅ Store the id and derive the selected item
const [selectedId, setSelectedId] = useState(null);
const selection = items.find((i) => i.id === selectedId) ?? null;
```

If the selected item disappears from `items`, `selection` is simply `null` — no effect, no extra render.

Only as a rare last resort, React supports setting state **during rendering** of the *same* component, guarded by a condition, to avoid a flash of stale UI. Prefer the restructuring above.

---

## 5. Put event-caused logic in event handlers

```jsx
// ❌ Effect reacts to state set by a click
useEffect(() => {
  if (product.isInCart) {
    showNotification(`Added ${product.name} to the cart!`);
  }
}, [product]);

// ✅ Do it where it happens
function handleBuyClick() {
  addToCart(product);
  showNotification(`Added ${product.name} to the cart!`);
}
```

Ask: *why* does this code run? If it runs **because the user did something** (clicked, submitted, typed), it belongs in a handler. If it runs **because the component is displayed**, it may belong in an effect (for example, logging a page view or connecting to a chat room).

### Sending a POST request

```jsx
// ❌ Effect for a submit
useEffect(() => {
  if (jsonToSubmit !== null) post("/api/register", jsonToSubmit);
}, [jsonToSubmit]);

// ✅ Handler
function handleSubmit(e) {
  e.preventDefault();
  post("/api/register", { firstName, lastName });
}
```

Analytics for "the form was shown" can be an effect; "the form was submitted" belongs in the handler.

---

## 6. Avoid chains of effects

```jsx
// ❌ Each effect triggers the next render and the next effect
useEffect(() => { if (card?.gold) setGoldCount((c) => c + 1); }, [card]);
useEffect(() => { if (goldCount > 3) { setRound((r) => r + 1); setGoldCount(0); } }, [goldCount]);
useEffect(() => { if (round > 5) setIsGameOver(true); }, [round]);
```

This causes many wasted renders and is fragile. Instead, **calculate what you can during render** (`isGameOver = round > 5`) and **compute all the next state in the event handler** that started it.

---

## 7. Notify a parent through the handler, not an effect

```jsx
// ❌ Child tells the parent after its own state changes
useEffect(() => {
  onChange(isOn);
}, [isOn, onChange]);

// ✅ Update both in the same event
function updateToggle(nextIsOn) {
  setIsOn(nextIsOn);
  onChange(nextIsOn);
}
```

Or lift the state to the parent and make the child controlled ([`../02-state-and-rendering/02-state-structure-and-lifting.md`](../02-state-and-rendering/02-state-structure-and-lifting.md)). Both updates are batched into one render pass.

---

## 8. Don't pass data *up* from child effects

Data should flow from parent to child. If a child fetches data and an effect hands it to the parent, the data flow is hard to trace. Fetch in the parent (or a shared hook or cache) and pass it down.

---

## 9. Subscribe to external stores with the right hook

For data that lives outside React (browser APIs, third-party stores), `useSyncExternalStore` handles subscription and avoids tearing:

```jsx
// ❌ Manual effect + state
const [isOnline, setIsOnline] = useState(true);
useEffect(() => {
  const update = () => setIsOnline(navigator.onLine);
  window.addEventListener("online", update);
  window.addEventListener("offline", update);
  return () => {
    window.removeEventListener("online", update);
    window.removeEventListener("offline", update);
  };
}, []);

// ✅ useSyncExternalStore — see ../16-advanced-react/02-external-stores.md
```

(The effect version is acceptable for simple cases, which is why it appears again in [`10-hook-recipes.md`](./10-hook-recipes.md).)

---

## 10. Fetching data

Fetching in effects is legitimate (it synchronizes with a server), but hand-rolled versions need race-condition handling and miss caching, deduplication, and retries. Prefer a data library or your framework's data loading ([`../12-server-state/03-tanstack-query.md`](../12-server-state/03-tanstack-query.md), [`../10-routing/05-route-data-loading.md`](../10-routing/05-route-data-loading.md)). If you do write an effect, use the cleanup patterns in [`02-useEffect.md`](./02-useEffect.md).

---

## 11. One-time app initialization

Code that must run once per **app load** (not per component mount) shouldn't rely on a `[]` effect, because StrictMode and remounting can run it more than once:

```jsx
// ❌ Runs twice in development; may duplicate work
useEffect(() => { loadFromLocalStorage(); checkAuthToken(); }, []);

// ✅ Run at module level, outside any component
if (typeof window !== "undefined") {
  checkAuthToken();
}
```

If it genuinely belongs inside the component, make it idempotent, or guard it with a module-level flag.

---

## Quick decision guide

| Situation | Use |
|-----------|-----|
| Value computable from props/state | Calculate during render |
| Expensive calculation | `useMemo` (after measuring) |
| Reset all state when an id changes | `key` |
| Adjust part of state on prop change | Store ids/derive; restructure |
| User action triggers work | Event handler |
| Notify parent of changes | Call parent callback in the handler, or lift state |
| External store subscription | `useSyncExternalStore` |
| Data fetching | Data library, or effect with cleanup |
| Sync with network/DOM/third-party system | `useEffect` ✅ |

---

## Common mistakes

- **State + effect that mirrors other state** — compute it during render.
- **Effects that respond to user actions** — move the logic into the handler.
- **Chains of effects** that set state in sequence — consolidate in one handler.
- **Resetting state in an effect** — use `key`.
- **Notifying parents from an effect** — call the callback in the same handler, or lift state.
- **Fetching without handling races** — use a library or cleanup.
- **Running app-wide init inside `useEffect(…, [])`** — it can run more than once.

## Quick summary

- An effect is for synchronizing with **external** systems only
- Derive values during render; memoize if expensive
- Use `key` to reset state; store ids instead of objects to avoid adjusting state
- Put logic caused by user actions in event handlers
- Avoid effect chains and effects that notify parents
- Use `useSyncExternalStore` for external stores and a data library for fetching

## Next

**[`04-useRef.md`](./04-useRef.md)** covers storing values that don't trigger renders and accessing DOM nodes.
