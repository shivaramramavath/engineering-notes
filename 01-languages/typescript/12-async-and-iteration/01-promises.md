# Promises

A `Promise<T>` represents a value that will be available later, or an error if it never arrives. It is the foundation under `async`/`await`, `fetch`, and most modern async APIs. In TypeScript, the type parameter tells you what the promise resolves to, and the compiler tracks it through chains and combinators.

**Prerequisites:**
- [The event loop](./00-event-loop.md)
- [Callbacks](../02-functions/02-callbacks.md)
- [Generic types](../06-generics/01-generic-types.md)

---

## States

A promise is in exactly one state, and it can only change once:

```text
            +--> fulfilled (has a value of type T)
pending ----+
            +--> rejected  (has a reason, typed as any)
```

Once settled, a promise never changes. Callbacks attached later still run, with the stored result.

## Creating promises

```ts
const sleep = (ms: number) =>
  new Promise<void>((resolve) => setTimeout(resolve, ms));

const delayed = new Promise<number>((resolve, reject) => {
  const ok = Math.random() > 0.5;
  if (ok) resolve(42);
  else reject(new Error("unlucky"));
});
```

- Give the type parameter explicitly: `new Promise<number>(...)`. Without it, TypeScript infers `Promise<unknown>`.
- `Promise<void>` is for "done, no value". Call `resolve()` with no argument.
- The executor function runs **synchronously** when the promise is constructed.
- `Promise.resolve(value)` and `Promise.reject(error)` make already-settled promises.

Usually you do not construct promises by hand. You call functions that return them (`fetch`, `fs.promises.readFile`). Wrap with `new Promise` mainly to adapt callback-based APIs:

```ts
function readFileP(path: string): Promise<string> {
  return new Promise((resolve, reject) => {
    fs.readFile(path, "utf8", (err, data) => {
      if (err) reject(err);
      else resolve(data);
    });
  });
}
```

For Node, `util.promisify` does this for you and keeps the types for common callback signatures.

## then, catch, finally

```ts
fetchUser(1)
  .then((user) => user.name)          // Promise<string>
  .then((name) => name.toUpperCase()) // Promise<string>
  .catch((e: unknown) => {
    console.error(e);
    return "anonymous";               // recovers with a string
  })
  .finally(() => console.log("done"));
```

How chaining behaves:

- `.then(f)` returns a **new** promise. Its type is whatever `f` returns.
- If `f` returns a promise, the result is **flattened**: returning `Promise<T>` from `.then` gives `Promise<T>`, not `Promise<Promise<T>>`.
- If `f` throws, the new promise **rejects**.
- `.catch(f)` handles rejection. If it returns a value, the chain continues as fulfilled.
- `.finally(f)` runs either way, receives no argument, and passes the original result through (unless it throws).
- Rejection **skips** `.then` handlers until a `.catch`.

The parameter of `.catch` callbacks is typed `any`. Annotate it as `unknown` to stay safe ([catching and narrowing errors](../11-error-handling/00-catching-and-narrowing-errors.md)).

## Combinators

| Method | Resolves when | Result type for `[Promise<A>, Promise<B>]` | Rejects when |
|---|---|---|---|
| `Promise.all` | all fulfil | `[A, B]` | any rejects (first rejection) |
| `Promise.allSettled` | all settle | `[PromiseSettledResult<A>, PromiseSettledResult<B>]` | never |
| `Promise.race` | first settles | `A \| B` | the first to settle rejects |
| `Promise.any` | first fulfils | `A \| B` | all reject (`AggregateError`) |

```ts
const [user, posts] = await Promise.all([getUser(), getPosts()]);
// user: User, posts: Post[]  - tuple positions are preserved
```

`Promise.all` preserves tuple types, so each result has its own type. For an array of same-typed promises you get `T[]`.

`allSettled` gives each outcome as a discriminated union:

```ts
const results = await Promise.allSettled([a(), b()]);
for (const r of results) {
  if (r.status === "fulfilled") console.log(r.value);
  else console.error(r.reason);          // reason: any
}
```

