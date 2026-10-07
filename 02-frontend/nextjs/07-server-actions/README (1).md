# 07 · Server Actions

How to change data from the UI: forms, validation, cache refresh after writes, optimistic updates and error handling. Server Actions are the default way to mutate data in the App Router.

> Verified against the Next.js 16.4 documentation. React 19 APIs (`useActionState`, `useOptimistic`, `useFormStatus`) are used throughout.

## The mental model

```text
Server Components  ── read data ──►  UI
        ▲                              │ user submits a form / clicks
        │ re-render with fresh data    ▼
   cache invalidated  ◄──────  Server Action (POST): auth → validate → write → revalidate
```

Reads happen in Server Components ([05 Data Fetching](../05-data-fetching/README.md)). Writes happen in Server Actions. What links them is cache invalidation ([06 Caching](../06-caching/README.md)).

## Reading order

| # | Note | Covers |
|---|---|---|
| 00 | [Server Actions](./00-server-actions.md) | Creating and calling actions, how they work, security, config |
| 01 | [Forms](./01-forms.md) | `action`, `FormData`, `bind`, `useActionState`, `useFormStatus`, multiple buttons, uploads |
| 02 | [Validation](./02-validation.md) | Zod, `FormData` gotchas, returning field errors, authorization vs validation |
| 03 | [Mutations and Optimistic UI](./03-mutations-and-optimistic-ui.md) | The mutation flow, `updateTag` / `revalidatePath` / `refresh`, `useOptimistic` |
| 04 | [Action Errors](./04-action-errors.md) | Expected vs unexpected errors, `redirect` in `try/catch`, boundaries |

## The five rules

1. **An action is a public endpoint.** Authenticate and authorize inside every one.
2. **Validate on the server.** Browser checks are only UX.
3. **Return expected errors, throw unexpected ones.**
4. **Invalidate after writing**, then redirect (the redirect throws).
5. **Reads belong in Server Components.** Actions are dispatched one at a time.

## Quick checklist for a new action

- [ ] In a `"use server"` file (or inline in a Server Component)
- [ ] `async`, session checked first
- [ ] Input parsed with a schema; field errors returned
- [ ] Row looked up by ID **and** owner
- [ ] `updateTag` / `revalidatePath` / `refresh` called
- [ ] `redirect` outside `try`, after revalidation
- [ ] Return value is a small DTO, not a database record
- [ ] Button disabled while pending

## Next

[08 · Route Handlers and Proxy](../08-route-handlers-and-proxy/README.md): HTTP endpoints for external callers, and request interception with `proxy.ts`.
