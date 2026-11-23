# React Component Props, Children, and React 19 Typing in TypeScript

> Typing React components in TypeScript: React.FC vs plain functions, ReactNode vs ReactElement vs JSX.Element, PropsWithChildren pitfalls, Discriminated Union props, and React 19 compiler-first typing.

| Metadata | Details |
| :--- | :--- |
| **Category** | React and Frontend Typing |
| **Difficulty** | Intermediate |
| **Prerequisites** | [04-objects-and-interfaces/00-interfaces.md](../04-objects-and-interfaces/00-interfaces.md), [04-objects-and-interfaces/02-discriminated-unions.md](../04-objects-and-interfaces/02-discriminated-unions.md) |
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

In frontend engineering with React, passing invalid props between parent and child components has historically been a leading cause of runtime crashes (`Cannot read properties of undefined`). React introduced runtime `PropTypes` in early versions, but runtime validation requires executing the component in the browser, adds bundle size, and does not provide IDE autocompletion.

TypeScript transforms React development by validating prop shapes at compile time:
- **Zero-Cost Typing**: Types are stripped completely during compilation; zero runtime bundle overhead.
- **Autocompletion & Refactoring**: Renaming a prop instantly updates all usages across thousands of component files.
- **Impossible States**: Using discriminated union props to make conflicting prop combinations (e.g. `variant="icon"` without providing `icon`) impossible to author.

With the release of **React 19** and `@types/react@19`:
1. `React.FC` is deprecated for modern function components.
2. `ref` is passed as a standard prop (eliminating the need for `forwardRef`).
3. Return types are simplified with `React.JSX.Element` and native `Promise<ReactNode>` for React Server Components (RSC).

```
+-----------------------------------------------------------+
|                   React 19 Component Model                |
|                                                           |
|   interface ButtonProps {                                 |
|     label: string;                                        |
|     ref?: React.Ref<HTMLButtonElement>; // Native in R19  |
|   }                                                       |
|                                                           |
|   export function Button({ label, ref }: ButtonProps) {   |
|     return <button ref={ref}>{label}</button>;            |
|   }                                                       |
+-----------------------------------------------------------+
```

---

## 2. Core Concepts

### 1. `ReactNode` vs `ReactElement` vs `JSX.Element`
Understanding React's hierarchy of return and child types:
- **`ReactNode`**: The broadest type. Anything renderable by React: `string`, `number`, `boolean`, `null`, `undefined`, `ReactElement`, or array of `ReactNode`. Use this for `children` props!
- **`ReactElement`**: An instantiated React virtual DOM object (`{ type, props, key }`). Does NOT include primitives like `string` or `number`.
- **`React.JSX.Element`**: Global JSX namespace alias for a `ReactElement` with `any` props and `any` type.

```
                  +--------------------------------+
                  |           ReactNode            |
                  | (Primitives, Elements, Arrays) |
                  +--------------------------------+
                                  ^
                                  | includes
                  +--------------------------------+
                  |          ReactElement          |
                  | (Object with type & props)     |
                  +--------------------------------+
                                  ^
                                  | alias
                  +--------------------------------+
                  |        React.JSX.Element       |
                  +--------------------------------+
```

### 2. Why Plain Functions Beat `React.FC`
In older TypeScript React code, `React.FC` (or `React.FunctionComponent`) was common:
```typescript
// Legacy Anti-Pattern:
const MyComponent: React.FC<Props> = (props) => { ... };
```
Problems with `React.FC`:
1. In React 17 and earlier, it automatically injected an implicit `children?: ReactNode` into every component, even components that should never accept children.
2. It breaks TypeScript generics: generic parameters on `React.FC<T>` are clumsy compared to plain generic functions `function MyComponent<T>(props: Props<T>)`.
3. It creates unneeded type indirection in error stack traces.

Modern idiom:
```typescript
export function MyComponent(props: Props): React.JSX.Element { ... }
```

