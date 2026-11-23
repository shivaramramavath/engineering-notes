# Module 28: Runtime Validation & Zod Cheatsheet

> **Quick Reference**: Fast reference for Zod schema primitives, transformations, refinements, and TypeScript type inference.

---

## 1. Core Zod Schemas

```typescript
import { z } from "zod";

// Primitives
const stringSchema = z.string().min(3).max(50);
const numberSchema = z.number().int().positive();
const boolSchema = z.boolean();
const dateSchema = z.coerce.date(); // Coerces ISO strings to Date

// Objects & Arrays
const userSchema = z.object({
  id: z.string().uuid(),
  email: z.string().email(),
  roles: z.array(z.enum(["admin", "user"])).default(["user"]),
  age: z.number().optional(),
});

// Infer static TypeScript type:
export type User = z.infer<typeof userSchema>;
```

---

## 2. Refinements & Transformations

```typescript
// Custom Refinement
const passwordSchema = z.string()
  .min(8)
  .refine(val => /[A-Z]/.test(val), { message: "Must contain uppercase letter" })
  .refine(val => /[0-9]/.test(val), { message: "Must contain a number" });

// Transformation (parse & convert)
const trimmedUpper = z.string().trim().transform(s => s.toUpperCase());

// Safe Parse (never throws)
const result = userSchema.safeParse(jsonPayload);
if (result.success) {
  console.log("Valid user:", result.data); // result.data is typed User
} else {
  console.error("Validation issues:", result.error.flatten());
}
```\n