# 11 — API Integration & Network Layer Architecture

This module covers client-side data communication architectures in production React applications: native Fetch, Axios interceptors, centralized API client abstractions, token authentication, refresh token rotation, error boundaries for network errors, real-time WebSockets, GraphQL & tRPC clients, Server-Sent Events (SSE) & LLM token streaming, and OAuth 2.0 / OIDC PKCE security flows.

---

## What you'll be able to do after this module

- **Master Modern Network Primitives**: Leverage `fetch` and `AbortController` to handle network timeouts, race conditions, and response parsing cleanly.
- **Architect Enterprise Axios Instances**: Build centralized HTTP clients with request/response interceptors, base URLs, standard headers, and retry policies.
- **Implement Robust Authentication & Refresh**: Implement silent token refreshing with mutex queues to prevent duplicate refresh requests during concurrent 401s.
- **Handle Real-Time Communication**: Build resilient WebSocket connections with exponential backoff reconnects and heartbeat pings.
- **Integrate GraphQL & tRPC**: Choose between Apollo Client's normalized entity cache and tRPC's zero-build end-to-end TypeScript RPC.
- **Stream Tokens & NDJSON**: Consume Server-Sent Events and HTTP/2 `ReadableStream` chunks for Generative AI streaming without UI frame drops.
- **Secure React SPAs with PKCE**: Implement RFC 7636 Authorization Code Flow with PKCE, avoiding vulnerable Implicit flows and insecure `localStorage` token storage.

---

## Module Syllabus

| # | Note | Description | Key Concept / Tool |
|---|------|-------------|:---:|
| 00 | [00-fetch.md](./00-fetch.md) | Standard Fetch API, `AbortController`, error classification | Native `fetch` |
| 01 | [01-axios.md](./01-axios.md) | Axios client setup, request/response interceptors, defaults | Axios |
| 02 | [02-api-client.md](./02-api-client.md) | Centralized, typed API layer, error serialization, env vars | Architecture Pattern |
| 03 | [03-authentication.md](./03-authentication.md) | Bearer token auth, credentials, session cookies | Auth Patterns |
| 04 | [04-refresh-token-flow.md](./04-refresh-token-flow.md) | Refresh token rotation, mutex locking during concurrent 401s | Interceptor Pattern |
| 05 | [05-api-error-handling.md](./05-api-error-handling.md) | Unified API error handling, status code classification, user feedback | Error Handling |
| 06 | [06-realtime-communication.md](./06-realtime-communication.md) | WebSockets, heartbeat pings, reconnect loops, presence state | WebSockets |
| 07 | [07-graphql-and-trpc-clients.md](./07-graphql-and-trpc-clients.md) | Apollo Client normalized cache, tRPC v11 fullstack type safety | GraphQL / tRPC |
| 08 | [08-sse-and-streaming.md](./08-sse-and-streaming.md) | Server-Sent Events, `ReadableStream`, LLM token streaming | SSE / Streams API |
| 09 | [09-oauth-oidc-spa-flows.md](./09-oauth-oidc-spa-flows.md) | OAuth 2.0 PKCE flow, OIDC claims, BFF token security | OAuth 2.0 / PKCE |

---

## Protocol Decision Matrix

| Requirement | Recommended Protocol | Client Library | Key Consideration |
| :--- | :--- | :--- | :--- |
| **Standard CRUD** | REST / JSON | Axios / Native `fetch` | Standard HTTP caching & status codes |
| **TypeScript Monorepo** | tRPC (RPC over HTTP) | `@trpc/react-query` | Zero schema compilation; instant TS inference |
| **Multi-platform GraphQL**| GraphQL | Apollo Client / urql | Normalized caching (`__typename` + `id`) |
| **LLM Token Streaming** | HTTP Chunked Streaming | Native `fetch` + `ReadableStream` | Supports POST payloads with `TextDecoder` |
| **Server Notifications** | Server-Sent Events (SSE) | Native `EventSource` | Built-in auto-reconnect over standard HTTP/2 |
| **Bi-directional / Gaming**| WebSockets | Native `WebSocket` / Socket.IO | Full duplex TCP socket; requires heartbeat logic |

---

## Boundary with Other Modules

- **Server State Caching**: Read [`12-server-state/`](../12-server-state/README.md) for caching and background synchronization with TanStack Query v5.
- **Routing & Guards**: Read [`10-routing/04-route-protection.md`](../10-routing/04-route-protection.md) for integrating auth state with route guards.
- **Form Submissions**: Read [`06-forms/07-react-19-form-actions.md`](../06-forms/07-react-19-form-actions.md) for server mutations with React 19 Actions.

---
