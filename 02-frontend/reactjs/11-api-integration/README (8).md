# 11 — API Integration

Almost every real React app talks to a server. This folder is about the **plumbing**: how requests are made, how a single API client is structured, how authentication and token refresh work, how errors are normalized, and how to get live updates.

It deliberately stops short of *server state* (caching, refetching, mutations, optimistic updates). That's [12 — Server State](../12-server-state/README.md) and TanStack Query, which sits **on top of** the layer built here.

```text
Components
    │  useQuery / useMutation          ← 12-server-state
    ▼
Resource functions  (getProjects, createProject)
    ▼
API client  (base URL, JSON, auth header, errors)   ← 02
    ▼
fetch / axios                                       ← 00, 01
    ▼
Network ◄── auth + refresh (03, 04) · realtime (06)
```

## Prerequisites

- [JavaScript for React](../00-setup/00-javascript-for-react.md): Promises, `async/await`, destructuring
- [useEffect](../03-hooks/02-useEffect.md): you'll see why fetching in effects is awkward
- [TypeScript with React](../04-typescript-with-react/README.md) for the typed client examples

## Contents

| # | File | What you'll learn |
|---|------|-------------------|
| 00 | [Fetch](./00-fetch.md) | The Fetch API, its sharp edges, cancellation, effect races |
| 01 | [Axios](./01-axios.md) | When it helps, instances, interceptors, differences from fetch |
| 02 | [API client](./02-api-client.md) | One typed client: base URL, JSON, errors, resource modules |
| 03 | [Authentication](./03-authentication.md) | Cookies vs tokens, storage trade-offs, session bootstrap, logout |
| 04 | [Refresh token flow](./04-refresh-token-flow.md) | Silent refresh, single-flight, retry, multi-tab |
| 05 | [API error handling](./05-api-error-handling.md) | Error taxonomy, retries, field errors, where to show them |
| 06 | [Realtime communication](./06-realtime-communication.md) | Polling, SSE, WebSockets, wiring events into the cache |

## Suggested order

Read 00 first (even if you plan to use axios; it explains what axios is smoothing over). 02 is the centerpiece, and 03 → 04 → 05 build on its client. 06 is independent.

## Conventions

- TypeScript, with Vite env vars (`import.meta.env.VITE_*`).
- Examples use `fetch`; the axios equivalent is shown where it differs meaningfully.
- Server-side implementation of auth and APIs is a backend topic. These notes cover the **client's** half.
