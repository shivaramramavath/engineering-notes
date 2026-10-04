# Utility Types

Utility types build **new types from existing ones**, so you describe your data once and derive everything else. That keeps types in sync automatically: change `User` and every derived type follows. This file covers TypeScript's built-in utilities most useful in React code, plus React's own helper types.

## Prerequisites

[`03-generics.md`](./03-generics.md) — utility types are generic types you apply to your own.

---

## The principle: derive, don't duplicate

```tsx
type User = { id: string; name: string; email: string; role: "user" | "admin" };

// ❌ Copies that drift out of sync
type UserPreview = { id: string; name: string };
type NewUser = { name: string; email: string; role: "user" | "admin" };

// ✅ Derived from the single source of truth
type UserPreview = Pick<User, "id" | "name">;
type NewUser = Omit<User, "id">;
```

---

## Object-shaping utilities

| Utility | Result | Typical React use |
|---------|--------|-------------------|
| `Partial<T>` | All properties optional | Update payloads, form state before completion |
| `Required<T>` | All properties required | Config after defaults are applied |
| `Readonly<T>` | All properties `readonly` | Immutable props and state shapes |
| `Pick<T, K>` | Only keys `K` | Components that need a few fields |
| `Omit<T, K>` | Everything except `K` | "Create" payloads without the `id`; overriding native props |
| `Record<K, V>` | An object with keys `K` and values `V` | Lookup maps, id → entity |

```tsx
// Update payload: any subset of fields
function updateUser(id: string, changes: Partial<Omit<User, "id">>) { /* ... */ }

// Keyed lookup
type UsersById = Record<string, User>;

// Exhaustive map: TypeScript errors if you miss a role
const roleLabels: Record<User["role"], string> = {
  user: "Member",
  admin: "Administrator",
};
```

Props that need only part of a model:

```tsx
function UserAvatar({ user }: { user: Pick<User, "name" | "id"> }) { /* ... */ }
```

This makes the component easier to test and reuse: it only demands the fields it uses.

---

## Union-shaping utilities

| Utility | Result |
|---------|--------|
| `Exclude<T, U>` | Removes members of `T` assignable to `U` |
| `Extract<T, U>` | Keeps only members of `T` assignable to `U` |
| `NonNullable<T>` | Removes `null` and `undefined` |

```tsx
type Status = "idle" | "loading" | "success" | "error";
type Settled = Exclude<Status, "idle" | "loading">;     // "success" | "error"
type MaybeUser = User | null | undefined;
type DefiniteUser = NonNullable<MaybeUser>;             // User
```

---

## Function-derived types

| Utility | Result |
|---------|--------|
| `ReturnType<typeof fn>` | The function's return type |
| `Parameters<typeof fn>` | A tuple of its parameter types |
| `Awaited<T>` | The resolved type of a promise |

```tsx
async function fetchUsers() {
  const res = await fetch("/api/users");
  return (await res.json()) as User[];
}

type FetchResult = Awaited<ReturnType<typeof fetchUsers>>;   // User[]
```

Handy when a function is the source of truth (for example, a selector or API function) and you want its result type without restating it.

---

## React-specific types

React ships helper types for describing components and elements. Import them as types:

```tsx
import type {
  ComponentProps,
  ComponentPropsWithoutRef,
  ReactNode,
  ReactElement,
  ElementType,
  CSSProperties,
  PropsWithChildren,
} from "react";
```

| Type | Meaning |
|------|---------|
| `ReactNode` | Anything renderable (the type for `children`) |
| `ReactElement` | A single JSX element |
| `ElementType` | A tag name or component (`"div"`, `Link`) — used for `as` props |
| `ComponentProps<T>` | All props of a tag or component, including `ref` |
| `ComponentPropsWithoutRef<T>` | Same, without `ref` |
| `CSSProperties` | An inline `style` object |
| `PropsWithChildren<P>` | `P & { children?: ReactNode }` |

### Wrapping native elements

```tsx
type ButtonProps = ComponentProps<"button"> & { variant?: "primary" | "ghost" };
```

### Reusing another component's props

```tsx
import { Dialog } from "./Dialog";

type ConfirmDialogProps = ComponentProps<typeof Dialog> & {
  onConfirm: () => void;
};
```

