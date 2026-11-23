# Generic and Polymorphic Components in TypeScript

> Authoring advanced generic React components, polymorphic `as` prop implementations, ComponentPropsWithRef, forwardRef with generics, and type-safe UI design systems.

| Metadata | Details |
| :--- | :--- |
| **Category** | React and Frontend Typing |
| **Difficulty** | Advanced |
| **Prerequisites** | [06-generics/00-generic-functions.md](../06-generics/00-generic-functions.md), [10-advanced-types/00-conditional-types.md](../10-advanced-types/00-conditional-types.md), [19-react-and-frontend/00-component-props-and-children.md](./00-component-props-and-children.md) |
| **TypeScript Baseline** | TS 5.0+, React 19 / @types/react 19 |
| **Tested tsconfig** | `"strict": true, "jsx": "react-jsx", "module": "NodeNext"` |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & API Reference](#4-syntax--api-reference)
5. [Practical Examples](#5-practical-examples)
6. [Edge Cases, Pitfalls & Gotchas](#6-edge-cases-pitfalls--gotchas)
7. [Common Mistakes & Anti-Patterns](#7-common-mistakes--anti-patterns)
8. [Performance & Type-Checker Complexity](#8-performance--type-checker-complexity)
9. [Compiler Internals](#9-compiler-internals)
10. [Best Practices & Design Guidelines](#10-best-practices--design-guidelines)
11. [Real-World Case Study](#11-real-world-case-study)
12. [Interview Questions](#12-interview-questions)
13. [Exercises & Challenges](#13-exercises--challenges)

---

## 1. What & Why

In modern UI design systems, two architectural patterns represent the pinnacle of component flexibility and type safety:
1. **Generic Components**: Components parameterized over data types (e.g. `<Table<User> data={users} onRowClick={(u) => u.email} />`). The child render props and callbacks automatically infer property types from the passed `data` array without type assertions.
2. **Polymorphic Components**: Components that can dynamically render as different underlying HTML elements or React components using an `as` prop (e.g. `<Button as="a" href="/dashboard">` or `<Text as="h1">`), while automatically swapping supported HTML attributes (`href` allowed only when `as="a"`, `type="submit"` allowed only when `as="button"`).

Without proper TypeScript typing, polymorphic components either degrade to `as: any` (losing attribute safety) or trigger crippling compiler errors when attempting to forward refs.

```
+-----------------------------------------------------------+
|                   Polymorphic Component                   |
|                                                           |
|  <Button as="button" type="submit"> -> Valid button props |
|                                                           |
|  <Button as="a" href="/home">       -> Valid anchor props |
|                                                           |
|  <Button as="a" type="submit">      -> COMPILER ERROR!    |
|  (href required, type invalid)                            |
+-----------------------------------------------------------+
```

---

## 2. Core Concepts

### 1. The Polymorphic `as` Prop Type Model
The type of allowed props depends directly on the element passed to `as`:
```typescript
import type { ElementType, ComponentPropsWithRef } from 'react';

type AsProp<E extends ElementType> = {
  as?: E;
};

export type PolymorphicProps<E extends ElementType, P = {}> =
  P & AsProp<E> & Omit<ComponentPropsWithRef<E>, keyof P | 'as'>;
```

### 2. Generic Function Components
In `.tsx` files, generic arrow function syntax `<T>` conflicts with JSX tags (`<T>` looks like an opening tag for an element named `T`).
Solutions:
- **Function Declaration (Recommended)**: `function List<T>(props: ListProps<T>)`
- **Trailing Comma in Arrow Functions**: `const List = <T,>(props: ListProps<T>) => ...`
- **Extends Constraint**: `const List = <T extends unknown>(props: ListProps<T>) => ...`

---

## 3. How It Works Internally

When a consumer renders `<Button as="a" href="/docs" />`:
1. `E` is inferred as `'a'`.
2. `Omit<ComponentPropsWithRef<'a'>, ...>` merges native anchor attributes (`href`, `target`, `rel`).
3. If an invalid attribute like `disabled` or `type="submit"` is passed, TypeScript evaluates `keyof ComponentPropsWithRef<'a'>` and flags the property with `TS2322`.

---

## 4. Syntax & API Reference

### Production Polymorphic Component Type Helper
```typescript
import type { ElementType, ComponentPropsWithRef, ReactElement } from 'react';

export type PolymorphicComponentPropWithRef<
  E extends ElementType,
  Props = {}
> = Props & { as?: E } & Omit<ComponentPropsWithRef<E>, keyof Props | 'as'>;
```

---

## 5. Practical Examples

### Example 1: Basic Generic Autocompleting List Component
```typescript
import type { ReactNode } from 'react';

export interface ListProps<T> {
  items: readonly T[];
  renderItem: (item: T, index: number) => ReactNode;
  keyExtractor: (item: T) => string | number;
}

export function List<T>({ items, renderItem, keyExtractor }: ListProps<T>): React.JSX.Element {
  return (
    <ul>
      {items.map((item, index) => (
        <li key={keyExtractor(item)}>{renderItem(item, index)}</li>
      ))}
    </ul>
  );
}

// Usage: Type of 'user' is automatically inferred as User!
interface User { id: string; name: string; }
const users: User[] = [{ id: '1', name: 'Alice' }];

const element = (
  <List
    items={users}
    keyExtractor={(u) => u.id}
    renderItem={(u) => <span>{u.name}</span>}
  />
);
```

### Example 2: Real-World Polymorphic Button / Link Component
```typescript
import type { ElementType, ReactElement } from 'react';

type BaseButtonProps = {
  variant?: 'primary' | 'secondary' | 'danger';
  size?: 'sm' | 'md' | 'lg';
};

export type ButtonProps<E extends ElementType> = PolymorphicComponentPropWithRef<E, BaseButtonProps>;

export function Button<E extends ElementType = 'button'>({
  as,
  variant = 'primary',
  size = 'md',
  className = '',
  ...rest
}: ButtonProps<E>): ReactElement {
  const Component = as || 'button';
  return <Component className={`btn btn-${variant} btn-${size} ${className}`} {...rest} />;
}
```

### Example 3: Edge Case - Generic Dropdown Select with Value Invariance
```typescript
export interface SelectProps<T extends string | number> {
  options: { label: string; value: T }[];
  value: T;
  onChange: (value: T) => void;
}

export function Select<T extends string | number>({ options, value, onChange }: SelectProps<T>) {
  return (
    <select
      value={value}
      onChange={(e) => {
        const selected = options.find((o) => String(o.value) === e.target.value);
        if (selected) onChange(selected.value);
      }}
    >
      {options.map((opt) => (
        <option key={String(opt.value)} value={opt.value}>
          {opt.label}
        </option>
      ))}
    </select>
  );
}
```

### Example 4: Failure Case - Invalid Prop for Polymorphic Tag
```typescript
// When as="a", type="submit" is invalid!
// @ts-expect-error TS2322: Type '"submit"' is not assignable to type 'string | undefined' for anchor element attributes.
const badLink = <Button as="a" href="/dashboard" type="submit">Dashboard</Button>;
```

### Example 5: Idiomatic Generic Autocomplete Combobox
```typescript
interface ComboboxProps<T> {
  items: T[];
  filterBy: (item: T, query: string) => boolean;
  onSelect: (item: T) => void;
  getDisplayValue: (item: T) => string;
}

export function Combobox<T>({ items, filterBy, onSelect, getDisplayValue }: ComboboxProps<T>) {
  return (
    <div className="combobox">
      {items.map((item, idx) => (
        <div key={idx} onClick={() => onSelect(item)}>
          {getDisplayValue(item)}
        </div>
      ))}
    </div>
  );
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **JSX Arrow Function Syntax Collision**:
   Writing `const List = <T>(props: Props<T>) => ...` in a `.tsx` file causes a JSX parser syntax error because `<T>` is interpreted as an unclosed JSX element. Fix: use `function List<T>` or `const List = <T,>(...)`.
2. **Ref Forwarding on Polymorphic Components (React 18)**:
   In React 18, wrapping a generic component in `forwardRef` loses generic inference because `forwardRef`'s type definition does not carry generics through easily without type casting. React 19 completely eliminates this issue by allowing `ref` as a normal generic prop!
3. **Prop Collisions in Polymorphic Types**:
   If your component defines a custom prop `color: 'red' | 'blue'`, and the native HTML element also defines `color: string`, an intersection conflict occurs unless wrapped in `Omit<ComponentPropsWithRef<E>, keyof Props>`.
4. **Deep Type Instantiations**:
   Overly complex nested polymorphic helpers can exceed TypeScript's max instantiation depth if applied across 10 layers of components.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Bad Code | Good Code | Compiler / Runtime Impact |
| :--- | :--- | :--- | :--- |
| **`as: any`** | `as: any` prop | Generic `as?: E` with `PolymorphicProps` | Disables compiler checks on element attributes |
| **Arrow Generic Syntax in TSX** | `const Comp = <T>(...)` | `function Comp<T>(...)` | Syntax error: unclosed JSX tag `<T>` |
| **Omitting Omit in Polymorphic Props** | `P & ComponentProps<E>` | `P & Omit<ComponentProps<E>, keyof P>` | Conflicting property type collisions |
| **Casting Table Rows to Any** | `(row as any).name` in render prop | Generic `Table<T>` inferring row | Fragile runtime property accesses |
| **Ref Type Mismatch on Polymorphic Element** | Hardcoding `ref: Ref<HTMLButtonElement>` | Inferred `ref: ComponentPropsWithRef<E>['ref']` | Type mismatch when `as="a"` is used |
| **Over-Abstracting Everything** | Polymorphic wrapper for single static div | Standard HTML `<div />` | Massive type-checker latency |

---

## 8. Performance & Type-Checker Complexity

- **Type Checker Load**: Polymorphic types containing `ComponentPropsWithRef<E>` query massive HTML element dictionaries. If an entire design system defines 50 polymorphic components, ensure they use clean `Omit` helpers to prevent TSServer autocomplete latency.
- **Zero Runtime Cost**: The `as` prop simply selects which tag to render (`const Component = as || 'div'`); zero overhead over standard JSX.

---

## 9. Compiler Internals

When resolving:
```typescript
<Button as="a" href="/" />
```
The compiler's type argument inference algorithm binds generic parameter `E` to literal string `'a'`. It then resolves the indexed type `JSX.IntrinsicElements['a']`, validating that `'href'` is an authentic property of anchor elements.

---

## 10. Best Practices & Design Guidelines

1. **Use Function Declarations for Generics**: Avoid TSX arrow function ambiguity.
2. **Always Use `Omit` for Custom Prop Overrides**: Avoid property type collisions.
3. **Default `as` to the Most Common Element**: E.g. `E extends ElementType = 'div'`.
4. **Use React 19 Native Ref Props**: Bypass legacy `forwardRef` generic limitations.

---

## 11. Real-World Case Study

### Design System Polymorphic `Text` Typography Component
A universal `Text` component supporting `p`, `span`, `h1`-`h6`, and `label` with type-safe semantic heading levels and truncated styles:

```typescript
import type { ElementType, ReactElement } from 'react';

type TextVariant = 'body' | 'caption' | 'heading-1' | 'heading-2';

export interface TextCustomProps {
  variant?: TextVariant;
  truncate?: boolean;
}

export type TextProps<E extends ElementType> = PolymorphicComponentPropWithRef<E, TextCustomProps>;

export function Text<E extends ElementType = 'p'>({
  as,
  variant = 'body',
  truncate = false,
  className = '',
  children,
  ...rest
}: TextProps<E>): ReactElement {
  const Component = as || 'p';
  const classes = `text text-${variant} ${truncate ? 'truncate' : ''} ${className}`.trim();

  return (
    <Component className={classes} {...rest}>
      {children}
    </Component>
  );
}
```

---

## 12. Interview Questions

<details>
<summary>1. What is a Polymorphic Component in React?</summary>

A polymorphic component is a component that can render as different HTML tags or other React components via an `as` (or `component`) prop, while adapting its accepted props, attributes, and ref types to match the target element at compile time.
</details>

<details>
<summary>2. Why does `<T>` fail in `.tsx` files when declaring a generic arrow function component?</summary>

In TSX files, the compiler cannot distinguish `<T>` from an opening JSX tag for an element named `T`. To disambiguate, use a trailing comma `<T,>`, an extends clause `<T extends unknown>`, or use a standard `function MyComp<T>()` declaration.
</details>

<details>
<summary>3. Why must we use `Omit<ComponentPropsWithRef<E>, keyof P>` in polymorphic type definitions?</summary>

If your component props `P` define a property with the same name as an HTML attribute (e.g. `color` or `size`) but with a different type, simply intersecting them (`P & ComponentPropsWithRef<E>`) creates an incompatible type collision (`string & ('sm' | 'lg')`), yielding `never`. `Omit` removes the conflicting HTML attribute before intersection.
</details>

<details>
<summary>4. Why was `forwardRef` problematic with generic components in React 18?</summary>

`forwardRef`'s type definition does not preserve generic type parameters without complex higher-order function type assertions. React 19 resolves this by passing `ref` as a normal component prop, allowing generics to flow naturally.
</details>

<details>
<summary>5. How does generic type inference work when passing an array of objects to a generic `Table<T>`?</summary>

When `<Table data={users} />` is called, TypeScript's type checker infers `T` from the type of `users` (`User[]`), and propagates `T` to row render callbacks, columns, and key extractors without manual type annotations.
</details>

<details>
<summary>6. What is the difference between `React.ElementType` and `React.ComponentType`?</summary>

`React.ElementType` includes both native HTML element tag strings (`'div'`, `'span'`, `'a'`) and React component types. `React.ComponentType` only includes React function or class components.
</details>

<details>
<summary>7. How do you constrain a generic component to objects that have an `id`?</summary>

By specifying a generic constraint:
`function List<T extends { id: string | number }>(props: ListProps<T>)`
</details>

<details>
<summary>8. Can polymorphic components accept another custom React component in the `as` prop?</summary>

Yes, because `ElementType` accepts custom component identifiers: `<Button as={Link} to="/home" />`.
</details>

---

## 13. Exercises & Challenges

<details>
<summary>Exercise 1 (Easy): Generic Item Filter Component</summary>

**Problem:** Type a component `Filter<T>` that accepts an array of items `T[]` and a predicate `(item: T) => boolean` and renders matching count.

**Solution:**
```typescript
interface FilterProps<T> { items: T[]; predicate: (item: T) => boolean; }

export function Filter<T>({ items, predicate }: FilterProps<T>): React.JSX.Element {
  const count = items.filter(predicate).length;
  return <span>Matching: {count}</span>;
}
```
</details>

<details>
<summary>Exercise 2 (Easy): Trailing Comma Generic Arrow Function</summary>

**Problem:** Write a generic arrow function component in TSX that renders a wrapper `<div>` around item property `item.title`.

**Solution:**
```typescript
export const Card = <T extends { title: string },>({ item }: { item: T }) => {
  return <div className="card">{item.title}</div>;
};
```
</details>

<details>
<summary>Exercise 3 (Medium): Polymorphic Heading Component</summary>

**Problem:** Create a `Heading` component where `as` is constrained strictly to `'h1' | 'h2' | 'h3' | 'h4' | 'h5' | 'h6'`.

**Solution:**
```typescript
import type { ComponentPropsWithoutRef, ReactNode } from 'react';

type HeadingLevel = 'h1' | 'h2' | 'h3' | 'h4' | 'h5' | 'h6';

export type HeadingProps<L extends HeadingLevel = 'h1'> = {
  as?: L;
  children: ReactNode;
} & ComponentPropsWithoutRef<L>;

export function Heading<L extends HeadingLevel = 'h1'>({ as, children, ...rest }: HeadingProps<L>) {
  const Tag = as || 'h1';
  return <Tag {...rest}>{children}</Tag>;
}
```
</details>

<details>
<summary>Exercise 4 (Medium): Generic Key Extractor Table</summary>

**Problem:** Build a `GenericGrid<T>` component where each column key is constrained to `keyof T`.

**Solution:**
```typescript
interface GridProps<T> {
  rows: T[];
  displayKey: keyof T;
}

export function GenericGrid<T extends { id: string }>({ rows, displayKey }: GridProps<T>) {
  return (
    <div>
      {rows.map((r) => <div key={r.id}>{String(r[displayKey])}</div>)}
    </div>
  );
}
```
</details>

<details>
<summary>Exercise 5 (Hard): Fully Type-Safe Polymorphic Container with Ref</summary>

**Problem:** Implement a polymorphic `Box` component supporting arbitrary element tags, custom padding prop, and correct ref forwarding.

**Solution:**
```typescript
import type { ElementType, ReactElement } from 'react';

type BoxCustomProps = { padding?: number };

export type BoxProps<E extends ElementType> = PolymorphicComponentPropWithRef<E, BoxCustomProps>;

export function Box<E extends ElementType = 'div'>({
  as,
  padding = 0,
  style,
  ...rest
}: BoxProps<E>): ReactElement {
  const Component = as || 'div';
  return <Component style={{ padding, ...style }} {...rest} />;
}
```
</details>

<details>
<summary>Exercise 6 (Hard): Generic State-Preserving Accordion</summary>

**Problem:** Write a generic `Accordion<T>` component where items can be selected, maintaining the selected `T` item in state and passing it to an expanded render prop.

**Solution:**
```typescript
import { useState, type ReactNode } from 'react';

interface AccordionProps<T> {
  items: T[];
  renderHeader: (item: T) => ReactNode;
  renderBody: (item: T) => ReactNode;
  keyFn: (item: T) => string;
}

export function Accordion<T>({ items, renderHeader, renderBody, keyFn }: AccordionProps<T>) {
  const [selectedKey, setSelectedKey] = useState<string | null>(null);

  return (
    <div>
      {items.map((item) => {
        const k = keyFn(item);
        const isOpen = selectedKey === k;
        return (
          <div key={k}>
            <div onClick={() => setSelectedKey(isOpen ? null : k)}>
              {renderHeader(item)}
            </div>
            {isOpen && <div>{renderBody(item)}</div>}
          </div>
        );
      })}
    </div>
  );
}
```
</details>
