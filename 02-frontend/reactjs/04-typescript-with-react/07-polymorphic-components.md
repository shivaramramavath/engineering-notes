# Type-Safe Polymorphic Components in React & TypeScript

- **Topic**: `04-typescript-with-react`
- **React version**: React 19+ (TypeScript 5.7+)
- **Tested with**: React 19.0.0, Vite 6.0, TypeScript 5.7, `@types/react` 19.0.0
- **Bundle / compiler notes**: Advanced compile-time typing pattern; zero runtime bundle overhead; contrasts the classic `as` prop with modern `asChild` slot patterns.

---

## 1. Concept Overview

In modern design systems, UI components frequently need to alter their underlying rendered HTML element or host component while preserving their visual styles and behaviors. For example:
- A `<Button>` might render as an HTML `<button>` by default, but render as an `<a>` anchor tag when configured with an `href`, or as a framework router `<Link>` component.
- A `<Typography>` component might render as an `<h1>`, `<h2>`, `<p>`, or `<span>` depending on semantic heading hierarchy.

This capability is known as **polymorphism**, typically exposed via an `as` prop (e.g., `<Button as="a" href="/login">`).

The core TypeScript challenge with polymorphic components is ensuring **strict compile-time type safety across varying tags**:
1. When `as="button"`, the component must allow button attributes (`type="submit"`, `disabled`), and **disallow** anchor attributes (`href`).
2. When `as="a"`, the component must allow anchor attributes (`href`, `target`), and **disallow** button attributes (`type`).
3. The component's `ref` prop must dynamically match the target element type (`HTMLButtonElement` vs `HTMLAnchorElement`).

```
Polymorphic Type Narrowing:
┌────────────────────────────────────────────────────────┐
│ <Button as="button" type="submit" disabled />          │ ✅ Valid HTMLButtonElement
└────────────────────────────────────────────────────────┘
                           ▲
                           │ Type adjusts dynamically based on `as` generic!
                           ▼
┌────────────────────────────────────────────────────────┐
│ <Button as="a" href="/dashboard" target="_blank" />    │ ✅ Valid HTMLAnchorElement
└────────────────────────────────────────────────────────┘
                           ▲
                           │ TypeScript catches invalid props!
                           ▼
┌────────────────────────────────────────────────────────┐
│ <Button as="a" href="/dashboard" disabled />           │ ❌ TypeError: 'disabled' does
│                                                        │    not exist on <a> tag!
└────────────────────────────────────────────────────────┘
```

### When to Use
- Building design system primitives: Buttons, Typography, Containers, Badges, Links.
- Allowing consumers to swap semantic tags for SEO and accessibility while retaining styles.

### When NOT to Use
- **Simple Styling Variants**: If the HTML tag never changes, use standard props (e.g. `variant="secondary"`).
- **Deep Component Trees**: Excessive generic polymorphism can degrade TypeScript compiler performance. Modern libraries often favor the `asChild` composition pattern (Slot pattern popularized by Radix UI).

---

## 2. Key Mechanics & Architecture

### The Type Architecture
To build a fully type-safe polymorphic component in TypeScript, we construct a generic utility type that:
1. Accepts a generic component type `C extends React.ElementType`.
2. Merges custom component props with `ComponentPropsWithoutRef<C>`.
3. Omits conflicting prop names to avoid duplicate declaration collisions.
4. Correctly types the `ref` prop as `ComponentPropsWithRef<C>['ref']`.

