# 05-05 — GraphQL APIs

GraphQL provides declarative, type-safe API communication, eliminating over-fetching and under-fetching by allowing clients to request the exact data shape required.

This module covers full production GraphQL architecture in NestJS: Code-First schema generation with Apollo Server, solving N+1 query storms with **DataLoader**, security hardening with depth limiting, real-time push with **Subscriptions (`graphql-ws`)**, and microservice composition via **Apollo Federation v2**.

---

## Module Index

| # | File | Core Topics |
|---|---|---|
| 01 | [GraphQL Fundamentals](./01-graphql-fundamentals.md) | Code-First vs Schema-First, `@ObjectType`, `@InputType`, `@Resolver`, Apollo Driver |
| 02 | [DataLoader & N+1 Problem](./02-dataloader-and-n-plus-one.md) | Batching queries, `process.nextTick()`, request-scoped caching, 1:1 order contracts |
| 03 | [Authentication & Security](./03-graphql-auth-and-security.md) | `GqlAuthGuard`, `GqlExecutionContext`, query depth limits, field-level middleware |
| 04 | [Subscriptions & Federation](./04-subscriptions-and-federation.md) | `graphql-ws` protocol, Redis PubSub, Apollo Federation v2 subgraphs, `@ResolveReference` |

---

## Key Learning Outcomes

1. Implement Code-First GraphQL APIs deriving schemas directly from TypeScript decorators.
2. Eliminate N+1 query bottlenecks using request-scoped DataLoaders.
3. Secure GraphQL resolvers using adapted execution context authentication guards.
4. Protect APIs against Denial of Service attacks using AST depth and complexity limits.
5. Decompose large monolith GraphQL schemas into distributed Apollo Federation v2 subgraphs.
