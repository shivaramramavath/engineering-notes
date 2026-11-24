# Server-Only Boundaries & Serialization (`server-only-and-client-only-boundaries`)

> Enforcing strict execution environments using `server-only` and `client-only`, React Flight Protocol serialization limits, Data Access Layer (DAL) isolation, and secrets protection.

| Metadata | Details |
| :--- | :--- |
| **Next.js Version** | 15.2+ / 16.0 |
| **React Version** | React 19.0.0 |
| **Node Version** | Node.js 20+ LTS |
| **Router** | App Router (`app/`) |
| **Runtime** | Server-Only Guard / React Flight |
| **Tested With** | Next.js 15.2, Vitest 3.0, TypeScript 5.7 |

---

## 1. Concept Overview & Mental Model

### 1.1 The Client-Server Boundary Illusion
In the React App Router, files without a `'use client'` directive execute on the server by default. However, if a Client Component imports a function from a server file that doesn't explicitly mark itself as server-only, bundlers will attempt to bundle that file into client JavaScript!
- If the file contains private API keys or database calls (`pg`, `prisma`, `fs`), the build may either crash with missing Node.js modules or worse: **leak private database credentials directly into client bundles!**
- The `server-only` package turns this boundary into a **hard compile-time build failure**.

```
[ Client Component ('use client') ] ──(attempts import)──> [ File with `import 'server-only'` ]
                                                                       │
                                                                       ▼
                                                       [ COMPILATION ERROR: Build Halted ]
                                            "This module cannot be imported from a Client Component."
```

---

## 2. Naive vs. Idiomatic Comparison

### Naive: Relying on Default Separation
```ts
// lib/db.ts (Naive - Unprotected)
import { Pool } from 'pg';

const pool = new Pool({ connectionString: process.env.DATABASE_URL });

export async function getUser(id: string) {
  // If imported by any client component, DATABASE_URL leaks or bundler crashes!
  return pool.query('SELECT * FROM users WHERE id = $1', [id]);
}
```

### Idiomatic: Explicit `server-only` Isolation
```ts
// dal/users.ts (Idiomatic)
import 'server-only'; // Enforces build-time assertion
import { db } from '@/db';

export async function getUser(id: string) {
  return db.user.findUnique({ where: { id } });
}
```

---

## 3. Five Progressive Examples

### Example 1: Installing and Asserting `server-only`
```bash
npm install server-only
```
```ts
// dal/auth.ts
import 'server-only';

export function getInternalSecret() {
  return process.env.JWT_SIGNING_SECRET!;
}
```

### Example 2: React Flight Protocol Serialization
```tsx
// app/dashboard/page.tsx (Server Component)
import { UserGreeting } from './UserGreeting';

export default async function Page() {
  const user = {
    name: 'Alice',
    createdAt: new Date(), // Date serializes across Flight protocol
    // Error if passing: function onClick() {} or Symbol() or class instance
  };

  return <UserGreeting user={user} />;
}
```

### Example 3: Data Transfer Object (DTO) Sanitization
```ts
// dal/dto.ts
import 'server-only';
import { UserRecord } from '@/types/db';

export function sanitizeUserDTO(raw: UserRecord) {
  // Explicitly strip passwordHash and internal audit keys
  return {
    id: raw.id,
    name: raw.name,
    email: raw.email,
    avatarUrl: raw.avatarUrl,
  };
}
```

### Example 4: Using React 19 Taint APIs
```ts
// dal/user.ts
import 'server-only';
import { experimental_taintObjectReference, experimental_taintUniqueValue } from 'react';
import { db } from '@/db';

export async function getUserWithTaint(id: string) {
  const user = await db.user.findUnique({ where: { id } });
  if (user) {
    // Throws error if full database record is passed to a Client Component!
    experimental_taintObjectReference('Do not pass raw DB user to client', user);
    experimental_taintUniqueValue('Do not leak password hash', user, user.passwordHash);
  }
  return user;
}
```

### Example 5: Asserting Browser-Only Environments with `client-only`
```bash
npm install client-only
```
```ts
// lib/indexedDB.ts
import 'client-only'; // Throws compile error if imported in Server Components

export function openClientDB() {
  return indexedDB.open('OfflineNotes', 1);
}
```

---

## 4. Under the Hood & Internals
`server-only` contains a package entry point with an empty file for Node runtimes, but maps its browser export condition to a throwing error:
```js
// server-only/index.js (browser condition)
throw new Error(
  "This module cannot be imported from a Client Component. It should only be used from a Server Component."
);
```
During Webpack/Turbopack compilation of Client Component chunks, the bundler encounters the browser condition and halts compilation before emitting code.

---

