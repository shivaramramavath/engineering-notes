# Async Generics

Writing reusable async helpers (retry, cache, wrap, batch, map with concurrency) means combining generics with promises. The key tools are `Promise<T>`, `PromiseLike<T>`, `Awaited<T>`, `MaybePromise<T>`, and the function utilities `Parameters` and `ReturnType`. This note collects the patterns and the typing traps that come with them.

**Prerequisites:**
- [Promises](./01-promises.md) and [async/await](./02-async-await.md)
- [Generic functions](../06-generics/00-generic-functions.md)
- [Function and class utilities](../07-utility-types/03-function-and-class-utilities.md)

---

## The basic shape

A generic async function takes a type parameter for the resolved value and returns `Promise<T>`:

```ts
async function retry<T>(fn: () => Promise<T>, attempts = 3): Promise<T> {
  let lastError: unknown;
  for (let i = 0; i < attempts; i++) {
    try {
      return await fn();
    } catch (e) {
      lastError = e;
    }
  }
  throw lastError;
}

const user = await retry(() => fetchUser("1"));   // T inferred as User
```

`T` is inferred from what `fn` returns. Callers get the exact type with no annotation.

## Awaited

`Awaited<T>` is the built-in type that models what `await` does: it unwraps promises, recursively, and leaves other types alone.

```ts
type A = Awaited<Promise<string>>;            // string
type B = Awaited<Promise<Promise<number>>>;   // number
type C = Awaited<boolean | Promise<Date>>;    // boolean | Date
```

The most common use is naming the result of an async function:

```ts
async function getUser(id: string) {
  return { id, name: "Asha" };
}

type User = Awaited<ReturnType<typeof getUser>>;   // { id: string; name: string }
```

`ReturnType` alone gives `Promise<{...}>`, so you need `Awaited` as well. `Awaited` was added in TS 4.5. It also unwraps any `PromiseLike` ("thenable"), matching runtime behavior.

## MaybePromise

APIs that accept a callback often work with both sync and async functions. A small alias expresses that:

```ts
type MaybePromise<T> = T | Promise<T>;

async function mapAsync<T, U>(
  items: readonly T[],
  fn: (item: T, index: number) => MaybePromise<U>,
): Promise<U[]> {
  return Promise.all(items.map(fn));
}

await mapAsync([1, 2, 3], (n) => n * 2);               // sync callback: U = number
await mapAsync([1, 2, 3], async (n) => fetchUser(n));  // async callback: U = User
```

`Promise.all` accepts non-promise values and passes them through, so one implementation handles both. For the accepting side, `PromiseLike<T>` is a slightly broader alternative: it matches anything with a `then` method.

## Wrapping functions generically

To add behavior around any async function while keeping its exact signature, combine `Parameters` and `ReturnType` (or `Awaited`):

```ts
function withLogging<F extends (...args: any[]) => Promise<unknown>>(fn: F) {
  return async (...args: Parameters<F>): Promise<Awaited<ReturnType<F>>> => {
    const start = Date.now();
    try {
      return (await fn(...args)) as Awaited<ReturnType<F>>;
    } finally {
      console.log(`${fn.name} took ${Date.now() - start}ms`);
    }
  };
}

const loggedGetUser = withLogging(getUser);
// (id: string) => Promise<{ id: string; name: string }>
```

Why the cast: inside a generic function, `Awaited<ReturnType<F>>` is a deferred type, so TypeScript cannot prove that `await fn(...)` produces it. The cast is safe here because the types line up by construction. Constrain `F` tightly (`Promise<unknown>`, not `any`) so the helper rejects non-async functions.

### Memoizing async results

Cache the **promise**, not the value. That also deduplicates concurrent calls:

```ts
function memoizeAsync<A extends string | number, R>(
  fn: (arg: A) => Promise<R>,
): (arg: A) => Promise<R> {
  const cache = new Map<A, Promise<R>>();
  return (arg) => {
    let p = cache.get(arg);
    if (!p) {
      p = fn(arg);
      cache.set(arg, p);
      p.catch(() => cache.delete(arg));   // do not cache failures
    }
    return p;
  };
}
```

## Typing collections of promises

`Promise.all` preserves tuple types. To write your own helper with the same behavior, map over the tuple and apply `Awaited`:

```ts
type AwaitedAll<T extends readonly unknown[]> = {
  -readonly [K in keyof T]: Awaited<T[K]>;
};

async function all<T extends readonly unknown[]>(values: readonly [...T]): Promise<AwaitedAll<T>> {
  return (await Promise.all(values)) as AwaitedAll<T>;
}

const [n, s] = await all([Promise.resolve(1), "text"] as const);
// n: number, s: string
```

