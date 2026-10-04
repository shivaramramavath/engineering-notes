# Compound Components

Compound components are a **family of components that work together and share implicit state**: `<select>` and `<option>`, `<table>` and `<tr>`, or a `Tabs` with `Tabs.List`, `Tabs.Trigger`, and `Tabs.Panel`. The parent owns the state; the children read it through **context**, so the caller can arrange, style, and omit parts freely without threading props through every level.

## Prerequisites

[`01-composition-patterns.md`](./01-composition-patterns.md), [`../03-hooks/05-useContext.md`](../03-hooks/05-useContext.md), and [`00-component-api-design.md`](./00-component-api-design.md) (for `useControllableState`)

---

## Why this pattern

Compare two APIs for tabs:

```tsx
// Configuration: structure locked inside the component
<Tabs
  tabs={[
    { id: "account", label: "Account", content: <AccountForm /> },
    { id: "billing", label: "Billing", content: <BillingForm />, disabled: true },
  ]}
/>

// Compound: the caller composes
<Tabs defaultValue="account">
  <TabsList>
    <TabsTrigger value="account">Account</TabsTrigger>
    <TabsTrigger value="billing" disabled>Billing</TabsTrigger>
  </TabsList>
  <TabsPanel value="account"><AccountForm /></TabsPanel>
  <TabsPanel value="billing"><BillingForm /></TabsPanel>
</Tabs>
```

The compound version needs no config object, lets you put anything between parts (a badge, a tooltip, a layout wrapper), and adds new capabilities without new props. This is the model used by Radix UI and shadcn/ui ([`../09-ui-components/00-shadcn-ui.md`](../09-ui-components/00-shadcn-ui.md)).

---

## The recipe

1. **Create a context** holding the shared state.
2. **A root component** owns the state and provides the context.
3. **Part components** consume the context via a hook that throws outside the root.

### 1. Context and hook

```tsx
import { createContext, useContext, useId, type ReactNode, type ComponentProps } from "react";
import { useControllableState } from "./useControllableState";   // see 00-component-api-design.md

type TabsContextValue = {
  value: string;
  setValue: (value: string) => void;
  baseId: string;
};

const TabsContext = createContext<TabsContextValue | null>(null);

function useTabsContext(): TabsContextValue {
  const ctx = useContext(TabsContext);
  if (ctx === null) {
    throw new Error("Tabs components must be rendered inside <Tabs>");
  }
  return ctx;
}
```

The throwing hook gives a clear message when someone renders `<TabsTrigger>` outside `<Tabs>` ([`../04-typescript-with-react/02-typing-hooks.md`](../04-typescript-with-react/02-typing-hooks.md)).

### 2. The root

```tsx
type TabsProps = {
  value?: string;
  defaultValue?: string;
  onValueChange?: (value: string) => void;
  children: ReactNode;
};

export function Tabs({ value, defaultValue = "", onValueChange, children }: TabsProps) {
  const [current, setCurrent] = useControllableState({
    value,
    defaultValue,
    onChange: onValueChange,
  });
  const baseId = useId();

  return (
    <TabsContext value={{ value: current, setValue: setCurrent, baseId }}>
      {children}
    </TabsContext>
  );
}
```

Because it uses `useControllableState`, the component works **uncontrolled** (`defaultValue`) and **controlled** (`value` + `onValueChange`). Use `<TabsContext.Provider>` on React versions before 19.

### 3. The parts

```tsx
export function TabsList(props: ComponentProps<"div">) {
  return <div role="tablist" {...props} />;
}

type TabsTriggerProps = ComponentProps<"button"> & { value: string };

export function TabsTrigger({ value, ...props }: TabsTriggerProps) {
  const ctx = useTabsContext();
  const selected = ctx.value === value;

  return (
    <button
      role="tab"
      type="button"
      id={`${ctx.baseId}-trigger-${value}`}
      aria-selected={selected}
      aria-controls={`${ctx.baseId}-panel-${value}`}
      tabIndex={selected ? 0 : -1}
      onClick={() => ctx.setValue(value)}
      {...props}
    />
  );
}

type TabsPanelProps = ComponentProps<"div"> & { value: string };

export function TabsPanel({ value, ...props }: TabsPanelProps) {
  const ctx = useTabsContext();
  if (ctx.value !== value) return null;

  return (
    <div
      role="tabpanel"
      id={`${ctx.baseId}-panel-${value}`}
      aria-labelledby={`${ctx.baseId}-trigger-${value}`}
      {...props}
    />
  );
}
```

Points to notice:

