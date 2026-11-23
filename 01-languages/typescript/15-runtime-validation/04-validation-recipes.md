# Production Validation Recipes in TypeScript

> Battle-tested production validation recipes in TypeScript: environment variables (`t3-env`), Express/Fastify request middleware, React Hook Form integration, polymorphic schemas, and custom branded types.

| Metadata | Details |
| :--- | :--- |
| **Category** | Runtime Validation |
| **Difficulty** | Advanced |
| **Prerequisites** | [15-runtime-validation/01-schema-validation.md](./01-schema-validation.md), [15-runtime-validation/02-zod.md](./02-zod.md) |
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
While understanding validation libraries conceptually is important, building real-world software requires applying validation patterns across diverse architectural layers: environment configuration, HTTP route parameter parsing, form handling, and domain branding.

This guide provides battle-tested, copy-pasteable production validation recipes that solve common engineering problems: validating environment variables at startup, safely parsing incoming HTTP headers/body/query parameters in Express and Fastify, validating multi-step forms with React Hook Form, and constructing nominally branded value objects.

---

## 2. Core Concepts
- **Environment Validation Recipe (`t3-env`)**: Validating `process.env` at startup, separating server-only variables from client-public variables (`NEXT_PUBLIC_`).
- **Express / Fastify Validation Middleware**: Intercepting incoming requests, validating body, query, and path parameters, and returning RFC 7807 problem details on failure.
- **Form Validation with React Hook Form**: Connecting Zod schemas directly into UI forms using `@hookform/resolvers/zod`.
- **Branded Types with Zod**: Creating nominally branded types (e.g. `UserId`, `OrderId`) that guarantee at compile time that an ID has passed through validation.
- **Polymorphic / Dynamic Schemas**: Validating arrays of mixed items using discriminated unions.

---

## 3. How It Works Internally

```
                   HTTP Request Ingress
                            |
                            v
       +-----------------------------------------+
       | Fastify / Express Validation Middleware |
       | - Validates req.body against BodySchema |
       | - Validates req.query against QuerySchema|
       +--------------------+--------------------+
                            |
                 Validation Succeeded?
                            |
            +---------------+---------------+
            |                               |
          (Yes)                           (No)
            v                               v
    Attach parsed data              Halt Pipeline Immediately
    to req.validatedBody            Return HTTP 400 Bad Request
            |                       with Field Errors JSON
            v                               |
    Call Route Handler                      x
```

---

## 4. Syntax & API Reference

### Recipe 1: Production Environment Validator
```typescript
import { z } from 'zod';

const ServerEnvSchema = z.object({
  NODE_ENV: z.enum(['development', 'production', 'test']).default('development'),
  PORT: z.coerce.number().int().positive().default(3000),
  DATABASE_URL: z.string().url(),
  JWT_SECRET: z.string().min(32),
});

export const env = ServerEnvSchema.parse(process.env);
export type ServerEnv = z.infer<typeof ServerEnvSchema>;
```

### Recipe 2: Express Request Validator Middleware
```typescript
import { Request, Response, NextFunction } from 'express';
import { AnyZodObject, ZodError } from 'zod';

export const validateRequest = (schemas: {
  body?: AnyZodObject;
  query?: AnyZodObject;
  params?: AnyZodObject;
}) => async (req: Request, res: Response, next: NextFunction) => {
  try {
    if (schemas.body) req.body = await schemas.body.parseAsync(req.body);
    if (schemas.query) req.query = await schemas.query.parseAsync(req.query);
    if (schemas.params) req.params = await schemas.params.parseAsync(req.params);
    return next();
  } catch (error) {
    if (error instanceof ZodError) {
      return res.status(400).json({
        status: 'error',
        message: 'Validation failed',
        errors: error.flatten().fieldErrors,
      });
    }
    return res.status(500).json({ status: 'error', message: 'Internal Server Error' });
  }
};
```

---

## 5. Practical Examples

