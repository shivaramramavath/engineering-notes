# Advanced Typing Patterns

Patterns that use TypeScript's type system to make **invalid states unrepresentable**: props that can't be combined wrongly, exhaustive handling of every case, polymorphic components, and runtime-validated data. These build on everything earlier in this chapter.

## Prerequisites

[`02-typing-hooks.md`](./02-typing-hooks.md), [`03-generics.md`](./03-generics.md), and [`04-utility-types.md`](./04-utility-types.md)

---

## 1. Discriminated unions for props

When props only make sense in certain combinations, a union of object types — tagged by a shared **discriminant** property — expresses that precisely.

### The problem: flags that can contradict

```tsx
// ❌ Which combinations are valid? Can there be data AND an error?
type Props = {
  isLoading?: boolean;
  error?: Error;
  data?: User[];
};
```

### The solution

```tsx
type Props =
  | { status: "loading" }
  | { status: "error"; error: Error }
  | { status: "success"; data: User[] };

function UserList(props: Props) {
  switch (props.status) {
    case "loading":
      return <Spinner />;
    case "error":
      return <p>{props.error.message}</p>;      // error exists here
    case "success":
      return <ul>{props.data.map(/* ... */)}</ul>;  // data exists here
  }
}

<UserList status="success" data={users} />        // ✅
<UserList status="success" />                     // ❌ data is required
<UserList status="loading" data={users} />        // ❌ data not allowed
```

Checking `props.status` **narrows** the type, so each branch sees only valid fields. The same technique works for reducer actions ([`02-typing-hooks.md`](./02-typing-hooks.md)) and state.

### Variant props

```tsx
type ButtonProps =
  | { variant: "link"; href: string; onClick?: never }
  | { variant: "button"; onClick: () => void; href?: never };
```

`never` on the other branch's property forbids passing both. Don't destructure props too early in the component; destructuring before checking the discriminant loses the narrowing in some cases, so check `props.variant` first (or destructure inside each branch).

---

## 2. Mutually exclusive props without a discriminant

Sometimes there's no natural tag — either prop A **or** prop B, never both. Use `never` to forbid the other:

```tsx
type ControlledProps = { value: string; onChange: (v: string) => void; defaultValue?: never };
type UncontrolledProps = { defaultValue?: string; value?: never; onChange?: never };

type InputProps = ControlledProps | UncontrolledProps;
```

This encodes the controlled/uncontrolled rule from [`../06-forms/00-controlled-and-uncontrolled-inputs.md`](../06-forms/00-controlled-and-uncontrolled-inputs.md) in the type system. Use sparingly; unions of many shapes get hard to read.

---

## 3. Exhaustiveness checking

Make the compiler tell you when you forget to handle a new case, with a `never` check:

```tsx
type Shape =
  | { kind: "circle"; radius: number }
  | { kind: "square"; size: number };

function assertNever(value: never): never {
  throw new Error(`Unhandled case: ${JSON.stringify(value)}`);
}

function area(shape: Shape): number {
  switch (shape.kind) {
    case "circle":
      return Math.PI * shape.radius ** 2;
    case "square":
      return shape.size ** 2;
    default:
      return assertNever(shape);   // error if a new kind is added but not handled
  }
}
```

If you later add `{ kind: "triangle" }` to `Shape` and forget a `case`, the `default` branch receives a non-`never` value, and TypeScript reports an error **at compile time**. Ideal for reducers and status-driven UI.

---

## 4. Polymorphic components (`as` prop)

A component that can render as different elements while keeping the correct props for each:

```tsx
import type { ElementType, ComponentPropsWithoutRef } from "react";

type PolymorphicProps<E extends ElementType, P = object> = P &
  Omit<ComponentPropsWithoutRef<E>, keyof P | "as"> & {
    as?: E;
  };

type TextOwnProps = { size?: "sm" | "md" | "lg" };

function Text<E extends ElementType = "span">({
  as,
  size = "md",
  ...rest
}: PolymorphicProps<E, TextOwnProps>) {
  const Component: ElementType = as ?? "span";
  return <Component className={`text-${size}`} {...rest} />;
}

<Text>default span</Text>
<Text as="h1" size="lg">heading</Text>
<Text as="a" href="/home">link</Text>          // ✅ href is valid for "a"
<Text as="span" href="/home">oops</Text>       // ❌ href is not valid for "span"
```

How it works:

- `E` is the element or component chosen via `as`, defaulting to `"span"`.
- `ComponentPropsWithoutRef<E>` supplies that element's native props.
- `Omit<…, keyof P | "as">` prevents conflicts with your own props.
- The inner `Component: ElementType` annotation avoids a deep inference problem when spreading `rest`.