### 3. Discriminated Union Props
Components that switch layouts based on a mode prop should strictly enforce mode-specific props:
```typescript
export type ModalProps =
  | { variant: 'alert'; message: string; onConfirm: () => void }
  | { variant: 'prompt'; message: string; defaultValue: string; onSubmit: (val: string) => void };
```

---

## 3. How It Works Internally

When you author JSX:
```typescript
<Card title="Hello" />
```
The TypeScript compiler's JSX transformer (`"jsx": "react-jsx"`) converts this syntax to:
```javascript
import { jsx as _jsx } from "react/jsx-runtime";
_jsx(Card, { title: "Hello" });
```
During type checking, TypeScript checks `_jsx(Component, props)`. It evaluates whether the object literal `{ title: "Hello" }` is assignable to the parameter type of `Component`. Excess Property Checks (EPC) run immediately, rejecting unknown props.

---

## 4. Syntax & API Reference

### Component Prop Type Utilities
```typescript
import type { ComponentProps, ComponentPropsWithoutRef, ComponentPropsWithRef, ReactNode } from 'react';

// Extract props of an HTML element
export type ButtonHtmlProps = ComponentPropsWithoutRef<'button'>;

// Explicit Children Props
export interface ContainerProps {
  children: ReactNode;
  className?: string;
}
```

---

## 5. Practical Examples

### Example 1: Basic Modern Function Component with Strict Props
```typescript
import type { ReactNode } from 'react';

export interface CardProps {
  title: string;
  subtitle?: string;
  children: ReactNode;
  footer?: ReactNode;
}

export function Card({ title, subtitle, children, footer }: CardProps): React.JSX.Element {
  return (
    <div className="card">
      <h3>{title}</h3>
      {subtitle && <h4>{subtitle}</h4>}
      <div className="card-body">{children}</div>
      {footer && <div className="card-footer">{footer}</div>}
    </div>
  );
}
```

### Example 2: Real-World Discriminated Union Button Component
```typescript
import type { ComponentPropsWithoutRef } from 'react';

type BaseProps = ComponentPropsWithoutRef<'button'> & {
  size?: 'sm' | 'md' | 'lg';
};

export type ButtonProps =
  | (BaseProps & {
      variant: 'text' | 'solid';
      children: React.ReactNode;
      icon?: never;
    })
  | (BaseProps & {
      variant: 'icon-only';
      icon: React.ReactElement;
      'aria-label': string; // Mandatory for accessibility!
      children?: never;
    });

export function ActionButton(props: ButtonProps): React.JSX.Element {
  if (props.variant === 'icon-only') {
    return <button {...props} className="btn-icon">{props.icon}</button>;
  }
  return <button {...props} className={`btn-${props.variant}`}>{props.children}</button>;
}
```

### Example 3: Edge Case - Extending Native HTML Attributes without Conflicting Props
```typescript
import type { ComponentPropsWithoutRef } from 'react';

export type CustomInputProps = Omit<ComponentPropsWithoutRef<'input'>, 'size'> & {
  size?: 'small' | 'medium' | 'large'; // Overriding native HTML 'size' (which is a number)
  label: string;
  errorMessage?: string;
};

export function CustomInput({ size = 'medium', label, errorMessage, ...rest }: CustomInputProps) {
  return (
    <label>
      <span>{label}</span>
      <input {...rest} className={`input-${size}`} />
      {errorMessage && <span className="error">{errorMessage}</span>}
    </label>
  );
}
```

### Example 4: Failure Case - Missing Mandatory Accessibility Prop on Icon Button
```typescript
// @ts-expect-error TS2322: Type '{ variant: "icon-only"; icon: Element; }' is not assignable to type 'ButtonProps'. Property 'aria-label' is missing.
const btn = <ActionButton variant="icon-only" icon={<svg />} />;
```

