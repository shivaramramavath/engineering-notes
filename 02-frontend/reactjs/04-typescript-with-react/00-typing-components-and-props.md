# Typing Components and Props

A component's props are its public API, so they're the most important thing to type. Once props are typed, TypeScript checks every usage of the component and powers autocomplete in your editor. This file covers props, `children`, defaults, function props, and extending native HTML props.

## Prerequisites

[`../01-fundamentals/02-components-and-props.md`](../01-fundamentals/02-components-and-props.md) and basic TypeScript (object types, unions, optional properties).

---

## Typing props

Describe the props with a `type` or `interface`, then annotate the destructured parameter:

```tsx
type GreetingProps = {
  name: string;
  age?: number;          // optional
  isAdmin: boolean;
};

function Greeting({ name, age, isAdmin }: GreetingProps) {
  return (
    <p>
      Hello, {name}
      {age !== undefined && ` (${age})`}
      {isAdmin && " – admin"}
    </p>
  );
}

<Greeting name="Ada" isAdmin />        // ✅
<Greeting name="Ada" />                // ❌ error: isAdmin is missing
<Greeting name={42} isAdmin />         // ❌ error: number is not a string
```

Inline types work for tiny components, but a named `type` is easier to read, export, and reuse.

### `type` vs `interface`

Both work for props. Pick one convention and stick with it:

| | `type` | `interface` |
|---|--------|-------------|
| Object shapes | ✅ | ✅ |
| Unions, intersections, mapped types | ✅ | ❌ (not directly) |
| Declaration merging | ❌ | ✅ |
| Extending | `&` intersection | `extends` |

Since props often need unions (see [`05-advanced-typing-patterns.md`](./05-advanced-typing-patterns.md)), many teams use `type` for props everywhere.

---

## Do you need `React.FC`?

You may see `React.FC<Props>`. It's **not required**, and most modern codebases write plain functions:

```tsx
// Recommended
function Card({ title }: CardProps) { /* ... */ }

// Also fine
const Card = ({ title }: CardProps) => { /* ... */ };

// Older style
const Card: React.FC<CardProps> = ({ title }) => { /* ... */ };
```

Why plain functions are preferred: `React.FC` no longer adds implicit `children` (older versions did, which caused confusion), makes generic components awkward, and adds nothing that props annotation doesn't already give you. Don't annotate the **return type** either — TypeScript infers it.

---

## Children

Use `React.ReactNode` for anything renderable (strings, numbers, elements, arrays, `null`, `undefined`, booleans):

```tsx
import type { ReactNode } from "react";

type CardProps = {
  title: string;
  children: ReactNode;
};

function Card({ title, children }: CardProps) {
  return (
    <section>
      <h2>{title}</h2>
      {children}
    </section>
  );
}
```

| Type | Accepts |
|------|---------|
| `ReactNode` | Anything React can render — **the default choice** for `children` |
| `ReactElement` | Only a JSX element (not strings, numbers, or arrays) |
| `string` | Only text children |
| `(value: T) => ReactNode` | A render-prop function ([`../05-component-design/03-legacy-component-patterns.md`](../05-component-design/03-legacy-component-patterns.md)) |

`PropsWithChildren<P>` adds `children?: ReactNode` to `P`, but writing `children: ReactNode` explicitly makes the contract clearer, and lets you make it required.

---

## Optional props and defaults

Mark optional props with `?` and give defaults in the destructuring:

```tsx
type ButtonProps = {
  label: string;
  size?: "sm" | "md" | "lg";
  disabled?: boolean;
};

function Button({ label, size = "md", disabled = false }: ButtonProps) {
  return <button className={`btn-${size}`} disabled={disabled}>{label}</button>;
}
```

Inside the component, `size` is typed `"sm" | "md" | "lg"` (not `| undefined`), because the default fills the gap.

### Literal unions beat plain strings

```tsx
size?: string;                       // accepts "banana"
size?: "sm" | "md" | "lg";           // only valid values; autocompletes in the editor
```

Prefer string-literal unions over `enum`: they're simpler, tree-shake well, and show up clearly in editor hints.

---

## Function props

Type callbacks with the arguments they receive and the result you ignore:

```tsx
type SearchBarProps = {
  query: string;
  onQueryChange: (query: string) => void;   // child tells parent the new text
  onSubmit?: () => void;                    // optional callback
};
```