Typing `ref` on polymorphic components is more involved; in React 19, `ref` is a regular prop, which simplifies it. Often a library (Radix's `asChild` with `Slot`) is a better choice for production components — see [`../05-component-design/01-composition-patterns.md`](../05-component-design/01-composition-patterns.md).

---

## 5. Typed context with a safe provider

A reusable pattern for a context that can never be `null` to consumers is covered in [`02-typing-hooks.md`](./02-typing-hooks.md) and generalized with a factory in [`03-generics.md`](./03-generics.md). For compound components, the same idea applies: the child components read the parent's context through a typed hook ([`../05-component-design/02-compound-components.md`](../05-component-design/02-compound-components.md)).

---

## 6. Typing data from outside your code

TypeScript types **disappear at runtime**. Data from APIs, `localStorage`, URL parameters, and form input hasn't been checked against your types, so casting it (`as User`) only lies to the compiler:

```tsx
// ❌ Trusts the server blindly
const user = (await res.json()) as User;
```

Validate at the boundary with a schema and **derive the type from the schema**:

```tsx
import { z } from "zod";

const userSchema = z.object({
  id: z.string(),
  name: z.string(),
  email: z.string().email(),
  role: z.enum(["user", "admin"]),
});

type User = z.infer<typeof userSchema>;

async function fetchUser(id: string): Promise<User> {
  const res = await fetch(`/api/users/${id}`);
  if (!res.ok) throw new Error(`HTTP ${res.status}`);
  return userSchema.parse(await res.json());   // throws if the shape is wrong
}
```

Now `User` is true at runtime *and* compile time. Integrations: forms ([`../06-forms/01-form-validation.md`](../06-forms/01-form-validation.md)), API clients ([`../11-api-integration/02-api-client.md`](../11-api-integration/02-api-client.md), [`../20-frontend-architecture/02-api-architecture.md`](../20-frontend-architecture/02-api-architecture.md)).

---

## 7. `unknown`, type guards, and assertions

Use `unknown` instead of `any` when you don't know a value's type; it forces you to check before use:

```tsx
try {
  await save();
} catch (err: unknown) {
  if (err instanceof Error) setMessage(err.message);
  else setMessage("Something went wrong");
}
```

A **type guard** packages a check so TypeScript narrows for you:

```tsx
function isUser(value: unknown): value is User {
  return (
    typeof value === "object" &&
    value !== null &&
    "id" in value &&
    "name" in value
  );
}
```

Prefer schema validation for anything non-trivial; hand-written guards drift from the real shape.

---

## 8. Template literal and literal-union types

Build precise string types from smaller pieces:

```tsx
type Size = "sm" | "md" | "lg";
type Color = "red" | "blue";
type ButtonClass = `btn-${Size}-${Color}`;    // "btn-sm-red" | "btn-sm-blue" | ...

type EventName = "click" | "hover";
type HandlerProp = `on${Capitalize<EventName>}`;   // "onClick" | "onHover"
```

Useful for design-token props, class names, and typed route paths. Don't go overboard: huge combinatorial unions slow the compiler.

---

## 9. Constants as the source of truth

Derive unions from `as const` objects instead of retyping them ([`04-utility-types.md`](./04-utility-types.md)):

```tsx
const STATUS = { idle: "idle", loading: "loading", done: "done" } as const;
type Status = (typeof STATUS)[keyof typeof STATUS];
```

---

## 10. Typed render props and children as function

```tsx
type ToggleProps = {
  children: (state: { on: boolean; toggle: () => void }) => ReactNode;
};

function Toggle({ children }: ToggleProps) {
  const [on, setOn] = useState(false);
  return <>{children({ on, toggle: () => setOn((o) => !o) })}</>;
}

<Toggle>{({ on, toggle }) => <button onClick={toggle}>{on ? "On" : "Off"}</button>}</Toggle>
```

Render props are mostly replaced by hooks ([`../05-component-design/03-legacy-component-patterns.md`](../05-component-design/03-legacy-component-patterns.md)), but the typing is straightforward and still appears in libraries.

---

## When to stop

Types are a tool, not the goal. Signs you've gone too far:

- Teammates can't read the type without a 10-minute explanation.
- Compiler errors are cryptic walls of text.
- You're adding `as` casts to satisfy your own clever types.
- The editor slows down.

Prefer a simpler API (fewer variants, separate components) to a heroic type. Aim for **types that make the common mistakes impossible and the common usage easy**.

---

## Common mistakes

- **Optional-flag soup** (`isLoading?`, `error?`, `data?`) — use a discriminated union.
- **Destructuring before narrowing** — can break discriminated-union narrowing; check the tag first.
- **Casting API data with `as`** — validate with a schema and derive the type.
- **Forgetting an exhaustiveness check** — new variants silently fall through; add `assertNever`.
- **`any` for caught errors or unknown data** — use `unknown` and narrow.
- **Overly clever polymorphic types** — consider `asChild`/`Slot` or two components.
- **Giant combinatorial literal types** — hurt compile times and readability.
- **Hand-written type guards for complex shapes** — use a schema validator.

## Quick summary

- Use discriminated unions so props and state can't be combined wrongly
- Forbid conflicting props with `never` in union branches
- Add `assertNever` for compile-time exhaustiveness
- Type polymorphic components with `ElementType` and `ComponentPropsWithoutRef<E>`
- Validate external data with a schema and derive types via `z.infer`
- Prefer `unknown` plus narrowing to `any`
- Keep types readable: simpler APIs beat clever types

## Next

You've finished the TypeScript chapter. Continue to **[`../05-component-design/README.md`](../05-component-design/README.md)** to design reusable component APIs using these tools. For form-specific schema usage, see [`../06-forms/01-form-validation.md`](../06-forms/01-form-validation.md).
