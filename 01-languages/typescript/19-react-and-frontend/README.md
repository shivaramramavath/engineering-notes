# Module 19: React and Frontend Typing

> Comprehensive curriculum for TypeScript integration in modern React ecosystems, covering React 19, Server Components, advanced prop patterns, forms, server state, client state, and Next.js.

## Overview
This module explores the complete typing angle of modern React development. It emphasizes compile-time safety, zero-cost abstractions, React 19 compiler-first patterns, and robust full-stack state management.

## Module Structure

| Note | Title | Description | Baseline |
| :--- | :--- | :--- | :--- |
| **[00-component-props-and-children.md](./00-component-props-and-children.md)** | Props & Children | Function components, ReactNode vs ReactElement, discriminated union props, React 19. | React 19 / TS 5.0+ |
| **[01-event-types.md](./01-event-types.md)** | Event Types | SyntheticEvent, ChangeEvent, FormEvent, MouseEvent, target vs currentTarget. | React 19 / TS 5.0+ |
| **[02-hooks.md](./02-hooks.md)** | React Hooks | useState inference, useReducer discriminated unions, React 19 useActionState and use(). | React 19 / TS 5.0+ |
| **[03-context.md](./03-context.md)** | Context API | Guarded custom hook patterns, React 19 `<Context>` providers, state/dispatch splitting. | React 19 / TS 5.0+ |
| **[04-refs.md](./04-refs.md)** | Refs & Handles | DOM refs, mutable containers, useImperativeHandle, React 19 native ref passing. | React 19 / TS 5.0+ |
| **[05-forms.md](./05-forms.md)** | Forms & Actions | React Hook Form with Zod validation, React 19 Server Actions, useFormStatus. | React 19 / TS 5.0+ |
| **[06-generic-and-polymorphic-components.md](./06-generic-and-polymorphic-components.md)** | Generic & Polymorphic | Generic lists, polymorphic `as` prop with ComponentPropsWithRef, design system typing. | React 19 / TS 5.0+ |
| **[07-server-state-tanstack-query.md](./07-server-state-tanstack-query.md)** | TanStack Query v5 | queryOptions helper, Query Key Factories, optimistic updates, and infinite queries. | TanStack Query v5 |
| **[08-client-state-zustand.md](./08-client-state-zustand.md)** | Zustand State | Curried `create` pattern, modular slices, atomic selectors, and middleware typing. | Zustand v5 |
| **[09-nextjs.md](./09-nextjs.md)** | Next.js App Router | Server vs Client Components, Next.js 15 async params, generateMetadata, route handlers. | Next.js 15 / RSC |

## Learning Path & Prerequisites
- Complete **[04-objects-and-interfaces](../04-objects-and-interfaces/README.md)** and **[06-generics](../06-generics/README.md)** before this module.
- After completing this module, proceed to **[20-nodejs-backend](../20-nodejs-backend/README.md)**.
