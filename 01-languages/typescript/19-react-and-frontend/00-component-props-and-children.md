# Component Props and Children

A React component is a function from **props** to UI, so typing a component is mostly typing its props. Done well, the compiler tells callers exactly what a component accepts, rejects invalid combinations, and gives autocomplete for every option. This note covers the standard ways to type props, `children`, HTML-element wrappers, and the union patterns that make invalid prop combinations impossible.

> **Version note.** Examples assume React 18 or 19 with the matching `@types/react`. Where the two differ, it is called out. In React 19 types, the global `JSX` namespace is gone: use `React.JSX.Element` if you need it, and prefer not to annotate component return types at all.

**Prerequisites:**
- [Interfaces](../04-objects-and-interfaces/00-interfaces.md) and [type aliases](../01-fundamentals/04-type-aliases.md)
- [Discriminated unions](../03-unions-and-narrowing/04-discriminated-unions.md)
- [Utility types](../07-utility-types/README.md)

---

## The basic shape

```tsx
interface GreetingProps {
  name: string;
  excited?: boolean;
}

function Greeting({ name, excited = false }: GreetingProps) {
  return <p>Hello, {name}{excited ? "!" : "."}</p>;
}

<Greeting name="Asha" />           // ok
<Greeting name="Asha" excited />   // ok
<Greeting />                       // error: Property 'name' is missing
<Greeting name={42} />             // error: number is not assignable to string
```

Annotate the **props parameter**. The component's return type is inferred, and there is no need to write `: JSX.Element` or `: ReactElement`. Default values go in the destructuring (`excited = false`), which also makes TypeScript treat the prop as optional for callers.

### Do I need `React.FC`?