```ts
import type { ElementType, ComponentPropsWithRef, ComponentPropsWithoutRef, PropsWithChildren } from 'react';

// 1. The `as` prop definition
type AsProp<C extends ElementType> = {
  as?: C;
};

// 2. Keys to omit from native HTML attributes to prevent property collisions
type PropsToOmit<C extends ElementType, P> = keyof (AsProp<C> & P);

// 3. Polymorphic Props without Ref
export type PolymorphicComponentProps<
  C extends ElementType,
  Props = {}
> = PropsWithChildren<Props & AsProp<C>> &
  Omit<ComponentPropsWithoutRef<C>, PropsToOmit<C, Props>>;

// 4. Polymorphic Ref type
export type PolymorphicRef<C extends ElementType> = ComponentPropsWithRef<C>['ref'];

// 5. Complete Polymorphic Props with Ref (React 19 native ref prop)
export type PolymorphicComponentPropsWithRef<
  C extends ElementType,
  Props = {}
> = PolymorphicComponentProps<C, Props> & {
  ref?: PolymorphicRef<C>;
};
```

### Technical Characteristics Table
| Property / Feature | Behavior / Specification |
| :--- | :--- |
| **Generic Parameter** | `C extends React.ElementType` (defaults to default tag, e.g. `'button'`) |
| **Attribute Forwarding** | `ComponentPropsWithoutRef<C>` dynamically binds HTML attributes |
| **Prop Collision Protection**| `Omit<ComponentPropsWithoutRef<C>, PropsToOmit<C, Props>>` |
| **React 19 Ref Handling** | Directly accepts `ref?: PolymorphicRef<C>` in props |
| **Alternative Pattern** | Radix UI `asChild` Slot pattern (delegates via cloneElement) |
| **TS Compiler Overhead** | Moderate; keep prop unions flat to avoid compiler slowdown |

---

## 3. Real-World Code Examples

### Example 1: Basic / Foundational Pattern
A polymorphic `Text` component supporting semantic tags (`p`, `h1`, `h2`, `span`, `label`).

```tsx
import React, { type ElementType, type ReactNode } from 'react';
import type { PolymorphicComponentPropsWithRef } from './polymorphic';

interface TextOwnProps {
  variant?: 'body' | 'caption' | 'title';
  children: ReactNode;
}

const DEFAULT_TEXT_TAG = 'p' as const;

export function Text<C extends ElementType = typeof DEFAULT_TEXT_TAG>({
  as,
  variant = 'body',
  children,
  className,
  ref,
  ...rest
}: PolymorphicComponentPropsWithRef<C, TextOwnProps>) {
  const Component = as || DEFAULT_TEXT_TAG;

  const variantStyles = {
    title: 'text-xl font-bold text-zinc-900',
    body: 'text-sm text-zinc-700',
    caption: 'text-xs text-zinc-400',
  }[variant];

  return (
    <Component ref={ref} className={`${variantStyles} ${className || ''}`} {...rest}>
      {children}
    </Component>
  );
}

// Usage:
export function Usage() {
  return (
    <div>
      {/* Defaults to <p> */}
      <Text>Standard paragraph body text</Text>

      {/* Polymorphically renders as <h1> with h1 attributes */}
      <Text as="h1" variant="title">
        Main Page Title
      </Text>

      {/* Polymorphically renders as <label> with htmlFor */}
      <Text as="label" htmlFor="user-field" variant="caption">
        Input Label
      </Text>
    </div>
  );
}
```
Output:
```html
<p class="text-sm text-zinc-700 ">Standard paragraph body text</p>
<h1 class="text-xl font-bold text-zinc-900 ">Main Page Title</h1>
<label for="user-field" class="text-xs text-zinc-400 ">Input Label</label>
```

### Example 2: Production / Real-World Integration
Polymorphic Design System `<Button>` component supporting native `<button>`, anchor `<a>`, or custom Router `<Link>` with type-safe `ref`.