Conventions from the fundamentals chapter still apply: props named `onSomething`, handlers named `handleSomething`. Prefer passing **values** (`(value: string) => void`) over raw events when the parent doesn't need the event. Event-typed handlers are in [`01-typing-events.md`](./01-typing-events.md).

Use `() => void` for "I don't care what you return"; a function returning a value is still assignable.

---

## Extending native element props

A reusable `Button` or `Input` should accept everything a native `<button>` accepts (`onClick`, `type`, `aria-*`, `className`, …) without you listing each one. Use `React.ComponentProps` (or `ComponentPropsWithoutRef` — see below):

```tsx
import type { ComponentProps } from "react";

type ButtonProps = ComponentProps<"button"> & {
  variant?: "primary" | "secondary";
};

function Button({ variant = "primary", className, ...rest }: ButtonProps) {
  return <button className={`btn btn-${variant} ${className ?? ""}`} {...rest} />;
}

<Button variant="secondary" onClick={() => {}} disabled aria-label="Save" />
```

- `ComponentProps<"button">` includes all native props **and the `ref`** prop.
- `ComponentPropsWithoutRef<"button">` omits `ref`; use it when you won't forward a ref.
- **Overriding** a native prop requires omitting it first (`Omit<ComponentProps<"input">, "size"> & { size?: "sm" | "lg" }`) — otherwise the intersection can produce impossible types. More in [`04-utility-types.md`](./04-utility-types.md).
- The same works for other components: `ComponentProps<typeof MyComponent>`.

---

## Refs as props (React 19)

In React 19, `ref` is a regular prop on function components, and `ComponentProps<"input">` already includes it:

```tsx
function TextInput(props: ComponentProps<"input">) {
  return <input {...props} />;   // ref flows through automatically
}

const inputRef = useRef<HTMLInputElement>(null);
<TextInput ref={inputRef} />
```

With older versions of React types, use `forwardRef<HTMLInputElement, Props>`. See [`../16-advanced-react/01-refs-and-imperative-handles.md`](../16-advanced-react/01-refs-and-imperative-handles.md).

---

## Style props and other React types

```tsx
import type { CSSProperties, ReactNode, ReactElement } from "react";

type BoxProps = {
  style?: CSSProperties;       // inline style object
  icon?: ReactElement;         // a single JSX element
  footer?: ReactNode;          // any renderable content
};
```

---

## Props that are objects or arrays

Describe the shape once and reuse it:

```tsx
type User = {
  id: string;
  name: string;
  email: string;
};

type UserListProps = {
  users: User[];
  onSelect: (user: User) => void;
};
```

Keep domain types (like `User`) in a shared module so components, API code, and tests use the same definition — see [`04-utility-types.md`](./04-utility-types.md) for deriving variants of them.

---

## Making invalid combinations impossible

Optional flags that interact create states that shouldn't exist (`isLoading` and `error` both set). Model them as **unions** instead; that's covered in [`05-advanced-typing-patterns.md`](./05-advanced-typing-patterns.md).

---

## Common mistakes

- **Typing props as `any` or leaving them implicit** — loses the main benefit of TypeScript.
- **Using `React.FC` out of habit** — unnecessary; plain functions are simpler and work with generics.
- **Typing `children` as `JSX.Element`** — rejects strings, numbers, and arrays; use `ReactNode`.
- **Using `string` where a literal union fits** — misses autocomplete and typo protection.
- **Redeclaring native props manually** — use `ComponentProps<"button">`.
- **Intersecting without `Omit` when overriding a native prop** — creates conflicting types.
- **Annotating the return type as `JSX.Element`** — unnecessary, and wrong when you return `null`.
- **Silencing errors with `as` casts or `!`** — fix the type instead.

## Quick summary

- Type props with a named `type`; annotate the destructured parameter
- Skip `React.FC`; let the return type be inferred
- Use `ReactNode` for `children` and `?` plus defaults for optional props
- Prefer string-literal unions to plain strings or enums
- Extend native element props with `ComponentProps<"button">`; `Omit` before overriding
- In React 19, `ref` is an ordinary prop

## Next

**[`01-typing-events.md`](./01-typing-events.md)** covers typing event objects and handlers.
