# Generics

A generic lets a component or hook work with **many types while keeping each use fully type-safe**. Instead of `any`, you write a **type parameter** (`T`) that the caller's data fills in. In React, generics power reusable lists, selects, tables, and data hooks.

## Prerequisites

[`02-typing-hooks.md`](./02-typing-hooks.md). Familiarity with TypeScript generic functions (`function identity<T>(x: T): T`) helps.

---

## The problem generics solve

A list component that renders any array:

```tsx
// Too loose: loses all type information
type ListProps = {
  items: any[];
  renderItem: (item: any) => ReactNode;
};
```

`renderItem` receives `any`, so typos like `item.nmae` go unnoticed. With a type parameter, the item type flows from `items` to `renderItem`:

```tsx
type ListProps<T> = {
  items: T[];
  renderItem: (item: T) => ReactNode;
  getKey: (item: T) => string | number;
};

function List<T>({ items, renderItem, getKey }: ListProps<T>) {
  return (
    <ul>
      {items.map((item) => (
        <li key={getKey(item)}>{renderItem(item)}</li>
      ))}
    </ul>
  );
}
```

Usage — **`T` is inferred** from `items`, with no annotation needed:

```tsx
type User = { id: string; name: string };
const users: User[] = [/* ... */];

<List
  items={users}
  getKey={(u) => u.id}
  renderItem={(u) => <span>{u.name}</span>}   // u: User
/>

<List items={users} renderItem={(u) => u.nmae} getKey={(u) => u.id} />
//                                       ^^^^ ❌ Property 'nmae' does not exist on type 'User'
```

---

## Generic function components in TSX

Write them as **function declarations** — simplest, no syntax surprises:

```tsx
function Select<T>(props: SelectProps<T>) { /* ... */ }
```

If you prefer arrow functions, `.tsx` files confuse `<T>` with a JSX tag. Add a trailing comma or an `extends` clause:

```tsx
const Select = <T,>(props: SelectProps<T>) => { /* ... */ };
const Select = <T extends unknown>(props: SelectProps<T>) => { /* ... */ };
```

Also avoid `React.FC` for generic components; it can't be generic ([`00-typing-components-and-props.md`](./00-typing-components-and-props.md)).

---

## Constraints with `extends`

Unconstrained `T` could be anything, so you can't access properties on it. Constrain it to the minimum shape you need:

```tsx
type SelectProps<T extends { id: string }> = {
  options: T[];
  value: T["id"] | null;
  onChange: (option: T) => void;
  getLabel: (option: T) => string;
};

function Select<T extends { id: string }>({
  options,
  value,
  onChange,
  getLabel,
}: SelectProps<T>) {
  return (
    <select
      value={value ?? ""}
      onChange={(e) => {
        const selected = options.find((o) => o.id === e.target.value);
        if (selected) onChange(selected);
      }}
    >
      {options.map((o) => (
        <option key={o.id} value={o.id}>
          {getLabel(o)}
        </option>
      ))}
    </select>
  );
}
```

Any type with an `id: string` works (`User`, `Product`, …), and everything else about it stays known. A component that requires a specific field is **explicit about its contract**.

---

## `keyof` for column and field names

Generics and `keyof` let a component refer to **properties of the data**, with checking:

```tsx
type Column<T> = {
  key: keyof T;
  header: string;
};

type TableProps<T> = {
  rows: T[];
  columns: Column<T>[];
};

function Table<T extends { id: string }>({ rows, columns }: TableProps<T>) {
  return (
    <table>
      <thead>
        <tr>
          {columns.map((c) => <th key={String(c.key)}>{c.header}</th>)}
        </tr>
      </thead>
      <tbody>
        {rows.map((row) => (
          <tr key={row.id}>
            {columns.map((c) => (
              <td key={String(c.key)}>{String(row[c.key])}</td>
            ))}
          </tr>
        ))}
      </tbody>
    </table>
  );
}

<Table
  rows={users}
  columns={[
    { key: "name", header: "Name" },
    { key: "email", header: "Email" },
    { key: "salary", header: "Salary" },   // ❌ "salary" is not a key of User
  ]}
/>
```

Real table libraries do far more ([`../09-ui-components/07-data-tables.md`](../09-ui-components/07-data-tables.md)), but this shows the technique.

---

## Generic hooks

Hooks that store or return arbitrary data take a type parameter. The caller can supply it, or it can be inferred from an argument:

