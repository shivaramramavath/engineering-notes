# 04 — TypeScript with React

How to add static types to React code: components and props, events, hooks, generics, and reusable type patterns. Good types catch bugs in your editor, document component APIs, and make refactors safer. This chapter assumes you already know React from the previous chapters; it adds types on top rather than re-teaching React.

Code examples throughout this chapter are **TSX** (TypeScript + JSX).

## Prerequisites

- The React fundamentals and hooks chapters ([`../01-fundamentals/`](../01-fundamentals/README.md), [`../03-hooks/`](../03-hooks/README.md))
- Basic TypeScript: primitive types, `interface` / `type`, union types, optional properties, and arrays. If generics are new, `03-generics.md` introduces them in a React context, but a quick pass through the TypeScript handbook helps.
- A project with `strict: true` ([`../00-setup/05-typescript-and-linting-setup.md`](../00-setup/05-typescript-and-linting-setup.md))

## What you'll be able to do after this chapter

- Type component props, children, and function props
- Type event handlers and form events correctly
- Type `useState`, `useReducer`, `useRef`, `useContext`, and custom hooks
- Write generic components and hooks
- Derive types from existing ones with utility types instead of duplicating them
- Model variants with discriminated unions and build polymorphic components

## Reading order

| # | File | What it covers | Prerequisite |
|---|------|----------------|--------------|
| 00 | [00-typing-components-and-props.md](./00-typing-components-and-props.md) | Props, children, defaults, extending HTML props | Basic TypeScript |
| 01 | [01-typing-events.md](./01-typing-events.md) | Event objects and handler types | 00 |
| 02 | [02-typing-hooks.md](./02-typing-hooks.md) | Typing the built-in hooks and custom hooks | 00 |
| 03 | [03-generics.md](./03-generics.md) | Generic components and hooks | 02 |
| 04 | [04-utility-types.md](./04-utility-types.md) | `Pick`, `Omit`, `ComponentProps`, and more | 03 |
| 05 | [05-advanced-typing-patterns.md](./05-advanced-typing-patterns.md) | Discriminated unions, polymorphic components, typed context | 04 |

## Principles used in this chapter

1. **Let TypeScript infer** whenever it can; annotate at boundaries (props, public hook APIs, exported functions).
2. **Don't fight `strict`.** Narrow types properly instead of using `any` or `!` casts.
3. **Make impossible states unrepresentable** with unions instead of optional flags.
4. **Derive, don't duplicate**: build new types from existing ones.
5. **Prefer `unknown` to `any`** when you truly don't know a type, and narrow it.

## Exercises

1. Type a `Button` component whose props extend the native `<button>` props and add a `variant` of `"primary" | "secondary"`.
2. Type a form's `onChange` and `onSubmit` handlers without using `any`.
3. Write `useLocalStorage<T>` with a generic and use it with two different types.
4. Type a context with a `null` default and a custom hook that throws outside the provider.
5. Build a generic `Select<T>` component that takes `options: T[]` and `getLabel: (item: T) => string`.
6. Model a `Notification` with a discriminated union (`success`, `error`, `loading`) so each variant has only its valid fields.

## Next

**[`../05-component-design/README.md`](../05-component-design/README.md)** covers designing reusable component APIs, building on these typing skills.
