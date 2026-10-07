# React 19 Features

React 19 is mostly about two things: making **async work and forms** first-class (Actions), and **removing boilerplate** (`ref` as a prop, `<Context>` as a provider, built-in metadata). It also removed several long-deprecated APIs. This note covers what you'll actually use and what to watch for when upgrading.

> Features keep landing in minor releases (19.1, 19.2, …). Where something is recent, this note says so; confirm details against the [React release notes](https://react.dev/blog) for your version.

## Actions

An **Action** is an async function run inside a [transition](./01-transitions.md#async-transitions-react-19). React tracks its pending state, handles errors, and can reset forms. The ideas:

- **Pending state** is automatic (`isPending`), with no manual `setLoading(true/false)`.
- **Errors** propagate to an [error boundary](./04-error-boundaries.md) (or you handle them in the action).
- **Optimistic updates** are supported (`useOptimistic`).
- **Forms** can call actions directly through `<form action={fn}>`.

### `<form action={function}>`

```tsx
async function addTodo(formData: FormData) {
  const text = String(formData.get("text") ?? "")
  await todosApi.create({ text })
}

<form action={addTodo}>
  <input name="text" required />
  <button type="submit">Add</button>
</form>
```

Passing a **function** as `action` makes React call it with the form's `FormData` when submitted, inside a transition. You don't write `onSubmit`, `preventDefault`, or `new FormData(e.currentTarget)`.

Two behaviors to know:

- After a successful action, React **resets uncontrolled form fields** automatically. If you want values to persist (for example, after a validation error), use `useActionState` below and return the values, or use controlled inputs.
- The form still works as a normal HTML form before JavaScript loads when the action is a framework-provided server function. With a plain client function, there's no progressive enhancement.

### `useActionState`: result, pending, and the action

```tsx
import { useActionState } from "react"

type State = { error: string | null }

async function createProject(prev: State, formData: FormData): Promise<State> {
  const name = String(formData.get("name") ?? "").trim()
  if (name.length < 2) return { error: "Name must be at least 2 characters" }

  try {
    await projectsApi.create({ name })
    return { error: null }
  } catch (e) {
    return { error: getErrorMessage(e) }
  }
}

function NewProjectForm() {
  const [state, formAction, isPending] = useActionState(createProject, { error: null })

  return (
    <form action={formAction}>
      <input name="name" aria-invalid={!!state.error} />
      {state.error && <p role="alert">{state.error}</p>}
      <button disabled={isPending}>{isPending ? "Creating…" : "Create"}</button>
    </form>
  )
}
```

- The action receives the **previous state** first and the `FormData` second, and returns the **next state**.
- You get `[state, formAction, isPending]`. Use `formAction` as the form's `action`.
- Returned state is how you show validation and server errors without extra `useState`.
- Actions queue: a second submission waits for the first.
- The hook was called `useFormState` in earlier React canary/`react-dom` releases. It's `useActionState` (from `react`) in React 19.

### `useFormStatus`: pending state for children

A submit button deep in a design system shouldn't need an `isPending` prop threaded to it:

```tsx
import { useFormStatus } from "react-dom"

function SubmitButton({ children }: { children: React.ReactNode }) {
  const { pending } = useFormStatus()           // status of the PARENT <form>
  return <button type="submit" disabled={pending}>{children}</button>
}
```

It must be rendered **inside** the `<form>` whose status it reads. Calling it in the same component that renders the `<form>` doesn't work.

### `useOptimistic`: show the result before it's confirmed

```tsx
import { useOptimistic } from "react"

function Todos({ todos }: { todos: Todo[] }) {
  const [optimisticTodos, addOptimisticTodo] = useOptimistic(
    todos,
    (current: Todo[], text: string) => [...current, { id: `temp-${text}`, text, pending: true }]
  )

  async function action(formData: FormData) {
    const text = String(formData.get("text"))
    addOptimisticTodo(text)             // shows immediately
    await todosApi.create({ text })     // when this settles, `todos` (real data) replaces the optimistic state
  }

  return (
    <>
      <form action={action}><input name="text" /><button>Add</button></form>
      <ul>
        {optimisticTodos.map((t) => <li key={t.id} className={t.pending ? "opacity-50" : undefined}>{t.text}</li>)}
      </ul>
    </>
  )
}
```

`useOptimistic` shows a temporary value **only while the Action is in flight**. When it finishes (success or failure), React reverts to the real state, so a failure rolls back automatically. The real `todos` must be refreshed by something (a refetch, a router revalidation, a server update), or the new item will vanish when the optimistic layer drops.

### Actions vs your existing tools

Actions aren't a replacement for everything:

| | Actions (`useActionState`, `<form action>`) | [TanStack Query mutations](../12-server-state/05-mutations.md) + [React Hook Form](../06-forms/02-react-hook-form.md) |
|---|---|---|
| Simple forms | **Very little code** | More setup |
| Cache invalidation and server-state sync | You do it yourself | **Built in** |
| Complex client validation, dynamic fields, multi-step | Limited | **Strong** (RHF + schema) |
| Framework server functions / progressive enhancement | **Natural fit** | Not applicable |
| Retries, caching, background refetch | No | Yes |

A reasonable approach: use Actions for straightforward form submissions (especially with a framework), and keep a query cache for server state. They can coexist, with an action calling a mutation or invalidating queries.

## The `use` API

Reads a promise or context during render, and can be called conditionally. Covered in [Suspense](./03-suspense.md#the-use-hook).

```tsx
const theme = use(ThemeContext)         // like useContext, but callable inside if/loops
const data = use(stablePromise)         // suspends until resolved
```

## `ref` is a regular prop

Function components can accept `ref` as a normal prop. No `forwardRef` wrapper:

```tsx
// React 19
function Input({ ref, ...props }: React.ComponentProps<"input">) {
  return <input ref={ref} {...props} />
}

<Input ref={inputRef} />
```

`forwardRef` still works but is no longer needed and is expected to be deprecated, and codemods exist to convert it. Types like `React.ComponentProps<"input">` already include `ref`, which is why generated [shadcn/ui](../09-ui-components/00-shadcn-ui.md) components no longer wrap in `forwardRef`.

**Ref callbacks can return a cleanup function**, called when the element is removed:

```tsx
<div ref={(node) => {
  const observer = new ResizeObserver(/* … */)
  observer.observe(node!)
  return () => observer.disconnect()          // cleanup (React 19)
}} />
```

Because a ref callback may now return a function, an implicit arrow return like `ref={(n) => (current = n)}` (returning the assigned value) gets flagged by TypeScript. Use a block body instead.

## Context as a provider

```tsx
const ThemeContext = createContext("light")

<ThemeContext value="dark">      {/* React 19: no .Provider */}
  <Page />
</ThemeContext>
```

`<Context.Provider>` still works for now and is expected to be deprecated. See [context patterns](../13-state-management/01-context-patterns-and-performance.md).

## Document metadata

Render `<title>`, `<meta>`, and `<link>` **anywhere** in a component; React hoists them into `<head>`:

```tsx
function ProjectPage({ project }: { project: Project }) {
  return (
    <article>
      <title>{project.name} · Acme</title>
      <meta name="description" content={project.summary} />
      <h1>{project.name}</h1>
    </article>
  )
}
```

That replaces most uses of libraries like React Helmet for simple cases, and it works with streaming SSR. In an SPA it's also the simplest way to update the page title on navigation, which is useful for the accessibility gap described in [navigation](../10-routing/03-navigation.md#scroll-and-focus). For complex needs (templates, per-route defaults), a framework's metadata system may still be better.

## Stylesheets and resource loading

- `<link rel="stylesheet" href="…" precedence="default">` lets React manage stylesheet **ordering and deduplication**, and suspends rendering until the sheet loads, avoiding unstyled flashes.
- `<script async src="…">` rendered in any component is **deduplicated**, so the same script isn't injected twice.
- Resource hint helpers from `react-dom`: `preload`, `preinit`, `preconnect`, `prefetchDNS`, for telling the browser what's coming ([network performance](../14-performance/06-network-performance.md#resource-hints)):

```tsx
import { preconnect, preload } from "react-dom"

function Page() {
  preconnect("https://api.example.com")
  preload("/fonts/inter.woff2", { as: "font", type: "font/woff2", crossOrigin: "anonymous" })
  // …
}
```

## Smaller improvements

- **Better error reporting**: hydration mismatches show a **diff** instead of several vague errors, and each error is reported once. Root options `onCaughtError`/`onUncaughtError` ([04](./04-error-boundaries.md#react-19-root-level-error-hooks)).
- **`useDeferredValue` initial value** ([02](./02-useDeferredValue.md#initial-value-react-19)).
- **Custom Elements** (web components) support is now complete, with props passed as properties/attributes correctly.
- **Server Components and Server Functions** are stable for frameworks ([07](./07-server-components-and-ssr.md)).
- **`act`** is imported from `react` (not `react-dom/test-utils`) in tests.

## Added after 19.0

Later minor releases added more. As of 19.2 these include `<Activity>` (hide a subtree while preserving its state and deprioritizing its work), `useEffectEvent` (read the latest props/state in an effect without re-triggering it), `cacheSignal`, and React-specific tracks in the Chrome Performance panel. Check the release notes to see what your installed version provides before relying on them.

## Removed and changed APIs (upgrading)

| Removed / changed | Use instead |
|---|---|
| `ReactDOM.render`, `hydrate` | `createRoot`, `hydrateRoot` |
| `propTypes` checks (silently ignored) | TypeScript |
| `defaultProps` on function components | Default parameter values |
| String refs, legacy context (`contextTypes`) | `useRef` / callback refs, `createContext` |
| `ReactDOM.findDOMNode` | Refs |
| `react-test-renderer` (deprecated) | React Testing Library |
| `forwardRef` (still works) | `ref` as a prop |
| `useRef()` with no argument (TypeScript) | `useRef<T>(null)` or `useRef<T>(undefined)`, since an argument is now required |
| `ReactElement["props"]` typed as `any` | Now `unknown`, which can break code that read props off an element |

Upgrade path:

1. Move to the latest **18.x** and fix every deprecation warning first.
2. Upgrade to 19 and run the official **codemods** (React publishes a migration recipe via `codemod`), which handle many of the changes above.
3. Check your dependencies for React 19 support (peer dependency ranges), since some UI libraries lag.
4. Type fixes: update `@types/react` and `@types/react-dom` together.
5. Run your tests, then watch for hydration and `ref`-related changes if you use SSR or component libraries.

## Common mistakes

- **Using `useFormStatus` in the component that renders the form**, instead of a child, so `pending` stays `false`.
- **Expecting uncontrolled inputs to keep their values** after an action completes (React resets them).
- **Forgetting `useOptimistic` only lasts while the action is pending**, so the real data must update.
- **Replacing a query cache with Actions** and losing caching, invalidation, and refetching.
- **State updates after `await` in a transition** not being part of the transition ([01](./01-transitions.md#async-transitions-react-19)).
- **Calling `use` with a promise created during render** ([03](./03-suspense.md#the-use-hook)).
- **Still wrapping everything in `forwardRef`** (unnecessary now), or ref callbacks with implicit returns.
- **Upgrading without clearing 18.x warnings first**, or without updating the type packages.
- **Relying on `defaultProps`/`propTypes`**, which no longer do anything on function components.
- **Assuming a 19.x feature exists in your version** without checking the release notes.

## Quick summary

- **Actions**: async functions in transitions with automatic pending state and error handling; `<form action={fn}>`, `useActionState`, `useFormStatus`, and `useOptimistic` build on them.
- Actions suit simple forms; keep a query cache and a form library where you need caching or complex validation.
- **`use`** reads promises (suspending) and context (conditionally).
- **`ref` is a prop**, ref callbacks can return cleanups, `<Context value>` replaces `.Provider`, and metadata/stylesheets/resource hints are built in.
- Several legacy APIs are gone (`ReactDOM.render`, `propTypes`, `defaultProps` on functions, string refs, legacy context); upgrade via 18.x cleanup, codemods, and updated types.
- Minor releases keep adding features, so verify against your installed version.

## Next

[06 — React Compiler](./06-react-compiler.md)