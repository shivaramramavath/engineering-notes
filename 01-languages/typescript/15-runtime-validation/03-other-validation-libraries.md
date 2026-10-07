# Other Validation Libraries

Zod is common, but it is not the only choice, and for some projects it is not the best one. This note surveys the main alternatives, what each is good at, and how to choose. Library APIs and package names change often, so treat the examples as orientation and check each project's documentation for current usage.

**Prerequisites:**
- [Schema validation](./01-schema-validation.md)
- [Zod](./02-zod.md)

---

## What differs between libraries

All of them turn a definition into a runtime check. They differ along a few axes:

| Axis | What to look at |
|---|---|
| **Source of truth** | TypeScript-first schema objects, JSON Schema, or decorated classes |
| **Type inference quality** | how accurate and how fast the inferred types are |
| **Bundle size and tree-shaking** | matters for browser code |
| **Runtime speed** | matters for hot paths and large payloads |
| **Ecosystem** | form libraries, API frameworks, ORMs, OpenAPI tools that integrate with it |
| **Interoperability** | JSON Schema export/import, Standard Schema support |
| **API style** | method chaining, function composition, string-based syntax, decorators |
| **Maturity and maintenance** | release cadence, documentation, community |

There is no universal winner. The right choice depends on where validation runs (browser, server, both), how much data, and what your framework expects.

## Standard Schema

Several libraries (Zod, Valibot, ArkType, and others) implement a small shared interface called **Standard Schema**. It lets tools accept "any compliant schema" instead of being tied to one library. A framework or form library that supports it works with whichever schema library you choose, and switching libraries later becomes cheaper. Check that the tools you depend on support it.

## Valibot

A modular, function-based library designed for small bundles: each function is imported individually, so unused ones can be removed by the bundler.

```ts
import * as v from "valibot";

const UserSchema = v.object({
  id: v.number(),
  email: v.pipe(v.string(), v.email()),
});

type User = v.InferOutput<typeof UserSchema>;

const result = v.safeParse(UserSchema, data);
if (result.success) {
  result.output;      // User
} else {
  result.issues;      // list of issues
}
```

- **Strengths:** small bundle size for browser code, composable `pipe` API, familiar concepts if you know Zod.
- **Trade-offs:** a different API style (functions plus `pipe` instead of chaining), smaller ecosystem than Zod.
- **Good for:** front-end heavy projects where bundle size matters.

## ArkType

Defines types using a string syntax that looks like TypeScript, and validates with high performance.

```ts
import { type } from "arktype";

const User = type({
  id: "number",
  email: "string.email",
  "age?": "number",
});

type User = typeof User.infer;

const out = User(data);
if (out instanceof type.errors) {
  console.log(out.summary);
} else {
  out;                // User
}
```

- **Strengths:** concise syntax close to real TypeScript types, strong runtime performance, rich error messages.
- **Trade-offs:** string-based definitions rely on the library's type-level parsing, and the ecosystem is smaller.
- **Good for:** performance-sensitive validation and people who like TypeScript-like definitions.

## TypeBox and Ajv (JSON Schema)

**TypeBox** builds **JSON Schema** objects with a TypeScript-friendly builder and infers static types from them. **Ajv** is a widely used JSON Schema validator that compiles schemas to fast functions.

```ts
import { Type, type Static } from "@sinclair/typebox";

const User = Type.Object({
  id: Type.Number(),
  email: Type.String({ format: "email" }),
});

type User = Static<typeof User>;
```