```tsx
import React, { type ElementType } from 'react';
import type { PolymorphicComponentPropsWithRef, PolymorphicRef } from './polymorphic';

interface ButtonOwnProps {
  variant?: 'primary' | 'secondary' | 'outline';
  size?: 'sm' | 'md' | 'lg';
}

const DEFAULT_BUTTON_TAG = 'button' as const;

export function Button<C extends ElementType = typeof DEFAULT_BUTTON_TAG>({
  as,
  variant = 'primary',
  size = 'md',
  className,
  children,
  ref,
  ...props
}: PolymorphicComponentPropsWithRef<C, ButtonOwnProps>) {
  const Component = as || DEFAULT_BUTTON_TAG;

  const styles = [
    'inline-flex items-center justify-center font-medium rounded-md transition-colors',
    variant === 'primary' && 'bg-blue-600 text-white hover:bg-blue-700',
    variant === 'secondary' && 'bg-zinc-100 text-zinc-800 hover:bg-zinc-200',
    variant === 'outline' && 'border border-zinc-300 text-zinc-700 hover:bg-zinc-50',
    size === 'sm' && 'px-2.5 py-1 text-xs',
    size === 'md' && 'px-4 py-2 text-sm',
    size === 'lg' && 'px-6 py-3 text-base',
    className,
  ].filter(Boolean).join(' ');

  return (
    <Component ref={ref} className={styles} {...props}>
      {children}
    </Component>
  );
}

// Testing Type Safety:
export function ButtonDemo() {
  const btnRef = React.useRef<HTMLButtonElement>(null);
  const anchorRef = React.useRef<HTMLAnchorElement>(null);

  return (
    <div className="flex gap-3">
      {/* 1. As HTML button */}
      <Button ref={btnRef} type="submit" variant="primary">
        Submit Order
      </Button>

      {/* 2. As Anchor link */}
      <Button as="a" ref={anchorRef} href="https://example.com" target="_blank" variant="secondary">
        External Link
      </Button>
    </div>
  );
}
```
Output:
```
// Renders <button type="submit"> and <a href="..."> with identical design system button styling.
```

### Example 3: Edge Case / Routing Library `<Link>` Integration
Passing a custom React component (such as React Router `<Link>` or Next.js `<Link>`) into the `as` prop.

```tsx
import React from 'react';
import { Button } from './Button';

// Mock Router Link component
interface RouterLinkProps {
  to: string;
  replace?: boolean;
  children: React.ReactNode;
  className?: string;
}

function RouterLink({ to, replace, children, className }: RouterLinkProps) {
  return (
    <a href={to} data-replace={replace} className={className}>
      {children}
    </a>
  );
}

export function NavigationBar() {
  return (
    <nav className="p-4 flex gap-4">
      {/* Polymorphically renders RouterLink with typed `to` prop! */}
      <Button as={RouterLink} to="/settings" variant="outline">
        Account Settings
      </Button>
    </div>
  );
}
```
Output:
```
// TypeScript verifies that `to="/settings"` is present and valid for RouterLink.
```

### Example 4: Failure Mode / Pitfall & Remediation
**Pitfall**: TypeScript prop collision when `as` element native prop names conflict with component custom props.

```tsx
// ❌ WRONG: Naive intersection without omitting collisions causes `never` types!
type NaivePolymorphicProps<C extends React.ElementType, P> = P &
  React.ComponentPropsWithoutRef<C> & { as?: C };

interface CardProps {
  // Suppose custom component defines title as a ReactNode:
  title: React.ReactNode;
}

// ⚠️ BUG: Native HTML attributes also have `title?: string`.
// ReactNode & string resolves to an impossible or conflicting type!
// When consumer passes title="Hello", TypeScript may throw:
// Type 'ReactNode' is not assignable to type 'string'.

// ✅ REMEDIATION: Always use Omit with PropsToOmit!
type SafePolymorphicProps<C extends React.ElementType, P> = P &
  Omit<React.ComponentPropsWithoutRef<C>, keyof P | 'as'> & { as?: C };
```
Output:
```
// SafePolymorphicProps guarantees that component custom props strictly take precedence.
```

### Example 5: Idiomatic Modern React 19 Alternative: Radix UI `asChild` Pattern
The modern industry alternative to `as` props: the `asChild` Slot pattern.

