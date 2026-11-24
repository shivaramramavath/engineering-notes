# Senior & Staff Interview Guide: React Server Components and Boundaries

> Comprehensive technical interview mastery guide covering React Server Components (RSC), Client Component boundaries, serialization protocols, and the `server-only` security boundary.

| Metadata | Details |
| :--- | :--- |
| **Category** | Technical Interview Preparation |
| **Target Level** | Senior / Staff Software Engineer |
| **Prerequisites** | [03-components/README.md](../03-components/README.md), [04-rendering/README.md](../04-rendering/README.md) |
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

Interviewers evaluating senior candidates on RSC expect mastery of:
- **Component Classification**: Pure Server Components (default), Client Components (`"use client"`), and Hybrid compositions.
- **RSC Wire Protocol**: Format of streamed flight payloads, JSON-like tree representation with module references (`$L`, `$S`), and client reconstruction.
- **Serialization Constraints**: Why functions, promises, class instances, and symbols cannot cross the Server $\rightarrow$ Client boundary as raw props (unless tagged as Server Actions).
- **Security Boundaries**: Enforcing execution environment isolation via `server-only` and protecting secrets with React Taint APIs (`taintObjectReference`, `experimental_taintUniqueValue`).

---

## 2. Deep-Dive Interview Questions & Answers

<details>
<summary>Q1: What actually happens under the hood when a component is marked with `"use client"`?</summary>

**Answer**:
`"use client"` does **NOT** mean "render this component only in the browser".
- It defines an **architectural split point (boundary)** between the server-only module graph and the client-hydrated module graph.
- During initial SSR, Client Components are still rendered on the server into static HTML to ensure fast First Contentful Paint (FCP) and SEO indexing.
- At build time, Next.js / Turbopack treats the `"use client"` boundary as an entry point for the browser JavaScript bundle. In the Server Component tree, the component is replaced with an entry reference (module ID and export name).
- On the client, React downloads the component bundle and **hydrates** the static HTML, attaching event listeners, initializing state (`useState`, `useReducer`), and executing effects (`useEffect`).
</details>

<details>
<summary>Q2: How can a Server Component be passed into a Client Component without turning the Server Component into a Client Component?</summary>

**Answer**:
By utilizing **Component Composition** via the `children` prop (or any other React slot prop).

```tsx
// Server Parent (app/page.tsx)
import { ClientContainer } from "./ClientContainer";
import { HeavyServerComponent } from "./HeavyServerComponent";

export default function Page() {
  return (
    <ClientContainer>
      <HeavyServerComponent />
    </ClientContainer>
  );
}
```

**Why this works**:
1. `HeavyServerComponent` executes entirely on the server within the Server Component graph.
2. Its rendered output (virtual DOM nodes serialized into RSC flight protocol) is passed into `ClientContainer` as the `children` prop.
3. `ClientContainer` only receives opaque, pre-rendered React elements. It never imports `HeavyServerComponent` directly, so `HeavyServerComponent`'s dependencies (e.g. database clients, markdown parsers) are never bundled into the client JavaScript bundle.
</details>

<details>
<summary>Q3: Explain the RSC Wire Protocol (Flight Protocol) and how React reconciles it on client navigation.</summary>

**Answer**:
When navigating between pages in the App Router, Next.js does not fetch a full HTML document. Instead, it issues a `fetch` request with the `RSC: 1` header to receive a raw stream of RSC payload chunks:
- Each row in the flight stream represents a serialized segment:
  - Lines starting with `M:` define client module references (bundles to load).
  - Lines starting with `J:` define serialized JSX trees with references to module IDs.
  - Lines starting with `S:` define Suspense boundaries and promise states.
- On the client, React parses the stream incrementally without waiting for the full response to finish. It reconciles the new virtual DOM against the current DOM tree, preserving client state in unchanged layouts while updating only the target segment.
</details>

<details>
<summary>Q4: What is `server-only` and why is it essential for protecting database credentials and private algorithms?</summary>

**Answer**:
`server-only` is an empty package with an `index.js` that throws a build-time compiler error if imported into any file within a `"use client"` module graph.
- Without `server-only`, a developer could accidentally import a utility function containing database credentials (e.g., `db.ts`) into a client component.
- Next.js would attempt to bundle `db.ts` for the browser, either leaking sensitive environment variables or failing at runtime with missing Node.js native modules (`fs`, `net`).
- Adding `import 'server-only';` at the top of data access modules turns accidental client-side leaks into immediate build failures.
</details>

---

## 3. Output Prediction & Code Analysis Questions

### Question 1: Non-Serializable Prop Violation
What error will Next.js throw at build or runtime for the following code?

```tsx
// app/dashboard/page.tsx (Server Component)
import { UserProfileCard } from "./UserProfileCard"; // "use client"

export default async function Page() {
  const user = {
    id: "usr_123",
    name: "Elena",
    getDisplayName: () => "Elena Rostova", // Function
    createdAt: new Date(),                // Date instance
  };

  return <UserProfileCard user={user} />;
}
```

**Evaluation**:
- Passing a plain JavaScript function `getDisplayName` across the Server $\rightarrow$ Client boundary throws a runtime error:
  `Error: Functions cannot be passed directly to Client Components unless you explicitly expose it by marking it with "use server".`
- Passing a `Date` object **is supported** in modern React 19 RSC serialization (it is automatically serialized and revived as a `Date`), but methods on classes or custom prototypes are stripped.
- **Remedy**: Pass plain serializable JSON objects or invoke the method on the server before passing the resulting string prop.

---

## 4. Architecture & System Design Scenario

**Scenario**: You are architecting a high-traffic analytics dashboard. The page has:
- A heavy 2MB charting library with complex interactive filters.
- A private query engine that connects to ClickHouse via native TCP sockets.
- Rapid real-time metrics refreshing every 5 seconds.

**Senior Architecture**:
1. **Data Layer**: Colocate ClickHouse queries in a Server Component or DAL with `import 'server-only';`.
2. **Dynamic Client Island**: Dynamically import the charting library (`next/dynamic` with `ssr: false`) inside a Client Component wrapper.
3. **Optimized Boundary**: Pass pre-aggregated metric data from the Server Component into the Chart Client Island as a plain serializable array.
4. **Polling/Refresh Strategy**: Use React 19 Server Actions or Route Handlers triggered via `useTransition` / SWR inside the client chart island to refresh data without unmounting the chart canvas.

---

## 5. Senior Technical Evaluation Rubric

| Level | Candidate Signals |
| :--- | :--- |
| **Mid-Level** | Knows `"use client"` is needed for hooks (`useState`, `useEffect`). |
| **Senior** | Understands how `children` slots prevent client-graph infection. Explains flight payload streaming and `server-only` compilation guards. |
| **Staff / Principal** | Articulates RSC payload compression over HTTP/2 multiplexed streams, React 19 taint APIs for zero-trust data protection, and micro-frontend module sharing strategies. |