You don't need to export the original `DialogProps`: `ComponentProps<typeof Dialog>` extracts them.

### Overriding native props: `Omit` first

If your prop has the same name as a native prop but a different type, **omit the native one**, otherwise the intersection yields an impossible type:

```tsx
// ❌ native size is number | undefined; yours is "sm" | "lg" → intersection collapses to never
type InputProps = ComponentProps<"input"> & { size?: "sm" | "lg" };

// ✅
type InputProps = Omit<ComponentProps<"input">, "size"> & {
  size?: "sm" | "lg";
};
```

Related patterns: [`00-typing-components-and-props.md`](./00-typing-components-and-props.md).

---

## Deriving types from values

Sometimes a **value** is the source of truth — a constants object or a config — and you derive the type from it.

### `typeof` and `as const`

```tsx
const VARIANTS = {
  primary: "bg-blue-600 text-white",
  secondary: "bg-gray-200 text-gray-900",
  danger: "bg-red-600 text-white",
} as const;

type Variant = keyof typeof VARIANTS;     // "primary" | "secondary" | "danger"

function Button({ variant }: { variant: Variant }) {
  return <button className={VARIANTS[variant]} />;
}
```

Add a variant to the object and the type, autocomplete, and every usage update. `as const` keeps the literal values instead of widening them to `string`. This is how variant libraries work ([`../09-ui-components/01-component-variants.md`](../09-ui-components/01-component-variants.md)).

### Arrays to unions

```tsx
const ROLES = ["user", "editor", "admin"] as const;
type Role = (typeof ROLES)[number];       // "user" | "editor" | "admin"
```

You get a runtime list (for rendering options, validation) **and** a type from one declaration.

### `satisfies`

`satisfies` checks that a value matches a type **without widening it**:

```tsx
const routes = {
  home: "/",
  profile: "/profile",
} satisfies Record<string, string>;

routes.home;   // type is the literal "/" (not just string)
```

Use it for config objects where you want validation plus precise inferred types.

---

## Deriving types from schemas

For data that crosses a boundary (forms, API responses), define a **runtime schema** and derive the type from it, so validation and typing can't disagree:

```tsx
import { z } from "zod";

const userSchema = z.object({
  name: z.string().min(1),
  email: z.string().email(),
});

type UserInput = z.infer<typeof userSchema>;   // { name: string; email: string }
```

Used with forms in [`../06-forms/01-form-validation.md`](../06-forms/01-form-validation.md) and for API typing in [`05-advanced-typing-patterns.md`](./05-advanced-typing-patterns.md).

---

## Mapped types and `keyof`

You can build your own utilities when the built-ins don't fit:

```tsx
// Make specific keys optional
type WithOptional<T, K extends keyof T> = Omit<T, K> & Partial<Pick<T, K>>;
type UserDraft = WithOptional<User, "id" | "role">;

// All fields as a form's string values
type FormValues<T> = { [K in keyof T]: string };
```

Keep these small and named for what they mean. If a type takes more than a few seconds to read, simplify it.

---

## Common mistakes

- **Copy-pasting types** instead of deriving them — they drift apart.
- **`ComponentProps<"input"> & { size: … }` without `Omit`** — creates `never` for conflicting props.
- **Forgetting `as const`** — values widen to `string`, so unions can't be derived.
- **Using `Partial` for everything** — makes all fields optional, hiding required data; prefer `Pick` or a precise type.
- **Using `Omit` on union types** — it collapses the union; use distributive helpers or restructure.
- **Overusing clever mapped types** — unreadable types cost more than they save.
- **Declaring a type and a schema separately** — derive the type with `z.infer`.

## Quick summary

- Derive types: `Pick`, `Omit`, `Partial`, `Record`, `Exclude`, `NonNullable`, `ReturnType`, `Awaited`
- `ComponentProps<"tag">` or `ComponentProps<typeof Component>` reuses existing props; `Omit` before overriding
- `typeof` + `as const` + `keyof` turn constant objects and arrays into unions
- `satisfies` validates a value while keeping its precise type
- Derive types from runtime schemas so validation and types never disagree
- Keep custom mapped types small and readable

## Next

**[`05-advanced-typing-patterns.md`](./05-advanced-typing-patterns.md)** covers discriminated unions, polymorphic components, and other patterns that make invalid states impossible.