```tsx
import React, { cloneElement, isValidElement, type ReactElement } from 'react';

export interface SlotProps extends React.HTMLAttributes<HTMLElement> {
  children?: React.ReactNode;
}

export function Slot({ children, ...props }: SlotProps) {
  if (isValidElement(children)) {
    const child = children as ReactElement<any>;
    return cloneElement(child, {
      ...props,
      ...child.props,
      className: [props.className, child.props.className].filter(Boolean).join(' '),
      style: { ...props.style, ...child.props.style },
    });
  }
  return null;
}

// Component using asChild:
export function SlotButton({
  asChild,
  className,
  children,
  ...props
}: { asChild?: boolean; className?: string; children: React.ReactNode } & React.ButtonHTMLAttributes<HTMLButtonElement>) {
  const Comp = asChild ? Slot : 'button';

  return (
    <Comp className={`px-4 py-2 bg-blue-600 text-white rounded font-medium ${className || ''}`} {...props}>
      {children}
    </Comp>
  );
}

// Usage with asChild: zero complex generics required!
export function SlotDemo() {
  return (
    <SlotButton asChild>
      <a href="/dashboard">Direct Anchor Link</a>
    </SlotButton>
  );
}
```
Output:
```html
<a href="/dashboard" class="px-4 py-2 bg-blue-600 text-white rounded font-medium ">Direct Anchor Link</a>
```

---

## 4. Common Anti-Patterns & Gotchas

| Bad Practice | Root Cause | Clean Solution |
| :--- | :--- | :--- |
| **Omitting `Omit` in polymorphic prop definitions** | Property name collisions (e.g., `title`, `color`, `onClick`) result in `never` types. | Always omit custom prop keys from `ComponentPropsWithoutRef<C>`. |
| **Typing `as` as `string` (`as?: string`)** | Destroys type safety; TypeScript cannot infer specific tag attributes. | Constrain generic: `C extends React.ElementType`. |
| **Forgetting to set a default tag generic** | Users must write `<Button<'button'>>` manually on every call. | Provide default generic: `<C extends ElementType = 'button'>`. |
| **Type assertion `as any` on `Component`** | Masking typing bugs with `any` loses ref and prop validation. | Use `const Component = as \|\| 'button'` with proper typing. |
| **Deep polymorphic nesting across 5+ layers** | Exploding TypeScript type-check times (compiler slowdowns). | Adopt the `asChild` composition pattern for deep layouts. |
| **Passing incompatible refs** | Passing an anchor ref to a component rendering as `<button>`. | Use `PolymorphicRef<C>` to enforce matching element types. |

---

## 5. React Internals: Beneath the Abstraction

Inside TypeScript and React's JSX Transform:
1. `React.ElementType` is defined as `keyof JSX.IntrinsicElements | React.ComponentType<any>`.
2. When TypeScript evaluates `<Component {...props} />`:
   - It checks whether `Component` is a string (intrinsic element like `'div'`) or a function/class component.
   - For intrinsic strings, it looks up the matching HTML interface in `JSX.IntrinsicElements[C]`.
   - It validates that every attribute passed matches the property descriptors of that specific DOM element.
3. At runtime, React's `jsx()` factory simply executes:
   ```js
   return {
     $$typeof: Symbol.for('react.transitional.element'),
     type: Component, // Passes string 'button' or component function directly
     props: props,
     key: key,
     ref: ref
   };
   ```

---

## 6. Performance, Bundling & React Compiler

- **Runtime Cost**: Exactly zero bytes added for TypeScript generics.
- **Compilation Speed**: Highly complex conditional polymorphic types can increase `tsc` build time. Using `PropsToOmit` with shallow keys minimizes compiler burden.
- **React Compiler**: The React Compiler handles polymorphic components without issues, auto-memoizing the returned JSX elements based on passed props.

---

## 7. Accessibility (a11y) & Usability