No. `React.FC<Props>` used to add an implicit `children` prop (removed in React 18's types) and does not support generics well. Plain functions with an annotated props parameter are simpler, work with generics ([generic components](./06-generic-and-polymorphic-components.md)), and are the common convention.

## `children`

`children` is just a prop. Type it explicitly:

```tsx
import type { ReactNode } from "react";

interface CardProps {
  title: string;
  children: ReactNode;
}

function Card({ title, children }: CardProps) {
  return (
    <section>
      <h2>{title}</h2>
      {children}
    </section>
  );
}
```

`ReactNode` is the right default: it accepts anything React can render (elements, strings, numbers, `null`, `undefined`, booleans, arrays, fragments). Related types:

| Type | Accepts | Use for |
|---|---|---|
| `ReactNode` | anything renderable | `children`, slots (the usual choice) |
| `ReactElement` | an element created with JSX (`<div />`), not strings or numbers | when you must receive an element, to clone it or inspect its props |
| `JSX.Element` (`React.JSX.Element`) | the result of a JSX expression | return types (rarely needed) |
| `string` | text only | when only text makes sense |

`PropsWithChildren<P>` is a helper that adds `children?: ReactNode` to `P`. It is fine, but writing `children: ReactNode` yourself is explicit about whether children are required.

### Slots and render props

Pass multiple regions as props, and functions as children when the component owns some state:

```tsx
interface LayoutProps {
  header: ReactNode;
  sidebar?: ReactNode;
  children: ReactNode;
}

interface ToggleProps {
  children: (state: { on: boolean; toggle: () => void }) => ReactNode;
}

function Toggle({ children }: ToggleProps) {
  const [on, setOn] = useState(false);
  return <>{children({ on, toggle: () => setOn((v) => !v) })}</>;
}

<Toggle>{({ on, toggle }) => <button onClick={toggle}>{on ? "On" : "Off"}</button>}</Toggle>
```

The callback parameters are contextually typed from `ToggleProps`, so you do not annotate them at the call site.

## Callback props

Type callbacks with the arguments the component will pass:

```tsx
interface SearchBoxProps {
  value: string;
  onChange: (value: string) => void;          // the child passes the string, not the event
  onSubmit?: () => void;
}
```

Prefer callbacks that receive **values** (`(value: string) => void`) over raw events when the parent does not need the event. That keeps parents free of DOM details. For event-typed props, see [event types](./01-event-types.md).

## Wrapping HTML elements

To build a component that behaves like a native element, reuse the element's own props instead of redeclaring them:

```tsx
import type { ComponentPropsWithoutRef } from "react";

interface ButtonProps extends ComponentPropsWithoutRef<"button"> {
  variant?: "primary" | "secondary";
}

function Button({ variant = "primary", className, ...rest }: ButtonProps) {
  return <button className={`btn btn-${variant} ${className ?? ""}`} {...rest} />;
}

<Button variant="secondary" disabled onClick={() => save()} aria-label="Save" />
```

`ComponentPropsWithoutRef<"button">` is every prop a `<button>` accepts (`onClick`, `disabled`, `type`, `aria-*`, ...). Spread `...rest` onto the element so all of them still work. Related helpers:

| Helper | Gives |
|---|---|
| `ComponentProps<"input">` | all props of an element **including** `ref` |
| `ComponentPropsWithoutRef<"input">` | props without `ref` (use when you handle refs separately) |
| `ComponentProps<typeof MyComponent>` | the props of another component |
| `HTMLAttributes<HTMLDivElement>` | the shared attributes (no element-specific ones) |

To get the props of any component you do not own: `ComponentProps<typeof ThirdPartyButton>`.

### Overriding a native prop

If your prop has the same name as a native one but a different type, `extends` conflicts. Omit the native one first:

```tsx
interface InputProps extends Omit<ComponentPropsWithoutRef<"input">, "size"> {
  size?: "sm" | "md" | "lg";       // the native `size` is a number
}
```

### Style and class names

```tsx
import type { CSSProperties } from "react";

interface BoxProps {
  className?: string;
  style?: CSSProperties;
}
```

## Prop combinations that cannot be invalid

A component whose props depend on each other is a **discriminated union** waiting to happen:

```tsx
type ActionProps =
  | { variant: "button"; onClick: () => void; href?: never }
  | { variant: "link"; href: string; onClick?: never };

function Action(props: ActionProps) {
  if (props.variant === "link") {
    return <a href={props.href}>Go</a>;       // href is string here
  }
  return <button onClick={props.onClick}>Go</button>;
}

<Action variant="link" href="/home" />          // ok
<Action variant="button" onClick={go} />        // ok
<Action variant="link" onClick={go} />          // error: onClick is not allowed with "link"
<Action variant="button" />                     // error: onClick is required
```

`?: never` on the other branch's props makes mixed use an error. The `variant` literal narrows the whole props object, which is the same technique as in [discriminated unions](../03-unions-and-narrowing/04-discriminated-unions.md).

Another common case is "either this prop or that one, never both" (for example `value` or `defaultValue`), which you can express with the same `?: never` trick.

## Components and elements as props

```tsx
import type { ComponentType, ElementType } from "react";

interface IconButtonProps {
  icon: ComponentType<{ className?: string }>;   // a component, e.g. <props.icon />
  label: string;
}

function IconButton({ icon: Icon, label }: IconButtonProps) {
  return (
    <button aria-label={label}>
      <Icon className="icon" />
    </button>
  );
}
```

- `ComponentType<P>` accepts function and class components that take props `P`.
- `ElementType` accepts a component **or** an intrinsic tag name (`"div"`, `"a"`), used for polymorphic components ([generic and polymorphic components](./06-generic-and-polymorphic-components.md)).
- If you need to pass an already-rendered element, use `ReactElement` or `ReactNode`.

## Optional vs `undefined`

```tsx
interface Props { label?: string }          // may be omitted, or passed as undefined
interface Props2 { label: string | undefined }   // must be passed, even if undefined
```

Use `?` for "may be omitted". With `exactOptionalPropertyTypes` enabled, `label?: string` means omitted only, and explicitly passing `undefined` becomes an error unless you add `| undefined` ([strict mode](../13-compiler-and-tsconfig/01-strict-mode.md)).

## Constant option sets

Derive prop types from a runtime list so the options and the type cannot drift:

```tsx
const SIZES = ["sm", "md", "lg"] as const;
type Size = (typeof SIZES)[number];          // "sm" | "md" | "lg"

function SizePicker({ value, onChange }: { value: Size; onChange: (s: Size) => void }) {
  return (
    <select value={value} onChange={(e) => onChange(e.target.value as Size)}>
      {SIZES.map((s) => <option key={s} value={s}>{s}</option>)}
    </select>
  );
}
```

The `as Size` cast on `e.target.value` is the one unavoidable assertion, since a DOM value is always `string`. See [event types](./01-event-types.md).

## Props are read-only

Props are immutable inputs. You do not need `Readonly<Props>`, but never mutate `props.items` or any object you receive. Copy before sorting or changing: `[...items].sort(...)`. Declaring array props as `readonly T[]` makes the compiler enforce this and lets callers pass readonly arrays ([variance](../14-type-system-internals/02-variance.md)):

```tsx
interface ListProps { items: readonly string[] }
```

## Common mistakes

- Typing props as `any` or leaving them untyped, which silently disables checking inside the component.
- Using `React.FC` and expecting implicit `children`.
- Typing `children` as `JSX.Element`, which rejects strings, numbers, and arrays. Use `ReactNode`.
- Redeclaring every HTML attribute instead of extending `ComponentPropsWithoutRef<"element">`.
- Extending a native props type and conflicting on a prop name without `Omit`.
- Several optional props that are really a discriminated union, so invalid combinations compile.
- Forgetting to spread `...rest` onto the element, silently dropping `onClick`, `aria-*`, and `data-*` attributes.
- Using `{}` or `object` as a props type.
- Mutating props.
- Passing an `Event` type where a `value` callback would be simpler.

## Debugging

- Hover the component in JSX to see the props type it resolved to.
- To see another component's props, use `ComponentProps<typeof X>` and hover the alias.
- Errors on JSX often read "Type '{...}' is not assignable to type 'IntrinsicAttributes & Props'". Read the **last** line for the real mismatch ([reading type errors](../18-testing-and-debugging/06-reading-type-errors.md)).
- If a prop you passed does not appear to reach the element, check that `...rest` is spread and that you did not destructure it away.
- If a union of props does not narrow, check that the discriminant is a literal type (`"link"`), not `string`.

## Quick summary

- Type the props parameter with an interface or type alias. Skip `React.FC`.
- `children: ReactNode` for anything renderable. Use function children and named slots for flexible composition.
- Wrap native elements by extending `ComponentPropsWithoutRef<"tag">`, `Omit` conflicting props, and spread `...rest`.
- Express dependent props as a discriminated union, with `?: never` to forbid mixing.
- Use `ComponentType`, `ElementType`, and `ReactElement` for components and elements passed as props, and `as const` lists to derive option types.

**Next:** [Event types](./01-event-types.md)