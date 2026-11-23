# Comprehensive Zod Guide in TypeScript

> Master modern runtime validation with Zod: primitives, objects, unions, refinements, transforms, asynchronous parsing, recursive schemas, custom error maps, and performance.

| Metadata | Details |
| :--- | :--- |
| **Category** | Runtime Validation |
| **Difficulty** | Intermediate |
| **Prerequisites** | [15-runtime-validation/01-schema-validation.md](./01-schema-validation.md) |
| **TypeScript Baseline** | TS 4.5+, Zod 3.22+ |
| **Tested tsconfig** | `"strict": true, "target": "ES2022", "module": "NodeNext"` |

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
Zod has become the de facto standard schema validation library for TypeScript. It bridges the gap between static type checking and dynamic runtime data, enabling developers to declare a single schema that simultaneously validates runtime inputs and generates static TypeScript types.

Unlike older validation libraries that relied on decorators or separate configuration files, Zod uses a clean, fluent, functional API with zero external dependencies. Understanding Zod's parsing mechanics, refinement pipelines, and error handling mechanisms is essential for modern fullstack TypeScript development (Next.js server actions, tRPC, React Hook Form, and Express APIs).

---

## 2. Core Concepts
- **Primitives and Modifiers**: `z.string()`, `z.number()`, `z.boolean()`, `z.bigint()`, with chainable modifiers: `.min()`, `.max()`, `.email()`, `.url()`, `.uuid()`, `.optional()`, `.nullable()`.
- **Object Schema Operations**:
  - `.extend()`: Appends new properties to an existing object schema.
  - `.merge()`: Combines two object schemas.
  - `.pick()` and `.omit()`: Creates sub-schemas matching TypeScript's utility types.
  - `.partial()` and `.required()`: Toggles optionality.
  - `.strict()` vs `.passthrough()` vs `.strip()`: Controls unknown property behavior.
- **Discriminated Unions (`z.discriminatedUnion`)**: High-performance union matching using a shared tag property.
- **Refinements (`.refine()`, `.superRefine()`)**: Custom validation predicates that inspect values and inject detailed issues into the error tree.
- **Transformations (`.transform()`)**: Modifies data during parsing (e.g. converting strings to numbers or Dates).
- **Asynchronous Validation (`.parseAsync()`)**: Handles async operations such as querying a database to check email uniqueness during validation.

---

## 3. How It Works Internally

```
                      z.object({...}).safeParse(data)
                                     |
                                     v
                           ParseContext Creation
                                     |
                +--------------------+--------------------+
                |                                         |
                v                                         v
         Valid Primitive?                         Has Unknown Keys?
                |                                         |
         Run Refinements                           Check .strict() /
         (e.g. min, regex)                         .strip() / .passthrough()
                |                                         |
                v                                         v
         Run .transform()                          Collect Issues into
         (Produce output)                          ZodError array
                |                                         |
                +--------------------+--------------------+
                                     |
                                     v
              Returns { success: true, data } | { success: false, error }
```

Zod traverses the schema AST recursively, passing a `ParseContext` down the tree. It accumulates all constraint violations into an array of `ZodIssue` objects, allowing developers to present complete lists of field errors to users in one pass.

---

## 4. Syntax & API Reference

```typescript
import { z } from 'zod';

// 1. Comprehensive Schema Definition
const RegistrationSchema = z.object({
  username: z.string().min(3).max(20).regex(/^[a-zA-Z0-9_]+$/),
  email: z.string().email(),
  password: z.string().min(8),
  confirmPassword: z.string(),
  age: z.coerce.number().min(18), // z.coerce transforms input to number
}).refine(data => data.password === data.confirmPassword, {
  message: "Passwords do not match",
  path: ["confirmPassword"], // Attaches error to confirmPassword field!
});

// 2. Inferring Output Type
type RegistrationDto = z.infer<typeof RegistrationSchema>;

// 3. Safe Parsing
const result = RegistrationSchema.safeParse(untrustedInput);
if (!result.success) {
  console.log(result.error.flatten().fieldErrors);
}
```