Polymorphism is critical for accessible semantic HTML:
- **Buttons vs Links**: If clicking an element navigates to a new URL, it **must** render as an `<a>` tag with `href`. If it triggers a state change or form action, it **must** render as a `<button>` tag. Polymorphism enables maintaining identical visual styles while enforcing valid semantic markup.
- **Screen Reader Headings**: Headings must strictly follow hierarchical levels (`<h1>` -> `<h2>` -> `<h3>`) without skipping levels. Polymorphic typography components allow styling a visual "title" as an `<h2>` or `<h3>` to preserve correct document outline.

---

## 8. Testing & Verification Patterns

Testing polymorphic rendering and type validation with Vitest:

```tsx
import { render, screen } from '@testing-library/react';
import { describe, it, expect } from 'vitest';
import React, { useRef } from 'react';
import { Button } from './Button';

describe('Polymorphic Button', () => {
  it('renders as button by default', () => {
    render(<Button>Click Me</Button>);
    const el = screen.getByRole('button', { name: /click me/i });
    expect(el.tagName).toBe('BUTTON');
  });

  it('renders as anchor tag when as="a" is passed', () => {
    render(
      <Button as="a" href="https://vite.dev">
        Vite Docs
      </Button>
    );
    const el = screen.getByRole('link', { name: /vite docs/i });
    expect(el.tagName).toBe('A');
    expect(el).toHaveAttribute('href', 'https://vite.dev');
  });
});
```

---

## 9. Version History & Migration (React 17 -> 18 -> 19)

| React Version | Behavior / API Status | Migration Notes |
| :--- | :--- | :--- |
| **React 17 / 18** | Polymorphic components required complex `forwardRef` type intersections that often broke generic inference. | Required manual type cast wrappers like `(Button as PolymorphicForwardRefComponent)`. |
| **React 19** | `ref` is a native prop. Polymorphic props directly accept `ref?: PolymorphicRef<C>`. | Simplified types; eliminate `forwardRef` casts. |

---

## 10. Security & Hardening

1. **Open Redirects on Polymorphic Links**: When components render as `<a>` or router links based on dynamic data, validate that `href` does not contain dangerous protocols like `javascript:`.
2. **`rel="noopener noreferrer"`**: When `as="a"` and `target="_blank"` are used, ensure `rel="noopener noreferrer"` is automatically applied to prevent tab-nabbing vulnerabilities.

---

## 11. Real-World Case Study

### Radix UI vs Mantine vs Chakra Polymorphic Architectures

Major React UI libraries took different paths for polymorphism:
- **Mantine / Chakra UI**: Adopted the generic `as` prop (`createPolymorphicComponent`). Provides great developer ergonomics for simple swaps, but introduces compile-time type-check delays at scale.
- **Radix UI / shadcn/ui**: Adopted the `asChild` composition pattern (`@radix-ui/react-slot`). Eliminates generic TypeScript complexity by using component composition rather than prop polymorphism.

```tsx
// Comparison:
// Mantine / Chakra:
<Button as="a" href="/login">Login</Button>

// Radix / shadcn:
<Button asChild>
  <a href="/login">Login</a>
</Button>
```

---

## 12. Interview Q&A

<details>
<summary>1. What is a polymorphic component in React?</summary>

A polymorphic component is a component that can render as different underlying HTML elements or other React components via a designated prop (typically `as`), while retaining its visual styles, variants, and behaviors, and maintaining full TypeScript type safety for the selected element's attributes.
</details>

<details>
<summary>2. Why does a naive `props: P & ComponentPropsWithoutRef<C> & { as?: C }` fail in TypeScript?</summary>

Because native HTML attributes contain overlapping prop names with different types (e.g. `title?: string`, `color?: string`). When intersecting your custom component props with native element props without `Omit`, TypeScript creates an intersection like `ReactNode & string`, which can resolve to `never` or throw type errors when passing valid props.
</details>

<details>
<summary>3. What is the difference between the `as` prop pattern and Radix UI's `asChild` pattern?</summary>

The `as` prop pattern uses generic parameter typing (`<C extends ElementType>`) to dynamically narrow props based on the passed tag.
The `asChild` pattern uses composition via a `<Slot>` element: instead of passing a tag name string, the consumer passes the desired child element (e.g. `<a href="...">`) directly as children, and `<Slot>` merges the parent's props into that child. This avoids generic type explosion and simplifies TypeScript compilation.
</details>

