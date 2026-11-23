# Module 01: TypeScript Fundamentals

> The foundational type system of TypeScript: primitive types, type inference, structural typing, objects, aliases, enums, top/bottom types, nullability, immutability, and control flow analysis.

---

## Module Overview

This module covers the core building blocks of TypeScript. Mastering these fundamentals is essential for writing type-safe applications and understanding advanced type-level programming.

| Note | Title | Level | Baseline | Description |
| :--- | :--- | :--- | :--- | :--- |
| **00** | [Types and Inference](file:///E:/Learning/engineering-notes/01-languages/typescript/01-fundamentals/00-types-and-inference.md) | Beginner | TS 1.0+ | Type annotations, contextual typing, widening, and structural subtyping. |
| **01** | [Primitive Types](file:///E:/Learning/engineering-notes/01-languages/typescript/01-fundamentals/01-primitive-types.md) | Beginner | TS 1.0+ | string, number, boolean, symbol, bigint, null, undefined, void. |
| **02** | [Arrays and Tuples](file:///E:/Learning/engineering-notes/01-languages/typescript/01-fundamentals/02-arrays-and-tuples.md) | Beginner | TS 1.0+ / TS 3.4 | Typed arrays, fixed-length tuples, rest elements, labeled tuples. |
| **03** | [Objects](file:///E:/Learning/engineering-notes/01-languages/typescript/01-fundamentals/03-objects.md) | Beginner | TS 1.0+ | Object shapes, excess property checks, optional & readonly properties. |
| **04** | [Type Aliases](file:///E:/Learning/engineering-notes/01-languages/typescript/01-fundamentals/04-type-aliases.md) | Intermediate | TS 1.4+ / TS 3.7 | Modeling unions, tuples, recursive types, and nominal branding. |
| **05** | [Enums and Const Objects](file:///E:/Learning/engineering-notes/01-languages/typescript/01-fundamentals/05-enums-and-const-objects.md) | Intermediate | TS 5.0 / TS 5.8 | Traditional enums vs `as const` dictionaries, erasable syntax boundaries. |
| **06** | [Any and Unknown](file:///E:/Learning/engineering-notes/01-languages/typescript/01-fundamentals/06-any-and-unknown.md) | Intermediate | TS 3.0+ | Safe top type (`unknown`) vs unchecked escape hatch (`any`). |
| **07** | [Never and Void](file:///E:/Learning/engineering-notes/01-languages/typescript/01-fundamentals/07-never-and-void.md) | Intermediate | TS 2.0+ | Bottom type (`never`), exhaustiveness checks, callback substitutability (`void`). |
| **08** | [Null and Undefined](file:///E:/Learning/engineering-notes/01-languages/typescript/01-fundamentals/08-null-and-undefined.md) | Beginner | TS 2.0+ / TS 4.4 | `strictNullChecks`, `exactOptionalPropertyTypes`, nullish coalescing. |
| **09** | [Object vs {} vs Object Wrapper](file:///E:/Learning/engineering-notes/01-languages/typescript/01-fundamentals/09-object-vs-empty-curly-vs-object-wrapper.md) | Intermediate | TS 2.2 / TS 4.8 | Demystifying non-primitives (`object`), non-nullish (`{}`), and prototype wrappers. |
| **10** | [BigInt, Symbol, and Unique Symbol](file:///E:/Learning/engineering-notes/01-languages/typescript/01-fundamentals/10-bigint-symbol-and-unique-symbol.md) | Intermediate | TS 2.7 / TS 5.2 | Arbitrary precision integers, nominal branding, and `Symbol.dispose`. |
| **11** | [Readonly Arrays and Tuples](file:///E:/Learning/engineering-notes/01-languages/typescript/01-fundamentals/11-readonly-arrays-and-tuples.md) | Intermediate | TS 3.4+ | Compile-time immutability, `as const`, and defensive array handling. |
| **12** | [Truthiness and Equality Narrowing](file:///E:/Learning/engineering-notes/01-languages/typescript/01-fundamentals/12-truthiness-and-equality-narrowing.md) | Beginner | TS 2.0+ | Control Flow Analysis, truthiness traps, loose vs strict equality. |
| **13** | [Typeof, Instanceof, and In Quick Reference](file:///E:/Learning/engineering-notes/01-languages/typescript/01-fundamentals/13-typeof-instanceof-in-quick-reference.md) | Beginner | TS 2.0 / TS 4.9 | Runtime type guards, prototype narrowing, and unconstrained `in` operator. |

---

## Architectural Concept Map

```
+-------------------------------------------------------------------------+
|                  TypeScript Fundamentals Architecture                   |
|                                                                         |
|                          [ TOP TYPE: unknown ]                          |
|                                    │                                    |
|              ┌─────────────────────┼─────────────────────┐              |
|              ▼                     ▼                     ▼              |
|        [ Primitives ]       [ Reference ]        [ Value Dictionaries ] |
|        - string, number     - object             - const Obj as const   |
|        - boolean, bigint    - Array<T>, tuples   - Record<string, T>    |
|        - symbol             - Function                                  |
|              │                     │                     │              |
|              └─────────────────────┼─────────────────────┘              |
|                                    │ (Control Flow Analysis)            |
|                                    ▼                                    |
|                         [ NARROWED CONCRETE TYPE ]                      |
|                                    │                                    |
|                                    ▼ (Unreachable / Exhausted)          |
|                          [ BOTTOM TYPE: never ]                         |
+-------------------------------------------------------------------------+
```

---

## Recommended Learning Sequence

1. **Foundations**: Start with `00-types-and-inference.md` and `01-primitive-types.md` to understand how TypeScript infers types and checks structural compatibility.
2. **Data Modeling**: Proceed to `02-arrays-and-tuples.md`, `03-objects.md`, and `04-type-aliases.md` to model domain entities.
3. **Soundness & Modern Constraints**: Read `05-enums-and-const-objects.md` (critical for Node 22+ type stripping and `--erasableSyntaxOnly`), `06-any-and-unknown.md`, `07-never-and-void.md`, and `08-null-and-undefined.md`.
4. **Deep Type Nuances**: Master `09-object-vs-empty-curly-vs-object-wrapper.md`, `10-bigint-symbol-and-unique-symbol.md`, and `11-readonly-arrays-and-tuples.md`.
5. **Control Flow Analysis**: Conclude with `12-truthiness-and-equality-narrowing.md` and `13-typeof-instanceof-in-quick-reference.md` to master runtime type discrimination.