The `readonly [...T]` parameter encourages TypeScript to infer a tuple rather than an array ([variadic tuple types](../10-advanced-types/06-variadic-tuple-types.md)). Mapped types over tuples stay tuples ([mapped types](../10-advanced-types/03-mapped-types.md)).

## A typed deferred

When you need to resolve a promise from outside the executor (bridging events, test helpers):

```ts
class Deferred<T> {
  readonly promise: Promise<T>;
  resolve!: (value: T | PromiseLike<T>) => void;
  reject!: (reason?: unknown) => void;

  constructor() {
    this.promise = new Promise<T>((resolve, reject) => {
      this.resolve = resolve;
      this.reject = reject;
    });
  }
}

const ready = new Deferred<void>();
setTimeout(() => ready.resolve(), 100);
await ready.promise;
```

The `!` tells TypeScript the fields are assigned (the executor runs synchronously inside the constructor). Newer runtimes provide `Promise.withResolvers()` for the same job.

## Results instead of rejections

For expected failures, return a `Result` rather than rejecting, so the caller must handle it:

```ts
type Result<T, E> = { ok: true; value: T } | { ok: false; error: E };

async function tryAsync<T>(fn: () => Promise<T>): Promise<Result<T, Error>> {
  try {
    return { ok: true, value: await fn() };
  } catch (e) {
    return { ok: false, error: e instanceof Error ? e : new Error(String(e)) };
  }
}
```

See [the Result pattern](../11-error-handling/02-result-pattern.md).

## Async iterables

Generic helpers over async sequences use `AsyncIterable<T>` as the input type:

```ts
async function collect<T>(source: AsyncIterable<T>): Promise<T[]> {
  const out: T[] = [];
  for await (const item of source) out.push(item);
  return out;
}
```

Accepting `AsyncIterable<T>` means any async generator or stream works. See [iterators and generators](./04-iterators-and-generators.md).

## Important rules and misconceptions

**`Promise<T>` is covariant.** A `Promise<Dog>` can be used as a `Promise<Animal>`.

**`Promise<Promise<T>>` does not exist at runtime.** Promises flatten, and `Awaited` mirrors that. Do not write types that pretend otherwise.

**Generic return types stay unresolved inside the function.** `Awaited<T>` for a generic `T` cannot be simplified until `T` is known, which is why casts are sometimes needed in wrappers.

**An `async` function's return type is always a promise.** A callback typed `() => T` does not accept an async function if `T` is not a promise type (the promise would be treated as the value `T`).

**`any` in the constraint erases safety.** `F extends (...args: any[]) => any` accepts everything and loses the "must return a promise" guarantee.

## Common mistakes

- **Annotating `Promise<Promise<T>>`** or forgetting that `ReturnType` of an async function is already `Promise<T>`.
- **Using `ReturnType<typeof fn>` where `Awaited<ReturnType<typeof fn>>` is needed.**
- **Typing a callback as `() => T` and then passing an async function,** so `T` becomes a promise.
- **Loose constraints (`any`)** that let non-async functions through a helper meant for async ones.
- **Caching resolved values and not promises,** causing duplicate in-flight calls.
- **Caching rejected promises** forever.
- **Casting away errors** in wrappers instead of keeping the signature faithful.

## Debugging

- Hover the helper call to see the inferred `T`. A `Promise<...>` inside where you expected a value means `Awaited` is missing.
- If `T` is inferred as `unknown`, add an explicit type argument or improve the callback's return annotation.
- For "Type 'X' is not assignable to type 'Awaited<T>'" errors in generic code, check whether the function should be generic over the unwrapped type instead, and use a justified cast at the boundary if not.
- Write type tests (`Expect<Equal<...>>`) for wrapper helpers ([type testing](../18-testing-and-debugging/04-type-testing.md)).

## Quick summary

- Generic async helpers take `fn: () => Promise<T>` and return `Promise<T>`. `T` is inferred from the callback.
- `Awaited<T>` mirrors `await` and is paired with `ReturnType` to name an async function's result.
- `MaybePromise<T>` and `PromiseLike<T>` let helpers accept sync and async inputs.
- Preserve signatures with `Parameters<F>` and `Awaited<ReturnType<F>>`. Casts at the boundary are sometimes unavoidable, so constrain generics tightly.
- Cache promises (not values) to deduplicate concurrent calls, and do not cache failures.

**Next:** [Iterators and generators](./04-iterators-and-generators.md)
