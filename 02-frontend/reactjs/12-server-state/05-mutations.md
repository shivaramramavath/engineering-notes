# Mutations

Queries **read**. Mutations **write**: create, update, delete, and anything else with side effects (login, upload, "send invite"). `useMutation` wraps a write function with state tracking and lifecycle callbacks, and, crucially, gives you the hook for **updating the cache** afterwards.

## Basic usage

```tsx
import { useMutation, useQueryClient } from "@tanstack/react-query"

function NewProjectForm() {
  const queryClient = useQueryClient()

  const createProject = useMutation({
    mutationFn: (input: { name: string }) => projectsApi.create(input),
    onSuccess: () => {
      queryClient.invalidateQueries({ queryKey: projectKeys.lists() })
    },
  })

  return (
    <form onSubmit={(e) => {
      e.preventDefault()
      const name = new FormData(e.currentTarget).get("name") as string
      createProject.mutate({ name })
    }}>
      <input name="name" required />
      <button disabled={createProject.isPending}>
        {createProject.isPending ? "Creating…" : "Create"}
      </button>
      {createProject.isError && <p role="alert">{getErrorMessage(createProject.error)}</p>}
    </form>
  )
}
```

How it differs from `useQuery`:

| | `useQuery` | `useMutation` |
|---|---|---|
| Runs | Automatically, when mounted | **Only when you call** `mutate()` |
| Cached by key | Yes | No (not cached/shared) |
| Retries | 3 by default | **0** by default (writes aren't safely repeatable) |
| Purpose | Read | Write + update the cache |

## `mutate` vs `mutateAsync`

```ts
createProject.mutate(values)                        // fire and forget; errors handled via callbacks/state
const project = await createProject.mutateAsync(values)   // returns a promise (throws on failure)
```

- **`mutate`** is the default. Success and failure are handled in callbacks or `isError`. No unhandled rejections.
- **`mutateAsync`** when you need to **sequence** work after it: navigate to the new record, or integrate with a form library's `handleSubmit` (which awaits the handler). You must `try/catch` it, or an error becomes an unhandled promise rejection.

```tsx
async function onSubmit(values: FormValues) {
  try {
    const project = await createProject.mutateAsync(values)
    navigate(`/projects/${project.id}`)
  } catch (error) {
    // map server validation errors to fields
  }
}
```

## State

`isPending`, `isError`, `isSuccess`, `isIdle`, `data` (the response), `error`, `variables` (what was passed in), and `reset()` to clear it. In v5 the in-flight flag is **`isPending`** (v4 called it `isLoading`).

Use `isPending` to disable the submit button and prevent double submits. That's the cheap fix for the classic duplicate-record bug.

## Lifecycle callbacks

```ts
useMutation({
  mutationFn,
  onMutate: (variables) => { /* before the request; used for optimistic updates (06) */ },
  onSuccess: (data, variables) => { /* request succeeded */ },
  onError: (error, variables) => { /* request failed */ },
  onSettled: (data, error, variables) => { /* always, success or failure */ },
})
```

> Recent v5 releases add extra trailing arguments to these callbacks (such as a context object). The leading arguments above are stable; check the docs for your version's exact signature.

These can be defined **on the hook** or **on the `mutate` call**:

```ts
createProject.mutate(values, {
  onSuccess: (project) => navigate(`/projects/${project.id}`),
})
```

Rules of thumb:

- **Hook-level callbacks** → *data concerns*: invalidating and updating the cache. They **always run**, even if the component unmounted.
- **`mutate`-level callbacks** → *UI concerns*: navigation, closing a dialog, resetting a form. They **don't fire if the component has unmounted** before the request finished, which is usually what you want (don't navigate after the user already left).

## Updating the cache after a write

Three strategies, from simplest to most precise:

### 1. Invalidate (default)

```ts
onSuccess: () => queryClient.invalidateQueries({ queryKey: projectKeys.lists() })
```

Correct by construction: the server decides what the list looks like now. Costs one extra request.

### 2. Update from the response

When the mutation returns the full updated record, write it to the cache directly:

```ts
onSuccess: (updated) => {
  queryClient.setQueryData(projectKeys.detail(updated.id), updated)
  queryClient.invalidateQueries({ queryKey: projectKeys.lists() })   // lists may be sorted/filtered differently
}
```

The detail page updates instantly with no refetch; lists are refreshed because only the server knows ordering, filters, and totals.

### 3. Optimistic update

Update the UI **before** the server responds, and roll back on failure ([06](./06-optimistic-updates.md)).

