# API Architecture

The [API integration folder](../11-api-integration/README.md) built the **mechanics**: a client, auth, refresh, error handling. This note is about **organization at scale**: where each piece of the API layer lives, how it's shared across features, and how to keep a growing app from turning into 300 scattered `api.get(...)` calls with 40 slightly different conventions.

## The pieces

An API layer has several distinct jobs. Keeping them in separate places is most of the architecture:

```text
┌─────────────────────────────────────────────────────────┐
│ Components                                              │  never touch the layers below directly
├─────────────────────────────────────────────────────────┤
│ Query layer       hooks / queryOptions / key factories  │  caching, loading, invalidation
├─────────────────────────────────────────────────────────┤
│ Endpoint modules  projectsApi.list(), .create() …       │  one function per backend operation
├─────────────────────────────────────────────────────────┤
│ Mapping/schemas   DTO → domain, validation              │  insulates from the wire format
├─────────────────────────────────────────────────────────┤
│ HTTP client       base URL, auth, errors, cancellation  │  transport, shared by everything
└─────────────────────────────────────────────────────────┘
```

| Piece | Responsibility | Lives in |
|---|---|---|
| **HTTP client** | Transport: base URL, headers, token, JSON, timeouts, `ApiError`, abort | `shared/lib/api/` |
| **Endpoint modules** | One typed function per backend operation | `features/<name>/api/*.api.ts` |
| **Schemas/mappers** | Validate responses, translate DTOs to domain types | `features/<name>/domain/` or next to the endpoints |
| **Query layer** | Query keys, `queryOptions`, hooks, mutation hooks, invalidation | `features/<name>/api/*.queries.ts` and `hooks/` |
| **Cross-cutting** | Auth refresh, request IDs, tracing, logging | The client, via a small middleware/hook mechanism |

