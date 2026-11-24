# Zod — Basic to Advanced

A comprehensive reference card for `zod` schema validation in React 19.

> **Target Version:** Zod v3.24+ / v4 preview  
> **Philosophy:** TypeScript-first schema declaration with static type inference  
> **Ecosystem:** Validating forms (React Hook Form), APIs, and route parameters  

---

## Level 0 — Concepts

- **Single Source of Truth**: You write a runtime schema once; TypeScript infers the exact type using `z.infer<typeof schema>`. Eliminates duplicate interface definitions that drift out of sync.
- **Parsing vs Validation**: Zod does not just check types; it transforms and parses data (`safeParse()`). Input strings can be coerced into numbers or trimmed before validation.
- **Core Primitives Table**:

| API | Type | Purpose |
| :--- | :--- | :--- |
| `z.object({})` | Function | Defines an object schema with typed properties |
| `z.string()`, `z.number()` | Function | Basic primitive schemas with chained rules (`min`, `email`) |
| `schema.parse()` | Method | Parses data synchronously; throws `ZodError` on failure |
| `schema.safeParse()` | Method | Returns `{ success: true, data }` or `{ success: false, error }` |
| `z.infer<typeof s>` | Type | Extracts the static TypeScript type representation |
| `z.coerce.*` | Function | Coerces types (e.g. `z.coerce.date()` converts strings to Dates) |

---

## Level 1 — Basic Setup & Usage

### 1.1 Installation
```bash
npm install zod
```

### 1.2 Defining a Schema and Inferring Types
```ts
import { z } from 'zod';

// 1. Define schema
export const userProfileSchema = z.object({
  id: z.string().uuid(),
  username: z.string().min(3).max(20),
  email: z.string().email(),
  age: z.number().int().positive().optional(),
  role: z.enum(['admin', 'member', 'guest']).default('member'),
});

// 2. Extract static TypeScript type
export type UserProfile = z.infer<typeof userProfileSchema>;
```

### 1.3 Safe Parsing API Payloads
```ts
export function parseIncomingUser(json: unknown): UserProfile | null {
  const result = userProfileSchema.safeParse(json);
  if (!result.success) {
    console.error('Validation errors:', result.error.format());
    return null;
  }
  return result.data; // 100% typed and validated!
}
```

---

## Level 2 — Intermediate: Refinements & Custom Rules

```ts
// Password Confirmation Match Validation
export const registrationSchema = z
  .object({
    password: z.string().min(8, 'Password must be at least 8 characters'),
    confirmPassword: z.string(),
  })
  .refine((data) => data.password === data.confirmPassword, {
    message: 'Passwords must match',
    path: ['confirmPassword'], // Highlights error on confirmPassword field
  });
```

---

## Level 3 — Advanced: Coercion & Transformations

```ts
// Search params coercion from URL strings
export const searchParamsSchema = z.object({
  page: z.coerce.number().int().default(1),
  limit: z.coerce.number().int().default(20),
  query: z.string().trim().toLowerCase(),
});
```

---

## Level 4 — Production Checklist & Anti-Patterns

| Anti-Pattern | Why It Breaks | Recommended Practice |
| :--- | :--- | :--- |
| **Using `schema.parse()` in React Render** | Throws an unhandled exception, triggering an Error Boundary crash on bad inputs. | Use `schema.safeParse()` or use form resolvers. |
| **Maintaining Duplicate TypeScript Interfaces** | Type definitions fall out of sync with validation rules. | Always derive types using `z.infer<typeof schema>`. |
| **Omitting Custom Error Messages** | Users see raw technical messages like `String must contain at least 5 character(s)`. | Provide user-friendly messages: `z.string().min(5, 'Minimum 5 characters')`. |
| **Deeply Nested Refinements Without `path`** | Validation error attaches to root object rather than the specific violating input field. | Explicitly specify `path: ['fieldName']` in `.refine()`. |