### Timeout with `race`

```ts
function withTimeout<T>(promise: Promise<T>, ms: number): Promise<T> {
  let timer: ReturnType<typeof setTimeout>;
  const timeout = new Promise<never>((_, reject) => {
    timer = setTimeout(() => reject(new Error(`Timed out after ${ms}ms`)), ms);
  });
  return Promise.race([promise, timeout]).finally(() => clearTimeout(timer));
}
```

`Promise<never>` is the right type for a promise that never fulfils. Note that this stops *waiting*, not the underlying work. To actually cancel, use an `AbortSignal` ([concurrency patterns](./05-concurrency-patterns.md)).

## Typing details

- `Promise<T>` is **covariant** in `T`: a `Promise<string>` is assignable to `Promise<string | number>`.
- `PromiseLike<T>` is the minimal "thenable" interface (just `then`). APIs that accept it work with any promise implementation.
- There is no `Promise<Promise<T>>` in practice. `Awaited<T>` models the unwrapping that the runtime does. See [async generics](./03-async-generics.md).
- The `lib` setting controls which promise APIs exist: `Promise.allSettled` needs ES2020, `Promise.any` needs ES2021. See [target, module, and lib](../13-compiler-and-tsconfig/02-target-module-and-lib.md).
- Newer runtimes add `Promise.withResolvers()` (returns `{ promise, resolve, reject }`). Check your runtime and `lib` before using it.

## Important rules and misconceptions

**A promise is eager.** It starts working when created, not when you `.then` it. `const p = fetch(url)` already sent the request.

**Handlers run asynchronously,** always, as microtasks, even if the promise is already resolved ([event loop](./00-event-loop.md)).

**A rejected promise with no handler becomes an unhandled rejection.** In Node, modern versions terminate the process by default. Always `await`, `.catch`, or return the promise to someone who will.

**`.then(a, b)` is not `.then(a).catch(b)`.** In the two-argument form, `b` does not catch errors thrown by `a`.

**Promises are not cancellable.** Once started, you can only ignore the result. Cancellation needs a cooperating API (`AbortController`).

## Common mistakes

- **Forgetting to `return` inside `.then`:** the next step receives `undefined`, and errors escape the chain.
- **The explicit-construction anti-pattern:** wrapping an existing promise in `new Promise` just to resolve with its result.
- **An `async` executor:** `new Promise(async (resolve) => ...)`. Errors thrown inside become unhandled rejections instead of rejecting the promise.
- **Nesting `.then` inside `.then`** (callback hell again) instead of flattening the chain.
- **Running sequentially what could run in parallel:** `await a(); await b();` when `a` and `b` are independent.
- **Floating promises:** calling an async function and ignoring the result. Use the lint rule `@typescript-eslint/no-floating-promises`.
- **Assuming `Promise.all` waits for all on failure.** It rejects at the first failure while the others keep running. Use `allSettled` to collect every outcome.

## Debugging

- Hover the chain at each step to see the inferred `Promise<T>`.
- A `Promise<unknown>` or `Promise<any>` usually means a missing type argument or an untyped source.
- Unhandled rejection warnings point to the origin of the rejection. In Node, `process.on("unhandledRejection", ...)` helps during development.
- Async stack traces are partial. Use `await` over long `.then` chains for clearer traces, and name your functions.
- Log inside `.finally` to confirm that a chain actually completed.

## Quick summary

- A `Promise<T>` is pending, then fulfilled with `T` or rejected with a reason, once.
- `.then` returns a new promise and flattens returned promises. Errors skip to the next `.catch`. `.finally` always runs.
- `all`, `allSettled`, `race`, and `any` combine promises. `all` preserves tuple types.
- Promises are eager, handlers are always asynchronous, and nothing can cancel a promise by itself.
- Annotate `new Promise<T>`, return from `.then`, and never leave a promise unhandled.

**Next:** [async/await](./02-async-await.md)
