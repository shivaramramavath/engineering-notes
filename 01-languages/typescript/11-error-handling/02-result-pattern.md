# The Result Pattern

The Result pattern makes failure **part of a function's return type** instead of an invisible exception. A function returns either a success value or an error value, and the type system forces the caller to deal with both before using the value. It is TypeScript's answer to the missing "checked exceptions" feature.

**Prerequisites:**
- [Discriminated unions](../03-unions-and-narrowing/04-discriminated-unions.md)
- [Exhaustiveness checking](../03-unions-and-narrowing/06-exhaustiveness-checking.md)
- [Custom errors](./01-custom-errors.md)

---

## The problem with exceptions

```ts
function parseAge(input: string): number {
  const n = Number(input);
  if (Number.isNaN(n)) throw new Error("Not a number");
  return n;
}

const age = parseAge(userInput);   // signature says: always a number
```

The signature promises `number`, but the function can throw. Nothing in the type tells a caller that, and nothing makes them handle it. Forgetting a `try/catch` compiles, and fails at runtime.

## The basic shape

```ts
type Result<T, E = Error> =
  | { ok: true; value: T }
  | { ok: false; error: E };

const ok  = <T>(value: T): Result<T, never> => ({ ok: true, value });
const err = <E>(error: E): Result<never, E> => ({ ok: false, error });
```

This is a [discriminated union](../03-unions-and-narrowing/04-discriminated-unions.md) on `ok`. Checking `ok` narrows the type, so you can only read `value` when it exists:

```ts
function parseAge(input: string): Result<number, string> {
  const n = Number(input);
  if (input.trim() === "" || Number.isNaN(n)) return err("Not a number");
  if (n < 0 || n > 150) return err("Out of range");
  return ok(n);
}

const result = parseAge(userInput);

if (result.ok) {
  console.log(result.value);   // number
} else {
  console.error(result.error); // string
}

result.value;   // error: Property 'value' does not exist on type '{ ok: false; ... }'
```

The compiler will not let you touch `value` without checking. That is the entire benefit.

## Typed errors

Because `E` is a type parameter, the error side can be precise, usually a union of the specific failures that can occur:

```ts
type ParseError =
  | { kind: "empty" }
  | { kind: "not-a-number"; input: string }
  | { kind: "out-of-range"; min: number; max: number };

function parseAge(input: string): Result<number, ParseError> {
  if (input.trim() === "") return err({ kind: "empty" });
  const n = Number(input);
  if (Number.isNaN(n)) return err({ kind: "not-a-number", input });
  if (n < 0 || n > 150) return err({ kind: "out-of-range", min: 0, max: 150 });
  return ok(n);
}
```

Handling every case is then checkable:

```ts
function describe(e: ParseError): string {
  switch (e.kind) {
    case "empty":         return "Please enter a value";
    case "not-a-number":  return `"${e.input}" is not a number`;
    case "out-of-range":  return `Must be between ${e.min} and ${e.max}`;
    default: {
      const _never: never = e;   // adding a new kind causes a compile error here
      return _never;
    }
  }
}
```

This is the main advantage over exceptions: the possible failures are visible in the signature, and adding a new one forces callers to update.

## Wrapping code that throws

Real code calls libraries that throw. Convert at the edge:

```ts
function tryCatch<T>(fn: () => T): Result<T, Error> {
  try {
    return ok(fn());
  } catch (e) {
    return err(e instanceof Error ? e : new Error(String(e)));
  }
}

async function tryCatchAsync<T>(fn: () => Promise<T>): Promise<Result<T, Error>> {
  try {
    return ok(await fn());
  } catch (e) {
    return err(e instanceof Error ? e : new Error(String(e)));
  }
}

const json = tryCatch(() => JSON.parse(text));
const res = await tryCatchAsync(() => fetch(url));
```

Async functions return `Promise<Result<...>>`. The promise should not reject for expected failures.

## Composing results

Without helpers, chaining several fallible steps becomes nested `if (!r.ok) return r;` checks. Small combinators help:

```ts
function map<T, U, E>(r: Result<T, E>, f: (v: T) => U): Result<U, E> {
  return r.ok ? ok(f(r.value)) : r;
}

function andThen<T, U, E>(r: Result<T, E>, f: (v: T) => Result<U, E>): Result<U, E> {
  return r.ok ? f(r.value) : r;
}

const total = andThen(parseAge(input), (age) => validateAdult(age));
```

Or write the early-return style by hand, which is often the clearest:

```ts
function register(input: RawInput): Result<User, RegisterError> {
  const email = parseEmail(input.email);
  if (!email.ok) return email;

  const age = parseAge(input.age);
  if (!age.ok) return age;

  return ok({ email: email.value, age: age.value });
}
```

If the error types differ, the function's return type must be the union of them (`RegisterError` here includes the parse errors).

Libraries such as `neverthrow` provide a richer, chainable `Result` with these helpers built in, if you prefer a ready-made API.

## When to use Result and when to throw

| Situation | Prefer |
|---|---|
| Expected, recoverable failures the caller must decide about (validation, not found, parse, "payment declined") | `Result` |
| Programmer errors and broken invariants (null where impossible, bad argument to an internal function) | `throw` |
| Unrecoverable conditions (cannot connect to a required database at startup) | `throw`, handled at the top level |
| Deep call stacks where only a boundary can handle the error | `throw`, caught at that boundary |
| Library code consumed by people who expect exceptions | `throw`, to match conventions |

A useful rule: **if the caller is expected to handle it, return it. If nothing local can handle it, throw it.**

Mixing is normal. Use `Result` in the domain and service layers where failure is part of the business logic. Let unexpected errors propagate as exceptions to a central handler. See [error handling strategies](./03-error-handling-strategies.md).

## Costs and trade-offs

- **Verbosity.** Every call site checks `ok`. Over-using it for failures nobody handles creates ceremony.
- **No automatic propagation.** Exceptions bubble up for free. With `Result`, each layer must pass the error along explicitly.
- **Nothing forces you to look at the result.** If a function returns a `Result` and you ignore it, TypeScript does not complain (there is no built-in "must use"). A lint rule or discipline is needed.
- **Mixed styles confuse.** If half the codebase throws and half returns `Result`, callers cannot tell which applies. Pick a convention per layer.
- **Existing ecosystem throws.** You will convert at the boundaries with `tryCatch`.

## Common mistakes

- **Returning `Result` and also throwing for the same failure.** Pick one.
- **Using `Result<T, Error>` everywhere.** A plain `Error` carries no structure. Use a union of specific errors where callers branch on them.
- **Using `Result<T, string>` for anything non-trivial.** Strings cannot be matched exhaustively or carry data.
- **Ignoring the returned `Result`.** The failure is silently dropped, which is worse than an exception.
- **Narrowing with truthiness.** Check `result.ok`, and do not assume `value` is truthy (`0` and `""` are valid values).
- **Forgetting the `never` default** in a `switch` on `error.kind`, so new error kinds go unhandled.
- **Wrapping every function.** Not every function can fail in an interesting way.

## Debugging

- If `result.value` errors with "does not exist", you have not narrowed on `ok` yet. Check `if (result.ok)` first.
- If narrowing does not work, make sure the `ok` property has literal types (`true` and `false`), not `boolean`. Using `as const` or explicit return types on helpers keeps the literals.
- If a union return type from `andThen` is hard to read, name the combined error type.
- If errors are being lost, look for a code path that returns `ok(...)` after a failed step, or a `catch` that swallows.

## Quick summary

- `Result<T, E>` is a discriminated union: `{ ok: true; value: T } | { ok: false; error: E }`. Narrowing on `ok` gives safe access.
- Typed error unions put failures in the signature and make handling exhaustive.
- Convert throwing code at the edge with `tryCatch`. Compose with early returns or small `map`/`andThen` helpers.
- Use `Result` for expected, recoverable failures. Use exceptions for bugs and failures only a boundary can handle.
- It costs verbosity and has no must-use check, so apply it where the compile-time guarantee is worth it.

**Next:** [Error handling strategies](./03-error-handling-strategies.md)