- Each part forwards native props (`...props`), per [`00-component-api-design.md`](./00-component-api-design.md).
- Ids are generated with **`useId`**, so they're unique per `Tabs` instance and stable between server and client.
- ARIA roles and attributes are baked in, so callers get accessibility by default.
- **Keyboard support is still missing.** The WAI-ARIA tabs pattern expects arrow keys to move between tabs, plus Home and End. Add an `onKeyDown` on `TabsList` (a "roving tabindex") — or, better, use a primitive library that already implements it ([`../08-accessibility/02-keyboard-and-focus-management.md`](../08-accessibility/02-keyboard-and-focus-management.md), [`../09-ui-components/04-tabs.md`](../09-ui-components/04-tabs.md)).

### Usage

```tsx
<Tabs defaultValue="account">
  <TabsList>
    <TabsTrigger value="account">Account</TabsTrigger>
    <TabsTrigger value="billing">Billing</TabsTrigger>
  </TabsList>
  <TabsPanel value="account">…</TabsPanel>
  <TabsPanel value="billing">…</TabsPanel>
</Tabs>
```

---

## Exposing the parts

Two common ways to export a compound family:

### Named exports (recommended)

```tsx
import { Tabs, TabsList, TabsTrigger, TabsPanel } from "@/components/tabs";
```

- Works well with tree-shaking, auto-imports, and server components.
- This is the shadcn/ui and Radix convention.

### Static properties

```tsx
Tabs.List = TabsList;
Tabs.Trigger = TabsTrigger;
Tabs.Panel = TabsPanel;

<Tabs.List> … </Tabs.List>
```

Reads nicely and groups the API, but it can hurt tree-shaking, complicates typing slightly, and has issues in React Server Components (an object property on a client component isn't accessible from a server module). If you use it, also export the parts individually.

---

## Why context and not `cloneElement`

The older way of building these components injected props into children with `Children.map` and `cloneElement`. It breaks as soon as a child is wrapped in a `<div>`, a fragment, or another component, because the injected props go to the wrapper. **Context** works at any depth and requires no knowledge of the children's structure ([`01-composition-patterns.md`](./01-composition-patterns.md)).

---

## Design guidelines

- **Keep the context value small and stable.** Every consumer re-renders when it changes. Memoize the object if the root re-renders frequently ([`../13-state-management/01-context-patterns-and-performance.md`](../13-state-management/01-context-patterns-and-performance.md)).
- **Pass identifiers, not indexes.** `value="billing"` survives reordering and conditional parts; an index doesn't.
- **Each part should be usable inside any wrapper**, not only as a direct child of the root.
- **Let parts compose.** Allow `asChild` on triggers so a router `Link` can be a trigger ([`01-composition-patterns.md`](./01-composition-patterns.md)).
- **Throw clear errors** when parts are used outside the root.
- **Support controlled and uncontrolled usage** from day one.
- **Document the intended structure** and show a full example.

---

## When *not* to use it

- **One-off UI** — a plain component with a few props is simpler.
- **The parts must be rendered in a rigid structure** that the caller never customizes — plain props are less code.
- **Data-driven repeated items** (for example, 100 rows from an array) — render items with `map` inside the parts; don't build a compound API per row.
- **Performance-sensitive trees** with many consumers and frequent state changes — consider splitting contexts or a store.

---

## Beyond tabs

The same recipe builds accordions, menus, selects, dialogs (`Dialog`, `DialogTrigger`, `DialogContent`), form fields (`Field`, `Label`, `Input`, `Error`), and cards. Examples in [`../09-ui-components/`](../09-ui-components/README.md).

---

## Common mistakes

- **Using `cloneElement` to inject props** — breaks with wrappers; use context.
- **No error when a part is used outside the root** — consumers get a silent `null` context and confusing crashes.
- **Passing a new object as the context value every render** — re-renders all parts needlessly.
- **Index-based identification** — breaks when parts are reordered or conditional.
- **Forgetting accessibility** (roles, `aria-*`, keyboard handling) — the compound structure doesn't provide it for you.
- **Static-property exports only** — hurts tree-shaking and server component compatibility.
- **Not forwarding native props** — callers can't add `className`, `data-*`, or handlers.
- **Hardcoding ids** — duplicates when two instances render; use `useId`.

## Quick summary

- A compound component is a family of parts sharing state through context
- Recipe: context → root owns state (controlled or uncontrolled) → parts consume through a throwing hook
- Callers compose and style freely; no config objects or `cloneElement`
- Bake in ARIA roles and use `useId`; add keyboard behavior or use a primitive library
- Prefer named exports over static properties
- Skip it when a plain component would do

## Next

**[`03-legacy-component-patterns.md`](./03-legacy-component-patterns.md)** covers the pre-hooks patterns — render props, HOCs, and container/presentational — that you'll still meet in older code.
