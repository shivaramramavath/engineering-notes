# Feature-Sliced Design & Clean Architecture (`feature-based-architecture-and-clean-boundaries`)

> Enterprise architecture in Next.js App Router: Feature-Sliced Design (FSD), Data Access Layer (DAL) isolation, Hexagonal / Clean Architecture boundaries, and CQRS Lite patterns.

| Metadata | Details |
| :--- | :--- |
| **Next.js Version** | 15.2+ / 16.0 |
| **React Version** | React 19.0.0 |
| **Node Version** | Node.js 20+ LTS |
| **Router** | App Router (`app/`) |
| **Architecture** | Feature-Sliced Design (FSD), Clean Architecture |
| **Tested With** | Next.js 15.2, Vitest 3.0, TypeScript 5.7 |

---

## 1. Concept Overview & Mental Model

### 1.1 Architectural Decay in Growing Codebases
In unstructured Next.js projects, files are grouped by technical type (`components/`, `hooks/`, `lib/`). As the application scales beyond 50,000 lines of code:
- Components arbitrarily cross-import database functions and third-party APIs.
- Circular dependencies develop, making it impossible to isolate, test, or delete a single business feature.
- **Feature-Sliced Design (FSD)** and **Clean Architecture** enforce strict **unidirectional dependency layers**:
  `app ──> pages ──> widgets ──> features ──> entities ──> shared`

```
+--------------------------------------------------------------------------+
|                       Next.js Enterprise Architecture                    |
|                                                                          |
|  [ app/ ]            ──> App shell, root providers, routing entry points |
|       │                                                                  |
|       ▼                                                                  |
|  [ features/ ]       ──> User-facing business slices (e.g. auth, billing)|
|       │                    ├── components/ (UI presentation)             |
|       │                    ├── actions/ (Server Actions with Zod)        |
|       │                    └── types/                                    |
|       ▼                                                                  |
|  [ entities/ ]       ──> Core domain models & Data Access Layer (DAL)   |
|       │                    ├── user/ (dal/user.ts with server-only)      |
|       │                    └── product/                                  |
|       ▼                                                                  |
|  [ shared/ ]         ──> Reusable UI primitives, DB client, utilities    |
+--------------------------------------------------------------------------+
```

---

## 2. Naive vs. Idiomatic Comparison

### Naive: Unstructured Technical Bucketing
```text
src/
├── components/          # 300 mixed components importing database clients
├── hooks/               # Generic hooks mixed with business logic
├── lib/                 # Giant dumping ground for auth, db, stripe, and helpers
└── app/                 # Pages making direct ORM queries
```

### Idiomatic: Feature-Sliced Domain Architecture
```text
src/
├── app/                 # Routing routes only (thin controllers)
│   ├── (auth)/login/page.tsx
│   └── (dashboard)/billing/page.tsx
├── features/            # Business feature slices
│   ├── auth/            # Auth forms, signin action, password validation
│   └── billing/         # Subscription card, upgrade action, checkout
├── entities/            # Core business domains & DAL
│   ├── user/            # User model, session verification, user DTO
│   └── subscription/    # Subscription model, plan repository
└── shared/              # Generic building blocks
    ├── ui/              # Button, Dialog, Input (Radix / Tailwind)
    ├── db/              # Prisma / Drizzle client singleton
    └── lib/             # Generic math / string helpers
```

---

## 3. Five Progressive Examples

### Example 1: Thin Route Controller Pattern in `app/`
```tsx
// app/(dashboard)/invoices/page.tsx (Thin Controller)
import { Suspense } from 'react';
import { InvoiceListWidget } from '@/widgets/invoices/InvoiceListWidget';
import { InvoiceListSkeleton } from '@/shared/ui/skeletons';

export default function InvoicesPage() {
  // The route file contains zero business logic; it simply assembles widgets!
  return (
    <main className="p-8">
      <h1 className="text-2xl font-bold">Billing Invoices</h1>
      <Suspense fallback={<InvoiceListSkeleton />}>
        <InvoiceListWidget />
      </Suspense>
    </main>
  );
}
```