<details>
<summary>4. How does React 19 simplify polymorphic component implementations?</summary>

In React 18, supporting `ref` on polymorphic components required complex `forwardRef` casts that frequently confused TypeScript's generic parameter solver. In React 19, `ref` is a regular prop, meaning `ref?: PolymorphicRef<C>` can be declared directly in the polymorphic props interface without wrapper functions.
</details>

<details>
<summary>5. How do you prevent a consumer from passing invalid HTML attributes to a polymorphic component?</summary>

By typing the component props with `Omit<ComponentPropsWithoutRef<C>, ...>`, TypeScript binds the accepted attributes directly to `C`. If `C` is `'button'`, passing `href` triggers a compile-time error: `Property 'href' does not exist on type...`.
</details>

<details>
<summary>6. What is `React.ElementType` in TypeScript?</summary>

`React.ElementType` is a TypeScript type representing anything that can be rendered as a React component: all intrinsic HTML tags (`'div'`, `'span'`, `'button'`), as well as functional and class components (`React.ComponentType<P>`).
</details>

<details>
<summary>7. Can polymorphic components accept custom third-party components like React Router's `<Link>`?</summary>

Yes. Because `C extends React.ElementType`, passing `as={Link}` causes TypeScript to look up `Link`'s prop interface (e.g. `to`, `replace`) and require those props on the polymorphic component.
</details>

<details>
<summary>8. Does using polymorphic components impact JavaScript bundle size?</summary>

No. Polymorphic types are purely compile-time TypeScript constructs. At runtime, the component simply computes `const Component = as || 'div'` and returns `<Component {...props} />`, which adds negligible code.
</details>

---

## 13. Practical Challenges & Exercises

<details>
<summary>Exercise 1 (Easy): Polymorphic Heading Component</summary>

**Specification**: Build a `Heading` component that defaults to `<h2>` but accepts `as="h1" | "h2" | "h3" | "h4"` with appropriate heading styling.

```tsx
import React, { type ElementType, type ReactNode } from 'react';
import type { PolymorphicComponentPropsWithRef } from './polymorphic';

type HeadingLevel = 'h1' | 'h2' | 'h3' | 'h4' | 'h5' | 'h6';

export function Heading<C extends HeadingLevel = 'h2'>({
  as,
  children,
  className,
  ref,
  ...props
}: PolymorphicComponentPropsWithRef<C, { children: ReactNode }>) {
  const Component = as || 'h2';
  return (
    <Component ref={ref} className={`font-bold tracking-tight ${className || ''}`} {...props}>
      {children}
    </Component>
  );
}
```
</details>

<details>
<summary>Exercise 2 (Easy): Polymorphic Container Component</summary>

**Specification**: Create a layout `Container` that defaults to `<div>` but can render as `<main>`, `<section>`, or `<article>`.

```tsx
import React, { type ElementType, type ReactNode } from 'react';
import type { PolymorphicComponentPropsWithRef } from './polymorphic';

type ContainerTag = 'div' | 'section' | 'article' | 'main' | 'aside';

export function Container<C extends ContainerTag = 'div'>({
  as,
  children,
  className,
  ref,
  ...props
}: PolymorphicComponentPropsWithRef<C, { children: ReactNode }>) {
  const Comp = as || 'div';
  return (
    <Comp ref={ref} className={`max-w-7xl mx-auto px-4 ${className || ''}`} {...props}>
      {children}
    </Comp>
  );
}
```
</details>

<details>
<summary>Exercise 3 (Medium): Polymorphic Badge with Icon Support</summary>

**Specification**: Build a `Badge` component that renders as `<span>` by default or `<a>` when clickable, with an optional icon slot.