```tsx
import { useState, useEffect } from "react";

function useLocalStorage<T>(key: string, initialValue: T) {
  const [value, setValue] = useState<T>(() => {
    try {
      const stored = localStorage.getItem(key);
      return stored !== null ? (JSON.parse(stored) as T) : initialValue;
    } catch {
      return initialValue;
    }
  });

  useEffect(() => {
    localStorage.setItem(key, JSON.stringify(value));
  }, [key, value]);

  return [value, setValue] as const;
}

const [theme, setTheme] = useLocalStorage("theme", "light");          // T inferred: string
const [mode, setMode] = useLocalStorage<"light" | "dark">("mode", "light");
```

**Caution:** `JSON.parse(...) as T` is a *promise* to TypeScript, not a check — stored data may not match `T` at runtime (old formats, manual edits). For data from outside your code (storage, APIs), validate with a schema library ([`05-advanced-typing-patterns.md`](./05-advanced-typing-patterns.md)). More hooks to type: [`../03-hooks/10-hook-recipes.md`](../03-hooks/10-hook-recipes.md).

### A fetching hook

```tsx
type FetchState<T> =
  | { status: "loading" }
  | { status: "success"; data: T }
  | { status: "error"; error: Error };

function useFetch<T>(url: string): FetchState<T> { /* ... */ }

const state = useFetch<User[]>("/api/users");
if (state.status === "success") {
  state.data;   // User[]
}
```

Here `T` can't be inferred from the argument, so the caller states it. In real apps, prefer a data library that provides this typing ([`../12-server-state/03-tanstack-query.md`](../12-server-state/03-tanstack-query.md)).

---

## Default type parameters and multiple parameters

```tsx
type ListProps<T, K extends string | number = string> = {
  items: T[];
  getKey: (item: T) => K;
};
```

- `= string` makes `K` optional for callers.
- Use multiple parameters when types are genuinely independent (input and output, key and value).
- Don't add parameters "just in case" — each one makes the API harder to read.

---

## Generic context

Contexts are created at module level, so they can't depend on a type parameter directly. To produce a typed context for any shape, wrap creation in a **factory function**:

```tsx
function createStrictContext<T>(name: string) {
  const Ctx = createContext<T | null>(null);
  function useStrictContext(): T {
    const value = useContext(Ctx);
    if (value === null) throw new Error(`${name} used outside its provider`);
    return value;
  }
  return [Ctx, useStrictContext] as const;
}

const [ThemeContext, useTheme] = createStrictContext<ThemeContextValue>("Theme");
```

Build on the context pattern in [`02-typing-hooks.md`](./02-typing-hooks.md).

---

## Limitations: `memo` and `forwardRef`

Wrapping a generic component in `React.memo` (or `forwardRef` in older React) **erases its generics**, because those functions' types aren't generic over your component:

```tsx
const MemoList = memo(List);   // T collapses to unknown
```

Workaround — cast back to the original type:

```tsx
const MemoList = memo(List) as typeof List;
```

Do this only when you've profiled and need `memo` ([`../14-performance/02-memoization.md`](../14-performance/02-memoization.md)). In React 19, `ref` is a normal prop, which removes the `forwardRef` case.

---

## When *not* to use generics

- **A single concrete type** — just use it. `function UserList(props: { users: User[] })` is clearer than a generic.
- **Complex type-level logic** that's hard to read — simplify the API or use discriminated unions.
- **To avoid writing types** — a generic `<T>` with no constraint and no relationship between parameters is usually just `unknown` in disguise.

Rule of thumb: introduce a type parameter when **two or more positions must share the same type** (items and their render function, options and the selected value).

---

## Common mistakes

- **Using `any` instead of a generic** — loses the link between inputs and outputs.
- **Arrow-function generics in `.tsx` without `<T,>`** — parsed as a JSX tag; use a declaration or the trailing comma.
- **No constraint, then accessing properties** — add `extends { id: string }`.
- **Casting `JSON.parse` / API data as `T`** — it's not validated; parse with a schema.
- **Using `React.FC` or unadjusted `memo` with generic components** — generics are lost.
- **Over-generalizing** — a generic component with five type parameters is harder to use than two focused components.
- **Forgetting that the caller may need to specify `T`** when it can't be inferred (`useFetch<User[]>`).

## Quick summary

- Generics link input and output types without `any`: `function List<T>(props: ListProps<T>)`
- `T` is usually inferred from props; constrain it with `extends` when you need properties
- Use `keyof T` to refer to the data's fields safely
- Write generic components as function declarations (or `<T,>` for arrows)
- Generic hooks need a type argument when it can't be inferred; validate external data
- `memo` and `forwardRef` erase generics; cast with `as typeof Component`
- Add a type parameter only when several positions must agree on one type

## Next

**[`04-utility-types.md`](./04-utility-types.md)** shows how to derive new types from existing ones instead of rewriting them.