### Example 1: Branded Value Object Factory with Zod
```typescript
import { z } from 'zod';

declare const UserIdBrand: unique symbol;
export type UserId = string & { readonly [UserIdBrand]: true };

export const UserIdSchema = z
  .string()
  .uuid()
  .transform(val => val as UserId);

export function parseUserId(raw: unknown): UserId {
  return UserIdSchema.parse(raw);
}
```

### Example 2: React Hook Form Schema Integration
```typescript
import { z } from 'zod';

export const LoginSchema = z.object({
  email: z.string().email('Please enter a valid email address'),
  password: z.string().min(8, 'Password must be at least 8 characters'),
  rememberMe: z.boolean().default(false),
});

export type LoginFormValues = z.infer<typeof LoginSchema>;

// In React Component:
// const { register, handleSubmit, formState: { errors } } = useForm<LoginFormValues>({
//   resolver: zodResolver(LoginSchema),
// });
```

### Example 3: Edge Case - Handling Null vs Undefined in Query Strings
```typescript
import { z } from 'zod';

// Express query params are strings or undefined:
export const SearchFilterSchema = z.object({
  search: z.string().optional().default(''),
  page: z.coerce.number().int().positive().default(1),
  activeOnly: z
    .enum(['true', 'false'])
    .optional()
    .transform(val => val === 'true'),
});
```

### Example 4: Compiler Failure Case - Type Mismatch with Branded IDs
```typescript
declare const OrderIdBrand: unique symbol;
type OrderId = string & { readonly [OrderIdBrand]: true };

declare function cancelOrder(id: OrderId): void;

const rawId: string = '550e8400-e29b-41d4-a716-446655440000';
// @ts-expect-error TS2345: Argument of type 'string' is not assignable to parameter of type 'OrderId'.
cancelOrder(rawId);
```

### Example 5: Idiomatic Polymorphic Notification Payload Schema
```typescript
import { z } from 'zod';

export const NotificationSchema = z.discriminatedUnion('channel', [
  z.object({
    channel: z.literal('EMAIL'),
    recipientEmail: z.string().email(),
    subject: z.string().min(1),
  }),
  z.object({
    channel: z.literal('SMS'),
    phoneNumber: z.string().regex(/^\+[1-9]\d{1,14}$/),
    text: z.string().max(160),
  }),
  z.object({
    channel: z.literal('WEBHOOK'),
    endpointUrl: z.string().url(),
    authToken: z.string(),
  }),
]);

export type NotificationPayload = z.infer<typeof NotificationSchema>;
```

---

## 6. Edge Cases, Pitfalls & Gotchas
1. **Query Strings are Always Strings**: In HTTP query strings, `?page=1` arrives in Node.js as the string `"1"`. Using `z.number()` directly will fail. Always use `z.coerce.number()`.
2. **`z.coerce.date` Parses Invalid Dates to Invalid Date Objects**: `new Date("invalid")` does not throw in JS; it creates `Invalid Date`. Ensure your schema validates `.refine(d => !isNaN(d.getTime()))`.
3. **Environment Leaks**: Never export server environment schemas to client bundles (Vite / Next.js client components), or secrets will be exposed in browser JavaScript bundles.
4. **Body Mutation in Middleware**: Re-assigning `req.body = parsedData` guarantees that transformed fields (e.g. string to Date) are available to downstream route handlers.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Consequence | Correction |
| :--- | :--- | :--- |
| `z.number()` on HTTP query parameters | Validations fail; query parameters arrive as strings | Use `z.coerce.number()` |
| Validating without re-assigning parsed result | Route handler still sees raw untransformed input | `req.body = await schema.parseAsync(req.body)` |
| Allowing process to boot with invalid env | Crashes unpredictably during runtime operation | Validate env in `config.ts` on startup |
| Hand-written regex for UUIDs/emails | Incomplete edge case coverage | Use built-in `.uuid()` and `.email()` |
| Passing plain strings to functions expecting IDs | Accidentally passing user ID to order ID | Use branded types (`UserId`, `OrderId`) |

---

## 8. Performance & Type-Checker Complexity
- Reusing schema instances (`const UserSchema = z.object(...)`) avoids re-allocating schema objects on every request.
- Middleware executing `safeParseAsync` runs in $<0.1\text{ms}$ for standard JSON payloads, adding negligible latency to HTTP routes.