(Client mechanics: [API client](../11-api-integration/02-api-client.md). This note doesn't repeat them.)

## One client, many endpoint modules

The client is **shared infrastructure**: one instance, configured once. Each feature owns **its own endpoints**:

```ts
// shared/lib/api/client.ts: transport only; knows nothing about projects or billing
export const api = { get, post, put, patch, delete: del }

// features/projects/api/projects.api.ts: knows the projects endpoints
import { api } from "@/shared/lib/api/client"
import { projectListSchema, projectSchema } from "./projects.schemas"

export const projectsApi = {
  list: async (filters: ProjectFilters, signal?: AbortSignal) =>
    projectListSchema.parse(await api.get("/projects", { params: filters, signal })),

  get: async (id: string, signal?: AbortSignal) =>
    projectSchema.parse(await api.get(`/projects/${id}`, { signal })),

  create: async (input: NewProject) =>
    projectSchema.parse(await api.post("/projects", input)),
}
```

Rules that keep this healthy:

- **Components never call `api` or `fetch`.** They use hooks from the query layer.
- **Endpoint functions are thin and typed**: path, params, parse. No UI logic, no caching, no toasts.
- **URLs live in exactly one place** (the endpoint module). Changing an endpoint is a one-file edit.
- **One function per operation**, named for the operation (`list`, `get`, `create`, `archive`), not for the HTTP verb.
- Endpoint modules accept an optional `AbortSignal` so the query layer can cancel.

## The query layer: keys, options, hooks

Query definitions are the **contract between the API layer and the UI**. Define them next to the endpoints, and export them as the feature's data interface:

```ts
// features/projects/api/projects.queries.ts
import { queryOptions, keepPreviousData } from "@tanstack/react-query"
import { projectsApi } from "./projects.api"

export const projectKeys = {
  all:     ["projects"] as const,
  lists:   () => [...projectKeys.all, "list"] as const,
  list:    (filters: ProjectFilters) => [...projectKeys.lists(), filters] as const,
  details: () => [...projectKeys.all, "detail"] as const,
  detail:  (id: string) => [...projectKeys.details(), id] as const,
}

export const projectQueries = {
  list: (filters: ProjectFilters) => queryOptions({
    queryKey: projectKeys.list(filters),
    queryFn: ({ signal }) => projectsApi.list(filters, signal),
    placeholderData: keepPreviousData,
  }),
  detail: (id: string) => queryOptions({
    queryKey: projectKeys.detail(id),
    queryFn: ({ signal }) => projectsApi.get(id, signal),
    staleTime: 60_000,
  }),
}
```

```ts
// features/projects/hooks/useProjects.ts: what the UI imports
export const useProjects = (filters: ProjectFilters) => useQuery(projectQueries.list(filters))
export const useProject = (id: string) => useQuery(projectQueries.detail(id))

export function useCreateProject() {
  const queryClient = useQueryClient()
  return useMutation({
    mutationFn: projectsApi.create,
    onSuccess: () => queryClient.invalidateQueries({ queryKey: projectKeys.lists() }),
  })
}
```

Why this structure scales:

- **Key factories and `queryOptions` are the single source of truth**, so route loaders, prefetching, components, and tests all use the same definitions ([TanStack Query](../12-server-state/03-tanstack-query.md#queryoptions-define-once-use-anywhere)).
- **Hierarchical keys** make invalidation precise and predictable ([invalidation](../12-server-state/04-caching-and-synchronization.md#invalidation)).
- **Cache policy lives with the query** (`staleTime`, `placeholderData`), per kind of data, not scattered in components.
- **The feature controls its own invalidation.** Mutations know which of *their own* queries they affect.

### Cross-feature invalidation

Sometimes a mutation in one feature makes another's data stale (archiving a project changes billing usage). Options:

1. **Export an invalidation helper** from the owning feature: `invalidateBilling(queryClient)`. The other feature calls it through the public API ([00](./00-feature-based-architecture.md#the-public-api-indexts)).
2. **Invalidate by a shared key namespace** that's part of an agreed contract (`["billing"]`), exported from the owner.
3. **Prefer server-driven refresh**: if the backend can return what changed, update the caches, or push an event ([realtime](../11-api-integration/06-realtime-communication.md#wiring-events-into-your-data-layer)).

Avoid reaching into another feature's key factory internals, since keys are private implementation unless exported.

## Types: DTOs, domain, and validation

Three kinds of types appear, and conflating them causes pain:

| Type | Meaning | Example |
|---|---|---|
| **DTO** (wire format) | Exactly what the API sends/receives | `{ id, created_at: string, owner_id }` |
| **Domain model** | What your app reasons about | `{ id, createdAt: Date, owner: UserRef }` |
| **View model** | Shaped for one screen | `{ title, statusLabel, canEdit }` |

Practical guidance:

- **Validate at the boundary** with a schema (`zod` or similar), parsing `unknown` into a typed value. The schema is both runtime check and type source ([validating responses](../11-api-integration/02-api-client.md#validating-responses)), and can transform DTO to domain in one step:

```ts
export const projectSchema = z.object({
  id: z.string(),
  name: z.string(),
  created_at: z.string().datetime(),
  owner_id: z.string(),
}).transform((dto) => ({
  id: dto.id,
  name: dto.name,
  createdAt: new Date(dto.created_at),
  ownerId: dto.owner_id,
}))
export type Project = z.output<typeof projectSchema>
```

- If you **control the backend**, prefer **generating types from its contract** rather than hand-writing them: OpenAPI generators (such as `openapi-typescript` or full client generators like Orval or Hey API), GraphQL codegen, or end-to-end typed RPC (tRPC) when frontend and backend share a TypeScript codebase. Check each tool's current docs. Generated types make "the backend changed and the frontend didn't notice" a compile error instead of a production bug.
- **Don't validate everything on every call** if payloads are large and the API is trusted and typed from a contract. Validate at least for third-party APIs and critical paths, since parse cost is real.
- Keep DTO types **private to the API layer** and export domain types. UI code shouldn't know about `created_at`.

## Contracts and change management

The API is a contract between teams and between deploys. Treat changes accordingly:

- **Backwards-compatible by default**: add fields, don't rename or remove. Frontend and backend deploy independently, and an old frontend must survive a new backend ([deployment rollbacks](../19-production/02-deployment.md#rollbacks)).
- **Version deliberately** (path or header) when breaking changes are unavoidable, and migrate callers feature by feature.
- **Single source of truth for the contract** (OpenAPI/GraphQL schema/shared types), checked in CI. Run **contract tests** or compare generated types in the pipeline to catch drift.
- **Mock from the contract**: MSW handlers typed with the same types, so mocks can't invent shapes ([MSW](../18-testing-and-debugging/04-mocking-and-msw.md)).
- **Log request IDs** (`X-Request-Id`) so frontend errors can be matched to backend logs ([error architecture](./03-error-handling-architecture.md#correlation-ids)).

## Cross-cutting concerns in the client

Things every request needs belong in the **client**, not in endpoint modules or components:

- **Authentication**: attach the token, refresh on 401, single-flight ([refresh flow](../11-api-integration/04-refresh-token-flow.md)).
- **Error normalization**: every failure becomes an `ApiError` with status, code, and details ([API errors](../11-api-integration/05-api-error-handling.md)).
- **Tracing/correlation**: add a request or trace ID header.
- **Locale and tenant headers** (`Accept-Language`, `X-Tenant`).
- **Timeouts and cancellation** (`AbortSignal.timeout`, forwarded signals).
- **Metrics**: record durations and statuses for [performance monitoring](../19-production/07-performance-monitoring.md#tracing-and-api-performance).

If the client gets crowded, give it a **small middleware pipeline** (an array of functions that wrap a request) so concerns compose instead of piling into one giant function.

## Multiple backends and BFFs

- **Multiple APIs**: create **one client instance per backend** (own base URL, auth, error mapping), and place the endpoints for each in the owning feature. Don't thread a base URL through call sites.
- **Backend-for-frontend (BFF)**: a thin server layer shaped for your UI that **aggregates** several services, hides secrets, and returns screen-sized payloads. It removes waterfalls, avoids over-fetching, and keeps keys out of the browser ([env vars](../19-production/00-environment-variables.md#how-to-use-a-secret-anyway)). It adds a deployable to own, so use it when API composition on the client is genuinely painful.
- **GraphQL**: changes the shape of the layer (queries instead of endpoint functions, a normalized client cache), but the same principles apply: typed boundary, colocated operations, no transport in components.

## Server state vs the rest

Keep the **API layer about server data only**. Don't push UI state (open dialogs, selected tab) into query keys or the query cache, and don't copy server data into global stores ([server vs client state](../12-server-state/00-server-vs-client-state.md)). Search/filter/page state that identifies a *view of server data* belongs in the **URL** and flows into query keys ([URL state](../10-routing/06-search-filter-and-url-state.md)).

## Conventions worth writing down

A short team doc (or ADR, [04](./04-scaling-large-applications.md#architecture-decision-records)) prevents drift:

1. Endpoints live in `features/<name>/api/*.api.ts`; the client in `shared/lib/api`.
2. Components use hooks only; no `fetch`/`api` in components.
3. Query keys come from factories; never inline arrays.
4. Responses are validated or typed from the contract; DTOs don't leave the API layer.
5. Mutations invalidate their own feature's keys; cross-feature through exported helpers.
6. Cache policy (`staleTime`) is set per query, with a sensible global default.
7. Errors are `ApiError`s; handling follows the [error architecture](./03-error-handling-architecture.md).
8. Every endpoint has a typed MSW handler for tests.

## Common mistakes

- **API calls scattered through components**, with URLs duplicated and inconsistent error handling.
- **One giant `api.ts`** for the whole app, instead of endpoint modules per feature.
- **Query keys as inline literals**, causing typos, split caches, and missed invalidation.
- **DTOs used directly across the UI**, so backend renames ripple everywhere.
- **No validation or generated types**, so contract drift only shows up in production.
- **Business logic in endpoint modules or the client** (pricing rules, UI decisions).
- **Auth and error handling re-implemented per call site** instead of centrally.
- **Features reaching into each other's query keys and internals** for invalidation.
- **Mixing UI state into the query cache**, or server data into global stores.
- **Breaking API changes with no versioning** or deployment ordering.
- **Hand-writing types for a backend you control**, and letting them rot.
- **Mocks that don't match the contract**, so tests pass against an API that doesn't exist.
- **Over-abstracting** (a generic repository/adapter layer with one implementation).

## Quick summary

- Separate the API layer's jobs: **HTTP client** (shared transport), **endpoint modules** (per feature, one function per operation), **schemas/mappers** (validation and DTO → domain), **query layer** (keys, `queryOptions`, hooks), and **cross-cutting** concerns in the client.
- **Components use hooks only**: no `fetch` or `api` calls in UI.
- Define **key factories and `queryOptions`** beside the endpoints as the feature's data contract; keep cache policy there.
- Distinguish **DTO / domain / view** types; validate at the boundary or generate types from the contract (OpenAPI, GraphQL codegen, tRPC).
- Treat the API as a **versioned, backwards-compatible contract**; mock from it, test against it, and correlate with request IDs.
- Cross-feature invalidation goes through **exported helpers or shared key namespaces**, not internals.
- Use one client per backend, a BFF when composition hurts, and keep server state separate from UI state.
- Write the conventions down and enforce them with lint and review.

## Next

[03 — Error handling architecture](./03-error-handling-architecture.md)