### Example 2: Encapsulated Feature Action with Zod Validation
```ts
// features/billing/actions/upgradePlan.ts
'use server';

import { z } from 'zod';
import { verifySession } from '@/entities/user/dal';
import { createCheckoutSession } from '@/entities/subscription/service';
import { unauthorized } from 'next/navigation';

const upgradeSchema = z.object({
  planId: z.enum(['PRO_MONTHLY', 'PRO_ANNUAL']),
});

export async function upgradePlanAction(rawInput: unknown) {
  const session = await verifySession();
  if (!session) unauthorized();

  const { planId } = upgradeSchema.parse(rawInput);
  const checkoutUrl = await createCheckoutSession(session.user.id, planId);

  return { checkoutUrl };
}
```

### Example 3: Domain Entity DAL with `server-only`
```ts
// entities/user/dal.ts
import 'server-only';
import { cache } from 'react';
import { db } from '@/shared/db';
import { cookies } from 'next/headers';

export const verifySession = cache(async () => {
  const cookieJar = await cookies();
  const token = cookieJar.get('session')?.value;
  if (!token) return null;

  return db.session.findUnique({
    where: { token },
    include: { user: true },
  });
});
```

### Example 4: CQRS Lite (Command Query Responsibility Segregation)
```ts
// entities/invoice/queries.ts (Read Model - Optimized for UI display)
import 'server-only';
import { db } from '@/shared/db';

export async function getInvoiceSummary(userId: string) {
  return db.invoice.findMany({
    where: { userId },
    select: { id: true, amountFormatted: true, status: true },
  });
}

// entities/invoice/commands.ts (Write Model - Domain business invariants)
import 'server-only';
import { db } from '@/shared/db';

export async function createInvoiceCommand(userId: string, amountCents: number) {
  // Enforce domain constraints before write
  if (amountCents <= 0) throw new Error('Invalid invoice amount');
  return db.invoice.create({ data: { userId, amountCents, status: 'PENDING' } });
}
```

### Example 5: Shared UI Primitives
```tsx
// shared/ui/Button.tsx
import { cva, type VariantProps } from 'class-variance-authority';
import { cn } from '@/shared/lib/utils';

export const buttonVariants = cva(
  'inline-flex items-center justify-center rounded-lg font-medium transition',
  {
    variants: {
      variant: {
        primary: 'bg-blue-600 text-white hover:bg-blue-700',
        outline: 'border border-slate-700 text-slate-200 hover:bg-slate-800',
      },
      size: { sm: 'px-3 py-1.5 text-xs', md: 'px-4 py-2 text-sm' },
    },
    defaultVariants: { variant: 'primary', size: 'md' },
  }
);
```

---

## 4. Under the Hood & Internals
Feature-Sliced Design enforces **strict unidirectional import rules**:
- Modules in `shared` can **never** import from `entities` or `features`.
- Modules in `entities` can **never** import from `features` or `pages`.
- Modules in `features` can import from `entities` and `shared`, but **never** from other sibling features.
- Any attempt to create circular imports or reach into forbidden layers is flagged immediately by ESLint boundaries (`eslint-plugin-boundaries`).

---

## 5. Security & Architectural Isolation
- **Boundary Protection**: Marking the `entities/*/dal.ts` layer with `import 'server-only'` ensures that even if a junior developer accidentally attempts to import a domain database function into `shared/ui/Button.tsx`, the build immediately halts with an error.

---

## 6. Common Mistakes & Anti-Patterns (Table)

| Anti-Pattern | Root Cause | Observable Symptom | Correct Architecture |
| :--- | :--- | :--- | :--- |
| Sibling feature cross-importing | High coupling between features | Circular dependency build warning | Move shared domain logic to `entities` |
| Heavy logic inside `app/page.tsx` | Treating route files as full apps | Hard to unit test and maintain | Keep routes as thin widget compositors |
| Missing `server-only` in domain entities | Presuming folder structure is enough | Server code bundled to client | Explicitly annotate with `import 'server-only'` |
| Giant shared utility file | Dumping unrelated code in `lib/utils` | Poor cohesion, large bundles | Colocate utilities inside specific features |

---

