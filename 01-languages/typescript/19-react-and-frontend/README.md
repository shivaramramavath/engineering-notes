# 19 - React and Frontend

Typing React applications: components and their props, events, hooks, context, refs, forms, reusable generic components, the state-management layers (server state and client state), and the Next.js server/client split. TypeScript is a particularly good fit for React, because components are just functions from props to UI, and the compiler can check every call site.

The theme of this section is **putting the right type in the right place**. Most of React's typing comes from inference. The notes show where you must annotate (empty state, `null` refs, extracted handlers), where inference is not enough (generic and polymorphic components), and where types stop helping and runtime validation takes over (data from servers, forms, URLs, and storage).

> **Version note.** These notes assume React 18 or 19 with matching `@types/react`. Differences (React 19's `ref` as a prop, `use`, `useActionState`, changes to `useRef` and `RefObject` types) are called out where they matter. Library notes cover TanStack Query v5, Zustand v4/v5, and the Next.js App Router. Check the documentation for the versions you use.

## Prerequisites

- [03 Unions and Narrowing](../03-unions-and-narrowing/README.md): discriminated unions drive props, reducers, and query results
- [06 Generics](../06-generics/README.md): hooks, generic components, and typed stores
- [07 Utility Types](../07-utility-types/README.md): `Omit`, `Pick`, `ComponentProps` for wrapping elements
- [15 Runtime Validation](../15-runtime-validation/README.md): data entering the UI is untrusted
- [13 Compiler and tsconfig: recipes](../13-compiler-and-tsconfig/05-tsconfig-recipes.md): `jsx`, `lib: DOM`, `moduleResolution: bundler`

## Notes in this section

| # | Note | Covers |
|---|---|---|
| 00 | [Component props and children](./00-component-props-and-children.md) | Props types, `children` and `ReactNode`, wrapping HTML elements, prop unions, callbacks |
| 01 | [Event types](./01-event-types.md) | `ChangeEvent`/`MouseEvent`/`FormEvent`, `target` vs `currentTarget`, native listeners |
| 02 | [Hooks](./02-hooks.md) | `useState`, `useReducer`, `useRef`, effects, memoization, writing generic custom hooks |
| 03 | [Context](./03-context.md) | The `undefined` default plus throwing hook, strict context factory, memoized values, split contexts |
| 04 | [Refs](./04-refs.md) | DOM refs vs value refs, callback refs, forwarding refs (18 vs 19), `useImperativeHandle` |
| 05 | [Forms](./05-forms.md) | Controlled and uncontrolled inputs, typed form state, schema validation, form libraries, form actions |
| 06 | [Generic and polymorphic components](./06-generic-and-polymorphic-components.md) | `List<T>`, constraints, `keyof` columns, `as` props, HOCs, compound components |
| 07 | [Server state with TanStack Query](./07-server-state-tanstack-query.md) | Typed queries and mutations, keys, `queryOptions`, error typing, optimistic updates |
| 08 | [Client state with Zustand](./08-client-state-zustand.md) | Typed stores, selectors and re-renders, middleware, slices, testing |
| 09 | [Next.js](./09-nextjs.md) | Server vs client components, serializable props, page and route typing, server actions, env vars |

Read 00 to 02 first. 03 to 06 can follow in any order. 07 and 08 are about state and are best read together. 09 builds on nearly everything else.

## Where does this state belong?

| State | Where it lives | Note |
|---|---|---|
| A text input's draft, a toggle in one component | `useState` in that component | [02](./02-hooks.md), [05](./05-forms.md) |
| Complex local transitions | `useReducer` with a union of actions | [02](./02-hooks.md) |
| A value many components read, changing rarely (theme, user, API client) | context | [03](./03-context.md) |
| Frequently changing shared client state (cart, UI layout) | a store with selectors | [08](./08-client-state-zustand.md) |
| Data from the server | a query cache | [07](./07-server-state-tanstack-query.md) |
| Filters, pagination, selected tab | the URL | [09](./09-nextjs.md) (and your router) |
| Form values | form state or a form library, validated with a schema | [05](./05-forms.md) |

## Which note answers my question?

| Question | Go to |
|---|---|
| How do I type `children`? | [00](./00-component-props-and-children.md) |
| How do I make a button component that accepts all native button props? | [00](./00-component-props-and-children.md) |
| How do I make invalid prop combinations a compile error? | [00](./00-component-props-and-children.md) |
| What is the type of the `onChange` event? Why is `e.target.value` an error? | [01](./01-event-types.md) |
| Why is my `useState([])` typed `never[]`? | [02](./02-hooks.md) |
| How do I avoid `undefined` from `useContext`? | [03](./03-context.md) |
| How do I type `useRef` for an input? | [04](./04-refs.md) |
| How do I forward a ref in React 18 and 19? | [04](./04-refs.md) |
| How do I validate a form with Zod? | [05](./05-forms.md) |
| How do I write `<List<T>>` or a component with an `as` prop? | [06](./06-generic-and-polymorphic-components.md) |
| How do I fetch data in a typed, cached way? | [07](./07-server-state-tanstack-query.md) |
| Why does my selector cause infinite re-renders? | [08](./08-client-state-zustand.md) |
| Can I pass this function from a server component? | [09](./09-nextjs.md) |
| How do I type `params` in an App Router page? | [09](./09-nextjs.md) |

## Ideas that recur across the section

- **Type the boundaries, infer the middle.** Annotate props, public hooks, and context values. Let local variables and inline handlers infer.
- **Model states as unions.** Query results, reducers, and component variants are discriminated unions that make impossible states unrepresentable.
- **Reuse existing types.** `ComponentProps`, `ReturnType`, `Awaited`, and derived `z.infer` types stay in sync with their source.
- **Assertions hide at the edges.** DOM values, polymorphic JSX, and `forwardRef` sometimes need a contained cast. Keep it in one place with a comment.
- **Data from outside is untrusted.** Server responses, form input, URL params, and storage need runtime validation, however well typed the component is.
- **Put state in the right layer.** Server state, URL state, local state, and shared client state each have a better tool than "one global store".
- **The server/client boundary is a serialization boundary.** Props, action arguments, and return values must be plain data.

## Related sections

- [04 Objects and Interfaces](../04-objects-and-interfaces/README.md): structural typing behind props
- [09 Declaration Files: global and module augmentation](../09-declaration-files/02-global-and-module-augmentation.md): registering default error types
- [12 Async and Iteration](../12-async-and-iteration/README.md): effects, cancellation, races
- [16 Type-Safe APIs](../16-type-safe-apis/README.md): the typed client and contracts these components consume
- [17 Design Patterns: state machines](../17-design-patterns/06-state-machines.md) and [dependency injection](../17-design-patterns/05-dependency-injection.md)
- [18 Testing and Debugging](../18-testing-and-debugging/README.md): testing components, hooks, and stores
- [21 Production Tooling](../21-production-tooling/README.md): bundling, linting, deployment
- [22 Performance](../22-performance/README.md): re-renders, bundle size, type-check speed
- [25 Real-World Patterns: state management](../25-real-world-patterns/02-state-management.md)
- [26 Projects: React dashboard](../26-projects/05-react-dashboard/README.md) and [fullstack application](../26-projects/07-fullstack-application/README.md)
- [28 Cheatsheets: React with TypeScript](../28-cheatsheets/05-react-typescript.md)

## Next

[20 Node.js Backend](../20-nodejs-backend/README.md)