```tsx
import React, { type ElementType, type ReactNode } from 'react';
import type { PolymorphicComponentPropsWithRef } from './polymorphic';

interface BadgeOwnProps {
  icon?: ReactNode;
  children: ReactNode;
}

export function Badge<C extends ElementType = 'span'>({
  as,
  icon,
  children,
  className,
  ref,
  ...props
}: PolymorphicComponentPropsWithRef<C, BadgeOwnProps>) {
  const Comp = as || 'span';

  return (
    <Comp
      ref={ref}
      className={`inline-flex items-center gap-1.5 px-2.5 py-0.5 rounded-full text-xs font-semibold bg-zinc-100 text-zinc-800 ${className || ''}`}
      {...props}
    >
      {icon && <span className="w-3 h-3">{icon}</span>}
      <span>{children}</span>
    </Comp>
  );
}
```
</details>

<details>
<summary>Exercise 4 (Medium): Polymorphic Card with Clickable Action Area</summary>

**Specification**: Implement a `Card` that can polymorphically render as an interactive `<button>` or a link `<a>`, retaining focus outline styles.

```tsx
import React, { type ElementType, type ReactNode } from 'react';
import type { PolymorphicComponentPropsWithRef } from './polymorphic';

export function InteractiveCard<C extends ElementType = 'div'>({
  as,
  children,
  className,
  ref,
  ...props
}: PolymorphicComponentPropsWithRef<C, { children: ReactNode }>) {
  const Comp = as || 'div';

  return (
    <Comp
      ref={ref}
      className={`block p-4 border rounded-xl shadow-sm hover:shadow-md transition-shadow text-left focus:outline-none focus:ring-2 focus:ring-blue-500 ${className || ''}`}
      {...props}
    >
      {children}
    </Comp>
  );
}
```
</details>

<details>
<summary>Exercise 5 (Hard): Fully Type-Safe Polymorphic Navigation List</summary>

**Specification**: Create a `NavList` compound component where `NavList.Item` can render as `<a>`, `<button>`, or framework `Link` with active indicators.

```tsx
import React, { type ElementType, type ReactNode } from 'react';
import type { PolymorphicComponentPropsWithRef } from './polymorphic';

interface NavItemOwnProps {
  isActive?: boolean;
  children: ReactNode;
}

function NavItem<C extends ElementType = 'a'>({
  as,
  isActive,
  children,
  className,
  ref,
  ...props
}: PolymorphicComponentPropsWithRef<C, NavItemOwnProps>) {
  const Comp = as || 'a';

  return (
    <li>
      <Comp
        ref={ref}
        aria-current={isActive ? 'page' : undefined}
        className={`px-3 py-1.5 text-sm font-medium rounded ${
          isActive ? 'bg-indigo-600 text-white' : 'text-zinc-600 hover:bg-zinc-100'
        } ${className || ''}`}
        {...props}
      >
        {children}
      </Comp>
    </li>
  );
}

export const NavList = {
  Root: ({ children }: { children: ReactNode }) => <ul className="flex gap-2">{children}</ul>,
  Item: NavItem,
};
```
</details>

<details>
<summary>Exercise 6 (Hard): Headless Slot Implementation with Deep Prop Merging</summary>

**Specification**: Implement a production-grade `Slot` component that merges event handlers (e.g. `onClick`), class names, and forwarded refs between parent and child elements.

```tsx
import React, { cloneElement, isValidElement, type ReactNode, type HTMLAttributes } from 'react';

export function Slot({ children, ...props }: HTMLAttributes<HTMLElement> & { children?: ReactNode }) {
  if (!isValidElement(children)) return null;

  const child = children as React.ReactElement<any>;

  const mergedProps = {
    ...props,
    ...child.props,
    className: [props.className, child.props.className].filter(Boolean).join(' '),
    style: { ...props.style, ...child.props.style },
    onClick: (e: React.MouseEvent) => {
      props.onClick?.(e as any);
      child.props.onClick?.(e);
    },
  };

  return cloneElement(child, mergedProps);
}
```
</details>