### Example 5: Idiomatic React 19 Native Ref Passing
```typescript
import type { Ref } from 'react';

export interface TextInputProps {
  label: string;
  ref?: Ref<HTMLInputElement>; // In React 19, ref is a regular prop!
}

export function TextInput({ label, ref }: TextInputProps): React.JSX.Element {
  return (
    <label>
      {label}
      <input ref={ref} type="text" />
    </label>
  );
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **`PropsWithChildren` Gotcha**:
   Using `PropsWithChildren<T>` makes `children` **optional** (`children?: ReactNode`). If your component *requires* children (e.g. an Accordion or Layout provider), `PropsWithChildren` will not enforce it! Always explicitly define `{ children: ReactNode }` when children are mandatory.
2. **Boolean Trap in JSX Children**:
   In React, `{false}`, `{null}`, and `{undefined}` render nothing, but `{0}` renders the text `"0"` on screen. Typing props as `boolean | undefined` prevents accidental `0` renders: `{count > 0 && <Badge />}` (renders `0` when `count === 0`!). Type count checks as boolean: `{Boolean(count) && <Badge />}`.
3. **`ComponentPropsWithoutRef` vs `ComponentProps`**:
   In React 18, `ComponentProps<'button'>` includes `ref: RefObject<HTMLButtonElement>`, which causes type mismatches when passing to components not wrapped in `forwardRef`. Always prefer `ComponentPropsWithoutRef<'element'>` in React 18.
4. **React Server Components (RSC) Async Return Types**:
   In Next.js App Router / React Server Components, server components can be `async` functions returning `Promise<ReactNode>`. React 18 client components will fail type checking if declared `async` because client components must return `ReactNode` synchronously.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Bad Code | Good Code | Compiler / Runtime Impact |
| :--- | :--- | :--- | :--- |
| **Using Deprecated `React.FC`** | `const App: React.FC<P> = ...` | `export function App(props: P)` | Complicates generics and error traces |
| **Implicit Optional Children** | `PropsWithChildren<{}>` | `{ children: ReactNode }` | Allows empty `<Layout />` without children |
| **`JSX.Element` as Children Type** | `children: JSX.Element` | `children: ReactNode` | Rejects plain strings, numbers, and fragments |
| **Overriding Native Prop Without Omit** | `type P = ButtonHTMLAttributes & { size: string }` | `type P = Omit<ButtonHTMLAttributes, 'size'> & { size: string }` | Conflicting type error: `string` not assignable to `number` |
| **Untyped Rest Spread** | `function Box(props: any)` | `ComponentPropsWithoutRef<'div'>` | Disables compiler checks on HTML attributes |
| **Using `any` for Event Handlers** | `onClick: (e: any) => void` | `onClick: (e: React.MouseEvent<HTMLButtonElement>) => void` | Unsafe property access (`e.target.val`) |

---

## 8. Performance & Type-Checker Complexity

- **Deep HTML Attribute Expansion**: `ComponentPropsWithoutRef<'div'>` includes over 250 standard HTML attributes, event handlers, and ARIA properties. In massive component libraries, avoid deeply nested mapped types over HTML attributes, as this can slow down TSServer autocomplete response times.
- **Type Erasure**: Component prop types are completely erased at compile time, adding zero runtime bytes to production bundles.

---

## 9. Compiler Internals

When TypeScript encounters a JSX opening tag `<MyComp />`:
1. It queries `JSX.IntrinsicElements` to see if the tag name is a known HTML element (e.g. `'div'`, `'button'`).
2. If the tag starts with a capital letter, the compiler treats it as a value identifier and looks up its function signature.
3. It checks `JSX.Element` as the valid return type constraint.
4. It resolves the first parameter of the component function and applies Excess Property Checks against the JSX attributes provided.

---

## 10. Best Practices & Design Guidelines

1. **Use Plain Functions**: Write `export function Component(props: Props)`.
2. **Require Children Explicitly**: Define `children: ReactNode` rather than using `PropsWithChildren`.
3. **Use Discriminated Unions for Mutually Exclusive Props**: Never use independent booleans (`isIconOnly`, `hasText`).
4. **Use `ComponentPropsWithoutRef` for HTML Wrappers**: Preserve standard HTML attributes like `aria-*`, `id`, `onClick`.

---

## 11. Real-World Case Study

### Enterprise Design System Alert Banner
A reusable Alert component supporting four distinct visual states with strictly typed action triggers:

```typescript
import type { ReactNode } from 'react';