---

## 9. Compiler Internals
In TypeScript branded types:
- `UserId = string & { readonly [UserIdBrand]: true }` creates an intersection type.
- The Type Checker verifies that an ordinary `string` cannot be assigned to `UserId` because it lacks the unique symbol brand property, establishing nominal safety on top of structural subtyping.

---

## 10. Best Practices & Design Guidelines
- Validate environment variables immediately upon application startup.
- Validate request inputs at the middleware boundary before route controllers execute.
- Use branded types for database primary keys and entity IDs to prevent ID swapping bugs.
- Always use `z.coerce` for query parameters and route parameters that arrive as strings.
- Format validation errors into standard RFC 7807 problem details or flattened field errors.

---

## 11. Real-World Case Study
A fin-tech remittance platform allowed users to transfer money between accounts. The endpoint signature was `transferFunds(fromAccountId: string, toAccountId: string)`. A developer accidentally swapped the arguments in an automated payment script: `transferFunds(targetAccount, sourceAccount)`, causing funds to be debited from the wrong user.

**Remediation with Branded Schemas:**
```typescript
declare const SourceAccountIdBrand: unique symbol;
export type SourceAccountId = string & { readonly [SourceAccountIdBrand]: true };

declare const TargetAccountIdBrand: unique symbol;
export type TargetAccountId = string & { readonly [TargetAccountIdBrand]: true };

export function transferFunds(from: SourceAccountId, to: TargetAccountId) {
  // Swapping arguments is now a compile-time error!
}
```
Argument swapping errors were permanently eliminated across all microservice boundaries.

---

## 12. Interview Questions

<details>
<summary>1. How do you safely parse numerical HTTP query parameters using Zod?</summary>

Use `z.coerce.number()`. Because query parameters in Node.js are parsed as strings from the URL (e.g. `"10"`), `z.coerce.number()` invokes `Number(val)` before validation, whereas `z.number()` would fail with an invalid type error.
</details>

<details>
<summary>2. What is a branded type and why is it useful in validation?</summary>

A branded type is an intersection of a primitive type with an object carrying a unique symbol brand (e.g. `type UserId = string & { readonly __brand: unique symbol }`). It creates nominal typing, guaranteeing that a string can only be used as a `UserId` if it was explicitly parsed and validated by a trusted validator.
</details>

<details>
<summary>3. Why should environment variables be validated at application boot?</summary>

To follow the "Fail Fast" principle. If a critical environment variable (like `DATABASE_URL` or `JWT_SECRET`) is missing or invalid, terminating the process immediately prevents the application from launching in an unstable, partially configured state.
</details>

<details>
<summary>4. What is the role of `zodResolver` when using React Hook Form?</summary>

`zodResolver` connects a Zod schema to React Hook Form's validation lifecycle, automatically converting Zod validation errors into React Hook Form error objects mapped directly to form input fields.
</details>

<details>
<summary>5. How can you handle polymorphic payloads in an API endpoint?</summary>

Using `z.discriminatedUnion('type', [SchemaA, SchemaB])`, which selects the appropriate schema based on a common discriminant tag property.
</details>

<details>
<summary>6. Why should you re-assign `req.body = parsedData` in Express validation middleware?</summary>

Because Zod schemas often perform data transformations (e.g. trimming strings, coercing numbers, parsing dates). Reassigning the parsed data ensures downstream controllers receive the sanitized, transformed values rather than the raw strings.
</details>

<details>
<summary>7. How do you validate that an uploaded file is within size limits?</summary>

By validating file metadata objects:
```typescript
const FileSchema = z.object({
  size: z.number().max(5 * 1024 * 1024, 'Max 5MB'),
  mimetype: z.enum(['image/png', 'image/jpeg']),
});
```
</details>

<details>
<summary>8. How do you handle optional fields that arrive as empty strings from HTML forms?</summary>

Using `.transform(val => val === '' ? undefined : val).optional()`, converting empty strings into `undefined`.
</details>

---

## 13. Exercises & Challenges

