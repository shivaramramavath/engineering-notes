# Components and Props

Typing components is ordinary React TypeScript plus three Next.js-specific facts: Server Components can be `async`, props that cross from a Server Component to a Client Component must be serializable, and Server Actions can be passed as props. This note covers all three, with the React patterns you use daily.

> Verified against the Next.js 16.4 TypeScript reference and React's list of serializable props. React typing patterns follow `@types/react` for React 19.

## What it is and why

A prop type is a contract between the component and its callers. In an App Router app the contract also decides **what data reaches the browser**: whatever a Server Component passes to a Client Component is serialized into the page payload. A narrow, explicit props type is both a type-safety tool and a data-exposure control ([Data Security](../12-database/00-database-architecture.md)).

## Basic props

```tsx
type ButtonProps = {
  label: string;
  onClick?: () => void;          // optional
  variant?: "primary" | "ghost"; // union of literals beats string
};

export function Button({ label, onClick, variant = "primary" }: ButtonProps) {
  return <button className={variant} onClick={onClick}>{label}</button>;
}
```

- Annotate the destructured parameter. You do not need `React.FC`; plain functions infer their return type, and `FC` adds nothing in React 19.
- Use `type` or `interface`; pick one per project. Prefer unions of **string literals** over `string`.
- Give defaults in destructuring, not with `defaultProps`.

### `children` and slots

```tsx
import type { ReactNode } from "react";

type CardProps = {
  title: string;
  children: ReactNode;       // anything React can render
  footer?: ReactNode;        // a named slot
};

export function Card({ title, children, footer }: CardProps) {
  return (
    <section>
      <h2>{title}</h2>
      {children}
      {footer}
    </section>
  );
}
```

`ReactNode` accepts strings, numbers, elements, arrays, `null` and `undefined`. `ReactElement` is narrower (a single element); use it only when you need to clone or inspect one. `PropsWithChildren<P>` is shorthand for `P & { children?: ReactNode }`.

## Extend native element props

Instead of re-declaring every attribute, borrow them:

```tsx
import type { ComponentProps } from "react";

type InputProps = ComponentProps<"input"> & {
  label: string;
  error?: string;
};

export function Field({ label, error, id, ...rest }: InputProps) {
  return (
    <div>
      <label htmlFor={id}>{label}</label>
      <input id={id} aria-invalid={!!error} {...rest} />
      {error && <p role="alert">{error}</p>}
    </div>
  );
}
```

- `ComponentProps<"input">` includes `ref` in React 19, where **`ref` is an ordinary prop for function components**, so `forwardRef` is no longer needed for new code. Use `ComponentPropsWithoutRef<"input">` when you do not want to expose `ref`.
- `ComponentProps<typeof SomeComponent>` extracts the props of any component, useful for wrappers.
- Use `Omit<ComponentProps<"button">, "onClick">` when you want to replace a prop's type rather than intersect it.

## Discriminated unions: make impossible states unrepresentable

```tsx
type AlertProps =
  | { status: "success"; message: string }
  | { status: "error"; message: string; retry: () => void };

function Alert(props: AlertProps) {
  if (props.status === "error") {
    return <button onClick={props.retry}>{props.message}</button>;   // retry is known here
  }
  return <p>{props.message}</p>;
}
```

A shared literal field (`status`) lets TypeScript narrow the rest. This beats several optional props that are only valid together.

## Generic components

```tsx
type ListProps<T> = {
  items: readonly T[];
  getKey: (item: T) => string;
  render: (item: T) => ReactNode;
};

export function List<T>({ items, getKey, render }: ListProps<T>) {
  return <ul>{items.map((item) => <li key={getKey(item)}>{render(item)}</li>)}</ul>;
}

<List items={posts} getKey={(p) => p.id} render={(p) => p.title} />   // T inferred as Post
```

In `.tsx` files an arrow-function generic needs a trailing comma or `extends` (`<T,>(...)`) to avoid being parsed as JSX; a `function` declaration avoids the issue.

## Events and refs

```tsx
"use client";

import { useRef, useState, type ChangeEvent, type FormEvent } from "react";

export function SearchForm({ onSearch }: { onSearch: (q: string) => void }) {
  const [q, setQ] = useState("");
  const inputRef = useRef<HTMLInputElement>(null);          // null until mounted

  function handleChange(e: ChangeEvent<HTMLInputElement>) {
    setQ(e.target.value);
  }
  function handleSubmit(e: FormEvent<HTMLFormElement>) {
    e.preventDefault();
    onSearch(q);
    inputRef.current?.focus();                               // optional chaining: may be null
  }

  return (
    <form onSubmit={handleSubmit}>
      <input ref={inputRef} value={q} onChange={handleChange} />
    </form>
  );
}
```