Which keys to invalidate is a design decision: what **reads** are affected by this write? Creating a project affects lists and counts, not other projects' details. Deleting affects the list *and* removes the detail (`removeQueries`, not invalidate, to avoid refetching a 404). Hierarchical [key factories](./03-tanstack-query.md#key-factories) make this readable.

### Keeping the mutation pending until data is fresh

`invalidateQueries` returns a promise. **Return it** from the callback and the mutation stays `isPending` until the refetch finishes:

```ts
onSuccess: () => queryClient.invalidateQueries({ queryKey: projectKeys.lists() }),   // returned → awaited
```

(With braces you must write `return`.) Benefits: the submit button stays disabled and the dialog stays open until fresh data is present, so the user never sees the old list after "saving". The cost is a slightly longer pending state. Choose by UX.

## Forms

With [React Hook Form](../06-forms/02-react-hook-form.md):

```tsx
const { register, handleSubmit, setError, formState: { errors, isSubmitting } } = useForm<FormValues>()
const updateProject = useMutation({ mutationFn: (v: FormValues) => projectsApi.update(id, v), /* … */ })

const onSubmit = handleSubmit(async (values) => {
  try {
    await updateProject.mutateAsync(values)
    toast.success("Saved")
  } catch (error) {
    if (isValidation(error)) applyServerErrors(error, setError)
    else toast.error(getErrorMessage(error))
  }
})
```

- `handleSubmit` awaits the handler, so `isSubmitting` stays true for the request's duration (you can also use `updateProject.isPending`).
- Server validation errors map to fields; other failures toast ([API error handling](../11-api-integration/05-api-error-handling.md#validation-errors--form-fields)).
- Don't reset the form until the mutation succeeds.

## Mutation state elsewhere

Show "saving…" somewhere other than the component that fired the mutation:

```ts
const savingCount = useIsMutating({ mutationKey: ["projects"] })      // how many in flight
const pending = useMutationState({ filters: { mutationKey: ["projects"], status: "pending" }, select: (m) => m.state.variables })
```

Give mutations a `mutationKey` to target them. This also enables sharing defaults via `queryClient.setMutationDefaults`.

## Global handling

Cross-cutting policy (toast unhandled failures, report to monitoring) belongs in the client's `MutationCache`, not repeated in each hook ([API error handling](../11-api-integration/05-api-error-handling.md#global-handlers)). Use a `meta` flag on specific mutations to opt out or customize.

## Retries and idempotency

Mutations default to **no retry**, because repeating a `POST` could create duplicates. If you enable retries, make the endpoint **idempotent**, for example with an idempotency key header generated once per user action, so a retried request can't double-apply.

## Deleting

```ts
const deleteProject = useMutation({
  mutationFn: (id: string) => projectsApi.remove(id),
  onSuccess: (_, id) => {
    queryClient.removeQueries({ queryKey: projectKeys.detail(id) })
    return queryClient.invalidateQueries({ queryKey: projectKeys.lists() })
  },
})
```

Confirm destructive deletes with an [AlertDialog](../09-ui-components/02-dialogs-and-modals.md#dialog-vs-alertdialog), or offer undo via a [toast](../09-ui-components/08-toasts-and-notifications.md). Removing the detail query avoids a pointless refetch of something that no longer exists.

## Common mistakes

- **Not invalidating or updating the cache**, so the UI shows stale data after a successful save.
- **Invalidating everything** (`invalidateQueries()`) rather than the affected keys.
- **Using `mutateAsync` without `try/catch`** → unhandled rejections.
- **Navigating in a hook-level `onSuccess`** (runs even after the user left). Use `mutate`-level callbacks for UI.
- **Not disabling the submit button** while `isPending` → duplicate submissions.
- **Mutating cached objects** inside `setQueryData` updaters.
- **Enabling retries on non-idempotent writes.**
- **Using `onSuccess` of a *query*** to react to writes (removed in v5): mutate-time logic belongs in mutations.
- **Forgetting to `return` the invalidate promise** when you want the pending state to cover the refetch (or returning it when you don't).
- **Resetting the form before the server accepted the data.**
- **Toast + inline error for the same failure.**

## Quick summary

- `useMutation` wraps a write; nothing runs until `mutate`/`mutateAsync`.
- Hook-level callbacks for **cache updates**; `mutate`-level callbacks for **UI reactions**.
- After a write: **invalidate** affected keys (default), or `setQueryData` from the response plus invalidate lists.
- Return the `invalidateQueries` promise to keep `isPending` until data is fresh.
- `isPending` guards against double submits; mutations don't retry by default.
- Map validation errors to fields; toast the rest; centralize global policy in `MutationCache`.

## Next

[06 — Optimistic updates](./06-optimistic-updates.md)