Validate with a library that understands JSON Schema (TypeBox's own value checker, or Ajv):

```ts
import Ajv from "ajv";

const ajv = new Ajv();
const validate = ajv.compile(User);        // compile once, reuse

if (validate(data)) {
  data;                                    // narrowed to User
} else {
  validate.errors;                         // issue list
}
```

(Package names and import paths for TypeBox have changed across versions. Check its current documentation.)

- **Strengths:** the schema **is** JSON Schema, so it plugs directly into OpenAPI, Swagger tooling, other languages, and validators like Ajv, which are fast. Good fit for **contract-first** APIs ([contract-first APIs](../16-type-safe-apis/06-contract-first-apis.md)) and for frameworks such as Fastify, which validate with JSON Schema natively.
- **Trade-offs:** more verbose than chained APIs, and fewer built-in transforms. Ajv compiles code at startup, which matters in some restricted runtimes.
- **Good for:** APIs documented or generated from OpenAPI, services in several languages, high-throughput validation.

## class-validator and class-transformer

Decorator-based validation on **classes**, most associated with NestJS.

```ts
import { IsEmail, IsInt, IsOptional, Min } from "class-validator";

class CreateUserDto {
  @IsEmail()
  email!: string;

  @IsInt()
  @Min(0)
  @IsOptional()
  age?: number;
}
```

Used with NestJS's `ValidationPipe`, incoming JSON is converted to a class instance (with `class-transformer`) and validated. See [NestJS](../20-nodejs-backend/08-nestjs.md) and [decorators](../05-classes/07-decorators.md).

- **Strengths:** natural fit for class-based frameworks and DI; validation rules live next to the DTO.
- **Trade-offs:** needs decorators and metadata (`experimentalDecorators`, `emitDecoratorMetadata`), the type and the validation rules are two parallel pieces that can disagree (decorators are not derived from the type), and it works only with classes. Nested types and unions require extra decorators.
- **Good for:** existing NestJS projects that follow its conventions.

## io-ts, Effect Schema, Superstruct, runtypes

- **io-ts** (functional style, built around `fp-ts`) was influential and is still used, but newer libraries have largely taken its place for new projects.
- **Effect Schema** is part of the Effect ecosystem. It supports encoding and decoding in both directions, rich transformations, and integrates tightly with Effect. A natural choice if you already use Effect.
- **Superstruct** and **runtypes** are smaller, simpler libraries with a compact API.

## Yup and Joi

- **Yup** is popular in form handling (historically with Formik). Its TypeScript inference is weaker than newer TypeScript-first libraries, particularly around optional and nullable fields.
- **Joi** is a mature, feature-rich validator from the JavaScript world. It has no first-class type inference, so you maintain the TypeScript types separately.

Both are reasonable if already in use. For new TypeScript projects, a TypeScript-first library usually gives a better experience.

## Choosing

| Situation | Reasonable choice |
|---|---|
| General app code, full-stack TypeScript, forms | **Zod** (largest ecosystem) |
| Browser bundle size is critical | **Valibot** |
| Performance is critical, and you like TypeScript-like syntax | **ArkType** |
| Contract-first / OpenAPI / polyglot services / Fastify | **TypeBox + Ajv** or JSON Schema directly |
| NestJS with DTO classes | **class-validator** (or a Zod-based pipe if you prefer schemas) |
| Already using Effect | **Effect Schema** |
| Existing codebase uses Yup, Joi, or io-ts | stay unless there is a clear reason to migrate |

When in doubt, start with Zod: examples, integrations, and answers are easy to find. If you hit a concrete problem (bundle size, speed, JSON Schema), switch where it hurts, ideally behind a small wrapper.

## Keeping the choice reversible

- **Isolate validation behind your own helpers.** For example, a `parseBody(schema, data)` function that returns your own `Result` and error shape. Application code then does not depend on library-specific error objects.
- **Prefer Standard Schema-compatible tools** where possible.
- **Share schemas in one package,** so a migration is a contained change.
- **Avoid exotic library-specific features** in widely shared schemas unless you need them.
- **Test your schemas.** A good set of valid and invalid examples makes migration much safer ([validation recipes](./04-validation-recipes.md)).

## Common mistakes

- Choosing a library for the demo rather than for the constraint (bundle, speed, ecosystem) that matters.
- Mixing several validation libraries in one codebase without a reason.
- Using decorator-based validation outside a framework designed around it, then fighting the metadata setup.
- Expecting JSON Schema tools to infer types with the same ergonomics as TypeScript-first libraries.
- Assuming a library is fast or small without measuring in your own setup.
- Letting a library's error objects leak into your public API.
- Copying package names and import paths from old tutorials. These have changed for several libraries.

## Debugging

- If inferred types are slow or huge in the editor, test a smaller schema, or compare with another library on the same shape.
- If bundle size is an issue, inspect the bundle with an analyzer instead of guessing which library is responsible.
- If a form or framework integration fails, check that it supports your library and version (resolver packages are version-specific).
- If behavior differs between libraries on the same input, check defaults for unknown keys, coercion, and optional/nullable handling.

## Quick summary

- Libraries differ in source of truth (TypeScript schemas, JSON Schema, classes), inference quality, size, speed, and ecosystem.
- **Zod** is the default for breadth of ecosystem. **Valibot** targets small bundles. **ArkType** targets speed and TypeScript-like syntax. **TypeBox/Ajv** targets JSON Schema and contract-first APIs. **class-validator** fits NestJS.
- Standard Schema support makes tools and libraries more interchangeable.
- Wrap validation behind your own helpers and error shape so the choice stays reversible.
- Always verify current package names and APIs in the official documentation.

**Next:** [Validation recipes](./04-validation-recipes.md)