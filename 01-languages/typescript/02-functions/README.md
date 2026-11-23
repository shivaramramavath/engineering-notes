# Module 02: Functions

> Complete architectural reference for TypeScript functions: function types, parameter mechanics, return inference, callbacks, overloads, `this` parameters, generic callbacks, assertion signatures, and constructor types.

---

## Module Overview

Functions are the central building block of modern JavaScript and TypeScript architectures. This module provides in-depth coverage of static function type theory, compiler checks, and higher-order composition patterns.

| Note | Title | Level | Baseline | Description |
| :--- | :--- | :--- | :--- | :--- |
| **00** | [Function Types](file:///E:/Learning/engineering-notes/01-languages/typescript/02-functions/00-function-types.md) | Beginner | TS 0.9 / TS 2.6 | Call signatures, parameter contravariance, return covariance, callable objects. |
| **01** | [Parameters and Return Types](file:///E:/Learning/engineering-notes/01-languages/typescript/02-functions/01-parameters-and-return-types.md) | Beginner | TS 0.9 / TS 3.0 | Required, optional, default parameters, tuple rest parameters, `noImplicitReturns`. |
| **02** | [Callbacks](file:///E:/Learning/engineering-notes/01-languages/typescript/02-functions/02-callbacks.md) | Intermediate | TS 0.9 / TS 2.0 | Callback contracts, error-first conventions, `void` return substitutability. |
| **03** | [Function Overloads](file:///E:/Learning/engineering-notes/01-languages/typescript/02-functions/03-function-overloads.md) | Intermediate | TS 0.9+ | Overload signatures, hidden implementation signature, union argument resolution. |
| **04** | [This Parameters and ThisType](file:///E:/Learning/engineering-notes/01-languages/typescript/02-functions/04-this-parameters.md) | Intermediate | TS 2.0 / TS 2.8 | Fake `this` parameters, `noImplicitThis`, and `ThisType<T>` object contextual typing. |
| **05** | [Generic Callbacks and Contextual Typing](file:///E:/Learning/engineering-notes/01-languages/typescript/02-functions/05-generic-callbacks-and-contextual-typing.md) | Advanced | TS 3.4 / TS 5.4 | Higher-order generics, bidirectional inference, and `NoInfer<T>` locking. |
| **06** | [Assertion Signatures and This Types](file:///E:/Learning/engineering-notes/01-languages/typescript/02-functions/06-assertion-signatures-and-this-types.md) | Advanced | TS 3.7 / TS 1.7 | Linear CFA mutation via `asserts`, polymorphic `this` returns, and `this is Type` method guards. |
| **07** | [Abstract Construct Signatures and Constructor Types](file:///E:/Learning/engineering-notes/01-languages/typescript/02-functions/07-abstract-construct-signatures-and-constructor-types.md) | Advanced | TS 0.9 / TS 4.2 | Constructor contracts, abstract construct signatures, mixins, and DI tokens. |

---

## Architectural Function Variance Matrix

```
+-------------------------------------------------------------------------+
|                  Function Subtyping and Variance Map                    |
|                                                                         |
|                          Base: (x: Animal) => Dog                       |
|                                     │                                   |
|      ┌──────────────────────────────┴──────────────────────────────┐    |
|      ▼                                                             ▼    |
|  Parameter Position:                                  Return Position:  |
|  CONTRAVARIANT                                        COVARIANT         |
|  Accepts Supertype of Animal                          Accepts Subtype   |
|  (e.g. Object, Creature)                              of Dog            |
|  Inverted direction:                                  Preserves         |
|  (x: Creature) => Dog is assignable to                direction:        |
|  (x: Animal) => Dog                                   () => GermanShep  |
|                                                       is assignable to  |
|                                                       () => Dog         |
+-------------------------------------------------------------------------+
```

---

## Recommended Learning Sequence

1. **Core Mechanics**: Start with `00-function-types.md` and `01-parameters-and-return-types.md` to master parameter lists, default initializers, tuple rest parameters, and variance.
2. **Callbacks & Overloading**: Study `02-callbacks.md` (understand `void` callback substitutability) and `03-function-overloads.md` (conditional return type mapping).
3. **Execution Context**: Read `04-this-parameters.md` to master fake `this` parameters and `ThisType<T>` options typing.
4. **Advanced Inference & Composition**: Progress to `05-generic-callbacks-and-contextual-typing.md` (leveraging `NoInfer`) and `06-assertion-signatures-and-this-types.md` (linear control-flow mutations and polymorphic `this`).
5. **Architectural Instantiation**: Complete the module with `07-abstract-construct-signatures-and-constructor-types.md` to build mixin pipelines and dependency injection containers.
