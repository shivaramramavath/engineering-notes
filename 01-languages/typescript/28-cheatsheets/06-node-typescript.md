# Module 28: Node.js & Backend TypeScript Cheatsheet

> **Quick Reference**: Key typing patterns for Node.js backends: Express/Fastify request augmentation, Buffer, Streams, and Zod environment parsing.

---

## 1. Global Augmentation for Express

```typescript
// types/express.d.ts
import { JwtPayload } from "../src/auth/jwt.js";

declare global {
  namespace Express {
    interface Request {
      user?: JwtPayload;
      requestId: string;
      startTime: number;
    }
  }
}
export {};
```

---

## 2. Environment Variable Validation with Zod

```typescript
import { z } from "zod";

const envSchema = z.object({
  NODE_ENV: z.enum(["development", "test", "production"]).default("development"),
  PORT: z.coerce.number().default(8080),
  DATABASE_URL: z.string().url(),
  API_KEY: z.string().min(16),
});

export const env = envSchema.parse(process.env);
export type EnvConfig = z.infer<typeof envSchema>;
```

---

## 3. Typing Streams & Buffers

```typescript
import { Readable, Writable } from "node:stream";
import { Buffer } from "node:buffer";

async function bufferStream(stream: Readable): Promise<Buffer> {
  const chunks: Buffer[] = [];
  for await (const chunk of stream) {
    chunks.push(Buffer.isBuffer(chunk) ? chunk : Buffer.from(chunk));
  }
  return Buffer.concat(chunks);
}
```\n