---

## 5. Practical Examples

### Example 1: Asynchronous Database Uniqueness Refinement
```typescript
import { z } from 'zod';

declare function isEmailTaken(email: string): Promise<boolean>;

export const SignUpSchema = z.object({
  email: z.string().email(),
}).refine(async data => {
  const taken = await isEmailTaken(data.email);
  return !taken;
}, {
  message: 'Email address is already in use',
  path: ['email'],
});

// Must use parseAsync for async refinements:
export async function validateSignUp(input: unknown) {
  return await SignUpSchema.parseAsync(input);
}
```

### Example 2: Discriminated Union Event Bus
```typescript
import { z } from 'zod';

export const PaymentEventSchema = z.discriminatedUnion('status', [
  z.object({
    status: z.literal('SUCCESS'),
    transactionId: z.string(),
    amountCents: z.number().positive(),
  }),
  z.object({
    status: z.literal('FAILED'),
    errorCode: z.string(),
    retryable: z.boolean(),
  }),
]);

export type PaymentEvent = z.infer<typeof PaymentEventSchema>;
```

### Example 3: Edge Case - Recursive Tree Structure with `z.lazy`
```typescript
import { z } from 'zod';

interface Comment {
  id: string;
  text: string;
  replies?: Comment[];
}

export const CommentSchema: z.ZodType<Comment> = z.lazy(() =>
  z.object({
    id: z.string(),
    text: z.string(),
    replies: z.array(CommentSchema).optional(),
  })
);
```

### Example 4: Compiler Failure Case - Synchronous Parsing with Async Refinements
```typescript
const AsyncSchema = z.string().refine(async val => val.length > 3);

// @ts-expect-error Zod throws at runtime if parse() is called on an async schema:
// Error: You must use parseAsync on an async schema!
AsyncSchema.parse('hello');
```

### Example 5: Custom Internationalized Error Map
```typescript
import { z } from 'zod';

const customErrorMap: z.ZodErrorMap = (issue, ctx) => {
  if (issue.code === z.ZodIssueCode.invalid_type) {
    if (issue.expected === 'string') {
      return { message: 'Ce champ doit être une chaîne de caractères' };
    }
  }
  return { message: ctx.defaultError };
};

z.setErrorMap(customErrorMap);
```

---

## 6. Edge Cases, Pitfalls & Gotchas
1. **Calling `.parse()` on Async Schemas**: If any `.refine()` or `.transform()` callback returns a Promise, calling `.parse()` throws a fatal runtime exception: `Error: Synchronous parse encountered promise`. Always use `.parseAsync()`.
2. **`z.coerce` Caveats**: `z.coerce.boolean()` coerces `"false"` to `true` because `Boolean("false") === true` in JavaScript! For boolean form parameters, use `.transform(val => val === 'true')`.
3. **`z.infer` with Transforms**: `z.infer<T>` reflects the *output* type. If you need the pre-transformed type accepted by the form, use `z.input<T>`.
4. **Deep Cloned Memory**: `.parse()` returns a cloned, sanitized object; it does not mutate the original input.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Consequence | Correction |
| :--- | :--- | :--- |
| `z.coerce.boolean()` on string form inputs | `"false"` coerces to `true`! | Use `.refine` or explicit string comparison |
| Calling `.parse()` on an async schema | Throws runtime exception | Use `await schema.parseAsync(data)` |
| Omitting `path` in `.refine()` | Error attached to root object, not form field | Add `path: ['fieldName']` in refinement |
| Overusing `.refine()` for standard regex | Missing automated documentation | Use built-in `.regex()`, `.email()`, `.uuid()` |
| Catching generic `Error` instead of `ZodError` | Misses structured field-level error arrays | Catch `if (err instanceof z.ZodError)` |

---