type AlertSeverity = 'info' | 'success' | 'warning' | 'error';

export interface AlertBannerProps {
  severity: AlertSeverity;
  title: string;
  children: ReactNode;
  dismissible?: boolean;
  onDismiss?: () => void;
  action?: {
    label: string;
    onClick: () => void;
  };
}

export function AlertBanner({
  severity,
  title,
  children,
  dismissible,
  onDismiss,
  action
}: AlertBannerProps): React.JSX.Element {
  return (
    <div role="alert" className={`alert-banner alert-${severity}`}>
      <div className="alert-header">
        <strong>{title}</strong>
        {dismissible && (
          <button aria-label="Close" onClick={onDismiss} className="btn-close">×</button>
        )}
      </div>
      <div className="alert-body">{children}</div>
      {action && (
        <button onClick={action.onClick} className="alert-action">{action.label}</button>
      )}
    </div>
  );
}
```

---

## 12. Interview Questions

<details>
<summary>1. What is the difference between `ReactNode`, `ReactElement`, and `JSX.Element`?</summary>

- `ReactNode`: The union of all values that can be rendered in JSX (`string | number | boolean | null | undefined | ReactElement | Iterable<ReactNode>`).
- `ReactElement`: A concrete React virtual DOM object with `type` and `props`, created via `createElement` or JSX transpilation. It does not include primitives.
- `JSX.Element`: A TypeScript global interface representing a `ReactElement` with `any` props and component types.
</details>

<details>
<summary>2. Why has the React and TypeScript community moved away from `React.FC`?</summary>

`React.FC` historically provided an implicit optional `children` prop on every component, made generic component syntax verbose, prevented proper prop type hover diagnostics, and added unnecessary abstraction over plain JavaScript functions.
</details>

<details>
<summary>3. How does React 19 simplify `ref` handling compared to React 18?</summary>

In React 19, `ref` is passed as a standard prop to function components (`function MyInput({ ref, ...props }: Props)`). The `forwardRef` wrapper function is no longer required for simple ref forwarding.
</details>

<details>
<summary>4. Why should you avoid `PropsWithChildren<T>` when children are mandatory?</summary>

`PropsWithChildren<T>` defines `children?: ReactNode` as optional. If your component relies on children to function (such as an Accordion or Provider), the compiler will not warn consumers if they render `<Accordion />` with no children.
</details>

<details>
<summary>5. How do you override an existing HTML attribute type (e.g. changing `size` from number to union)?</summary>

By using `Omit`:
```typescript
type InputProps = Omit<ComponentPropsWithoutRef<'input'>, 'size'> & {
  size: 'sm' | 'md' | 'lg';
};
```
Without `Omit`, TypeScript creates an intersection `number & ('sm' | 'md' | 'lg')`, which evaluates to `never`.
</details>

<details>
<summary>6. How do you type a component that accepts a component as a prop (render prop or icon)?</summary>

- If it accepts an already instantiated JSX element: `icon: React.ReactElement`.
- If it accepts a component type to be rendered by the child: `component: React.ComponentType<P>`.
</details>

<details>
<summary>7. What is the difference between `ComponentPropsWithRef` and `ComponentPropsWithoutRef`?</summary>

`ComponentPropsWithRef` includes the `ref` prop corresponding to the underlying element, whereas `ComponentPropsWithoutRef` strips the `ref` prop.
</details>

<details>
<summary>8. How do you write a generic component in TypeScript React?</summary>

By declaring generic type parameters before the parameter list:
```typescript
export function List<T>({ items, renderItem }: { items: T[]; renderItem: (item: T) => ReactNode }) {
  return <ul>{items.map(renderItem)}</ul>;
}
```
</details>

---

## 13. Exercises & Challenges

<details>
<summary>Exercise 1 (Easy): Basic Card Component</summary>

**Problem:** Type a `Badge` component that accepts a mandatory `label: string` and optional `color: 'red' | 'green' | 'blue'`.

**Solution:**
```typescript
interface BadgeProps {
  label: string;
  color?: 'red' | 'green' | 'blue';
}

