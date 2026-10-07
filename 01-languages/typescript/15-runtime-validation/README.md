# 15 - Runtime Validation

TypeScript's types disappear before your program runs, so they cannot check the data that actually arrives: HTTP requests, API responses, environment variables, stored JSON, webhooks. This section covers how to check that data at runtime, convert it into trusted, typed values, and keep the runtime check and the static type in sync by deriving one from the other.

If the earlier sections were about describing what your data *should* look like, this one is about **proving it**, at the points where you cannot otherwise know.

## Prerequisites

- [14 Type System Internals: type erasure and runtime](../14-type-system-internals/00-type-erasure-and-runtime.md): why types cannot check data
- [03 Unions and Narrowing](../03-unions-and-narrowing/README.md): type guards and discriminated unions
- [11 Error Handling](../11-error-handling/README.md): how validation failures are reported and handled

## Notes in this section

| # | Note | Covers |
|---|---|---|
| 00 | [Trust boundaries](./00-trust-boundaries.md) | What counts as untrusted input, "parse at the edge", `unknown`, what to validate where, security reasons |
| 01 | [Schema validation](./01-schema-validation.md) | One definition for runtime check and static type, a tiny schema library, design decisions (input vs output, unknown keys, coercion, errors) |
| 02 | [Zod](./02-zod.md) | Everyday Zod API, parsing, errors, objects, unions, refine, transform, coerce, branding, Zod 3 vs 4 notes |
| 03 | [Other validation libraries](./03-other-validation-libraries.md) | Valibot, ArkType, TypeBox/Ajv, class-validator, Standard Schema, how to choose |
| 04 | [Validation recipes](./04-validation-recipes.md) | `fetchJson`, Express middleware, env config, query strings, `localStorage`, webhooks, mass assignment, testing |

Read 00 and 01 for the ideas, then 02 for the tool most projects use. 03 is a reference for choosing. 04 is where you copy patterns from.

## Do I need validation here?

| Data source | Validate? | Why |
|---|---|---|
| HTTP request body, query, params, headers | **Yes** | client-controlled, possibly malicious |
| Response from an external API or SDK | **Yes** | other side can change or fail |
| `JSON.parse`, `res.json()`, JSON files | **Yes** | returns `any` |
| Environment variables | **Yes, at startup** | always strings, often missing |
| `localStorage` / cookies / URL params | **Yes, with fallback** | old versions, user edits |
| Webhook or queue payload | **Yes** (and verify authenticity first) | produced by other systems |
| Loosely typed DB data (raw SQL, JSON columns) | **Usually** | schema drift |
| Values passed between your own typed functions | No | already inside the boundary |
| Data you just validated | No | do not validate twice |

## Which note answers my question?

| Question | Go to |
|---|---|
| Why does `res.json() as User` not protect me? | [00](./00-trust-boundaries.md) |
| Where in my app should validation happen? | [00](./00-trust-boundaries.md) |
| How does a schema produce both a type and a check? | [01](./01-schema-validation.md) |
| What should happen to extra fields in a request body? | [01](./01-schema-validation.md) and [04](./04-validation-recipes.md) |
| How do I validate and convert query-string numbers? | [02](./02-zod.md) and [04](./04-validation-recipes.md) |
| Why did `"false"` become `true`? | [02](./02-zod.md) (`coerce.boolean`) |
| Should I use Zod, Valibot, ArkType, or JSON Schema? | [03](./03-other-validation-libraries.md) |
| How do I type and validate an API client? | [04](./04-validation-recipes.md) |
| How do I make my app fail fast on bad configuration? | [04](./04-validation-recipes.md) |
| How do I keep users from setting fields they should not? | [04](./04-validation-recipes.md) |

## Ideas that recur across the section

- **Types describe belief, validation checks reality.** Only code you wrote is checked by the compiler, not data that arrives later.
- **Receive as `unknown`, produce a typed value.** The boundary function is where the conversion happens.
- **Validate once, at the edge.** The core of the program works with already-trusted types.
- **Derive types from schemas.** One definition keeps the check and the type from drifting apart.
- **Shape is not business correctness.** A schema proves the form of data. Domain rules (uniqueness, balances) belong in your service layer.
- **Use the parsed output.** Not the original input. Stripping, defaults, and transforms apply only to the result.
- **Allow-list, bound, and fail closed.** Validation is a security control as well as a correctness tool.
- **Keep the library replaceable.** Wrap it behind your own helper and error shape.

## Related sections

- [10 Advanced Types: branded types](../10-advanced-types/07-branded-types.md): marking validated values
- [11 Error Handling: Result pattern](../11-error-handling/02-result-pattern.md): returning validation failures as values
- [16 Type-Safe APIs](../16-type-safe-apis/README.md): request/response types, DTOs, typed clients, contract-first design
- [19 React and Frontend: forms](../19-react-and-frontend/05-forms.md): sharing schemas with form libraries
- [20 Node.js Backend: config and environment](../20-nodejs-backend/01-config-and-environment.md)
- [23 Security: input validation](../23-security/01-input-validation.md)
- [27 Interview: type system and runtime](../27-interview/01-type-system-and-runtime.md)

## Next

[16 Type-Safe APIs](../16-type-safe-apis/README.md)