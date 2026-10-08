# Builder

A **builder** constructs a complex object step by step, usually through a chain of method calls, and produces the final object at the end. It exists to tame constructors with many parameters, many optional settings, or construction rules. In TypeScript, much of the traditional need for builders is met by **options objects** and optional properties, so it is worth knowing both what the pattern offers and when a simpler tool is better.

**Prerequisites:**
- [Classes](../05-classes/00-classes.md)
- [Generic types](../06-generics/01-generic-types.md)
- [Readonly and optional properties](../04-objects-and-interfaces/02-readonly-and-optional-properties.md)

---

## The problem

```ts
const req = new HttpRequest("https://api.example.com", "POST", undefined, 5000, true, false, "json");
```

Positional parameters are unreadable, easy to mix up (`true, false`), and every new option makes the constructor worse.

## First choice in TypeScript: an options object

```ts
interface RequestOptions {
  url: string;
  method?: "GET" | "POST" | "PUT" | "DELETE";
  headers?: Record<string, string>;
  timeoutMs?: number;
  retries?: number;
}

function createRequest({ url, method = "GET", headers = {}, timeoutMs = 10_000, retries = 0 }: RequestOptions) {
  return { url, method, headers, timeoutMs, retries };
}

createRequest({ url: "https://api.example.com", method: "POST", timeoutMs: 5000 });
```

This gives you named arguments, defaults, optional fields, and compile-time checking, with no extra classes. The compiler enforces required properties (`url`) and rejects unknown ones. Reach for this **before** a builder.

## When a builder is worth it

Use a builder when the options object is not enough:

- **Construction happens in steps over time** (collecting pieces across functions or conditionally).
- **Steps must happen in an order, or some are required**, and you want the compiler to enforce that.
- **Building is complex** (validation across fields, derived values, assembling nested structures).
- **A fluent API** reads clearly for the domain (query builders, test data, request pipelines).

## A basic fluent builder

```ts
interface Config {
  url: string;
  method: "GET" | "POST";
  headers: Record<string, string>;
  timeoutMs: number;
}

class RequestBuilder {
  private config: Partial<Config> = { method: "GET", headers: {}, timeoutMs: 10_000 };

  url(url: string): this {
    this.config.url = url;
    return this;
  }

  method(method: Config["method"]): this {
    this.config.method = method;
    return this;
  }

  header(name: string, value: string): this {
    this.config.headers = { ...this.config.headers, [name]: value };
    return this;
  }

  build(): Config {
    if (!this.config.url) throw new Error("url is required");
    return this.config as Config;
  }
}

const config = new RequestBuilder().url("https://api.example.com").method("POST").header("X-Id", "1").build();
```

- Each method returns `this`, which enables chaining. The return type `this` (rather than `RequestBuilder`) keeps subclasses chainable.
- `build()` validates and returns the finished object.
- The weakness: **required fields are checked at runtime**. Forgetting `.url()` compiles and throws when `build()` runs.

## Making the compiler enforce required steps

You can track which required fields have been set **in the type**, so `build()` is only callable when everything required is present. Each step returns a new builder with an updated type parameter:

```ts
type Method = "GET" | "POST";

interface Config {
  url: string;
  method: Method;
  headers: Record<string, string>;
}

type RequiredKey = "url" | "method";

class RequestBuilder<Done extends RequiredKey = never> {
  private constructor(private readonly cfg: Partial<Config>) {}

  static create(): RequestBuilder {
    return new RequestBuilder({ headers: {} });
  }

  url(url: string): RequestBuilder<Done | "url"> {
    return new RequestBuilder({ ...this.cfg, url });
  }

  method(method: Method): RequestBuilder<Done | "method"> {
    return new RequestBuilder({ ...this.cfg, method });
  }

  header(name: string, value: string): RequestBuilder<Done> {
    return new RequestBuilder({ ...this.cfg, headers: { ...this.cfg.headers, [name]: value } });
  }

  build(
    ...missing: [Exclude<RequiredKey, Done>] extends [never] ? [] : [error: `Missing: ${Exclude<RequiredKey, Done>}`]
  ): Config {
    return this.cfg as Config;
  }
}

RequestBuilder.create().url("/a").method("GET").build();   // ok

RequestBuilder.create().url("/a").build();
// error: Expected 1 arguments, but got 0 ... (parameter 'error' is "Missing: method")
```