## 5. Security & Defence-in-Depth
- **Zero Secrets Leakage**: Without `server-only`, a single misplaced import in a deeply nested component tree can bundle internal private keys into public client `.js` chunks.
- **Taint API Layer**: Combine `server-only` with React 19 `experimental_taintObjectReference` to protect against human errors during DTO mapping.

---

## 6. Common Mistakes & Anti-Patterns (Table)

| Anti-Pattern | Root Cause | Observable Symptom | Correct Architecture |
| :--- | :--- | :--- | :--- |
| Passing functions across boundary | Flight protocol only supports serializable JSON/primitives | Error: Functions cannot be passed directly to Client Components | Use Server Actions (`'use server'`) |
| Unsanitized database object props | Passing raw Prisma/Drizzle row to client | Internal columns (`passwordHash`) sent over network | Map through explicit DTO function |
| Relying on filename conventions | Assuming `db.server.ts` prevents client imports | File is bundled if imported in `'use client'` | Add `import 'server-only'` |
| Mutating tainted objects | Shallow clone strips taint marker | Taint check bypassed on clone | Re-taint or use strict DTO builders |

---

## 7. Performance & Optimization Guide
Keeping heavy libraries (`bcrypt`, `prisma`, `aws-sdk`) strictly on the server eliminates megabytes of JavaScript from client bundles, directly reducing First Load JS and improving Largest Contentful Paint (LCP).

---

## 8. TypeScript & Strict Typing Recipes
```ts
export interface SafeUserDTO {
  id: string;
  name: string;
  email: string;
}
```

---

## 9. Testing & Quality Verification

```ts
import { describe, it, expect } from 'vitest';
import { sanitizeUserDTO } from './dto';

describe('DTO Sanitization', () => {
  it('strips private credentials', () => {
    const raw = { id: '1', name: 'Dan', email: 'dan@test.com', passwordHash: 'secret' };
    const dto = sanitizeUserDTO(raw as any);
    expect((dto as any).passwordHash).toBeUndefined();
  });
});
```

---

## 10. When NOT to Use This Pattern
- Do not add `server-only` to pure utility functions (e.g. date formatters or string helpers) that need to be shared symmetrically across both Server and Client Components.

---

## 11. Frequently Asked Interview Questions

<details>
<summary>1. How does `import 'server-only'` prevent client bundle leakage?</summary>
The package uses export condition mapping. When a bundler resolves client bundles, it matches the browser condition which exports an error-throwing module, failing the build at compile time.
</details>

<details>
<summary>2. What data types can be serialized across the React Flight boundary?</summary>
Strings, numbers, booleans, null, undefined, plain objects, arrays, Maps, Sets, TypedArrays, Dates, and Promises. Functions and Symbols cannot be passed.
</details>

<details>
<summary>3. What is the role of `client-only`?</summary>
It is the inverse of `server-only`: it prevents modules that rely strictly on browser APIs (like `window`, `localStorage`, or `indexedDB`) from being executed on the server.
</details>

<details>
<summary>4. What are React 19 Taint APIs?</summary>
`experimental_taintObjectReference` and `experimental_taintUniqueValue` allow developers to mark specific objects or values so that React automatically throws an error if they are passed across the Flight protocol.
</details>

<details>
<summary>5. Why should database rows always be transformed into DTOs?</summary>
To prevent accidental over-fetching and leakage of sensitive database fields (password hashes, tenant IDs, deleted flags) into the client RSC payload.
</details>

<details>
<summary>6. Does `'use client'` mean the component runs only on the client?</summary>
No. Client Components still pre-render on the server into HTML during initial page load, and then hydrate in the browser.
</details>

<details>
<summary>7. Can you pass a Server Action to a Client Component as a prop?</summary>
Yes. Server Actions (`'use server'`) are serializable RPC endpoints and can be passed to Client Components as props or form actions.
</details>

<details>
<summary>8. Where should `import 'server-only'` be placed?</summary>
At the very top of files containing database logic, authentication helpers, or private environment variables.
</details>

---

## 12. Hands-on Practice Exercises

<details>
<summary>Exercise 1 (Easy): Install and add server-only to a service</summary>
Add `import 'server-only'` to a user service file and test that importing it into a Client Component breaks the build.
</details>

<details>
<summary>Exercise 2 (Medium): Create a DTO sanitizer</summary>
Implement a DTO mapping function that strips sensitive fields from user profiles.
</details>

<details>
<summary>Exercise 3 (Hard): Configure React Taint API</summary>
Enable `experimental.taint = true` in `next.config.ts` and taint sensitive API tokens.
</details>

---

## 13. Cross-References & Further Reading
- [Data Access Layer Architecture](file:///E:/Learning/engineering-notes/02-frontend/nextjs/05-data-fetching/README.md)
- [Official Server-Only Package](https://nextjs.org/docs/app/building-your-application/rendering/composition-patterns#keeping-server-only-code-out-of-the-client-environment)