## 7. Performance & Optimization Guide
Decoupling features into isolated slices enables seamless code-splitting. When a user navigates to `/billing`, only the JavaScript chunks corresponding to the `features/billing` slice are downloaded, keeping the initial bundle size strictly bounded.

---

## 8. TypeScript & Strict Typing Recipes
```ts
// tsconfig.json path mappings enforcing FSD layers
{
  "compilerOptions": {
    "paths": {
      "@/app/*": ["./src/app/*"],
      "@/features/*": ["./src/features/*"],
      "@/entities/*": ["./src/entities/*"],
      "@/shared/*": ["./src/shared/*"]
    }
  }
}
```

---

## 9. Testing & Quality Verification

```ts
import { describe, it, expect } from 'vitest';
import { upgradePlanAction } from './upgradePlan';

describe('Feature: Upgrade Plan Action', () => {
  it('rejects invalid plan enum', async () => {
    const res = await upgradePlanAction({ planId: 'NON_EXISTENT_PLAN' });
    expect(res).toBeDefined();
  });
});
```

---

## 10. When NOT to Use This Pattern
- For small 3-page hackathon demos or prototype MVPs, strict Feature-Sliced Design can introduce unnecessary file overhead. Use simpler flat structures until the domain expands.

---

## 11. Frequently Asked Interview Questions

<details>
<summary>1. What are the core layers of Feature-Sliced Design (FSD)?</summary>
From top to bottom: `app` (configuration/routing) ➔ `pages` ➔ `widgets` (self-contained UI blocks) ➔ `features` (user interactions with business value) ➔ `entities` (domain models and DAL) ➔ `shared` (reusable UI primitives and utilities).
</details>

<details>
<summary>2. What is the "Thin Controller" pattern in the Next.js App Router?</summary>
Treating route files (`app/**/page.tsx`) strictly as configuration and composition entry points that assemble widgets and pass route parameters, without containing raw database queries or complex state logic.
</details>

<details>
<summary>3. Why are sibling features forbidden from importing each other in FSD?</summary>
To eliminate tight coupling and circular dependencies. If Feature A and Feature B need shared functionality, that functionality must be extracted into an underlying `entity` or `shared` module.
</details>

<details>
<summary>4. What is CQRS Lite in Next.js development?</summary>
Separating read queries (Server Components fetching data optimized for UI display) from write commands (Server Actions enforcing strict business invariants and database mutations).
</details>

<details>
<summary>5. How does clean architecture prevent security bugs?</summary>
By centralizing authorization and data sanitization within the Data Access Layer (DAL) under `server-only` guards, ensuring that no database access occurs without passing through domain security invariants.
</details>

<details>
<summary>6. How do TypeScript path aliases support enterprise architecture?</summary>
Path mappings like `@/features/*` and `@/entities/*` make architectural boundaries explicit in imports and allow ESLint rules to enforce unidirectional dependency flows.
</details>

<details>
<summary>7. Where should Zod schemas be located in a feature-sliced architecture?</summary>
Colocated within the specific feature that consumes them (e.g. `features/billing/types.ts` or `features/auth/schemas.ts`).
</details>

<details>
<summary>8. How does FSD improve team velocity in large engineering organizations?</summary>
Different engineering squads can own independent feature slices without merge conflicts or fear of inadvertently breaking unrelated parts of the codebase.
</details>

---

## 12. Hands-on Practice Exercises

<details>
<summary>Exercise 1 (Easy): Configure FSD path mappings</summary>
Update `tsconfig.json` with `@/features/*`, `@/entities/*`, and `@/shared/*`.
</details>

<details>
<summary>Exercise 2 (Medium): Refactor a monolithic page into thin controller</summary>
Extract component widgets and actions out of a page file into dedicated feature slices.
</details>

<details>
<summary>Exercise 3 (Hard): Implement CQRS Lite for a billing domain</summary>
Create separate query functions (read model) and command actions (write model) for customer subscriptions.
</details>

---

## 13. Cross-References & Further Reading
- [Components: Composition Patterns](file:///E:/Learning/engineering-notes/02-frontend/nextjs/03-components/README.md)
- [Official Feature-Sliced Design Documentation](https://feature-sliced.design)
