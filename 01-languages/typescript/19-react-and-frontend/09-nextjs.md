# Next.js App Router, Server Components, and Metadata Typing in TypeScript

> Typing Next.js 14/15 App Router in TypeScript: Server Components (RSC) vs Client Components ('use client'), PageProps, LayoutProps, Route Handlers, generateMetadata, and Server Actions.

| Metadata | Details |
| :--- | :--- |
| **Category** | React and Frontend Typing |
| **Difficulty** | Advanced |
| **Prerequisites** | [19-react-and-frontend/00-component-props-and-children.md](./00-component-props-and-children.md), [19-react-and-frontend/05-forms.md](./05-forms.md) |
| **TypeScript Baseline** | TS 5.0+, Next.js 14/15 / React 19 |
| **Tested tsconfig** | `"strict": true, "jsx": "preserve", "moduleResolution": "bundler"` |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & API Reference](#4-syntax--api-reference)
5. [Practical Examples](#5-practical-examples)
6. [Edge Cases, Pitfalls & Gotchas](#6-edge-cases-pitfalls--gotchas)
7. [Common Mistakes & Anti-Patterns](#7-common-mistakes--anti-patterns)
8. [Performance & Type-Checker Complexity](#8-performance--type-checker-complexity)
9. [Compiler Internals](#9-compiler-internals)
10. [Best Practices & Design Guidelines](#10-best-practices--design-guidelines)
11. [Real-World Case Study](#11-real-world-case-study)
12. [Interview Questions](#12-interview-questions)
13. [Exercises & Challenges](#13-exercises--challenges)

---

## 1. What & Why

Next.js introduces a hybrid architecture combining **React Server Components (RSC)** with interactive Client Components. In the App Router (`app/` directory), components execute on the server by default, enabling direct database queries, zero client bundle weight, and streaming SSR.

In TypeScript, the App Router introduces several unique type rules:
- **Async Server Components**: Server components can be `async` functions returning `Promise<React.JSX.Element>`, which client components strictly disallow.
- **Route Parameters**: In Next.js 15, dynamic route parameters (`params` and `searchParams`) in `page.tsx` are asynchronous promises: `params: Promise<{ id: string }>`.
- **Serialization Boundaries**: Passing non-serializable objects (Functions, Dates, Classes) across the `'use client'` boundary produces runtime serialization exceptions.
- **`generateMetadata`**: Dynamic SEO metadata generation must conform to Next.js's strict `Metadata` and `ResolvingMetadata` interfaces.

```
+-----------------------------------------------------------+
|                   Next.js App Router Pipeline             |
|                                                           |
|  Server Component (app/users/[id]/page.tsx)               |
|  - Reads DB directly: const user = await db.user.find()   |
|  - Generates HTML & React Server Component payload        |
|                            |                              |
|                            | (Serializable Props)         |
|                            v                              |
|  Client Component ('use client')                          |
|  - Interactive state: useState, onClick, useEffect        |
+-----------------------------------------------------------+
```

---

## 2. Core Concepts

### 1. Page and Layout Props (Next.js 15 Baseline)
In Next.js 15, route parameters are asynchronous promises:
```typescript
// app/blog/[slug]/page.tsx
export interface PageProps {
  params: Promise<{ slug: string }>;
  searchParams: Promise<{ [key: string]: string | string[] | undefined }>;
}

export default async function BlogPostPage({ params, searchParams }: PageProps) {
  const { slug } = await params;
  const { ref } = await searchParams;
  return <h1>Post: {slug}</h1>;
}
```

### 2. Typing Dynamic Metadata (`generateMetadata`)
```typescript
import type { Metadata, ResolvingMetadata } from 'next';

export async function generateMetadata(
  { params }: PageProps,
  parent: ResolvingMetadata
): Promise<Metadata> {
  const { slug } = await params;
  return {
    title: `Post: ${slug}`,
    description: `Read post about ${slug}`
  };
}
```

### 3. Route Handlers (`app/api/.../route.ts`)
Typing API endpoints in the App Router:
```typescript
import { NextResponse, type NextRequest } from 'next/server';

export async function GET(
  request: NextRequest,
  { params }: { params: Promise<{ id: string }> }
): Promise<NextResponse> {
  const { id } = await params;
  return NextResponse.json({ id });
}
```

---

## 3. How It Works Internally

When Next.js builds a project:
1. It runs the Next.js compiler type generator, generating `.next/types/app/**/*.ts`.
2. These generated types validate that default exports of `page.tsx` and `layout.tsx` conform to expected Next.js entrypoint signatures.
3. If you export a non-component from `page.tsx` or mis-type `params`, `next build` halts with an informative diagnostic.

---

## 4. Syntax & API Reference

### Layout Props Interface
```typescript
import type { ReactNode } from 'react';

export interface LayoutProps {
  children: ReactNode;
  params: Promise<Record<string, string>>;
}
```

---

## 5. Practical Examples

### Example 1: Basic Async Server Component with Database Access
```typescript
interface UserProfileProps {
  userId: string;
}

async function getUser(id: string) {
  // Direct DB or fetch query on the server!
  return { id, name: 'Alice', role: 'Engineer' };
}

export default async function UserProfile({ userId }: UserProfileProps): Promise<React.JSX.Element> {
  const user = await getUser(userId);
  return (
    <div>
      <h2>{user.name}</h2>
      <p>Role: {user.role}</p>
    </div>
  );
}
```

### Example 2: Real-World Next.js 15 Dynamic Page with Search Params
```typescript
interface SearchPageProps {
  params: Promise<{ category: string }>;
  searchParams: Promise<{ q?: string; page?: string }>;
}

export default async function CategorySearchPage({ params, searchParams }: SearchPageProps) {
  const { category } = await params;
  const { q = '', page = '1' } = await searchParams;

  return (
    <div>
      <h1>Category: {category}</h1>
      <p>Searching for: "{q}" on Page {page}</p>
    </div>
  );
}
```

### Example 3: Edge Case - Server Action with Form Revalidation
```typescript
'use server';

import { revalidatePath } from 'next/cache';
import { z } from 'zod';

const UpdateNameSchema = z.object({
  name: z.string().min(2)
});

export async function updateUserNameAction(formData: FormData): Promise<{ success: boolean; error?: string }> {
  const parsed = UpdateNameSchema.safeParse({
    name: formData.get('name')
  });

  if (!parsed.success) {
    return { success: false, error: parsed.error.issues[0].message };
  }

  // Update in DB
  revalidatePath('/dashboard');
  return { success: true };
}
```

### Example 4: Failure Case - Passing Functions Across Client Boundaries
```typescript
// Server Component:
// function ServerParent() {
//   const handleSave = () => { console.log('Saving'); };
//   // @ts-expect-error TS2322: Functions cannot be passed directly to Client Components unless marked with 'use server'!
//   return <ClientChild onSave={handleSave} />;
// }
```

### Example 5: Idiomatic Route Handler with Validation
```typescript
import { NextResponse, type NextRequest } from 'next/server';
import { z } from 'zod';

const CreateItemSchema = z.object({
  title: z.string().min(1)
});

export async function POST(req: NextRequest): Promise<NextResponse> {
  const body = await req.json();
  const parsed = CreateItemSchema.safeParse(body);

  if (!parsed.success) {
    return NextResponse.json({ error: parsed.error.issues }, { status: 400 });
  }

  return NextResponse.json({ message: 'Item created', item: parsed.data }, { status: 201 });
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **Next.js 15 Async `params`**:
   In Next.js 14, `params` was synchronous `{ slug: string }`. In Next.js 15, `params` and `searchParams` are Promises (`Promise<{ slug: string }>`). Accessing `params.slug` without `await` throws a runtime warning and causes type errors in Next.js 15.
2. **Server/Client Component Boundaries**:
   Props passed from a Server Component to a Client Component must be **serializable** (strings, numbers, booleans, plain objects, arrays). Passing class instances, `Date` objects, or functions triggers serialization errors.
3. **`'use client'` Does Not Mean Client-Only**:
   Client components are still pre-rendered to HTML on the server during SSR! Code inside Client components cannot assume `window` or `localStorage` exists during initial render without checking `typeof window !== 'undefined'`.
4. **Environment Variables**:
   Variables prefixed with `NEXT_PUBLIC_` are inlined into the client JavaScript bundle. Variables without this prefix remain server-only. Accidental usage of server env vars in Client components evaluates to `undefined`.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Bad Code | Good Code | Compiler / Runtime Impact |
| :--- | :--- | :--- | :--- |
| **Sync `params` in Next.js 15** | `const slug = params.slug` | `const { slug } = await params` | Runtime warning / type mismatch |
| **Passing Dates across RSC Boundary** | `<ClientComp date={new Date()} />` | `<ClientComp date={date.toISOString()} />` | Server-Client serialization warning |
| **`'use client'` at the Very Top of File Missing** | Component using `useState` without `'use client'` | Prepend `'use client'` at line 1 | `Error: useState can only be used in Client Component` |
| **Using `window` in Client Component Body** | `const w = window.innerWidth;` | Access inside `useEffect` | Server crash: `window is not defined` |
| **Fetching via HTTP to Self in RSC** | `fetch('http://localhost/api/user')` | Call DB / service function directly | Wasteful loopback HTTP overhead |
| **Exporting Helper from `route.ts`** | Exporting non-HTTP method functions | Keep helpers private or in `lib/` | Next.js interprets exports as HTTP verbs |

---

## 8. Performance & Type-Checker Complexity

- **Zero Bundle Weight for Server Components**: Dependencies imported exclusively in Server Components (e.g. `markdown-it`, `bcrypt`, `pg`) never reach the client bundle, drastically reducing JS bundle sizes.
- **Next.js Plugin Type Checking**: `next.config.js` with TypeScript generates route types in `.next/types`, providing strict route validation with minimal TSServer overhead.

---

## 9. Compiler Internals

When Next.js builds:
The custom Next.js TypeScript plugin checks `app/**/page.tsx`. It verifies that the default export conforms to `(props: PageProps) => ReactNode | Promise<ReactNode>`. If an invalid prop is declared or the component returns an illegal type, Next.js compiler diagnostics halt compilation.

---

## 10. Best Practices & Design Guidelines

1. **Default to Server Components**: Add `'use client'` only when components need user interaction (`onClick`, `useState`, `useEffect`).
2. **Serialize Props at Boundaries**: Pass ISO date strings instead of raw `Date` objects.
3. **Await `params` in Next.js 15**: Use `const { slug } = await params`.
4. **Use Direct Database Access in RSC**: Avoid calling your own API route handlers from Server Components.

---

## 11. Real-World Case Study

### High-Performance Blog with Dynamic SEO Metadata
A blog platform generating dynamic OpenGraph tags and caching database reads:

```typescript
import type { Metadata } from 'next';
import { notFound } from 'next/navigation';

interface PostPageProps {
  params: Promise<{ slug: string }>;
}

async function getPostFromDb(slug: string) {
  // Simulated database lookup
  if (slug === 'missing') return null;
  return { title: 'Understanding TypeScript', content: 'Comprehensive guide...' };
}

export async function generateMetadata({ params }: PostPageProps): Promise<Metadata> {
  const { slug } = await params;
  const post = await getPostFromDb(slug);
  if (!post) return { title: 'Post Not Found' };

  return {
    title: `${post.title} | Engineering Notes`,
    description: post.content.slice(0, 160),
    openGraph: {
      title: post.title,
      type: 'article'
    }
  };
}

export default async function BlogPostPage({ params }: PostPageProps) {
  const { slug } = await params;
  const post = await getPostFromDb(slug);
  if (!post) notFound();

  return (
    <article>
      <h1>{post.title}</h1>
      <p>{post.content}</p>
    </article>
  );
}
```

---

## 12. Interview Questions

<details>
<summary>1. What are React Server Components (RSC) and how do they differ from Client Components?</summary>

React Server Components execute exclusively on the server and never ship JavaScript to the browser. They can be asynchronous, can query databases directly, and have zero client bundle size impact. Client Components (`'use client'`) ship JavaScript to the browser to enable client-side interactivity, state (`useState`), and event handlers.
</details>

<details>
<summary>2. Why are `params` and `searchParams` asynchronous in Next.js 15?</summary>

Next.js 15 made `params` and `searchParams` asynchronous Promises (`Promise<{ slug: string }>`) to support partial prerendering (PPR) and streaming optimizations where route segments can be rendered before all parameters or search parameters are settled.
</details>

<details>
<summary>3. Why can't functions or class instances be passed from a Server Component to a Client Component?</summary>

Props passed across the server/client boundary must be serialized into a React Server Component (RSC) JSON stream. Functions and complex prototypes cannot be serialized to JSON, producing a runtime serialization error.
</details>

<details>
<summary>4. How do you type dynamic metadata in Next.js?</summary>

By exporting `async function generateMetadata(props: PageProps, parent: ResolvingMetadata): Promise<Metadata>`.
</details>

<details>
<summary>5. What is the difference between `NextRequest` / `NextResponse` and native `Request` / `Response`?</summary>

`NextRequest` and `NextResponse` extend the standard Web Fetch API `Request` and `Response` with helper methods for reading/writing cookies, extracting URL parameters, managing geolocation, and returning typed JSON.
</details>

<details>
<summary>6. How do you trigger an intentional 404 page in an App Router page?</summary>

By calling `notFound()` from `next/navigation`. It throws an internal Next.js error caught by the closest `not-found.tsx` boundary.
</details>

<details>
<summary>7. What does the `'use server'` directive do?</summary>

`'use server'` marks an asynchronous function as a **Server Action**, allowing it to be invoked from client components or HTML form actions. Next.js creates an internal RPC endpoint for the action automatically.
</details>

<details>
<summary>8. How do you type route segment parameters in Route Handlers?</summary>

```typescript
export async function GET(
  req: NextRequest,
  context: { params: Promise<{ id: string }> }
) { ... }
```
</details>

---

## 13. Exercises & Challenges

<details>
<summary>Exercise 1 (Easy): Basic Layout Props Typing</summary>

**Problem:** Type an App Router `RootLayout` component accepting `children`.

**Solution:**
```typescript
import type { ReactNode } from 'react';

export default function RootLayout({ children }: { children: ReactNode }) {
  return (
    <html lang="en">
      <body>{children}</body>
    </html>
  );
}
```
</details>

<details>
<summary>Exercise 2 (Easy): Static Metadata Export</summary>

**Problem:** Type a static `metadata` export with title and description for an about page.

**Solution:**
```typescript
import type { Metadata } from 'next';

export const metadata: Metadata = {
  title: 'About Us',
  description: 'Learn more about our team and mission'
};
```
</details>

<details>
<summary>Exercise 3 (Medium): Route Handler with Status Code</summary>

**Problem:** Implement a typed `GET` route handler returning `{ status: 'ok', timestamp: number }`.

**Solution:**
```typescript
import { NextResponse } from 'next/server';

export async function GET(): Promise<NextResponse> {
  return NextResponse.json({
    status: 'ok',
    timestamp: Date.now()
  });
}
```
</details>

<details>
<summary>Exercise 4 (Medium): Next.js 15 Async Dynamic Page</summary>

**Problem:** Type a `UserPage` component in Next.js 15 that awaits dynamic parameter `id`.

**Solution:**
```typescript
interface UserPageProps {
  params: Promise<{ id: string }>;
}

export default async function UserPage({ params }: UserPageProps) {
  const { id } = await params;
  return <h1>User ID: {id}</h1>;
}
```
</details>

<details>
<summary>Exercise 5 (Hard): Dynamic OpenGraph Metadata Generator</summary>

**Problem:** Implement a `generateMetadata` function that merges parent metadata with dynamic post title and custom OpenGraph images.

**Solution:**
```typescript
import type { Metadata, ResolvingMetadata } from 'next';

interface Props { params: Promise<{ id: string }>; }

export async function generateMetadata(
  { params }: Props,
  parent: ResolvingMetadata
): Promise<Metadata> {
  const { id } = await params;
  const previousImages = (await parent).openGraph?.images || [];

  return {
    title: `Item ${id}`,
    openGraph: {
      images: [`/api/og?id=${id}`, ...previousImages]
    }
  };
}
```
</details>

<details>
<summary>Exercise 6 (Hard): Type-Safe Server Action Form Handler</summary>

**Problem:** Write a server action that parses `FormData` with Zod, returning typed error messages on failure and revalidating cache on success.

**Solution:**
```typescript
'use server';

import { revalidatePath } from 'next/cache';
import { z } from 'zod';

const Schema = z.object({ comment: z.string().min(5) });

export async function submitComment(
  prevState: { error?: string; success?: boolean },
  formData: FormData
) {
  const parsed = Schema.safeParse({ comment: formData.get('comment') });
  if (!parsed.success) {
    return { error: parsed.error.issues[0].message };
  }
  revalidatePath('/comments');
  return { success: true };
}
```
</details>
