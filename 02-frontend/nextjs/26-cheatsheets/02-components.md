# Module 26: Server & Client Components Cheatsheet

> **Quick Reference**: React Server Components (RSC) vs Client Components, boundary rules, composition patterns, and isolation guards in Next.js 15+.

---

## 1. Server vs Client Component Feature Matrix

| Capability | Server Component (Default) | Client Component (`"use client"`) |
| :--- | :--- | :--- |
| **Directive** | None needed (implicit default) | `'use client'` at top of file |
| **Execute Environment** | Server only (Node.js / Edge) | Server (during SSR) + Browser (hydration) |
| **Client JS Shipped** | **0 KB** (Zero client bundle) | Included in client JS bundle |
| **Data Access** | Direct DB queries, ORM, filesystem | `fetch()`, React Query, SWR, Server Actions |
| **React Hooks** | ❌ No `useState`, `useEffect` | ✅ `useState`, `useEffect`, `useRef`, etc. |
| **Browser APIs** | ❌ No `window`, `document`, `navigator` | ✅ Full access in `useEffect` / event handlers |
| **Async Components** | ✅ Native `async function Component()` | ❌ Not supported (must use `use()` or hooks) |
| **Event Listeners** | ❌ No `onClick`, `onChange` | ✅ Full interactive event listeners |

---

## 2. Component Composition Rule: Interleaving

> **Golden Rule**: Never import a Server Component directly into a Client Component file. Instead, pass the Server Component as `children` or another prop.

```tsx
// ❌ WRONG: ServerComponent becomes bundled as Client Component
'use client';
import { ServerComponent } from './ServerComponent'; // INFECTION!

export function ClientWrapper() {
  return <div><ServerComponent /></div>;
}

// ✅ CORRECT: Composition via children slot
// app/page.tsx (Server Component)
import { ClientWrapper } from './ClientWrapper';
import { ServerComponent } from './ServerComponent';

export default function Page() {
  return (
    <ClientWrapper>
      <ServerComponent />
    </ClientWrapper>
  );
}
```

---

## 3. Serialization Rules (Props Across Boundaries)

Props passed from a Server Component to a Client Component must be **JSON-serializable**:

| Data Type | Can Pass as Prop? | Notes |
| :--- | :--- | :--- |
| **Primitives** (`string`, `number`, `boolean`) | ✅ Yes | Direct serialization |
| **Plain Objects / Arrays** | ✅ Yes | Deeply serialized |
| **`Date` Instances** | ✅ Yes | Revived automatically in React 19 |
| **`Promise`** | ✅ Yes | Resolved on client via React 19 `use(promise)` |
| **Server Actions** | ✅ Yes | Must have `'use server'` directive |
| **Functions / Callbacks** | ❌ No | Cannot serialize JavaScript executable closures |
| **Class Instances** | ❌ No | Prototype methods are stripped |

---

## 4. Compilation Guards: `server-only` and `client-only`

Install guards to prevent accidental bundle leaks:
```bash
npm install server-only client-only
```

### Server Guard (`lib/db.ts`)
```typescript
import 'server-only'; // Throws build error if imported in "use client" file

export const db = new DatabaseClient();
```

### Client Guard (`lib/canvas-utils.ts`)
```typescript
import 'client-only'; // Throws build error if imported in Server Component

export function setupWebGL() {
  return window.WebGLRenderingContext;
}
```
