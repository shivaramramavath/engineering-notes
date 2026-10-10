# Generic and Polymorphic Components

Two techniques make components reusable without giving up type safety. A **generic component** takes a type parameter, so a `List<User>` or a `Select<Country>` knows the shape of its data and passes that shape to its callbacks. A **polymorphic component** takes an `as` prop that changes which element it renders (`<Button as="a" href="/x">`), and its props follow the chosen element. Both lean on the generics and utility types from earlier sections, and both have a few TSX-specific traps.

**Prerequisites:**
- [Component props and children](./00-component-props-and-children.md)
- [Generic functions](../06-generics/00-generic-functions.md) and [constraints](../06-generics/02-generic-constraints-and-defaults.md)
- [Refs](./04-refs.md)

---

## Generic components

A generic component is a function with a type parameter, used to relate several props to the same type.

```tsx
interface ListProps<T> {
  items: readonly T[];
  renderItem: (item: T, index: number) => ReactNode;
  getKey: (item: T) => string;
}

function List<T>({ items, renderItem, getKey }: ListProps<T>) {
  return (
    <ul>
      {items.map((item, i) => (
        <li key={getKey(item)}>{renderItem(item, i)}</li>
      ))}
    </ul>
  );
}

<List
  items={users}                                // T is inferred as User
  getKey={(u) => u.id}                         // u: User
  renderItem={(u) => <span>{u.name}</span>}    // u: User
/>
```

`T` is inferred from `items`, and the callbacks get the exact item type with no annotation. This is the main payoff: callbacks stay typed to the data.

### Constraints

Require only what the component actually uses:

```tsx
interface SelectProps<T extends { id: string }> {
  options: readonly T[];
  value: T | null;
  onChange: (option: T) => void;
  getLabel: (option: T) => string;
}

function Select<T extends { id: string }>({ options, value, onChange, getLabel }: SelectProps<T>) {
  return (
    <select
      value={value?.id ?? ""}
      onChange={(e) => {
        const next = options.find((o) => o.id === e.target.value);
        if (next) onChange(next);
      }}
    >
      {options.map((o) => (
        <option key={o.id} value={o.id}>{getLabel(o)}</option>
      ))}
    </select>
  );
}
```

`T extends { id: string }` lets the component read `option.id` while staying generic over everything else. Passing options without an `id` is a compile error.

### Generic arrow functions in `.tsx` files

In a `.tsx` file, `<T>(props) => ...` is parsed as a JSX tag. Use a trailing comma or an `extends` clause:

```tsx
const List = <T,>({ items }: { items: T[] }) => <ul>{items.map(String)}</ul>;
const Select = <T extends object>(props: SelectProps<T>) => { /* ... */ };
```

A plain `function List<T>(...)` declaration avoids the problem, and is the simpler choice.

### Keys of a type

Components that take a **column** or **field name** can tie it to the data type:

```tsx
interface Column<T> {
  key: keyof T & string;
  header: string;
  render?: (row: T) => ReactNode;
}

interface TableProps<T extends { id: string }> {
  rows: readonly T[];
  columns: readonly Column<T>[];
}

function Table<T extends { id: string }>({ rows, columns }: TableProps<T>) {
  return (
    <table>
      <thead>
        <tr>{columns.map((c) => <th key={c.key}>{c.header}</th>)}</tr>
      </thead>
      <tbody>
        {rows.map((row) => (
          <tr key={row.id}>
            {columns.map((c) => (
              <td key={c.key}>{c.render ? c.render(row) : String(row[c.key])}</td>
            ))}
          </tr>
        ))}
      </tbody>
    </table>
  );
}

<Table rows={users} columns={[{ key: "name", header: "Name" }, { key: "nmae", header: "Typo" }]} />
//                                                                    ^ error: not a key of User
```

`keyof T & string` rejects typos and renames show up everywhere. See [keyof and typeof](../06-generics/03-keyof-and-typeof.md).

### Generics and `forwardRef`

In React 18, `forwardRef` returns a non-generic component, so the type parameter is lost. Common workarounds are a cast, or declaring the exported type by hand:

```tsx
const ListImpl = forwardRef(function List<T>(props: ListProps<T>, ref: ForwardedRef<HTMLUListElement>) {
  /* ... */
});

export const List = ListImpl as <T>(
  props: ListProps<T> & { ref?: Ref<HTMLUListElement> },
) => ReactElement | null;
```

In React 19, where `ref` is an ordinary prop, a generic function component just declares it in its props, and no cast is needed ([refs](./04-refs.md)).

## Polymorphic components (`as` prop)

A polymorphic component can render as different elements or components:

```tsx
<Text>Paragraph</Text>                       // renders a <span> by default
<Text as="h1">Heading</Text>
<Text as="a" href="/home">Link</Text>        // href is allowed because "a" accepts it
<Text as="button" onClick={save}>Save</Text>
<Text as="a" hreff="/typo">Oops</Text>       // error: unknown prop for <a>
```

The challenge: the props must depend on the `as` value. The standard approach is a generic over `React.ElementType`.

```tsx
import type { ComponentPropsWithoutRef, ElementType, ReactNode } from "react";

type AsProp<E extends ElementType> = { as?: E };

type PolymorphicProps<E extends ElementType, Own = {}> =
  Own & AsProp<E> & Omit<ComponentPropsWithoutRef<E>, keyof (Own & AsProp<E>)>;

type TextOwnProps = { size?: "sm" | "md" | "lg"; children?: ReactNode };

function Text<E extends ElementType = "span">({
  as,
  size = "md",
  ...rest
}: PolymorphicProps<E, TextOwnProps>) {
  const Component: ElementType = as ?? "span";
  return <Component className={`text-${size}`} {...rest} />;
}
```