How it works:

- `Done` accumulates the names of required fields set so far (a **type-state**).
- Each method returns a **new** builder whose type includes the field it set.
- `build` has a rest parameter whose type depends on what is still missing. When nothing is missing it takes no arguments. When something is missing it demands an argument of an impossible-to-satisfy literal type, which makes the call a compile error with a message naming the missing field.
- The final `as Config` is a contained assertion: the type state proves it.

Returning new builders (immutable style) also lets you reuse a half-built builder as a template without one chain affecting another. The mutable version above shares state across calls.

This is powerful, and it costs complexity. Use it where forgetting a step is a real, repeated bug, such as in a library API. For application code, an options object with required properties gives the same guarantee more simply.

## Test data builders

A very practical use, and a lighter shape of the pattern: functions that create valid objects with sensible defaults, overridden per test.

```ts
let nextId = 1;

function buildUser(overrides: Partial<User> = {}): User {
  return {
    id: String(nextId++),
    name: "Test User",
    email: `user${nextId}@example.com`,
    role: "member",
    createdAt: new Date("2024-01-01"),
    ...overrides,
  };
}

const admin = buildUser({ role: "admin" });
const old = buildUser({ createdAt: new Date("2000-01-01") });
```

Tests state only what matters to them, and when `User` gains a required field you update one function. This is a builder in spirit, with no chain at all. See [unit testing](../18-testing-and-debugging/00-unit-testing.md).

## Query builders and fluent APIs

Many libraries expose builders: SQL query builders, validation schemas, HTTP clients. Their TypeScript value is **progressive typing**, where each call refines the result type:

```ts
declare const query: QueryBuilder<User>;

const rows = await query
  .select("id", "email")          // result type becomes Pick<User, "id" | "email">
  .where("role", "=", "admin")    // "role" and its value are type-checked against User
  .limit(10)
  .execute();
```

These use generics, `keyof`, and mapped types so the builder tracks the shape of the result ([keyof and typeof](../06-generics/03-keyof-and-typeof.md), [mapped types](../10-advanced-types/03-mapped-types.md)). Writing one is an advanced exercise, and usually unnecessary. Using one is common.

## Builder vs related patterns

| Pattern | Use when |
|---|---|
| **Options object** | many optional settings, one construction step |
| **Builder** | step-by-step or conditional assembly, ordered or required steps |
| **[Factory](./00-factory.md)** | choosing *which* object to create |
| **Test data builder** | valid defaults plus per-test overrides |
| **`Partial<T>` and spread** | updating an existing object immutably |

## Common mistakes

- Building a class-based builder for an object that an options object would handle.
- Required fields validated only at runtime in an API where compile-time enforcement is feasible.
- Mutating a shared builder instance across chains and getting leaked state.
- A builder that returns `this` typed as the base class, breaking chaining in subclasses.
- Forgetting to copy nested mutable values (`headers`) when cloning.
- Building giant type-state builders that nobody can read or debug.
- Letting `build()` return a type that claims more than the builder actually validated.

## Debugging

- If the type-state builder's error is unclear, hover `build` to see the computed rest parameter type and which fields are missing.
- If chain methods stop being available after a call, check that the method returns the builder type (or `this`) rather than `void`.
- For leaked state between builders, check whether methods mutate `this.config` and whether instances are shared.
- If the `as Config` cast hides a bug, add a runtime check in `build()` as well for non-type-safe callers (plain JavaScript, `any`).

## Quick summary

- A builder assembles a complex object step by step and produces it with `build()`.
- In TypeScript, try an **options object** first: named, optional, defaulted, and type-checked.
- Use a builder for stepwise or conditional assembly, a fluent domain API, or compile-time enforcement of required steps.
- A type-state builder tracks completed steps in a generic parameter, and a conditional rest parameter on `build` rejects incomplete use.
- Test data builders (defaults plus `Partial<T>` overrides) are the most practical everyday form.

**Next:** [Strategy](./02-strategy.md)