### Easy
1. **Positive Int Coercer**: Write a query param schema validating that `limit` is a number between 1 and 100, defaulting to 10.
   <details>
   <summary>Solution</summary>

   ```typescript
   import { z } from 'zod';

   export const LimitSchema = z.coerce.number().int().min(1).max(100).default(10);
   ```
   </details>

2. **UUID Validator**: Write a schema that validates a string as a valid UUID v4.
   <details>
   <summary>Solution</summary>

   ```typescript
   import { z } from 'zod';

   export const UuidSchema = z.string().uuid();
   ```
   </details>

### Medium
3. **Branded Email Parser**: Create a branded type `EmailAddress` and a Zod schema that transforms valid email strings into `EmailAddress`.
   <details>
   <summary>Solution</summary>

   ```typescript
   import { z } from 'zod';

   declare const EmailBrand: unique symbol;
   export type EmailAddress = string & { readonly [EmailBrand]: true };

   export const EmailAddressSchema = z
     .string()
     .email()
     .transform(e => e.toLowerCase() as EmailAddress);
   ```
   </details>

4. **Multi-Field Form Schema**: Write a Zod schema for a subscription form with `plan: 'FREE' | 'PRO'` where if `plan` is `'PRO'`, `creditCardToken` is required.
   <details>
   <summary>Solution</summary>

   ```typescript
   import { z } from 'zod';

   export const SubscriptionSchema = z.object({
     plan: z.enum(['FREE', 'PRO']),
     creditCardToken: z.string().optional(),
   }).refine(data => {
     if (data.plan === 'PRO') {
       return typeof data.creditCardToken === 'string' && data.creditCardToken.length > 0;
     }
     return true;
   }, {
     message: 'Credit card token is required for PRO plan',
     path: ['creditCardToken'],
   });
   ```
   </details>

### Hard
5. **Type-Safe Fastify Route Validation Hook**: Write a Fastify preHandler hook function that accepts a Zod schema for body and query, parses the request, attaches the typed data to `request.parsedBody`, and returns HTTP 400 on error.
   <details>
   <summary>Solution</summary>

   ```typescript
   import { FastifyRequest, FastifyReply } from 'fastify';
   import { z, ZodSchema } from 'zod';

   export function createFastifyValidator<B, Q>(options: {
     bodySchema?: ZodSchema<B>;
     querySchema?: ZodSchema<Q>;
   }) {
     return async (req: FastifyRequest, reply: FastifyReply) => {
       if (options.bodySchema) {
         const res = options.bodySchema.safeParse(req.body);
         if (!res.success) {
           return reply.status(400).send({ error: 'Invalid Body', issues: res.error.issues });
         }
         (req as any).parsedBody = res.data;
       }

       if (options.querySchema) {
         const res = options.querySchema.safeParse(req.query);
         if (!res.success) {
           return reply.status(400).send({ error: 'Invalid Query', issues: res.error.issues });
         }
         (req as any).parsedQuery = res.data;
       }
     };
   }
   ```
   </details>

6. **Secure Startup Bootstrapper with Secret Masking**: Implement an environment validator class that validates required production variables, logs the loaded configuration with secret variables (like passwords, keys) masked (`***`), and exits the Node process immediately if validation fails.
   <details>
   <summary>Solution</summary>

   ```typescript
   import { z } from 'zod';

   export const EnvSchema = z.object({
     NODE_ENV: z.enum(['development', 'production']).default('development'),
     DATABASE_URL: z.string().url(),
     API_KEY: z.string().min(16),
   });

   export function bootstrapEnvironment(): z.infer<typeof EnvSchema> {
     const result = EnvSchema.safeParse(process.env);
     if (!result.success) {
       console.error('FATAL: Invalid Environment Configuration:\n', result.error.flatten().fieldErrors);
       process.exit(1);
     }

     const masked = {
       ...result.data,
       DATABASE_URL: result.data.DATABASE_URL.replace(/:[^:@]+@/, ':****@'),
       API_KEY: '****************',
     };
     console.log('Environment initialized safely:', masked);
     return result.data;
   }
   ```
   </details>
