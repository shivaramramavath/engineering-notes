# 14 - Type System Internals

What TypeScript's types actually are, how the compiler relates them, and where the system is deliberately loose. The earlier sections teach you to *use* types. This one explains *why they behave as they do*: why types disappear at runtime, what "assignable" really means, why `Dog[]` can stand in for `Animal[]`, when a literal widens, where the type system cannot be trusted, and how the compiler is organized.

You can write good TypeScript without reading this section. It pays off when you are debugging a confusing error, designing a library's public types, reviewing risky casts, or tracking down slow type-checking.

## Prerequisites

- [06 Generics](../06-generics/README.md) and [10 Advanced Types](../10-advanced-types/README.md): variance and inference rely on them
- [04 Objects and Interfaces: structural typing](../04-objects-and-interfaces/04-structural-typing.md)
- [13 Compiler and tsconfig](../13-compiler-and-tsconfig/README.md): especially [strict mode](../13-compiler-and-tsconfig/01-strict-mode.md)

## Notes in this section

| # | Note | Covers |
|---|---|---|
| 00 | [Type erasure and runtime](./00-type-erasure-and-runtime.md) | What disappears at compile time, what emits code, why you cannot check types at runtime |
| 01 | [Assignability and subtyping](./01-assignability-and-subtyping.md) | The compatibility rules, `unknown`/`never`/`any`, freshness, weak types, reading errors |
| 02 | [Variance](./02-variance.md) | Covariance, contravariance, invariance, bivariance, `in`/`out` annotations |
| 03 | [Type widening and inference](./03-type-widening-and-inference.md) | `const` vs `let`, literal widening, contextual typing, generic inference, `const` type parameters |
| 04 | [Soundness and escape hatches](./04-soundness-and-escape-hatches.md) | Where TypeScript is unsound, the escape hatches ranked by safety, containing risk |
| 05 | [Compiler architecture](./05-compiler-architecture.md) | Scanner, parser, binder, checker, emitter, `Program`, language service, performance tooling |

Read 00 to 02 in order. 03 and 04 can follow in either order. 05 is background that explains tooling and performance behavior.

## Which note answers my question?

| Question | Go to |
|---|---|
| Why can't I use `instanceof` with an interface? | [00](./00-type-erasure-and-runtime.md) |
| Why does `JSON.parse(...) as User` not protect me? | [00](./00-type-erasure-and-runtime.md) and [04](./04-soundness-and-escape-hatches.md) |
| Why does this extra property error only sometimes appear? | [01](./01-assignability-and-subtyping.md) (freshness) |
| What does "Type X is not assignable to type Y" mean, and how do I read it? | [01](./01-assignability-and-subtyping.md) |
| Why is `Dog[]` assignable to `Animal[]`, but `(d: Dog) => void` is not assignable to `(a: Animal) => void`? | [02](./02-variance.md) |
| What do `in` and `out` on a type parameter do? | [02](./02-variance.md) |
| Why is my object property `string`, not `"dark"`? | [03](./03-type-widening-and-inference.md) |
| Why did a generic infer `unknown` or a weird union? | [03](./03-type-widening-and-inference.md) |
| What are the risks of `as`, `!`, and `any`? Which one should I use? | [04](./04-soundness-and-escape-hatches.md) |
| Where can TypeScript's types lie to me? | [04](./04-soundness-and-escape-hatches.md) |
| Why is type-checking slow, and why is stripping types fast? | [05](./05-compiler-architecture.md) |
| Why do the editor and `tsc` disagree? | [05](./05-compiler-architecture.md) |

## Ideas that recur across the section

- **Types are erased.** They guide the compiler and vanish in the output. Anything needed at runtime must exist as a value.
- **Compatibility is structural.** Shape, not name, decides assignability, except for private and protected members.
- **Direction matters.** Whether `F<Dog>` fits `F<Animal>` depends on how `F` uses its parameter: outputs are covariant, inputs contravariant.
- **Inference follows rules.** Mutable positions widen literals. Context flows inward. Generic arguments contribute candidates.
- **Types are claims, not proofs.** Assertions, `any`, declaration files, and external data can all make the types disagree with reality.
- **Checking is separate from emitting.** The compiler has distinct stages, and many tools use only some of them.

## Related sections

- [03 Unions and Narrowing](../03-unions-and-narrowing/README.md): narrowing is control-flow refinement of declared types
- [09 Declaration Files](../09-declaration-files/README.md): declarations are unchecked claims about runtime code
- [10 Advanced Types: branded types](../10-advanced-types/07-branded-types.md): nominal-style typing on top of structural rules
- [15 Runtime Validation](../15-runtime-validation/README.md): checking the data that types only describe
- [18 Testing and Debugging: reading type errors](../18-testing-and-debugging/06-reading-type-errors.md)
- [22 Performance: type-checking performance](../22-performance/00-type-checking-performance.md)
- [23 Security: unsafe types and assertions](../23-security/00-unsafe-types-and-assertions.md)
- [27 Interview: type system and runtime](../27-interview/01-type-system-and-runtime.md)

## Next

[15 Runtime Validation](../15-runtime-validation/README.md)