## 8. Performance & Type-Checker Complexity
- Zod parses objects in pure JavaScript without JIT compilation. For large payloads ($>10\text{MB}$ JSON), validation can take tens of milliseconds. For ultra-high throughput network services, consider TypeBox or ArkType.
- In TypeScript's type checker, complex recursive schemas or unions of 30+ variants can increase TSServer completion latency. Break complex schemas into modular sub-schemas.

---

## 9. Compiler Internals
In Zod's type definitions:
- `z.infer<T>` is defined as `T extends z.ZodType<infer Output, any, any> ? Output : never`.
- Object schema extension (`.extend()`) computes type intersections and mapped overwrites:
  `T & { [K in keyof Extra]: Extra[K] }`.

---

## 10. Best Practices & Design Guidelines
- Use `z.safeParse()` instead of `.parse()` in web servers to handle failures gracefully without throwing exceptions.
- Always attach field paths (`path: ['field']`) when refining multiple fields.
- Combine Zod with React Hook Form using `@hookform/resolvers/zod` for zero-boilerplate form validation.
- Separate database persistence models from API transport schemas using `.pick()` and `.omit()`.
- Use `z.discriminatedUnion()` rather than `z.union()` for polymorphic event payloads.

---

## 11. Real-World Case Study
A healthcare appointments scheduling API validated time slots. A user could submit an appointment where `endTime` was before `startTime`. The database saved the invalid appointment, which subsequently crashed the calendar renderer for all doctors in the clinic.

**Zod Refinement Solution:**
```typescript
export const AppointmentSlotSchema = z.object({
  doctorId: z.string().uuid(),
  startTime: z.string().datetime(),
  endTime: z.string().datetime(),
}).refine(data => {
  const start = new Date(data.startTime).getTime();
  const end = new Date(data.endTime).getTime();
  return end > start;
}, {
  message: 'End time must be after start time',
  path: ['endTime'],
});
```
Invalid slot submissions were rejected at the API gateway with field-level highlights on the `endTime` form input.

---

## 12. Interview Questions

<details>
<summary>1. What is the difference between `.parse()` and `.safeParse()` in Zod?</summary>

- `.parse(data)` returns the validated data directly or throws a `ZodError` exception if validation fails.
- `.safeParse(data)` never throws; it returns a discriminated union: `{ success: true, data: T }` or `{ success: false, error: ZodError }`.
</details>

<details>
<summary>2. Why does `z.coerce.boolean()` coerce the string `"false"` to `true`?</summary>

Because `z.coerce.boolean()` wraps the input in JavaScript's native `Boolean(x)` constructor. In JavaScript, any non-empty string is truthy, so `Boolean("false")` evaluates to `true`.
</details>

<details>
<summary>3. What is the advantage of `z.discriminatedUnion()` over `z.union()`?</summary>

`z.discriminatedUnion()` uses a specific discriminator key (like `type: 'A' | 'B'`) to perform an immediate $\mathcal{O}(1)$ dictionary lookup of the target schema. `z.union()` must test each candidate schema sequentially until one matches, which is slower and produces ambiguous error messages.
</details>

<details>
<summary>4. What does `.superRefine()` offer that `.refine()` does not?</summary>

`.superRefine()` provides direct access to the `RefinementCtx`, allowing developers to add multiple distinct issues with custom error codes, custom paths, and custom messages from a single validation pass.
</details>

<details>
<summary>5. How can you extract field errors formatted cleanly for a frontend form?</summary>

Using `result.error.flatten().fieldErrors`, which produces an object mapping field names to arrays of error message strings: `Record<string, string[]>`.
</details>

<details>
<summary>6. How do you implement recursive data structures like JSON trees in Zod?</summary>

By using `z.lazy(() => Schema)` and explicitly annotating the schema with a manual TypeScript interface to break circular compiler inference.
</details>

<details>
<summary>7. How do you prevent unknown keys from being silently stripped in Zod?</summary>

By calling `.strict()` on the object schema: `z.object({...}).strict()`. Any extra keys will trigger a validation error.
</details>

