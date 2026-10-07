# async/await

`async`/`await` is syntax over promises that lets asynchronous code read like synchronous code. An `async` function always returns a promise, and `await` pauses that function until a promise settles, without blocking the thread. It is how most TypeScript async code is written today, and the typing is simple: `await` turns a `Promise<T>` into a `T`.

**Prerequisites:**
- [Promises](./01-promises.md)
- [The event loop](./00-event-loop.md)
- [Catching and narrowing errors](../11-error-handling/00-catching-and-narrowing-errors.md)

---

## Basics

```ts
async function getUserName(id: string): Promise<string> {
  const res = await fetch(`/api/users/${id}`);   // Response
  const user = await res.json();                  // any
  return user.name;                               // wrapped into Promise<string>
}
```

- An `async` function's return type is always `Promise<...>`. If you annotate it, it must be `Promise<T>`. Annotating `: string` is an error.
- `return value` inside resolves the promise with `value`. `throw` inside rejects it.
- `await expr` gives the settled value, typed as `Awaited<typeof expr>`. Awaiting a non-promise is allowed and returns it unchanged (after a microtask tick).
- `await` is only allowed inside `async` functions, and at the top level of ES modules.

The same with arrow functions and methods:

```ts
const load = async (id: string) => (await fetch(`/api/${id}`)).json();

class Repo {
  async find(id: string): Promise<User | undefined> { /* ... */ }
}
```

## How it works

An `async` function runs **synchronously until its first `await`**. At the `await`, the function suspends and control returns to the caller (which receives a pending promise). When the awaited promise settles, the rest of the function resumes as a microtask.

```ts
async function demo() {
  console.log("1");        // sync
  await Promise.resolve();
  console.log("3");        // resumed later
}
demo();
console.log("2");          // runs before "3"
```

The compiler turns `async` functions into state machines (with generators) when `target` is below ES2017. With a modern target they are emitted as-is.

## Sequential vs parallel

`await` in sequence waits for each step:

```ts
const user = await getUser();     // wait ~100ms
const posts = await getPosts();   // then wait ~100ms  -> ~200ms total
```

If the calls are independent, start them together and wait once:

```ts
const [user, posts] = await Promise.all([getUser(), getPosts()]);   // ~100ms
```

Or start first, await later:

```ts
const userP = getUser();
const postsP = getPosts();
const user = await userP;
const posts = await postsP;
```

If the second call **depends on** the first (`getPosts(user.id)`), sequential is correct.

## Loops

**Sequential, one at a time:**

```ts
for (const id of ids) {
  await process(id);
}
```

**All at once:**

```ts
const results = await Promise.all(ids.map((id) => process(id)));
```

`ids.map(async ...)` returns `Promise<R>[]`, which `Promise.all` turns into `R[]`. For large lists, an unbounded `Promise.all` can overwhelm a server or API. Limit concurrency ([concurrency patterns](./05-concurrency-patterns.md)).

**Do not use `forEach` with `async`:**

```ts
ids.forEach(async (id) => {
  await process(id);   // forEach does not wait for these
});
console.log("done");   // runs before any process(id) finishes
```

`forEach` ignores the returned promises, so nothing waits for them and errors become unhandled rejections. Use `for...of` or `Promise.all(map(...))`.

## Errors

`try/catch` works around `await`:

```ts
async function safeLoad(id: string) {
  try {
    return await load(id);        // `await` is needed for the catch to apply
  } catch (e) {
    return handle(e);
  } finally {
    cleanup();
  }
}
```

Without `await` on the returned promise (`return load(id)`), a rejection **bypasses** the `catch` in the same function. Use `return await` inside `try`.

Calling an `async` function that throws does not throw at the call site. It returns a rejected promise, and the error appears where you `await`. See [error handling strategies](../11-error-handling/03-error-handling-strategies.md).

## Top-level await

In an ES module, you can `await` at the top level:

```ts
// config.ts
const res = await fetch("https://example.com/config.json");
export const config = await res.json();
```

Requirements: the file must be an ES module (see [ES modules and CommonJS](../08-modules/01-es-modules-and-commonjs.md)), `module` set to `es2022`, `esnext`, `nodenext`, `preserve`, or similar, and `target` of `es2017` or higher. It is not available in CommonJS output. Importers of a module with top-level await wait for it to finish, so it delays startup of everything that depends on it.

## Cancellation and timeouts

Promises cannot be cancelled, but many APIs accept an `AbortSignal`:

```ts
const controller = new AbortController();
const timer = setTimeout(() => controller.abort(), 5000);

try {
  const res = await fetch(url, { signal: controller.signal });
  return await res.json();
} catch (e) {
  if (e instanceof DOMException && e.name === "AbortError") return null;
  throw e;
} finally {
  clearTimeout(timer);
}
```

`AbortSignal.timeout(ms)` creates a signal that aborts after a delay, where supported. Aborting makes the API reject, which is how work actually stops.

## Typing patterns

**A function that may or may not be async:**

```ts
type MaybePromise<T> = T | Promise<T>;

async function run<T>(fn: () => MaybePromise<T>): Promise<T> {
  return await fn();
}
```

**Getting the resolved type of an async function:**

```ts
type User = Awaited<ReturnType<typeof getUser>>;
```

More in [async generics](./03-async-generics.md).

**Async constructors do not exist.** A constructor cannot be `async`. Use a static factory:

```ts
class Client {
  private constructor(private readonly conn: Connection) {}
  static async create(url: string): Promise<Client> {
    return new Client(await connect(url));
  }
}
```

**Async event handlers:** `onClick={async () => ...}` compiles, but returns a promise that nothing handles, so errors become unhandled rejections. Wrap the body in `try/catch`, or call a function that handles errors itself. The lint rule `@typescript-eslint/no-misused-promises` flags these.

## Common mistakes

- **Sequential `await`s for independent work,** making code slower than needed.
- **`forEach(async ...)`** and expecting it to wait.
- **Missing `await`:** the function returns a pending promise. A `Promise<User>` used where `User` was expected is usually a compile error, but `if (promise)` is always truthy and compiles.
- **`return promise` inside `try` without `await`,** so the `catch` does not apply.
- **Unbounded `Promise.all` over thousands of items.**
- **Using `async` on a function that never awaits,** adding a needless promise wrapper (the lint rule `require-await` can flag it).
- **Mixing `.then` chains and `await`** in one function, making flow hard to read.
- **Swallowing errors with an empty `catch`.**

## Debugging

- Hover an `await` expression to see the unwrapped type. If it still shows a `Promise`, you may be awaiting a function reference instead of calling it.
- Enable `@typescript-eslint/no-floating-promises` and `no-misused-promises` to catch missing `await`s.
- Add temporary logging before and after each `await` to see the actual order.
- Async stack traces include `await` frames in modern V8, so `async` function names appear. Name your functions rather than using anonymous arrows for better traces.
- If code "hangs", a promise never settled. Look for a missing `resolve` call or an unresolved dependency.

## Quick summary

- `async` functions always return `Promise<T>`. `await` unwraps a promise to its value and suspends only that function.
- Code runs synchronously until the first `await`. The rest resumes as a microtask.
- Use `Promise.all` for independent work, `for...of` for sequential work, and never `forEach(async ...)`.
- Use `return await` inside `try/catch`, and `AbortSignal` for real cancellation.
- Top-level `await` needs an ES module output. Constructors cannot be async: use static factories.

**Next:** [Async generics](./03-async-generics.md)
