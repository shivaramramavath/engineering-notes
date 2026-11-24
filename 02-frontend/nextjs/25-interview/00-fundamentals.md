# Senior & Staff Interview Guide: Next.js Architecture and Core Fundamentals

> Comprehensive technical interview mastery guide covering Next.js fundamentals: App Router vs Pages Router, file-system conventions, Turbopack compilation engine, and React 19 integration.

| Metadata | Details |
| :--- | :--- |
| **Category** | Technical Interview Preparation |
| **Target Level** | Senior / Staff Software Engineer |
| **Prerequisites** | [01-fundamentals/README.md](../01-fundamentals/README.md), [02-routing/README.md](../02-routing/README.md) |
| **Next.js Baseline** | Next.js 15.2+ / 16.0, React 19.0.0, Node.js 20+ LTS |

---

## Table of Contents
1. [Core Architectural Concepts](#1-core-architectural-concepts)
2. [Deep-Dive Interview Questions & Answers](#2-deep-dive-interview-questions--answers)
3. [Output Prediction & Code Analysis Questions](#3-output-prediction--code-analysis-questions)
4. [Architecture & System Design Scenario](#4-architecture--system-design-scenario)
5. [Senior Technical Evaluation Rubric](#5-senior-technical-evaluation-rubric)

---

## 1. Core Architectural Concepts

Interviewers evaluating senior candidates on Next.js fundamentals look for architectural clarity across:
- **App Router Paradigms**: Server Component default tree, component-level data fetching, streaming SSR via React Suspense, and nested layouts.
- **Pages Router Differences**: Monolithic `getServerSideProps` / `getStaticProps` vs granular RSC component boundaries.
- **Turbopack Build Pipeline**: Incremental compilation architecture, AST graph caching, and Rust-based fast refresh vs legacy Webpack chunk splitting.
- **React 19 Synergy**: Deep integration with Actions, `useActionState`, async transitions, and React Taint APIs.

---

## 2. Deep-Dive Interview Questions & Answers

<details>
<summary>Q1: How does the Next.js App Router fundamentally invert the data-fetching and rendering model of the Pages Router?</summary>

**Answer**:
In the Pages Router, rendering was **page-scoped**:
- A single lifecycle function (`getServerSideProps` or `getStaticProps`) fetched all data required for the entire page tree and passed it as top-level props.
- If one section of the page stalled on a slow database query, the entire page response was blocked (TTFB suffered).
- The client received a monolithic JavaScript bundle to hydrate the entire HTML tree.

In the App Router:
1. **Component-Scoped Fetching**: Data fetching is colocated inside individual React Server Components (RSC) anywhere in the sub-tree using native `async/await`.
2. **Streaming via Suspense**: Slow queries are wrapped in `<Suspense fallback={<Skeleton />}>`, enabling instant TTFB with static shell rendering while dynamic content streams over the HTTP wire.
3. **Selective Hydration & Zero Client JS**: Only components explicitly marked with `"use client"` are shipped in the client bundle. Pure Server Components emit only serialized RSC payload strings, reducing client bundle size by up to 70-80%.
</details>

<details>
<summary>Q2: What is Turbopack, and how does its internal architecture enable instant Hot Module Replacement (HMR) at enterprise scale?</summary>

**Answer**:
Turbopack is an incremental bundler written in Rust designed by the creator of Webpack (Tobias Koppers).
- **Function-Level Turbo Engine**: Unlike Webpack, which operates on file graphs and recompiles chunks, Turbopack models your application as a directed acyclic graph (DAG) of pure function calls.
- **Computation Memoization**: Turbopack caches the outputs of every compilation step in memory. When a file changes, Turbopack only re-evaluates the specific functions whose inputs changed, rather than re-traversing and re-bundling entire module chunks.
- **On-Demand Compilation**: In development mode (`next dev --turbo`), Turbopack compiles only the routes and assets currently requested by the browser, meaning a project with 5,000 routes boots as quickly as a project with 5 routes.
</details>

<details>
<summary>Q3: Explain the role and lifecycle of the special file conventions in the Next.js `app` directory (`layout.js`, `template.js`, `error.js`, `loading.js`, `not-found.js`).</summary>

**Answer**:
Next.js translates the file hierarchy into a nested React component tree:
- `layout.tsx`: Wraps child pages and nested layouts. **Preserves state and does not re-render** across route navigations within the same segment.
- `template.tsx`: Similar to layout, but **creates a brand new component instance** on each navigation. Useful for enter/exit CSS animations, resetting `useEffect`, or logging page views.
- `loading.tsx`: Automatically wraps the route segment and its children in a React `<Suspense fallback={<Loading />}>` boundary.
- `error.tsx`: Must be a Client Component (`"use client"`). Wraps children in a React Error Boundary. Receives `error` and a `reset()` callback to retry rendering.
- `not-found.tsx`: Rendered when `notFound()` is invoked or when an unmatched route is reached.
</details>

<details>
<summary>Q4: In Next.js 15+, why were dynamic request APIs (`cookies`, `headers`, `params`, `searchParams`) made asynchronous?</summary>

**Answer**:
In Next.js 14 and earlier, `cookies()` and `headers()` were synchronous getters. This created two critical architectural limitations:
1. **Blocked Speculative Prerendering**: The server had to wait for incoming HTTP headers before executing any part of a component that might inspect cookies.
2. **Impediment to Partial Prerendering (PPR)**: PPR requires the build engine to speculatively execute server components up to the point of dynamic data access.

By making `cookies()`, `headers()`, `params`, and `searchParams` return `Promise<...>`, Next.js allows the runtime to defer awaiting dynamic values until the exact microtask they are accessed. The outer shell can be rendered statically or streamed ahead of time, while dynamic accesses trigger Suspense boundaries speculatively.
</details>

---

## 3. Output Prediction & Code Analysis Questions

### Question 1: Async Parameter Unwrapping
What will happen when accessing `params` in Next.js 15+ without `await`?

```tsx
// app/users/[id]/page.tsx
export default async function Page({
  params,
}: {
  params: Promise<{ id: string }>;
}) {
  // @ts-expect-error - synchronous access
  const id = params.id;
  return <h1>User: {id}</h1>;
}
```

**Evaluation**:
- In development and production under Next.js 15+, accessing a property on `params` synchronously will log a runtime deprecation warning and `id` will evaluate to `undefined` (or cause build errors in strict TypeScript).
- **Correct Fix**:
```tsx
export default async function Page({
  params,
}: {
  params: Promise<{ id: string }>;
}) {
  const { id } = await params;
  return <h1>User: {id}</h1>;
}
```

---

## 4. Architecture & System Design Scenario

**Scenario**: An engineering team is migrating a 300-page e-commerce web application from Next.js 12 Pages Router to Next.js 15 App Router.
**Requirements**:
1. Zero regression in SEO rankings and Core Web Vitals.
2. Incremental migration path without a total rewrite.
3. Proper handling of shared authentication state across both routers.

**Senior Architectural Strategy**:
- **Coexistence**: Run Pages Router (`pages/`) and App Router (`app/`) simultaneously in the same codebase. Next.js natively routes App Router routes with higher precedence over matching Pages Router routes.
- **Shared Session Token**: Standardize session cookies (`HttpOnly; Secure; SameSite=Lax`) readable by both `getServerSideProps` and App Router Server Components / DAL.
- **Route-by-Route Migration**: Migrate low-risk marketing pages first, followed by static catalog pages with PPR, and finally authenticated checkout flows.

---

## 5. Senior Technical Evaluation Rubric

| Level | Candidate Signals |
| :--- | :--- |
| **Mid-Level** | Explains basic differences between `app` and `pages`. Understands `"use client"` directive. |
| **Senior** | Explains RSC serialization over the wire, Turbopack incremental computation graph, Suspense streaming mechanics, and why request APIs became async in v15. |
| **Staff / Principal** | Details Partial Prerendering (PPR) compile-time AST analysis, security implications of edge runtime vs Node runtime, and architecting hybrid migration strategies without downtime. |
