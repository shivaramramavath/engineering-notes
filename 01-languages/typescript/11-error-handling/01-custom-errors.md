# Custom Errors

A custom error is a class that extends `Error` and carries extra, typed information: a machine-readable `code`, an HTTP status, the offending field, the underlying `cause`. It lets callers tell failures apart with `instanceof` or a discriminant instead of parsing message strings, and it lets each layer of an application add context while keeping the original error.

**Prerequisites:**
- [Catching and narrowing errors](./00-catching-and-narrowing-errors.md)
- [Inheritance and abstract classes](../05-classes/02-inheritance-and-abstract-classes.md)
- [Discriminated unions](../03-unions-and-narrowing/04-discriminated-unions.md)

---

## A minimal custom error

```ts
class ValidationError extends Error {
  constructor(message: string, readonly field: string) {
    super(message);
    this.name = "ValidationError";
  }
}

try {
  throw new ValidationError("Email is required", "email");
} catch (e) {
  if (e instanceof ValidationError) {
    console.log(e.field);    // string, fully typed
  }
}
```

The pieces:

- `extends Error` gives you `message`, `stack`, and `instanceof Error`.
- `super(message)` must be called first.
- Setting `this.name` makes stack traces and logs read `ValidationError: ...` instead of `Error: ...`.
- `readonly field` is a [parameter property](../05-classes/00-classes.md), shorthand for declaring and assigning the field.

## The ES5 target gotcha

If `target` is `ES5` (or lower), extending built-ins like `Error` breaks the prototype chain and `instanceof` returns `false` for your own class:

```ts
// target: ES5
const e = new ValidationError("x", "email");
e instanceof ValidationError;   // false (!)
```

The fix is to restore the prototype in the constructor:

```ts
constructor(message: string, readonly field: string) {
  super(message);
  Object.setPrototypeOf(this, new.target.prototype);
  this.name = "ValidationError";
}
```

With `target: ES2015` or newer, native classes work correctly and this line is unnecessary. Most modern projects target ES2020 or later, so you will mostly meet it in older code and libraries. See [target, module, and lib](../13-compiler-and-tsconfig/02-target-module-and-lib.md).

## A base class for your application

Rather than writing the same boilerplate in every error, define one base and extend it:

```ts
type ErrorCode = "NOT_FOUND" | "VALIDATION" | "UNAUTHORIZED" | "CONFLICT" | "INTERNAL";

class AppError extends Error {
  constructor(
    message: string,
    readonly code: ErrorCode,
    options?: { cause?: unknown },
  ) {
    super(message, options);
    this.name = new.target.name;
  }
}

class NotFoundError extends AppError {
  constructor(resource: string, id: string, options?: { cause?: unknown }) {
    super(`${resource} ${id} not found`, "NOT_FOUND", options);
  }
}

class ValidationError extends AppError {
  constructor(message: string, readonly issues: { path: string; message: string }[]) {
    super(message, "VALIDATION");
  }
}
```

Notes:

- `super(message, options)` forwards `cause` to the built-in `Error` (needs `lib` ES2022 or later).
- `new.target.name` sets `name` to the class name automatically. In **minified** builds, class names can be mangled, so if the name is part of your logging or matching, set it explicitly with a string instead.
- `code` is a **literal union**, so a `switch` over it can be checked for exhaustiveness.

## Distinguishing errors

Three common ways, from most to least preferred for application logic:

```ts
// 1. instanceof: simple, works within one codebase
if (e instanceof NotFoundError) { /* ... */ }

// 2. a discriminant property: survives serialization and cross-realm
if (e instanceof AppError && e.code === "NOT_FOUND") { /* ... */ }

// 3. matching on e.name or e.message: avoid, fragile
```

For exhaustive handling of known codes:

```ts
function toStatus(e: AppError): number {
  switch (e.code) {
    case "NOT_FOUND":     return 404;
    case "VALIDATION":    return 400;
    case "UNAUTHORIZED":  return 401;
    case "CONFLICT":      return 409;
    case "INTERNAL":      return 500;
    default: {
      const _exhaustive: never = e.code;   // compile error if a code is missing
      return _exhaustive;
    }
  }
}
```

See [exhaustiveness checking](../03-unions-and-narrowing/06-exhaustiveness-checking.md).

## Wrapping and causes

Add context as an error passes through a layer, without losing the original:

```ts
async function getUser(id: string) {
  try {
    return await repo.findById(id);
  } catch (e) {
    throw new AppError(`Failed to load user ${id}`, "INTERNAL", { cause: e });
  }
}
```

Log the whole chain, not just the top error. Many loggers print `cause` for you. Otherwise walk it:

```ts
function* causeChain(e: unknown): Generator<unknown> {
  let current: unknown = e;
  while (current) {
    yield current;
    current = current instanceof Error ? current.cause : undefined;
  }
}
```

## Serialization

Error fields like `message` and `stack` are **not enumerable**, so `JSON.stringify(error)` gives `{}` (plus any own enumerable properties you added, such as `code`). When you need to send or log an error as data, convert it explicitly:

```ts
class AppError extends Error {
  // ...
  toJSON() {
    return { name: this.name, code: this.code, message: this.message };
  }
}
```

Do **not** send `stack` or internal `cause` details to clients. Map errors to a safe response shape at the boundary ([error response types](../16-type-safe-apis/04-error-response-types.md)).

## Practical guidelines

- **Few, meaningful classes.** Create a class when callers need to *react differently*. A hundred error classes nobody checks is noise. A `code` field often does the same job with less ceremony.
- **Errors for expected conditions, bugs for the rest.** `NotFoundError` is a normal outcome. A `TypeError` from your own bug should propagate and be logged, not be wrapped into a friendly error and hidden.
- **Include data, not prose.** Put the id, the field, the status in typed properties. Keep `message` short and human-readable.
- **Make fields `readonly`.** An error is a record of what happened.
- **Keep errors free of secrets.** They end up in logs and sometimes in responses.

## Common mistakes

- **Forgetting `Object.setPrototypeOf` on an ES5 target,** so `instanceof` fails.
- **Not setting `name`,** so every error logs as `Error`.
- **Checking errors by message text.** Messages change, are translated, and are not part of the contract.
- **Throwing from a constructor of an error class** or doing heavy work in it.
- **Losing the original error** by creating a new one without `cause`.
- **Putting sensitive data in `message`.**
- **Relying on `instanceof` across package boundaries.** Two copies of the same library in `node_modules` produce two distinct classes. Use a `code` discriminant when errors cross package or process boundaries.

## Debugging

- If `instanceof` returns `false` unexpectedly, check `target`, check whether the error crossed a package or realm boundary, and check that you are comparing against the same class object.
- If a log shows `Error: ...` instead of your class name, `name` was not set.
- If `cause` is missing, check that `lib` includes ES2022 and that you passed it to `super`.
- Print `e.stack` and the `cause` chain together to see the whole path.

## Quick summary

- Extend `Error`, call `super`, and set `name`. Add typed fields such as `code`, `status`, or `field`.
- On ES5 targets, restore the prototype with `Object.setPrototypeOf(this, new.target.prototype)`.
- Use an `AppError` base class with a literal-union `code`, so handling can be exhaustive.
- Wrap with `{ cause }` to add context without losing the original.
- Errors do not serialize by default, and they must not leak internals to clients.
- Few, meaningful classes beat many unused ones.

**Next:** [The Result pattern](./02-result-pattern.md)