export function Badge({ label, color = 'blue' }: BadgeProps): React.JSX.Element {
  return <span className={`badge badge-${color}`}>{label}</span>;
}
```
</details>

<details>
<summary>Exercise 2 (Easy): Mandatory Children Wrapper</summary>

**Problem:** Create a `StrictContainerProps` interface where `children` is mandatory and cannot be undefined.

**Solution:**
```typescript
import type { ReactNode } from 'react';

export interface StrictContainerProps {
  children: ReactNode;
}
```
</details>

<details>
<summary>Exercise 3 (Medium): HTML Button Wrapper with Loading Spinner</summary>

**Problem:** Create a `LoadingButton` component that extends all native `<button>` attributes, adding an `isLoading?: boolean` prop that disables the button and renders a spinner.

**Solution:**
```typescript
import type { ComponentPropsWithoutRef } from 'react';

export interface LoadingButtonProps extends ComponentPropsWithoutRef<'button'> {
  isLoading?: boolean;
}

export function LoadingButton({ isLoading, disabled, children, ...rest }: LoadingButtonProps) {
  return (
    <button disabled={disabled || isLoading} {...rest}>
      {isLoading ? <span className="spinner">Loading...</span> : children}
    </button>
  );
}
```
</details>

<details>
<summary>Exercise 4 (Medium): Mutually Exclusive Tab Props</summary>

**Problem:** Create a `Tab` component that requires either an `href` (renders an `<a>`) OR an `onClick` (renders a `<button>`), but never both.

**Solution:**
```typescript
type LinkTab = { href: string; onClick?: never; children: React.ReactNode };
type ButtonTab = { onClick: () => void; href?: never; children: React.ReactNode };
export type TabProps = LinkTab | ButtonTab;

export function Tab(props: TabProps) {
  if (props.href) return <a href={props.href}>{props.children}</a>;
  return <button onClick={props.onClick}>{props.children}</button>;
}
```
</details>

<details>
<summary>Exercise 5 (Hard): Generic Table Component with Column Extractors</summary>

**Problem:** Write a fully type-safe generic `Table<T>` component where columns are typed strictly against keys of `T`.

**Solution:**
```typescript
export interface Column<T> {
  key: keyof T;
  header: string;
}

export interface TableProps<T> {
  data: readonly T[];
  columns: readonly Column<T>[];
}

export function Table<T extends { id: string | number }>({ data, columns }: TableProps<T>) {
  return (
    <table>
      <thead>
        <tr>{columns.map((c) => <th key={String(c.key)}>{c.header}</th>)}</tr>
      </thead>
      <tbody>
        {data.map((row) => (
          <tr key={row.id}>
            {columns.map((c) => <td key={String(c.key)}>{String(row[c.key])}</td>)}
          </tr>
        ))}
      </tbody>
    </table>
  );
}
```
</details>

<details>
<summary>Exercise 6 (Hard): Polymorphic Component with Dynamic Element Tag</summary>

**Problem:** Create a polymorphic `Box` component that accepts an `as?: React.ElementType` prop and correctly types its props and ref based on the specified element.

**Solution:**
```typescript
import type { ElementType, ComponentPropsWithRef, ReactElement } from 'react';

type BoxProps<E extends ElementType> = {
  as?: E;
} & ComponentPropsWithRef<E>;

export function Box<E extends ElementType = 'div'>({ as, ...rest }: BoxProps<E>): ReactElement {
  const Component = as || 'div';
  return <Component {...rest} />;
}
```
</details>