How it works:

- `E extends ElementType` is any tag name or component. `= "span"` is the default when `as` is omitted.
- `ComponentPropsWithoutRef<E>` gives the props of whatever `E` is.
- `Omit<..., keyof (Own & AsProp<E>)>` removes any native props that conflict with your own (`size`, `as`), so your versions win.
- The final intersection is therefore: your props, `as`, and the rest of the chosen element's props.
- `const Component: ElementType = as ?? "span"` widens the element type so the JSX compiles. TypeScript cannot fully resolve a generic JSX tag's props, so this single loosening is the usual compromise. The **public signature** (what callers see) is still precisely typed.

Callers get autocomplete for the chosen element: `<Text as="a">` offers `href`, `target`, and `rel`, and rejects `disabled`.

### Polymorphic components and refs

Adding `ref` support to polymorphic components is where the types become heavy (the ref's element type must follow `E`). If you need it:

- Use `ComponentPropsWithRef<E>` instead of `ComponentPropsWithoutRef<E>`, so the ref type follows the element (works naturally with React 19's `ref` as a prop).
- In React 18, you also need `forwardRef` plus a cast to restore the generic signature.

Consider whether you need full polymorphism. Often a small fixed set of variants is simpler and more type-safe.

### Alternatives to full polymorphism

| Need | Simpler option |
|---|---|
| A button that can also be a link | a discriminated union of props (`variant: "button" \| "link"`) ([component props](./00-component-props-and-children.md)) |
| Passing a custom component to render | a `component: ComponentType<Props>` prop |
| Merging props onto a child element | a "slot" pattern (clone the child element) with `ReactElement` |
| A fixed set of tags | `tag: "h1" \| "h2" \| "h3"` and a lookup |

Use `as` when you truly want arbitrary elements and components (design-system primitives such as `Box` or `Text`). Otherwise prefer the simpler options.

## Typing higher-order components

A HOC wraps a component and injects props. Type the injected props and remove them from what the caller must supply:

```tsx
interface WithUserProps { user: User }

function withUser<P extends WithUserProps>(Component: ComponentType<P>) {
  return function WithUser(props: Omit<P, keyof WithUserProps>) {
    const user = useCurrentUser();
    return <Component {...(props as P)} user={user} />;
  };
}
```

The cast `props as P` is the usual compromise, since TypeScript cannot prove that `Omit<P, "user"> & { user }` equals `P`. Hooks and context are usually a simpler alternative today ([context](./03-context.md)), so use HOCs mainly when working with older code or libraries that expect them.

## Compound components

Related components that share an implicit context (`<Tabs>`, `<Tabs.List>`, `<Tabs.Panel>`) can be typed with a strict context and attached as static properties:

```tsx
function Tabs({ children }: { children: ReactNode }) { /* provides context */ }
function TabsList({ children }: { children: ReactNode }) { /* ... */ }
Tabs.List = TabsList;

<Tabs><Tabs.List>...</Tabs.List></Tabs>
```

Assigning properties onto a function declaration is allowed by TypeScript (it is treated as a namespace merge), and the compound type is inferred ([declaration merging](../09-declaration-files/03-declaration-merging.md), [context](./03-context.md)).

## Common mistakes

- Making a component generic when nothing relates two props to the same type.
- Over-constraining `T` so valid data is rejected, or under-constraining so the component cannot read what it needs.
- Using `any` for the item type "to make it work" instead of a generic.
- `<T>(...) =>` arrow functions in `.tsx` files without a comma or `extends`.
- Losing generics through `forwardRef` (React 18) and not realizing the component is now typed with `unknown`.
- Building a polymorphic component without `Omit`, so your `size` prop conflicts with a native `size` and the intersection becomes `never`.
- Spreading a polymorphic `Component` without widening and fighting unreadable JSX errors.
- Reaching for `as` when a prop union would be clearer.
- Giving callbacks explicit annotations that duplicate (and may contradict) the inferred type.

## Debugging

- Hover the component in JSX: the displayed signature shows what `T` was inferred as. If it is `unknown`, `T` had no inference source (add a type argument or a typed prop).
- Pass explicit type arguments to isolate inference problems: `<List<User> items={...} />`.
- If a polymorphic prop type collapses to `never`, check for conflicting prop names that need `Omit`.
- For "Type instantiation is excessively deep", simplify the polymorphic helper types, or limit the `as` options to a union of allowed elements.
- Read the **last line** of long JSX type errors for the actual mismatch ([reading type errors](../18-testing-and-debugging/06-reading-type-errors.md)).

## Quick summary

- A generic component declares `<T>` and uses it across props, so callbacks and keys are typed to the data. Use constraints (`T extends { id: string }`) for what the component needs.
- In `.tsx`, use `function Name<T>(...)` or `<T,>` / `<T extends X>` for arrows.
- `keyof T & string` ties column and field names to the data shape.
- Polymorphic `as` components use `E extends ElementType`, `ComponentPropsWithoutRef<E>`, and an `Omit` of your own props. A small internal widening is the usual compromise.
- Prefer prop unions and simple variants where full polymorphism is not needed. React 19's `ref` prop removes most generic and `forwardRef` friction.

**Next:** [Server state with TanStack Query](./07-server-state-tanstack-query.md)
