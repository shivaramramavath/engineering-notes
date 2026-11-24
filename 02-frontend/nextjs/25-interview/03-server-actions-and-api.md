# Senior & Staff Interview Guide: Server Actions and Route Handlers

> Comprehensive technical interview mastery guide covering Server Actions internals, progressive enhancement, closure capturing, Route Handlers (`route.ts`), and the Edge vs Node.js runtime.

| Metadata | Details |
| :--- | :--- |
| **Category** | Technical Interview Preparation |
| **Target Level** | Senior / Staff Software Engineer |
| **Prerequisites** | [07-server-actions/README.md](../07-server-actions/README.md), [08-route-handlers-and-proxy/README.md](../08-route-handlers-and-proxy/README.md) |
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

Interviewers evaluating senior engineers on Server Actions and APIs probe:
- **Server Action Dispatch**: How `"use server"` transforms functions into cryptographically hashed POST endpoints behind the scenes.
- **Progressive Enhancement**: Native HTML `<form action={serverAction}>` execution without client JavaScript vs JavaScript-enhanced `useActionState`.
- **Security & Authorization**: Server Actions as public API entry points; why Server Actions must authenticate callers independently.
- **Route Handlers vs Server Actions**: When to use REST/Webhook endpoints (`route.ts`) vs RPC mutation functions (Server Actions).

---

## 2. Deep-Dive Interview Questions & Answers

<details>
<summary>Q1: What does Next.js actually do under the hood when you declare a function with `"use server"`?</summary>

**Answer**:
1. **Compilation Transformation**: The compiler extracts the function body from the component file and compiles it into an internal server-only module.
2. **Action ID Generation**: Next.js assigns the function an opaque, cryptographic hash (e.g. `408a2b5e9...`).
3. **Client Stub**: On the client, the function is replaced with a stub that dispatches an HTTP `POST` request to the current URL with headers:
   - `Next-Action: <hash>`
   - `Content-Type: multipart/form-data` or `application/json`
4. **Server Dispatch**: When the request reaches the server, the Next.js router intercepts the `Next-Action` header, matches the hash against the action manifest, decodes the arguments, executes the server function, and streams back the return value alongside any revalidated RSC trees.
</details>

<details>
<summary>Q2: Why are Server Actions considered public HTTP endpoints, and what security risks does this introduce?</summary>

**Answer**:
Even if a Server Action is declared inside a private or nested component, **its generated action ID is exposed over the network**.
- Anyone with the action ID can issue raw `POST` requests to your server using `curl` or Postman.
- **Critical Risk**: Treating Server Actions like private internal functions and omitting authorization checks.
- **Rule of Thumb**: Treat every Server Action as a public REST API route. You must validate inputs (e.g. using Zod) and verify the caller's session and permissions in the Data Access Layer (DAL) before mutating data.
</details>

<details>
<summary>Q3: How does closure capturing (lexical scoping) work in Server Actions, and what is the danger of hidden inputs?</summary>

**Answer**:
If a Server Action is declared inline inside a Server Component:
```tsx
export default async function ProductPage({ params }: { params: Promise<{ id: string }> }) {
  const { id } = await params;

  async function updateStock() {
    'use server';
    await db.update(products).set({ inStock: true }).where(eq(products.id, id));
  }

  return <form action={updateStock}><button>Restock</button></form>;
}
```
- Next.js automatically **captures the closed-over variable `id`**, serializes it, and sends it to the client as an encrypted hidden field attached to the form action stub.
- **The Danger**: If the server encryption key rotates, pending client forms fail decryption. Furthermore, capturing large objects in closures inflates the payload size. Best practice is to pass IDs via hidden inputs or bind arguments explicitly via `.bind(null, id)`.
</details>

<details>
<summary>Q4: When should you use a Route Handler (`route.ts`) instead of a Server Action?</summary>

**Answer**:
- **Use Server Actions for**:
  - UI mutations and form submissions triggered directly by user interaction.
  - Operations requiring seamless UI transitions, optimistic updates (`useOptimistic`), and automatic page revalidation (`updateTag`, `revalidatePath`).
- **Use Route Handlers for**:
  - Third-party webhooks (e.g. Stripe, GitHub, Slack) that require standard HTTP request/response payloads and raw body signature verification.
  - Public REST/GraphQL APIs consumed by external mobile apps or third-party developers.
  - Streaming binary data, file downloads, or custom streaming SSE (Server-Sent Events) feeds.
</details>

---

## 3. Output Prediction & Code Analysis Questions

### Question 1: Unprotected Server Action Mutation
Identify the critical vulnerability in this Server Action:

```tsx
// app/actions/deletePost.ts
'use server';

import { db } from '@/lib/db';
import { posts } from '@/lib/schema';
import { eq } from 'drizzle-orm';

export async function deletePostAction(postId: string) {
  // Directly deleting the post
  await db.delete(posts).where(eq(posts.id, postId));
}
```

**Evaluation**:
- **Critical Insecure Direct Object Reference (IDOR)**: There is zero authentication or authorization check. An attacker who discovers the action ID can iterate through post IDs and delete all posts in the database.
- **Fix**:
```tsx
'use server';

import { verifySession } from '@/lib/dal';
import { db } from '@/lib/db';
import { posts } from '@/lib/schema';
import { and, eq } from 'drizzle-orm';

export async function deletePostAction(postId: string) {
  const session = await verifySession(); // Authenticate
  // Authorize: ensure the user owns the post or is an admin
  const result = await db.delete(posts).where(
    and(eq(posts.id, postId), eq(posts.userId, session.userId))
  );
  if (!result.rowCount) throw new Error("Unauthorized or post not found");
}
```

---

## 4. Architecture & System Design Scenario

**Scenario**: Designing a CSV file bulk-import system handling 100,000 records.
**Approach**:
- Do **not** process 100,000 records synchronously in a single Server Action (this will hit serverless timeout limits of 10s–60s).
- Use a Server Action or Route Handler to validate file metadata, upload the raw file to S3 via a pre-signed URL, and enqueue a background job to BullMQ / Redis.
- Return a job ID immediately to the client and poll or stream progress via SSE / Route Handler.

---

## 5. Senior Technical Evaluation Rubric

| Level | Candidate Signals |
| :--- | :--- |
| **Mid-Level** | Uses Server Actions with `useActionState` and handles form submission. |
| **Senior** | Understands cryptographic action manifests, closure encryption mechanics, and enforces strict DAL authorization on all actions. |
| **Staff / Principal** | Designs resilient background queue offloading architectures, prevents race conditions with optimistic concurrency tokens, and audits actions for CVE vulnerabilities. |
