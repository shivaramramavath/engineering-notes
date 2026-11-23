# Schema-First vs Type-First Validation in TypeScript

> Architectural guide to schema-first vs type-first modeling: deriving TypeScript types from runtime schemas, JSON Schema standards, bidirectional code generation, and single source of truth.

| Metadata | Details |
| :--- | :--- |
| **Category** | Runtime Validation |
| **Difficulty** | Intermediate |
| **Prerequisites** | [01-fundamentals/04-type-aliases.md](../01-fundamentals/04-type-aliases.md), [15-runtime-validation/00-trust-boundaries.md](./00-trust-boundaries.md) |
| **TypeScript Baseline** | TS 4.0+, TS 5.0+ |
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
When designing data contracts in TypeScript applications, teams face a fundamental architectural choice:
1. **Type-First (TypeScript-First)**: Define TypeScript interfaces (`interface User { id: string }`) and manually write validation functions or generate schemas using reflection/decorators.
2. **Schema-First (Runtime-First)**: Define runtime schemas using a validation library (Zod, TypeBox, Valibot) and infer the static TypeScript types automatically using utility types (`type User = z.infer<typeof UserSchema>`).

Manual synchronization between hand-written interfaces and separate validation functions inevitably drifts over time: an engineer adds a field to the interface but forgets to update the validator, leading to silent bugs. Schema-first guarantees that your static types and runtime validation rules remain **100% in lockstep**.

---

## 2. Core Concepts
- **Type Drift**: The divergence between static compile-time type definitions and actual runtime validation checks.
- **Single Source of Truth**: Defining the data contract exactly once so that compile-time types and runtime validators are generated from the same declaration.
- **Type Inference from Schemas**:
  ```typescript
  type InferredType = z.infer<typeof MySchema>;
  ```
- **JSON Schema Standard**: An IETF standard specification for describing JSON data shapes. Enables interoperability between TypeScript, Python, Go, and OpenAPI specifications.
- **Contract-Driven Development**: Generating schemas from an OpenAPI spec or database schema, or generating OpenAPI specs directly from runtime schemas.

---

## 3. How It Works Internally

```
                   Schema-First Architecture
                               |
                               v
            Define Runtime Schema (e.g. Zod / TypeBox)
            const UserSchema = z.object({ id: z.string() });
                               |
            +------------------+------------------+
            |                                     |
            v                                     v
   Runtime Engine                        Static Type System
   UserSchema.parse(input)               type User = z.infer<typeof UserSchema>
   (Validates data in V8)                (Understands User as { id: string })
```

The schema library uses advanced conditional and mapped types to recursively inspect the shape of the schema object. For example, `z.ZodString` resolves to `string`, and `z.ZodObject<{ id: z.ZodString }>` maps to `{ id: string }`.

---

## 4. Syntax & API Reference

```typescript
import { z } from 'zod';

// 1. Defining Schema (Single Source of Truth)
export const ProductSchema = z.object({
  id: z.string().uuid(),
  name: z.string().min(1),
  priceCents: z.number().int().positive(),
  tags: z.array(z.string()).default([]),
});

// 2. Inferring TypeScript Type Automatically
export type Product = z.infer<typeof ProductSchema>;

// 3. Parsing at Boundary
export function parseProduct(input: unknown): Product {
  return ProductSchema.parse(input); // Throws ZodError if invalid
}
```

---

## 5. Practical Examples

### Example 1: Demonstrating Type Drift (Anti-Pattern)
```typescript
// INSECURE TYPE-FIRST APPROACH:
interface Order {
  id: string;
  total: number;
  status: 'PENDING' | 'PAID'; // Added in v2!
}

function validateOrder(data: any): boolean {
  // Developer forgot to update validator for 'status'!
  return typeof data.id === 'string' && typeof data.total === 'number';
}

const badData = { id: 'ord_1', total: 100 }; // Missing 'status'!
if (validateOrder(badData)) {
  const order = badData as Order;
  // order.status is undefined, but TypeScript thinks it is 'PENDING' | 'PAID'!
}
```

