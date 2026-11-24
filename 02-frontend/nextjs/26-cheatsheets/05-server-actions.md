# Module 26: Server Actions & Mutations Cheatsheet

> **Quick Reference**: Server Actions (`"use server"`), form handling, React 19 `useActionState`, optimistic updates with `useOptimistic`, and mutation security.

---

## 1. Server Action Syntax Fundamentals

Server Actions are asynchronous functions with `'use server'` that execute securely on the server:

```typescript
// app/actions/todos.ts
'use server';

import { verifySession } from '@/lib/dal';
import { db } from '@/lib/db';
import { todos } from '@/lib/schema';
import { revalidatePath } from 'next/cache';
import { z } from 'zod';

const createTodoSchema = z.object({
  title: z.string().min(1).max(100),
});

export async function createTodo(prevState: unknown, formData: FormData) {
  const session = await verifySession(); // Authorization
  const parse = createTodoSchema.safeParse({ title: formData.get('title') });

  if (!parse.success) {
    return { error: parse.error.flatten().fieldErrors.title?.[0] };
  }

  await db.insert(todos).values({ title: parse.data.title, userId: session.userId });
  revalidatePath('/todos');
  return { success: true };
}
```

---

## 2. React 19 Hook: `useActionState`

Replaces legacy `useFormState` for managing pending states and form return values:

```tsx
// app/components/TodoForm.tsx
'use client';

import { useActionState } from 'react';
import { createTodo } from '@/app/actions/todos';

export function TodoForm() {
  const [state, formAction, isPending] = useActionState(createTodo, null);

  return (
    <form action={formAction} className="space-y-4">
      <input
        type="text"
        name="title"
        placeholder="New todo..."
        required
        disabled={isPending}
        className="border p-2 rounded"
      />
      <button type="submit" disabled={isPending} className="bg-blue-600 text-white px-4 py-2 rounded">
        {isPending ? 'Adding...' : 'Add Todo'}
      </button>
      {state?.error && <p className="text-red-500">{state.error}</p>}
    </form>
  );
}
```

---

## 3. Optimistic Updates with `useOptimistic`

Provides zero-latency visual feedback while the server action processes in the background:

```tsx
'use client';

import { useOptimistic, useTransition } from 'react';
import { toggleLikeAction } from '@/app/actions/likes';

export function LikeButton({ id, initialLikes }: { id: string; initialLikes: number }) {
  const [isPending, startTransition] = useTransition();
  const [optimisticLikes, setOptimisticLikes] = useOptimistic(
    initialLikes,
    (current, update: number) => current + update
  );

  return (
    <button
      onClick={() => {
        startTransition(async () => {
          setOptimisticLikes(1); // Immediate visual increment
          await toggleLikeAction(id); // Network round-trip
        });
      }}
      className="flex items-center gap-2"
    >
      ❤️ <span>{optimisticLikes}</span>
    </button>
  );
}
```

---

## 4. Server Action Security Checklist

| Rule | Rationale | Code Implementation |
| :--- | :--- | :--- |
| **Authenticate & Authorize** | Action IDs are public endpoints reachable by `curl`. | `const session = await verifySession();` |
| **Input Validation** | Never trust client arguments. | `schema.safeParse(formData)` |
| **Body Size Limits** | Mitigate Denial-of-Service (DoS). | `experimental.serverActions.bodySizeLimit: '2mb'` |
| **Allowed Origins** | Mitigate CSRF behind reverse proxies. | `allowedOrigins: ['app.example.com']` |
| **Avoid Unbounded Closures** | Don't leak private variables into encrypted client stubs. | Pass IDs as form arguments instead of nested closures. |
