# Typed Fetch and API Client

`fetch` is untyped at the edges: `res.json()` returns `any`, a path is just a string, and you can send any body to any URL. A typed client closes those gaps, so a call with the wrong parameters fails to compile and a response that does not match the contract fails loudly **at the boundary**. This note builds one up in stages, from a tiny wrapper to a route-table client that derives its types from runtime schemas.

**Prerequisites:**
- [API contracts](./00-api-contracts.md)
- [Error response types](./04-error-response-types.md)
- [Zod](../15-runtime-validation/02-zod.md) and [validation recipes](../15-runtime-validation/04-validation-recipes.md)
- [Template literal types](../10-advanced-types/04-template-literal-types.md) (for path parameter extraction)

---

## Stage 0: the naive wrapper that lies

```ts
async function getJson<T>(url: string): Promise<T> {
  const res = await fetch(url);
  return res.json();                  // any, silently accepted as T
}

const user = await getJson<User>("/api/users/1");   // looks typed, is a claim
```

The generic parameter is an **assertion in disguise**. TypeScript cannot check it, so a wrong shape surfaces later as an unrelated `TypeError`. It also ignores HTTP errors. `fetch` only rejects on network failure, so a `404` or `500` flows through as if it were data.

## Stage 1: validate the response

Pass the schema in, and let it produce the type:

```ts
import { z } from "zod";

async function request<S extends z.ZodType>(url: string, schema: S, init?: RequestInit): Promise<z.output<S>> {
  const res = await fetch(url, init);
  if (!res.ok) throw new Error(`HTTP ${res.status}`);
  return schema.parse(await res.json());
}

const user = await request("/api/users/1", UserSchema);   // typed from the schema, and checked
```

This is the single biggest improvement: the type is now **proven** at the boundary. What is still weak: the URL is a free string, parameters and bodies are not tied to the endpoint, and errors are a bare `Error`.

## Stage 2: a route table

Describe every endpoint once, as a runtime object, and derive everything from it:

```ts
import { z } from "zod";

const UserDto = z.object({ id: z.string(), name: z.string(), email: z.string() });
const CreateUserBody = z.object({ name: z.string().min(1), email: z.string().email() });

export const routes = {
  "GET /users": { response: z.array(UserDto) },
  "GET /users/:id": { response: UserDto },
  "POST /users": { body: CreateUserBody, response: UserDto },
} satisfies Record<string, { body?: z.ZodType; response: z.ZodType }>;

type Routes = typeof routes;
type RouteKey = keyof Routes & string;
```

`satisfies` checks that every entry has the right shape while keeping each entry's precise type ([type assertions and satisfies](../03-unions-and-narrowing/07-type-assertions-and-satisfies.md)). The same table is the runtime source of response schemas and the compile-time source of types.

### Deriving the call signature

The path template tells us which parameters are required, using the pattern-matching types from [template literal types](../10-advanced-types/04-template-literal-types.md):

```ts
type ParamNames<S extends string> =
  S extends `${string}:${infer P}/${infer Rest}` ? P | ParamNames<`/${Rest}`> :
  S extends `${string}:${infer P}` ? P :
  never;

type PathOf<K extends string> = K extends `${string} ${infer P}` ? P : never;

type ParamsOption<K extends string> =
  [ParamNames<PathOf<K>>] extends [never] ? {} : { params: Record<ParamNames<PathOf<K>>, string> };

type BodyOption<K extends RouteKey> =
  Routes[K] extends { body: infer B extends z.ZodType } ? { body: z.input<B> } : {};

type CommonOptions = {
  query?: Record<string, string | number | boolean | undefined>;
  signal?: AbortSignal;
};

type Options<K extends RouteKey> = ParamsOption<K> & BodyOption<K> & CommonOptions;

// Options are required if any required key exists; otherwise the argument is optional
type Args<K extends RouteKey> = {} extends Options<K> ? [options?: Options<K>] : [options: Options<K>];

type Output<K extends RouteKey> = z.output<Routes[K]["response"]>;
```

What these compute for each route:

| Route | `params` | `body` | Argument |
|---|---|---|---|
| `"GET /users"` | none | none | optional |
| `"GET /users/:id"` | `{ id: string }` | none | **required** |
| `"POST /users"` | none | `{ name: string; email: string }` | **required** |

The `{} extends Options<K>` check is a standard trick: it is `true` only when every property of `Options<K>` is optional.

### The implementation