### Example 2: Schema-First with TypeBox (JSON Schema Native)
```typescript
import { Type, Static } from '@sinclair/typebox';

// Defines a valid JSON Schema object:
export const UserSchema = Type.Object({
  id: Type.String({ format: 'uuid' }),
  email: Type.String({ format: 'email' }),
  active: Type.Boolean({ default: true }),
});

// Infer static TypeScript type:
export type User = Static<typeof UserSchema>;
```

### Example 3: Edge Case - Input vs Output Types in Transforms
```typescript
import { z } from 'zod';

// Schema that parses string input into a Date object:
export const DateSchema = z.string().transform(str => new Date(str));

// Input type: string
export type DateInput = z.input<typeof DateSchema>;

// Output type: Date
export type DateOutput = z.output<typeof DateSchema>;
```

### Example 4: Compiler Failure Case - Type Mismatch with Inferred Type
```typescript
import { z } from 'zod';

const NumberListSchema = z.array(z.number());
type NumberList = z.infer<typeof NumberListSchema>;

// @ts-expect-error TS2322: Type 'string' is not assignable to type 'number'.
const invalidList: NumberList = [1, 2, 'three'];
```

### Example 5: Idiomatic Reusable Validation Middleware
```typescript
import { z } from 'zod';

export function validateBody<T extends z.ZodTypeAny>(schema: T) {
  return (req: { body: unknown }, res: { status: (code: number) => { json: (data: unknown) => void } }, next: () => void) => {
    const result = schema.safeParse(req.body);
    if (!result.success) {
      return res.status(400).json({
        errors: result.error.flatten().fieldErrors,
      });
    }
    req.body = result.data;
    next();
  };
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas
1. **`z.infer` vs `z.input` vs `z.output`**: When using `.transform()`, the input type (e.g. `string`) differs from the output type (e.g. `number`). `z.infer` aliases `z.output`. If validating function parameters, use `z.input`.
2. **Circular / Recursive Schemas**: Recursive types (like JSON trees or nested comments) cannot be inferred directly with simple `z.infer`. You must use `z.lazy()` and explicitly annotate the base interface.
3. **Bundle Size Considerations**: Large schema libraries (like full Zod) add ~12KB gzipped to client bundles. For frontend forms, modular alternatives like `valibot` (~1KB) are often preferred.
4. **Stripping Unknown Keys**: By default, Zod objects strip unknown keys (`.strip()`), while TypeBox keeps them. Be conscious of whether your boundary needs strict rejection (`.strict()`) or stripping.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Consequence | Correction |
| :--- | :--- | :--- |
| Hand-writing both `interface X` and `validateX()` | Inevitable type drift and silent bugs | Use schema-first and derive type via `z.infer` |
| Using `z.infer` on schemas with `.transform()` | Confuses input types with output types | Use `z.input<T>` for inputs, `z.output<T>` for parsed results |
| Over-using `.passthrough()` on schemas | Permits malicious un-sanitized keys | Use `.strict()` or default stripping |
| Manually writing JSON Schema alongside TS interfaces | Desynchronization between OpenAPI and code | Generate JSON schema from TypeBox or Zod |
| Omitting custom error messages | Incomprehensible error messages for API users | Provide custom message strings in schema |

---

## 8. Performance & Type-Checker Complexity
- In TypeBox, schemas evaluate to standard JSON Schema objects with zero runtime compilation penalty. Validation using `Ajv` compiles to native JIT code that executes $20\times - 50\times$ faster than object traversal libraries.
- TypeScript's type checker infers `z.infer` via mapped types. Excessively deep nested schemas ($>15$ levels) can increase TSServer memory consumption.

---

## 9. Compiler Internals
In schema libraries:
- `Static<T>` or `z.infer<T>` uses conditional distributive mapped types:
  ```typescript
  type Infer<T> = T extends ZodString ? string : T extends ZodNumber ? number : ...;
  ```
- The TypeScript compiler resolves these generic constraints lazily when the type alias is evaluated.

---

## 10. Best Practices & Design Guidelines
- Adopt **Schema-First** for all data entering trust boundaries (APIs, webhooks, forms).
- Adopt **Type-First** for internal domain algorithms and pure domain models that do not cross I/O boundaries.
- Use `z.input` and `z.output` explicitly when transforms are involved.
- Use TypeBox if high-speed JSON Schema serialization or AJV compilation is required.
- Use Valibot for client-side applications where minimal bundle size is critical.

---

## 11. Real-World Case Study
A financial services portal managed loan applications. An engineer updated `interface LoanApplication` to make `coSignerEmail` required. However, the backend validation middleware used a hand-written validation function that was not updated. Over 3 weeks, 450 loan applications submitted without co-signers were saved to the database. Downstream underwriting background workers crashed on `app.coSignerEmail.toLowerCase()`, halting all loan processing.

**Refactoring to Schema-First:**
The team replaced all manual validation functions with Zod schemas:
```typescript
export const LoanApplicationSchema = z.object({
  applicantId: z.string().uuid(),
  amount: z.number().positive(),
  coSignerEmail: z.string().email(),
});
export type LoanApplication = z.infer<typeof LoanApplicationSchema>;
```
Type drift became mathematically impossible: any change to the schema instantly updated both runtime validation and the TypeScript type definition.

---

## 12. Interview Questions

<details>
<summary>1. What is the fundamental difference between Schema-First and Type-First validation?</summary>

- **Type-First**: You declare TypeScript interfaces first and write validation logic separately, which risks type drift when either changes.
- **Schema-First**: You define a runtime schema object as the single source of truth, and TypeScript infers the static type definition automatically, guaranteeing that compile-time types and runtime validators never diverge.
</details>

<details>
<summary>2. How does `z.infer<typeof Schema>` derive a TypeScript type from a runtime value?</summary>

Through TypeScript's `typeof` operator (which captures the type of the runtime schema value) and conditional generic mapped types inside the library that inspect the schema tokens and map them to standard TypeScript primitives and objects.
</details>

<details>
<summary>3. What is the difference between `z.input` and `z.output`?</summary>

When a schema contains transformations (e.g. `z.string().transform(s => Number(s))`), the data accepted by `.parse()` is the **input type** (`string`), while the data returned after transformation is the **output type** (`number`). `z.infer` aliases `z.output`.
</details>

<details>
<summary>4. Why is JSON Schema considered an industry standard?</summary>

Because JSON Schema is a language-agnostic IETF standard. A single JSON Schema definition can be used to validate data in Python, Go, Java, and TypeScript, and forms the basis for OpenAPI (Swagger) specifications.
</details>

<details>
<summary>5. What are the advantages of TypeBox over Zod?</summary>

TypeBox generates standard JSON Schema objects directly, has zero dependencies, and can be compiled with `Ajv` to achieve near-native C++ validation speeds, whereas Zod uses a custom object-tree evaluation model.
</details>

<details>
<summary>6. How can you model recursive types (like trees) in schema-first libraries?</summary>

By using lazy evaluation primitives (e.g. `z.lazy(() => NodeSchema)`) and providing an explicit TypeScript interface annotation to break circular type inference.
</details>

<details>
<summary>7. What happens to extra un-modeled properties by default in Zod?</summary>

By default, Zod strips unknown properties (`.strip()`), returning an object containing only the declared keys. Call `.strict()` to reject payloads with extra keys, or `.passthrough()` to retain them.
</details>

<details>
<summary>8. When is a Type-First approach still appropriate?</summary>

For internal pure business domain logic, in-memory data structures, utility helpers, and mathematical algorithms that never serialize across external network or I/O boundaries.
</details>

---

## 13. Exercises & Challenges

### Easy
1. **Derive User Type**: Define a Zod schema for a user with `id` (string) and `score` (number), and export the inferred type.
   <details>
   <summary>Solution</summary>

   ```typescript
   import { z } from 'zod';

   export const UserSchema = z.object({
     id: z.string(),
     score: z.number(),
   });

   export type User = z.infer<typeof UserSchema>;
   ```
   </details>

2. **Safe Parser Helper**: Write a function `parseSafe<T>(schema: z.ZodType<T>, data: unknown): T | null`.
   <details>
   <summary>Solution</summary>

   ```typescript
   import { z } from 'zod';

   export function parseSafe<T>(schema: z.ZodType<T>, data: unknown): T | null {
     const res = schema.safeParse(data);
     return res.success ? res.data : null;
   }
   ```
   </details>

### Medium
3. **Transforming Schema**: Create a Zod schema that accepts a comma-separated string (`"tag1,tag2"`) and transforms it into an array of trimmed strings (`["tag1", "tag2"]`), exporting both input and output types.
   <details>
   <summary>Solution</summary>

   ```typescript
   import { z } from 'zod';

   export const TagsSchema = z.string().transform(str =>
     str.split(',').map(s => s.trim()).filter(Boolean)
   );

   export type TagsInput = z.input<typeof TagsSchema>;   // string
   export type TagsOutput = z.output<typeof TagsSchema>; // string[]
   ```
   </details>

4. **Discriminated Union Schema**: Define a schema for events with a `type: 'CLICK' | 'HOVER'` discriminant and appropriate payload per event.
   <details>
   <summary>Solution</summary>

   ```typescript
   import { z } from 'zod';

   export const EventSchema = z.discriminatedUnion('type', [
     z.object({ type: z.literal('CLICK'), x: z.number(), y: z.number() }),
     z.object({ type: z.literal('HOVER'), elementId: z.string() }),
   ]);

   export type AppEvent = z.infer<typeof EventSchema>;
   ```
   </details>

### Hard
5. **Recursive Tree Schema**: Build a fully typed recursive schema for a file-system directory tree (`DirectoryNode` has `name: string` and optional `children: DirectoryNode[]`), correctly handling typing without infinite recursion errors.
   <details>
   <summary>Solution</summary>

   ```typescript
   import { z } from 'zod';

   export interface DirectoryNode {
     name: string;
     children?: DirectoryNode[];
   }

   export const DirectoryNodeSchema: z.ZodType<DirectoryNode> = z.lazy(() =>
     z.object({
       name: z.string(),
       children: z.array(DirectoryNodeSchema).optional(),
     })
   );
   ```
   </details>

6. **Automated Schema-to-TypeScript Generator CLI**: Write a Node.js utility that takes a raw JSON Schema file from disk and programmatically outputs a formatted TypeScript declaration file containing matching interface declarations.
   <details>
   <summary>Solution</summary>

   ```typescript
   import * as fs from 'fs';

   export function jsonSchemaToTypeScript(jsonSchemaPath: string, outputTsPath: string): void {
     const schema = JSON.parse(fs.readFileSync(jsonSchemaPath, 'utf8'));
     const typeName = schema.title || 'GeneratedType';
     const props = schema.properties || {};

     let tsCode = `export interface ${typeName} {\n`;
     for (const [key, value] of Object.entries(props)) {
       const propType = (value as any).type === 'integer' ? 'number' : (value as any).type || 'unknown';
       const isRequired = schema.required?.includes(key);
       tsCode += `  ${key}${isRequired ? '' : '?'}: ${propType};\n`;
     }
     tsCode += '}\n';

     fs.writeFileSync(outputTsPath, tsCode, 'utf8');
     console.log(`Generated ${outputTsPath}`);
   }
   ```
   </details>