<details>
<summary>8. What is the difference between `z.optional()` and `z.nullable()`?</summary>

- `.optional()` allows the value to be `undefined` (or omitted).
- `.nullable()` allows the value to be `null`.
- `.nullish()` allows either `null` or `undefined`.
</details>

---

## 13. Exercises & Challenges

### Easy
1. **Username Validator**: Create a schema validating that a username is alphanumeric, between 4 and 16 characters, and lowercase.
   <details>
   <summary>Solution</summary>

   ```typescript
   import { z } from 'zod';

   export const UsernameSchema = z
     .string()
     .min(4)
     .max(16)
     .regex(/^[a-z0-9_]+$/, 'Must be lowercase alphanumeric');
   ```
   </details>

2. **Integer Extractor**: Write a schema that coerces a string to a positive integer.
   <details>
   <summary>Solution</summary>

   ```typescript
   import { z } from 'zod';

   export const PositiveIntSchema = z.coerce.number().int().positive();
   ```
   </details>

### Medium
3. **Password Confirmation Validator**: Build a schema for password registration checking minimum 8 chars, at least one number, and confirming that `password === confirmPassword`.
   <details>
   <summary>Solution</summary>

   ```typescript
   import { z } from 'zod';

   export const PasswordFormSchema = z.object({
     password: z.string().min(8).regex(/\d/, 'Must contain a digit'),
     confirmPassword: z.string(),
   }).refine(data => data.password === data.confirmPassword, {
     message: 'Passwords do not match',
     path: ['confirmPassword'],
   });
   ```
   </details>

4. **Pagination Query Validator**: Write a query string parser schema that defaults `page` to 1 and `limit` to 20, coercing strings from URL search params.
   <details>
   <summary>Solution</summary>

   ```typescript
   import { z } from 'zod';

   export const PaginationQuerySchema = z.object({
     page: z.coerce.number().int().positive().default(1),
     limit: z.coerce.number().int().min(1).max(100).default(20),
   });
   ```
   </details>

### Hard
5. **Polymorphic Form Schema with SuperRefine**: Implement a dynamic checkout schema where if `paymentMethod` is `'CREDIT_CARD'`, `cardNumber` and `cvv` are strictly required; if `paymentMethod` is `'BANK_TRANSFER'`, `iban` is required.
   <details>
   <summary>Solution</summary>

   ```typescript
   import { z } from 'zod';

   export const CheckoutSchema = z.object({
     paymentMethod: z.enum(['CREDIT_CARD', 'BANK_TRANSFER']),
     cardNumber: z.string().optional(),
     cvv: z.string().optional(),
     iban: z.string().optional(),
   }).superRefine((data, ctx) => {
     if (data.paymentMethod === 'CREDIT_CARD') {
       if (!data.cardNumber) {
         ctx.addIssue({ code: z.ZodIssueCode.custom, message: 'Card number is required', path: ['cardNumber'] });
       }
       if (!data.cvv) {
         ctx.addIssue({ code: z.ZodIssueCode.custom, message: 'CVV is required', path: ['cvv'] });
       }
     } else if (data.paymentMethod === 'BANK_TRANSFER') {
       if (!data.iban) {
         ctx.addIssue({ code: z.ZodIssueCode.custom, message: 'IBAN is required', path: ['iban'] });
       }
     }
   });
   ```
   </details>

6. **Zod OpenAPI Metadata Extractor**: Write a wrapper function `withDoc<T extends z.ZodTypeAny>(schema: T, meta: { title: string; description: string }): T` that attaches OpenAPI metadata to a schema's `.description` field.
   <details>
   <summary>Solution</summary>

   ```typescript
   import { z } from 'zod';

   export function withDoc<T extends z.ZodTypeAny>(
     schema: T,
     meta: { title: string; description: string }
   ): T {
     return schema.describe(JSON.stringify(meta));
   }
   ```
   </details>
