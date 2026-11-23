# Module 27: Node.js & Backend TypeScript Interview Mastery

> **Topic Overview**: Critical backend TypeScript interview topics: Express/Fastify request augmentation, database typing with Drizzle/Prisma, async error propagation, environment variable safety, and binary Streams.

---

## Metadata Table

| Field | Value |
|---|---|
| **Category** | Interview & Preparation |
| **Target Level** | Senior to Lead Backend Engineer |
| **TypeScript Baseline** | TypeScript 5.0 - 5.8+ & Node.js 20/22 LTS |
| **Focus Areas** | Express/Fastify Typings, Declaration Merging, Environment Schemas |

---

## Key Interview Questions

### Q1: How does Declaration Merging work to augment `Express.Request` with authenticated user context?
<details>
<summary><strong>Answer & Technical Analysis</strong></summary>

Express attaches arbitrary properties to the `req` object. In standard TypeScript, accessing `req.user` yields `Property 'user' does not exist on type 'Request'`.

**Declaration Merging Solution**:
Create an ambient declaration file (`express.d.ts`):
```typescript
// types/express.d.ts
import { JwtPayload } from "./auth.js";

declare global {
  namespace Express {
    interface Request {
      user?: JwtPayload;
      requestId: string;
    }
  }
}
export {};
```
**Mechanism**:
TypeScript merges interface declarations sharing the same namespace and identifier. The `declare global` block ensures the global `Express` namespace is targeted. Adding `export {}` marks the file as a module so TypeScript parses the file without global pollution errors.
</details>

---

### Q2: How do you guarantee type safety for environment variables (`process.env`) at application boot?
<details>
<summary><strong>Answer & Technical Analysis</strong></summary>

By default, `process.env` typed in `@types/node` is `Dict<string>`, meaning every property is `string | undefined`. Relying on non-null assertions (`process.env.DATABASE_URL!`) leads to runtime crashes in production.

**Enterprise Pattern: Zod Schema Parse at Boot**:
```typescript
import { z } from "zod";

const envSchema = z.object({
  NODE_ENV: z.enum(["development", "test", "production"]).default("development"),
  PORT: z.coerce.number().default(3000),
  DATABASE_URL: z.string().url(),
  JWT_SECRET: z.string().min(32),
});

export const env = envSchema.parse(process.env);
export type Env = z.infer<typeof envSchema>;

// Global type augmentation (optional):
declare global {
  namespace NodeJS {
    interface ProcessEnv extends z.infer<typeof envSchema> {}
  }
}
```
</details>

---

### Q3: How do you type asynchronous error handling middleware without losing type safety or causing unhandled promise rejections?
<details>
<summary><strong>Answer & Technical Analysis</strong></summary>

In Express 4, route handlers returning rejected promises do not automatically invoke `next(err)`, requiring manual forwarding or wrapper utilities.

```typescript
import { Request, Response, NextFunction, RequestHandler } from "express";

type AsyncRequestHandler<P = any, ResBody = any, ReqBody = any, ReqQuery = any> = (
  req: Request<P, ResBody, ReqBody, ReqQuery>,
  res: Response<ResBody>,
  next: NextFunction
) => Promise<any>;

export const asyncHandler = (fn: AsyncRequestHandler): RequestHandler => {
  return (req, res, next) => {
    Promise.resolve(fn(req, res, next)).catch(next);
  };
};

// Usage:
app.get("/users/:id", asyncHandler(async (req, res) => {
  const user = await db.findUser(req.params.id);
  res.json(user);
}));
```
*(Note: Express 5.x handles rejected promises natively, but typed request params still require explicit generic parameters).*
</details>\n