```ts
type RuntimeOptions = {
  params?: Record<string, string>;
  query?: CommonOptions["query"];
  body?: unknown;
  signal?: AbortSignal;
};

const BASE_URL = "https://api.example.com";

export async function api<K extends RouteKey>(route: K, ...args: Args<K>): Promise<Output<K>> {
  const options: RuntimeOptions = ((args as unknown[])[0] as RuntimeOptions | undefined) ?? {};

  const [method, template] = route.split(" ") as [string, string];

  const path = template.replace(/:(\w+)/g, (_match, name: string) => {
    const value = options.params?.[name];
    if (value === undefined) throw new Error(`Missing path parameter: ${name}`);
    return encodeURIComponent(value);
  });

  const url = new URL(BASE_URL + path);
  for (const [key, value] of Object.entries(options.query ?? {})) {
    if (value !== undefined) url.searchParams.set(key, String(value));
  }

  const headers = new Headers({ Accept: "application/json" });
  if (options.body !== undefined) headers.set("Content-Type", "application/json");
  const token = getAuthToken();
  if (token) headers.set("Authorization", `Bearer ${token}`);

  const res = await fetch(url, {
    method,
    headers,
    body: options.body !== undefined ? JSON.stringify(options.body) : undefined,
    signal: options.signal,
  });

  if (!res.ok) throw await ApiClientError.fromResponse(res);

  const schema = routes[route].response;
  return schema.parse(await res.json()) as Output<K>;
}
```

The casts inside are contained and explained: the **public signature** is precisely typed, and inside, the generic machinery cannot be followed by the compiler, so we step to runtime-shaped types (`RuntimeOptions`) in one place. This is the "contain your assertions" rule ([soundness and escape hatches](../14-type-system-internals/04-soundness-and-escape-hatches.md)).

### Using it

```ts
const users = await api("GET /users");
// { id: string; name: string; email: string }[]

const user = await api("GET /users/:id", { params: { id: "1" } });
// { id: string; name: string; email: string }

const created = await api("POST /users", { body: { name: "Asha", email: "a@b.com" } });

await api("GET /users/:id");                          // error: options are required
await api("GET /users/:id", { params: {} });          // error: missing 'id'
await api("POST /users", { body: { name: "Asha" } }); // error: missing 'email'
await api("PUT /users/:id");                          // error: not a known route
```

Autocomplete lists the valid routes, and each call shows exactly the options it needs. Renaming a route key or changing a body schema highlights every affected call site.

## Errors

Turn failed responses into one typed error, using the error contract from [error response types](./04-error-response-types.md):

```ts
export class ApiClientError extends Error {
  constructor(
    readonly status: number,
    readonly error: ApiError,
    readonly requestId?: string,
  ) {
    super(error.message);
    this.name = "ApiClientError";
  }

  static async fromResponse(res: Response): Promise<ApiClientError> {
    const error = await readError(res);   // defensive parsing: handles non-JSON bodies
    return new ApiClientError(res.status, error, res.headers.get("x-request-id") ?? undefined);
  }
}
```

Callers narrow on the class and then on `error.code`:

```ts
try {
  await api("POST /users", { body });
} catch (e) {
  if (e instanceof ApiClientError && e.error.code === "VALIDATION") {
    showFieldErrors(e.error.issues);
  } else {
    throw e;                              // network errors, bugs, aborts: not handled here
  }
}
```

Three kinds of failure to keep distinct:

| Failure | How it appears |
|---|---|
| **Transport** (offline, DNS, timeout, abort) | `fetch` **rejects** (`TypeError`, `AbortError`, `TimeoutError`) |
| **HTTP error** (`4xx`, `5xx`) | `fetch` resolves with `res.ok === false` and your client throws `ApiClientError` |
| **Contract violation** (2xx but wrong shape) | `schema.parse` throws a validation error |

The third one is the most valuable to catch: it means client and server disagree, which is exactly what the validation is for.

If callers should handle failures as values instead of exceptions, wrap the client with the `tryCatchAsync` helper from [the Result pattern](../11-error-handling/02-result-pattern.md):

```ts
const result = await tryCatchAsync(() => api("GET /users/:id", { params: { id } }));
if (!result.ok) { /* result.error is the thrown value, narrow it */ }
```

## Practical concerns

### Authentication

Inject credentials in one place (as above). For token refresh, handle a `401` by refreshing once and retrying the request once, guarding against loops. Keep that logic inside the client, not in every caller.

### Timeouts and cancellation

A request without a timeout can hang forever. Pass an `AbortSignal`, or default to one:

```ts
signal: options.signal ?? AbortSignal.timeout(10_000)   // where AbortSignal.timeout is supported
```

Expose `signal` so UI code can cancel superseded requests ([concurrency patterns](../12-async-and-iteration/05-concurrency-patterns.md)). Data-fetching libraries pass a signal into the query function:

```ts
useQuery({
  queryKey: ["user", id],
  queryFn: ({ signal }) => api("GET /users/:id", { params: { id }, signal }),
});
```

