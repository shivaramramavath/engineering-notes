# 04 — TypeScript with React

This module provides an exhaustive, production-grade guide to leveraging TypeScript with modern React. Covering TypeScript 5.7+ compiler features, `@types/react` 19.0+ types, the deprecation of `forwardRef`, native `ref` props, React 19 Form Actions, generics, utility types, and polymorphic component patterns.

---

## What you'll be able to do after this module

- **Type Components & Props Idiomatically**: Use `ComponentPropsWithoutRef`, `PropsWithChildren`, and discriminated union props without legacy boilerplate like `React.FC`.
- **Type Synthetic & Native DOM Events**: Master `React.MouseEvent`, `React.ChangeEvent<HTMLInputElement>`, and keyboard event handlers with strict target narrowing.
- **Type Built-in & Custom Hooks**: Type `useState`, `useReducer` state machines, `useRef`, `useSyncExternalStore`, and custom generic hooks with exact return type inference.
- **Implement Generic Components**: Build type-safe `<DataTable<T>>`, `<List<T>>`, and `<Select<T>>` components with inferred data structures.
- **Master React Utility Types**: Leverage `ComponentPropsWithRef`, `ElementRef`, `Awaited`, and `ReturnType` to build resilient type layers.
- **Modern React 19 Ref Typing**: Type refs directly as component props (`ref?: React.Ref<T>`) without `forwardRef`, and implement typed ref cleanup callbacks.
- **Type React 19 Form Actions**: Type `(formData: FormData) => void | Promise<void>` and `useActionState<State, Payload>`.
- **Author Type-Safe Polymorphic Components**: Build components that safely change underlying HTML elements via an `as` prop or modern `asChild` composition.

---

## Module Syllabus

| # | Note | Description | React & TS Version |
|---|------|-------------|:---:|
| 00 | [00-typing-components-and-props.md](./00-typing-components-and-props.md) | Component signatures, props interfaces, discriminated unions, `children` typing | React 19 / TS 5.7+ |
| 01 | [01-typing-events.md](./01-typing-events.md) | Form events, change events, keyboard/mouse events, event delegation | React 19 / TS 5.7+ |
| 02 | [02-typing-hooks.md](./02-typing-hooks.md) | Typing `useState`, `useReducer` disc unions, `useRef`, custom hook tuples | React 19 / TS 5.7+ |
| 03 | [03-generics.md](./03-generics.md) | Generic components, generic props, type parameters, default generic constraints | React 19 / TS 5.7+ |
| 04 | [04-utility-types.md](./04-utility-types.md) | `ComponentProps`, `ComponentPropsWithoutRef`, `ElementRef`, conditional types | React 19 / TS 5.7+ |
| 05 | [05-advanced-typing-patterns.md](./05-advanced-typing-patterns.md) | Compound components typing, context provider patterns, recursive tree typing | React 19 / TS 5.7+ |
| 06 | [06-typing-refs-and-props-in-19.md](./06-typing-refs-and-props-in-19.md) | React 19 native `ref` props, `forwardRef` deprecation, ref cleanup typing, Form Actions | React 19 / TS 5.7+ |
| 07 | [07-polymorphic-components.md](./07-polymorphic-components.md) | Type-safe `as` prop, `PolymorphicComponentPropsWithRef`, Radix `asChild` Slot pattern | React 19 / TS 5.7+ |

---

## Recommended `tsconfig.json` Configuration

For modern React 19 + Vite applications with TypeScript 5.7+:

```json
{
  "compilerOptions": {
    "target": "ES2022",
    "lib": ["DOM", "DOM.Iterable", "ES2022"],
    "module": "ESNext",
    "moduleResolution": "bundler",
    "jsx": "react-jsx",
    "strict": true,
    "noUncheckedIndexedAccess": true,
    "exactOptionalPropertyTypes": true,
    "noImplicitOverride": true,
    "skipLibCheck": true,
    "isolatedModules": true
  }
}
```

---

## Architectural Relationships & Mental Models

```
┌────────────────────────────────────────────────────────────────────────┐
│                   React + TypeScript Typing Stack                      │
├────────────────────────────────┬───────────────────────────────────────┤
│ Foundation Layer               │ Generic & Dynamic Layer               │
│ • ComponentPropsWithoutRef<T>  │ • Generic Components <T>              │
│ • Discriminated Union Props    │ • PolymorphicComponentProps<C, P>     │
│ • SyntheticEvent<T>            │ • Compound Component Contexts         │
├────────────────────────────────┼───────────────────────────────────────┤
│ React 19 Modern Layer          │ Hook & State Layer                    │
│ • Native Ref Props (no fwdRef) │ • useReducer Discriminated Actions    │
│ • Ref Cleanup callbacks        │ • useActionState<State, Payload>      │
│ • Form Action (FormData)       │ • useOptimistic Reducer Typing        │
└────────────────────────────────┴───────────────────────────────────────┘
```

---

## Boundary with Other Modules

- **Core Hooks**: Read [`03-hooks/`](../03-hooks/README.md) for detailed runtime behavior of all React hooks.
- **Component Architecture**: Read [`05-component-design/`](../05-component-design/README.md) for headless UI, render props, and compound components.
- **Forms & Validation**: Read [`06-forms/`](../06-forms/README.md) for schema validation with Zod and TanStack Form.
- **Global TypeScript Knowledge**: Cross-link to [`01-languages/typescript/`](../../01-languages/typescript/README.md) for language fundamentals, utility types, and compiler internals.

---

## Practical Exercises

1. **Discriminated Union Card**: Build an alert card that requires an `actionLabel` and `onAction` callback only when `variant="actionable"`.
2. **Type-Safe Autocomplete**: Build a `<Combobox<T>>` that accepts arbitrary item records and infers the item type in its `renderOption` callback.
3. **React 19 Ref Callback Observer**: Create a component with a typed ref callback returning a cleanup function to disconnect a `MutationObserver`.
4. **Typed Form Action with Zod**: Write a profile update action validating `FormData` with a typed return error structure via `useActionState`.
5. **Polymorphic Heading**: Implement a `<Heading as="h1" | "h2" | "h3">` ensuring valid heading attributes and ref binding.
6. **Slot-Based Button**: Implement an `asChild` button using the Slot pattern that passes down all button attributes to a custom anchor child.

---

## Next Steps

Advance to **[05-component-design/README.md](../05-component-design/README.md)** to master headless component patterns, compound components, and slots.
