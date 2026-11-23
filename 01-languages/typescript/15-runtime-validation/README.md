# Module 15: Runtime Validation and Schema Modeling in TypeScript

Welcome to **Module 15: Runtime Validation and Schema Modeling in TypeScript**. This module provides a complete reference for bridging static compile-time types with dynamic runtime I/O, establishing trust boundaries, schema-first design, Zod, and modern high-speed alternatives.

---

## Module Overview

| Note | Title | Level | Core Topics |
| :--- | :--- | :--- | :--- |
| **00** | [Trust Boundaries and Runtime Validation](./00-trust-boundaries.md) | Intermediate | Why static types erase, trust boundaries, Parse Don't Validate, prototype pollution |
| **01** | [Schema-First vs Type-First Modeling](./01-schema-validation.md) | Intermediate | Eliminating type drift, single source of truth, deriving types from schemas, JSON Schema |
| **02** | [Comprehensive Zod Guide](./02-zod.md) | Intermediate | Primitives, objects, refinements, transforms, async parsing, lazy recursive schemas |
| **03** | [Alternative Validation Libraries](./03-other-validation-libraries.md) | Advanced | Valibot (modular & tiny), TypeBox (JSON Schema & JIT), ArkType (inline syntax), io-ts |
| **04** | [Production Validation Recipes](./04-validation-recipes.md) | Advanced | Environment validation, Express/Fastify middleware, React Hook Form, branded IDs |

---

## Learning Path

```
                    +------------------------------------+
                    | 00-trust-boundaries                |
                    | (Type erasure, boundary defenses)  |
                    +-----------------+------------------+
                                      |
                                      v
                    +------------------------------------+
                    | 01-schema-validation               |
                    | (Schema-first vs type-first drift) |
                    +-----------------+------------------+
                                      |
                 +--------------------+--------------------+
                 |                                         |
                 v                                         v
+------------------------------------+   +------------------------------------+
| 02-zod                             |   | 03-other-validation-libraries      |
| (Zod schemas, refinements, infer)  |   | (Valibot, TypeBox, ArkType, io-ts) |
+----------------+-------------------+   +-----------------+------------------+
                 |                                         |
                 +-------------------+---------------------+
                                     |
                                     v
                   +----------------------------------+
                   | 04-validation-recipes            |
                   | (Env vars, middleware, branded)  |
                   +----------------------------------+
```

---

## Key Best Practices
1. **Treat All Boundary I/O as `unknown`**: Never cast incoming JSON directly using `as MyType`.
2. **Derive Types from Schemas**: Use `z.infer<typeof Schema>` to keep validation and types in lockstep.
3. **Fail Fast on Environment Variables**: Validate `process.env` at startup and terminate on missing keys.
4. **Choose the Right Library for the Task**: Use Valibot for small frontend bundles; use TypeBox for high-frequency APIs; use Zod for rich fullstack ecosystems.
5. **Use Branded Types for IDs**: Prevent accidental ID swapping by combining runtime validation with nominal brands.