For inline handlers the event type is inferred; annotate only when you extract the handler. Common types: `MouseEvent<HTMLButtonElement>`, `KeyboardEvent<HTMLInputElement>`, `ChangeEvent<HTMLSelectElement>`.

## Typing hooks

```tsx
const [user, setUser] = useState<User | null>(null);          // state that starts empty
const [items, setItems] = useState<Item[]>([]);               // [] alone would infer never[]

type Action = { type: "add"; id: string } | { type: "clear" };
function reducer(state: string[], action: Action): string[] {
  switch (action.type) {
    case "add": return [...state, action.id];
    case "clear": return [];
  }                                                            // exhaustive: no default needed
}
const [ids, dispatch] = useReducer(reducer, []);
```

Context with a non-null accessor ([Context](../10-state-management/01-context.md)):

```tsx
const ThemeContext = createContext<ThemeValue | null>(null);

export function useTheme(): ThemeValue {
  const ctx = useContext(ThemeContext);
  if (!ctx) throw new Error("useTheme must be used within ThemeProvider");
  return ctx;                                                  // narrowed: not null
}
```

## The server/client boundary

Props passed from a Server Component to a Client Component are serialized. React's documented serializable values:

| Allowed | Not allowed |
|---|---|
| `string`, `number`, `bigint`, `boolean`, `undefined`, `null` | Functions that are not Server Functions |
| `Symbol.for(...)` (globally registered symbols only) | Classes, and instances of any class (other than the built-ins listed) |
| Arrays, `Map`, `Set`, typed arrays, `ArrayBuffer` | Objects with a `null` prototype |
| `Date` | Symbols that are not globally registered |
| Plain objects with serializable properties | |
| **Server Functions** (Server Actions) | |
| JSX elements (Server or Client Components) | |
| `Promise`s | |

TypeScript does **not** enforce this: a class instance or a function type will compile and then fail at runtime. Make the rule visible in the type:

```tsx
// Server Component
import { getPost } from "@/data/posts";
import { PostActions } from "./post-actions";

export default async function Page(props: PageProps<"/posts/[id]">) {
  const { id } = await props.params;
  const post = await getPost(id);                 // returns a DTO
  return <PostActions post={post} />;
}
```

```tsx
// post-actions.tsx
"use client";

import type { PostDTO } from "@/data/posts";

export function PostActions({ post }: { post: PostDTO }) {   // narrow DTO, not the database row
  /* ... */
}
```

```ts
// data/posts.ts
export type PostDTO = { id: string; title: string; publishedAt: Date | null };   // plain data only
```

Rules of thumb:

- Define a `*DTO` type for everything a Client Component receives; map ORM results into it in the DAL.
- Convert class instances (Mongo `ObjectId`, Prisma `Decimal`) to strings or numbers in the DTO.
- Do not type a prop as `any` or as the full ORM model: both hide the boundary.

### Callbacks do not cross; Server Actions do

```tsx
// Server Component: Error, a function prop is not allowed
<ClientForm onSave={(x) => console.log(x)} />

// OK: a Server Action
import { saveAction } from "@/app/actions";
<ClientForm action={saveAction} />
```

Client Components can accept a Server Action as an ordinary function-typed prop:

```tsx
"use client";

type ClientFormProps = {
  action: (formData: FormData) => void | Promise<void>;
};

export function ClientForm({ action }: ClientFormProps) {
  return <form action={action}><input name="title" /><button>Save</button></form>;
}
```

For action state and results see [Actions and Forms](./02-actions-and-forms.md).

### Server Components as `children`

A Client Component can accept Server Components through `children` or any `ReactNode` prop, which keeps them server-rendered ([Composition Patterns](../03-components/03-composition-patterns.md)). The type is just `ReactNode`:

```tsx
"use client";

export function Tabs({ tabs }: { tabs: { label: string; panel: ReactNode }[] }) { /* ... */ }
```

## Async Server Components

```tsx
export default async function Page() {
  const posts = await getPosts();
  return <PostList posts={posts} />;
}
```

Async components need **TypeScript 5.1.3 or later and `@types/react` 18.2.8 or later**; older versions report `'Promise<Element>' is not a valid JSX element`. They are only valid for Server Components: Client Components cannot be `async`, so read promises with `use()`:

```tsx
"use client";

import { use } from "react";

export function Comments({ commentsPromise }: { commentsPromise: Promise<CommentDTO[]> }) {
  const comments = use(commentsPromise);       // typed CommentDTO[]; suspends until resolved
  return <ul>{comments.map((c) => <li key={c.id}>{c.text}</li>)}</ul>;
}
```

Promises are serializable props, so a Server Component can start a fetch without awaiting and pass the promise down ([Context](../10-state-management/01-context.md#streaming-data-through-context-with-use)).

## Deriving types instead of duplicating them

```ts
// From a function
type Post = Awaited<ReturnType<typeof getPost>>;               // Promise unwrapped
type PostList = Awaited<ReturnType<typeof getPosts>>;
type PostItem = PostList[number];

// From a schema
const PostSchema = z.object({ title: z.string(), body: z.string() });
type PostInput = z.infer<typeof PostSchema>;

// From a constant
const ROLES = ["user", "editor", "admin"] as const;
type Role = (typeof ROLES)[number];                            // "user" | "editor" | "admin"

// Validate a config object's shape without widening it
const nav = [{ href: "/", label: "Home" }] satisfies { href: string; label: string }[];
```

Derive from the DAL's return type so a change to the query flows into every component that uses it.

## Typing the Next.js pieces you meet in components

| Thing | Type |
|---|---|
| Layout `children` | `React.ReactNode` |
| Metadata | `import type { Metadata } from "next"` |
| `next/image` props | `import Image, { type ImageProps } from "next/image"` |
| `Link` `href` | A route string; with `typedRoutes`, `Route<T>` ([Routes and Params](./01-routes-and-params.md)) |
| Error boundary file | `{ error: Error & { digest?: string }; reset: () => void }` |
| Client hooks | `useRouter()`, `usePathname()`, `useSearchParams()` from `next/navigation` |

Confirm the exact `error.tsx` prop shape in the Next.js error-handling docs for your version.

## Debugging

| Symptom | Likely cause | Fix |
|---|---|---|
| `'Promise<Element>' is not a valid JSX element` | Old TypeScript or `@types/react` | TS ≥ 5.1.3, `@types/react` ≥ 18.2.8 |
| Runtime: "Only plain objects can be passed to Client Components" | Class instance in props (ObjectId, Decimal, model) | Map to a DTO |
| Runtime: "Functions cannot be passed directly to Client Components" | Plain callback prop from a Server Component | Use a Server Action, or move the handler into a Client Component |
| `Type 'undefined' is not assignable` | Optional prop used without a check | Default it or narrow |
| `Property 'x' does not exist on type 'never'` | Exhausted a union, or `useState([])` inferred `never[]` | `useState<Item[]>([])` |
| `Object is possibly 'null'` on `ref.current` | Ref is `null` before mount | `ref.current?.` |
| JSX generic arrow parsed as a tag | `<T>(...) =>` in `.tsx` | `<T,>` or a `function` declaration |
| Types seem to ignore `strict` | `strict` off, or editor using the wrong TypeScript | Turn it on; select the workspace TypeScript version |
| Prop type error after changing a query | Duplicated hand-written type | Derive from the DAL return type |

## Common mistakes

| Mistake | Fix |
|---|---|
| `React.FC` everywhere | Plain functions with typed props |
| `any` for props or event handlers | Real types; `unknown` plus narrowing |
| Passing the full database row to a Client Component | A DTO type |
| `string` where a union of literals is meant | `"primary" | "ghost"` |
| Many optional props that only make sense together | Discriminated union |
| Re-declaring every `<button>` attribute | `ComponentProps<"button">` |
| `as SomeType` casts to silence errors | Fix the type, or validate at runtime |
| Making a component a Client Component just to satisfy a type | Keep it a Server Component; type the boundary |
| `useState(null)` then setting an object | `useState<T | null>(null)` |

## Quick Summary

- Type props with a `type` alias; use `ReactNode` for `children` and slots, `ComponentProps<"el">` to extend native elements.
- Use discriminated unions for variant props and generics for reusable containers.
- Server to client props must be serializable (plain data, `Date`, `bigint`, `Map`, `Set`, Promises, Server Actions); the compiler will not check, so type DTOs.
- Async components are for the server (TS ≥ 5.1.3); Client Components read promises with `use()`.
- Derive types from the DAL, schemas and constants instead of copying them.

## Next

- [Routes and Params](./01-routes-and-params.md)
- [Actions and Forms](./02-actions-and-forms.md)
- [Server and Client Components](../03-components/README.md)

Sources: [Next.js TypeScript reference](https://nextjs.org/docs/app/api-reference/config/typescript), [React `use client` reference](https://react.dev/reference/rsc/use-client)