See [server state with TanStack Query](../19-react-and-frontend/07-server-state-tanstack-query.md).

### Retries

Retry only **idempotent** requests (`GET`, `PUT`, `DELETE`) on **transient** failures (network errors, `502`/`503`/`504`, `429` with `Retry-After`), with backoff and a cap ([error handling strategies](../11-error-handling/03-error-handling-strategies.md)). Do not retry `POST` unless the endpoint supports idempotency keys.

### Empty responses

`204 No Content` has no body, so `res.json()` would throw. Handle it explicitly, with a response schema for "nothing" (for example `z.void()` or `z.undefined()`) and a branch that skips parsing when `res.status === 204`.

### Serialization

Query values are strings. Arrays and nested objects need a convention (`tag=a&tag=b`, or comma-separated). Document it in the [contract](./00-api-contracts.md), and encode it in one helper rather than at each call site.

### Validating requests too

You can parse the **body** against its schema on the client before sending (`routes[route].body?.parse(options.body)`) to fail fast. The server must still validate again, since clients cannot be trusted.

## Testing

Mock at the network level so the whole client, including URL building and parsing, is exercised:

```ts
import { http, HttpResponse } from "msw";
import { setupServer } from "msw/node";

const server = setupServer(
  http.get("https://api.example.com/users/:id", () =>
    HttpResponse.json({ id: "1", name: "Asha", email: "a@b.com" }),
  ),
);

beforeAll(() => server.listen());
afterAll(() => server.close());

it("returns a validated user", async () => {
  const user = await api("GET /users/:id", { params: { id: "1" } });
  expect(user.name).toBe("Asha");
});
```

Also test the failure paths: a `404` with your error body, a `502` with an HTML body, and a `200` with the wrong shape. See [integration testing](../18-testing-and-debugging/01-integration-testing.md) and [mocking](../18-testing-and-debugging/02-mocking.md).

## Alternatives to hand-rolling

| Option | What it gives you |
|---|---|
| **Generated clients** from an OpenAPI document (for example `openapi-typescript` with `openapi-fetch`) | types and paths derived from a spec ([contract-first APIs](./06-contract-first-apis.md)) |
| **RPC frameworks** (for example tRPC) | the client's types come from the server's procedures, with no separate route table |
| **HTTP helper libraries** (`ky`, `ofetch`, and similar) | retries, hooks, JSON handling, but typing the contract is still up to you |
| **Data-fetching libraries** (TanStack Query, SWR) | caching and state on top of any client |

A hand-written client like this one suits a full-stack TypeScript team that owns both sides and wants a small, readable, dependency-light solution. Check the current documentation of each tool before adopting it.

## Important rules and misconceptions

- **`fetch<T>`-style generics are claims.** Only a runtime check makes the type true.
- **`fetch` does not reject on HTTP errors.** Check `res.ok`.
- **Types describe the route table, not the server.** If the server disagrees, validation catches it, or nothing does.
- **A generic response type with no schema is unvalidated,** however neat the call site looks.
- **Compile-time safety needs literal route strings.** If a route key is computed as `string`, you lose the checking.

## Common mistakes

- Using `res.json() as T`.
- Ignoring non-2xx statuses.
- Typing URLs as plain strings and building them with concatenation.
- Not encoding path parameters (`encodeURIComponent`).
- Sending `undefined` in query objects, producing `"undefined"` in the URL.
- No timeout or cancellation.
- Retrying non-idempotent requests.
- Parsing a `204` as JSON.
- Treating the error body as always JSON in your format.
- Duplicating the contract in the client, server, and tests rather than sharing it.

## Debugging

- If the compiler rejects a valid call, hover `Options<"...">` for that route to see what it computes, and check the path template spelling.
- If a call that should be an error compiles, check whether `K` widened to `string` (a variable instead of a literal route).
- If parsing fails, log the schema issues (path and message) together with the raw body.
- Inspect the actual request in the browser's network panel or with `curl -i`.
- If types become slow in the editor, reduce the size of the route table's type computations by splitting it per feature.

## Quick summary

- A typed client closes `fetch`'s gaps: validated responses, typed parameters and bodies, one error type.
- Stage up: validate with a schema, then drive everything from a route table of schemas, with path parameter types derived from the route string.
- Keep casts inside the implementation. The public signature is where type safety lives.
- Separate transport errors, HTTP errors, and contract violations. Add auth, timeouts, cancellation, and careful retries in the client.
- Test with a network-level mock, including error and wrong-shape responses.
- Consider generated clients or RPC when a spec or a server-owned contract already exists.

**Next:** [Contract-first APIs](./06-contract-first-apis.md)